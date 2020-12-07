/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Client.h"

// Global includes
#include "BackgroundParent.h"
#include "mozilla/Assertions.h"
#include "mozilla/SpinEventLoopUntil.h"

namespace mozilla::dom::quota {

using mozilla::ipc::AssertIsOnBackgroundThread;

namespace {

const char kIDBPrefix = 'I';
const char kDOMCachePrefix = 'C';
const char kSDBPrefix = 'S';
const char kLSPrefix = 'L';

/**
 * If shutdown takes this long, kill actors of a quota client, to avoid reaching
 * the crash timeout.
 */
#define SHUTDOWN_FORCE_KILL_TIMEOUT_MS 5000

/**
 * Automatically crash the browser if shutdown of a quota client takes this
 * long. We've chosen a value that is longer than the value for QuotaManager
 * shutdown timer which is currently set to 30 seconds, so that the QuotaManager
 * abort may still make the quota client's shutdown complete in time.  We've
 * also chosen a value that is long enough that it is unlikely for the problem
 * to be falsely triggered by slow system I/O.  We've also chosen a value long
 * enough so that automated tests should time out and fail if shutdown of a
 * quota client takes too long.  Also, this value is long enough so that testers
 * can notice the timeout; we want to know about the timeouts, not hide them. On
 * the other hand this value is less than 60 seconds which is used by
 * nsTerminator to crash a hung main process.
 */
#define SHUTDOWN_FORCE_CRASH_TIMEOUT_MS 45000

template <Client::Type type>
struct ClientTypeTraits;

template <>
struct ClientTypeTraits<Client::Type::IDB> {
  template <typename T>
  static void To(T& aData) {
    aData.AssignLiteral(IDB_DIRECTORY_NAME);
  }

  static void To(char& aData) { aData = kIDBPrefix; }

  template <typename T>
  static bool From(const T& aData) {
    return aData.EqualsLiteral(IDB_DIRECTORY_NAME);
  }

  static bool From(char aData) { return aData == kIDBPrefix; }
};

template <>
struct ClientTypeTraits<Client::Type::DOMCACHE> {
  template <typename T>
  static void To(T& aData) {
    aData.AssignLiteral(DOMCACHE_DIRECTORY_NAME);
  }

  static void To(char& aData) { aData = kDOMCachePrefix; }

  template <typename T>
  static bool From(const T& aData) {
    return aData.EqualsLiteral(DOMCACHE_DIRECTORY_NAME);
  }

  static bool From(char aData) { return aData == kDOMCachePrefix; }
};

template <>
struct ClientTypeTraits<Client::Type::SDB> {
  template <typename T>
  static void To(T& aData) {
    aData.AssignLiteral(SDB_DIRECTORY_NAME);
  }

  static void To(char& aData) { aData = kSDBPrefix; }

  template <typename T>
  static bool From(const T& aData) {
    return aData.EqualsLiteral(SDB_DIRECTORY_NAME);
  }

  static bool From(char aData) { return aData == kSDBPrefix; }
};

template <>
struct ClientTypeTraits<Client::Type::LS> {
  template <typename T>
  static void To(T& aData) {
    aData.AssignLiteral(LS_DIRECTORY_NAME);
  }

  static void To(char& aData) { aData = kLSPrefix; }

  template <typename T>
  static bool From(const T& aData) {
    return aData.EqualsLiteral(LS_DIRECTORY_NAME);
  }

