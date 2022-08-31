/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Client.h"

// Global includes
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::IsOnBackgroundThread;

namespace {

const char kIDBPrefix = 'I';
const char kDOMCachePrefix = 'C';
const char kSDBPrefix = 'S';
const char kFILESYSTEMPrefix = 'F';
const char kLSPrefix = 'L';

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
struct ClientTypeTraits<Client::Type::FILESYSTEM> {
  template <typename T>
  static void To(T& aData) {
    aData.AssignLiteral(FILESYSTEM_DIRECTORY_NAME);
  }

  static void To(char& aData) { aData = kFILESYSTEMPrefix; }

  template <typename T>
  static bool From(const T& aData) {
    return aData.EqualsLiteral(FILESYSTEM_DIRECTORY_NAME);
  }

  static bool From(char aData) { return aData == kFILESYSTEMPrefix; }
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

    case Client::FILESYSTEM:
      ClientTypeTraits<Client::Type::FILESYSTEM>::To(aData);
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

  if (ClientTypeTraits<Client::Type::FILESYSTEM>::From(aData)) {
    aType = Client::FILESYSTEM;
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

bool Client::InitiateShutdownWorkThreads() {
  AssertIsOnBackgroundThread();

  QuotaManager::MaybeRecordQuotaClientShutdownStep(GetType(), "starting"_ns);

  InitiateShutdown();

  return IsShutdownCompleted();
}

void Client::FinalizeShutdownWorkThreads() {
  QuotaManager::MaybeRecordQuotaClientShutdownStep(GetType(), "completed"_ns);

  FinalizeShutdown();
}

// static
bool Client::IsShuttingDownOnBackgroundThread() {
  MOZ_ASSERT(IsOnBackgroundThread());
  return QuotaManager::IsShuttingDown();
}

// static
bool Client::IsShuttingDownOnNonBackgroundThread() {
  MOZ_ASSERT(!IsOnBackgroundThread());
  return QuotaManager::IsShuttingDown();
}

}  // namespace mozilla::dom::quota