  static bool From(char aData) { return aData == kLSPrefix; }
};

template <typename T>
bool TypeTo_impl(Client::Type aType, T& aData) {
  switch (aType) {
    case Client::IDB:
      ClientTypeTraits<Client::Type::IDB>::To(aData);
      return true;

    case Client::DOMCACHE:
      ClientTypeTraits<Client::Type::DOMCACHE>::To(aData);
      return true;

    case Client::SDB:
      ClientTypeTraits<Client::Type::SDB>::To(aData);
      return true;

    case Client::LS:
      if (CachedNextGenLocalStorageEnabled()) {
        ClientTypeTraits<Client::Type::LS>::To(aData);
        return true;
      }
      [[fallthrough]];

    case Client::TYPE_MAX:
    default:
      return false;
  }

  MOZ_CRASH("Should never get here!");
}

template <typename T>
bool TypeFrom_impl(const T& aData, Client::Type& aType) {
  if (ClientTypeTraits<Client::Type::IDB>::From(aData)) {
    aType = Client::IDB;
    return true;
  }

  if (ClientTypeTraits<Client::Type::DOMCACHE>::From(aData)) {
    aType = Client::DOMCACHE;
    return true;
  }

  if (ClientTypeTraits<Client::Type::SDB>::From(aData)) {
    aType = Client::SDB;
    return true;
  }

  if (CachedNextGenLocalStorageEnabled() &&
      ClientTypeTraits<Client::Type::LS>::From(aData)) {
    aType = Client::LS;
    return true;
  }

  return false;
}

void BadType() { MOZ_CRASH("Bad client type value!"); }

}  // namespace

// static
bool Client::IsValidType(Type aType) {
  switch (aType) {
    case Client::IDB:
    case Client::DOMCACHE:
    case Client::SDB:
      return true;

    case Client::LS:
      if (CachedNextGenLocalStorageEnabled()) {
        return true;
      }
      [[fallthrough]];

    default:
      return false;
  }
}

// static
bool Client::TypeToText(Type aType, nsAString& aText, const fallible_t&) {
  nsString text;
  if (!TypeTo_impl(aType, text)) {
    return false;
  }
  aText = text;
  return true;
}

// static
nsAutoCString Client::TypeToText(Type aType) {
  nsAutoCString res;
  if (!TypeTo_impl(aType, res)) {
    BadType();
  }
  return res;
}

// static
bool Client::TypeFromText(const nsAString& aText, Type& aType,
                          const fallible_t&) {
  Type type;
  if (!TypeFrom_impl(aText, type)) {
    return false;
  }
  aType = type;
  return true;
}

// static
Client::Type Client::TypeFromText(const nsACString& aText) {
  Type type;
  if (!TypeFrom_impl(aText, type)) {
    BadType();
  }
  return type;
}

// static
char Client::TypeToPrefix(Type aType) {
  char prefix;
  if (!TypeTo_impl(aType, prefix)) {
    BadType();
  }
  return prefix;
}

// static
bool Client::TypeFromPrefix(char aPrefix, Type& aType, const fallible_t&) {
  Type type;
  if (!TypeFrom_impl(aPrefix, type)) {
    return false;
  }
  aType = type;
  return true;
}

void Client::MaybeRecordShutdownStep(const nsACString& aStepDescription) {
  AssertIsOnBackgroundThread();

  if (!mShutdownStartedAt) {
    // We are not shutting down yet, we intentionally ignore this here to avoid
    // that every caller has to make a distinction for shutdown vs. non-shutdown
    // situations.
    return;
  }

  const TimeDuration elapsedSinceShutdownStart =
      TimeStamp::NowLoRes() - *mShutdownStartedAt;

  const auto stepString =
      nsPrintfCString("%fs: %s", elapsedSinceShutdownStart.ToSeconds(),
                      nsPromiseFlatCString(aStepDescription).get());

  mShutdownSteps.Append(stepString + "\n"_ns);

#ifdef DEBUG
  // XXX Probably this isn't the mechanism that should be used here.

  NS_DebugBreak(
      NS_DEBUG_WARNING,
      nsAutoCString(TypeToText(GetType()) + " shutdown step"_ns).get(),
      stepString.get(), __FILE__, __LINE__);
#endif
}

bool Client::InitiateShutdownWorkThreads() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownTimer);
  MOZ_ASSERT(mShutdownSteps.IsEmpty());

  mShutdownStartedAt.init(TimeStamp::NowLoRes());

  MaybeRecordShutdownStep("starting"_ns);

  InitiateShutdown();

  if (!IsShutdownCompleted()) {
    mShutdownTimer = NS_NewTimer();

    MOZ_ALWAYS_SUCCEEDS(mShutdownTimer->InitWithNamedFuncCallback(
        [](nsITimer* aTimer, void* aClosure) {
          auto* const quotaClient = static_cast<Client*>(aClosure);

          quotaClient->ForceKillActors();

          MOZ_ALWAYS_SUCCEEDS(aTimer->InitWithNamedFuncCallback(
              [](nsITimer* aTimer, void* aClosure) {
                auto* const quotaClient = static_cast<Client*>(aClosure);

                MOZ_DIAGNOSTIC_ASSERT(!quotaClient->IsShutdownCompleted());

                const auto type = TypeToText(quotaClient->GetType());

                CrashReporter::AnnotateCrashReport(
                    CrashReporter::Annotation::QuotaManagerShutdownTimeout,
                    nsPrintfCString("%s: %s\nIntermediate steps:\n%s",
                                    type.get(),
                                    quotaClient->GetShutdownStatus().get(),
                                    quotaClient->mShutdownSteps.get()));

                MOZ_CRASH_UNSAFE_PRINTF("%s shutdown timed out", type.get());
              },
              aClosure, SHUTDOWN_FORCE_CRASH_TIMEOUT_MS,
              nsITimer::TYPE_ONE_SHOT,
              "quota::Client::ShutdownWorkThreads::ForceCrashTimer"));
        },
        this, SHUTDOWN_FORCE_KILL_TIMEOUT_MS, nsITimer::TYPE_ONE_SHOT,
        "quota::Client::ShutdownWorkThreads::ForceKillTimer"));

    return true;
  }

  return false;
}

void Client::FinalizeShutdownWorkThreads() {
  MOZ_ASSERT(mShutdownStartedAt);

  if (mShutdownTimer) {
    MOZ_ALWAYS_SUCCEEDS(mShutdownTimer->Cancel());
  }

  MaybeRecordShutdownStep("completed"_ns);

  FinalizeShutdown();
}

}  // namespace mozilla::dom::quota
