/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaCommon.h"

#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryComms.h"
#include "mozilla/TelemetryEventEnums.h"
#include "mozilla/TextUtils.h"
#include "nsIConsoleService.h"
#include "nsIFile.h"
#include "nsServiceManagerUtils.h"
#include "nsStringFlags.h"
#include "nsTStringRepr.h"
#include "nsUnicharUtils.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"

#ifdef XP_WIN
#  include "mozilla/Atomics.h"
#  include "mozilla/ipc/BackgroundParent.h"
#  include "mozilla/StaticPrefs_dom.h"
#  include "nsILocalFileWin.h"
#endif

namespace mozilla::dom::quota {

using namespace mozilla::Telemetry;

namespace {

#ifdef DEBUG
constexpr auto kDSStoreFileName = u".DS_Store"_ns;
constexpr auto kDesktopFileName = u".desktop"_ns;
constexpr auto kDesktopIniFileName = u"desktop.ini"_ns;
constexpr auto kThumbsDbFileName = u"thumbs.db"_ns;
#endif

#ifdef XP_WIN
Atomic<int32_t> gUseDOSDevicePathSyntax(-1);
#endif

LazyLogModule gLogger("QuotaManager");

void AnonymizeCString(nsACString& aCString, uint32_t aStart) {
  MOZ_ASSERT(!aCString.IsEmpty());
  MOZ_ASSERT(aStart < aCString.Length());

  char* iter = aCString.BeginWriting() + aStart;
  char* end = aCString.EndWriting();

  while (iter != end) {
    char c = *iter;

    if (IsAsciiAlpha(c)) {
      *iter = 'a';
    } else if (IsAsciiDigit(c)) {
      *iter = 'D';
    }

    ++iter;
  }
}

}  // namespace

const char kQuotaGenericDelimiter = '|';

#ifdef NIGHTLY_BUILD
const nsLiteralCString kQuotaInternalError = "internal"_ns;
const nsLiteralCString kQuotaExternalError = "external"_ns;
#endif

LogModule* GetQuotaManagerLogger() { return gLogger; }

void AnonymizeCString(nsACString& aCString) {
  if (aCString.IsEmpty()) {
    return;
  }
  AnonymizeCString(aCString, /* aStart */ 0);
}

void AnonymizeOriginString(nsACString& aOriginString) {
  if (aOriginString.IsEmpty()) {
    return;
  }

  int32_t start = aOriginString.FindChar(':');
  if (start < 0) {
    start = 0;
  }

  AnonymizeCString(aOriginString, start);
}

#ifdef XP_WIN
void CacheUseDOSDevicePathSyntaxPrefValue() {
  MOZ_ASSERT(XRE_IsParentProcess());
  AssertIsOnBackgroundThread();

  if (gUseDOSDevicePathSyntax == -1) {
    bool useDOSDevicePathSyntax =
        StaticPrefs::dom_quotaManager_useDOSDevicePathSyntax_DoNotUseDirectly();
    gUseDOSDevicePathSyntax = useDOSDevicePathSyntax ? 1 : 0;
  }
}
#endif

Result<nsCOMPtr<nsIFile>, nsresult> QM_NewLocalFile(const nsAString& aPath) {
  QM_TRY_UNWRAP(auto file,
                ToResultInvoke<nsCOMPtr<nsIFile>>(NS_NewLocalFile, aPath,
                                                  /* aFollowLinks */ false),
                QM_PROPAGATE, [&aPath](const nsresult rv) {
                  QM_WARNING("Failed to construct a file for path (%s)",
                             NS_ConvertUTF16toUTF8(aPath).get());
                });

#ifdef XP_WIN
  MOZ_ASSERT(gUseDOSDevicePathSyntax != -1);

  if (gUseDOSDevicePathSyntax) {
    QM_TRY_INSPECT(const auto& winFile,
                   ToResultGet<nsCOMPtr<nsILocalFileWin>>(
                       MOZ_SELECT_OVERLOAD(do_QueryInterface), file));

    MOZ_ASSERT(winFile);
    winFile->SetUseDOSDevicePathSyntax(true);
  }
#endif

  return file;
}

nsDependentCSubstring GetLeafName(const nsACString& aPath) {
  nsACString::const_iterator start, end;
  aPath.BeginReading(start);
  aPath.EndReading(end);

  bool found = RFindInReadable("/"_ns, start, end);
  if (found) {
    start = end;
  }

  aPath.EndReading(end);

  return nsDependentCSubstring(start.get(), end.get());
}

Result<nsCOMPtr<nsIFile>, nsresult> CloneFileAndAppend(
    nsIFile& aDirectory, const nsAString& aPathElement) {
  QM_TRY_UNWRAP(auto resultFile, MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<nsIFile>,
                                                            aDirectory, Clone));

  QM_TRY(resultFile->Append(aPathElement));

  return resultFile;
}

Result<nsIFileKind, nsresult> GetDirEntryKind(nsIFile& aFile) {
  QM_TRY_RETURN(
      MOZ_TO_RESULT_INVOKE(aFile, IsDirectory)
          .map([](const bool isDirectory) {
            return isDirectory ? nsIFileKind::ExistsAsDirectory
                               : nsIFileKind::ExistsAsFile;
          })
          .orElse([](const nsresult rv) -> Result<nsIFileKind, nsresult> {
            if (rv == NS_ERROR_FILE_NOT_FOUND ||
                rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST
#ifdef WIN32
                // We treat ERROR_FILE_CORRUPT as if the file did not exist at
                // all.
                || (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_WIN32 &&
                    NS_ERROR_GET_CODE(rv) == ERROR_FILE_CORRUPT)
#endif
            ) {
              return nsIFileKind::DoesNotExist;
            }

            return Err(rv);
          }));
}

Result<nsCOMPtr<mozIStorageStatement>, nsresult> CreateStatement(
    mozIStorageConnection& aConnection, const nsACString& aStatementString) {
  QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageStatement>,
                                           aConnection, CreateStatement,
                                           aStatementString));
}

template <SingleStepResult ResultHandling>
Result<SingleStepSuccessType<ResultHandling>, nsresult> ExecuteSingleStep(
    nsCOMPtr<mozIStorageStatement>&& aStatement) {
  QM_TRY_INSPECT(const bool& hasResult,
                 MOZ_TO_RESULT_INVOKE(aStatement, ExecuteStep));

  if constexpr (ResultHandling == SingleStepResult::AssertHasResult) {
    MOZ_ASSERT(hasResult);
    (void)hasResult;

    return WrapNotNullUnchecked(std::move(aStatement));
  } else {
    return hasResult ? std::move(aStatement) : nullptr;
  }
}

template Result<SingleStepSuccessType<SingleStepResult::AssertHasResult>,
                nsresult>
ExecuteSingleStep<SingleStepResult::AssertHasResult>(
    nsCOMPtr<mozIStorageStatement>&&);

template Result<SingleStepSuccessType<SingleStepResult::ReturnNullIfNoResult>,
                nsresult>
ExecuteSingleStep<SingleStepResult::ReturnNullIfNoResult>(
    nsCOMPtr<mozIStorageStatement>&&);

template <SingleStepResult ResultHandling>
Result<SingleStepSuccessType<ResultHandling>, nsresult>
CreateAndExecuteSingleStepStatement(mozIStorageConnection& aConnection,
                                    const nsACString& aStatementString) {
  QM_TRY_UNWRAP(auto stmt, MOZ_TO_RESULT_INVOKE_TYPED(
                               nsCOMPtr<mozIStorageStatement>, aConnection,
                               CreateStatement, aStatementString));

  return ExecuteSingleStep<ResultHandling>(std::move(stmt));
}

template Result<SingleStepSuccessType<SingleStepResult::AssertHasResult>,
                nsresult>
CreateAndExecuteSingleStepStatement<SingleStepResult::AssertHasResult>(
    mozIStorageConnection& aConnection, const nsACString& aStatementString);

template Result<SingleStepSuccessType<SingleStepResult::ReturnNullIfNoResult>,
                nsresult>
CreateAndExecuteSingleStepStatement<SingleStepResult::ReturnNullIfNoResult>(
    mozIStorageConnection& aConnection, const nsACString& aStatementString);

#ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
MOZ_THREAD_LOCAL(const nsACString*) ScopedLogExtraInfo::sQueryValue;
MOZ_THREAD_LOCAL(const nsACString*) ScopedLogExtraInfo::sContextValue;

/* static */
auto ScopedLogExtraInfo::FindSlot(const char* aTag) {
  // XXX For now, don't use a real map but just allow the known tag values.

  if (aTag == kTagQuery) {
    return &sQueryValue;
  }

  if (aTag == kTagContext) {
    return &sContextValue;
  }

  MOZ_CRASH("Unknown tag!");
}

ScopedLogExtraInfo::~ScopedLogExtraInfo() {
  if (mTag) {
    MOZ_ASSERT(&mCurrentValue == FindSlot(mTag)->get(),
               "Bad scoping of ScopedLogExtraInfo, must not be interleaved!");

    FindSlot(mTag)->set(mPreviousValue);
  }
}

ScopedLogExtraInfo::ScopedLogExtraInfo(ScopedLogExtraInfo&& aOther)
    : mTag(aOther.mTag),
      mPreviousValue(aOther.mPreviousValue),
      mCurrentValue(std::move(aOther.mCurrentValue)) {
  aOther.mTag = nullptr;
  FindSlot(mTag)->set(&mCurrentValue);
}

/* static */ ScopedLogExtraInfo::ScopedLogExtraInfoMap
ScopedLogExtraInfo::GetExtraInfoMap() {
  // This could be done in a cheaper way, but this is never called on a hot
  // path, so we anticipate using a real map inside here to make use simpler for
  // the caller(s).

  ScopedLogExtraInfoMap map;
  if (XRE_IsParentProcess()) {
    if (sQueryValue.get()) {
      map.emplace(kTagQuery, sQueryValue.get());
    }

    if (sContextValue.get()) {
      map.emplace(kTagContext, sContextValue.get());
    }
  }
  return map;
}

/* static */ void ScopedLogExtraInfo::Initialize() {
  MOZ_ALWAYS_TRUE(sQueryValue.init());
  MOZ_ALWAYS_TRUE(sContextValue.init());
}

void ScopedLogExtraInfo::AddInfo() {
  auto* slot = FindSlot(mTag);
  MOZ_ASSERT(slot);
  mPreviousValue = slot->get();

  slot->set(&mCurrentValue);
}
#endif

namespace detail {

nsDependentCSubstring GetSourceTreeBase() {
  static constexpr auto thisFileRelativeSourceFileName =
      "/dom/quota/QuotaCommon.cpp"_ns;

  static constexpr auto path = nsLiteralCString(__FILE__);

  MOZ_ASSERT(StringEndsWith(path, thisFileRelativeSourceFileName));
  return Substring(path, 0,
                   path.Length() - thisFileRelativeSourceFileName.Length());
}

nsDependentCSubstring MakeRelativeSourceFileName(
    const nsACString& aSourceFile) {
  static constexpr auto error = "ERROR"_ns;

  static const auto sourceTreeBase = GetSourceTreeBase();

  if (MOZ_LIKELY(StringBeginsWith(aSourceFile, sourceTreeBase))) {
    return Substring(aSourceFile, sourceTreeBase.Length() + 1);
  }

  nsCString::const_iterator begin, end;
  if (RFindInReadable("/"_ns, aSourceFile.BeginReading(begin),
                      aSourceFile.EndReading(end))) {
    // Use the basename as a fallback, to avoid exposing any user parts of the
    // path.
    ++begin;
    return Substring(begin, aSourceFile.EndReading(end));
  }

  return nsDependentCSubstring{static_cast<mozilla::Span<const char>>(
      static_cast<const nsCString&>(error))};
}

}  // namespace detail

void LogError(const nsACString& aExpr, const nsACString& aSourceFile,
              const int32_t aSourceLine, const Maybe<nsresult> aRv,
              const bool aIsWarning) {
#if defined(EARLY_BETA_OR_EARLIER) || defined(DEBUG)
  nsAutoCString extraInfosString;

  nsAutoCString rvName;
  if (aRv) {
    if (NS_ERROR_GET_MODULE(*aRv) == NS_ERROR_MODULE_WIN32) {
      // XXX We could also try to get the Win32 error name here.
      rvName = nsPrintfCString("WIN32(0x%" PRIX16 ")", NS_ERROR_GET_CODE(*aRv));
    } else {
      rvName = mozilla::GetStaticErrorName(*aRv);
    }
    extraInfosString.AppendPrintf(
        " failed with "
        "result 0x%" PRIX32 "%s%s%s",
        static_cast<uint32_t>(*aRv), !rvName.IsEmpty() ? " (" : "",
        !rvName.IsEmpty() ? rvName.get() : "", !rvName.IsEmpty() ? ")" : "");
  }

  const auto relativeSourceFile =
      detail::MakeRelativeSourceFileName(aSourceFile);
#endif

#ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
  const auto& extraInfos = ScopedLogExtraInfo::GetExtraInfoMap();
  for (const auto& item : extraInfos) {
    extraInfosString.Append(", "_ns + nsDependentCString(item.first) + "="_ns +
                            *item.second);
  }
#endif

#ifdef DEBUG
  NS_DebugBreak(NS_DEBUG_WARNING, nsAutoCString("QM_TRY failure"_ns).get(),
                (extraInfosString.IsEmpty()
                     ? nsPromiseFlatCString(aExpr)
                     : static_cast<const nsCString&>(
                           nsAutoCString(aExpr + extraInfosString)))
                    .get(),
                nsPromiseFlatCString(relativeSourceFile).get(), aSourceLine);
#endif

#if defined(EARLY_BETA_OR_EARLIER) || defined(DEBUG)
  nsCOMPtr<nsIConsoleService> console =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (console) {
    NS_ConvertUTF8toUTF16 message("QM_TRY failure: '"_ns + aExpr + "' at "_ns +
                                  relativeSourceFile + ":"_ns +
                                  IntToCString(aSourceLine) + extraInfosString);

    // The concatenation above results in a message like:
    // QM_TRY failure: 'EXPR' failed with result NS_ERROR_FAILURE at
    // dom/quota/Foo.cpp:12345

    console->LogStringMessage(message.get());
  }

#  ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
  if (const auto contextIt = extraInfos.find(ScopedLogExtraInfo::kTagContext);
      contextIt != extraInfos.cend()) {
    // For now, we don't include aExpr in the telemetry event. It might help to
    // match locations across versions, but they might be large.
    auto extra = Some([&] {
      auto res = CopyableTArray<EventExtraEntry>{};
      res.SetCapacity(6);
      // TODO We could still fill the module field, based on the source
      // directory, but we probably don't need to.
      // res.AppendElement(EventExtraEntry{"module"_ns, aModule});
      res.AppendElement(
          EventExtraEntry{"source_file"_ns, nsCString(relativeSourceFile)});
      res.AppendElement(
          EventExtraEntry{"source_line"_ns, IntToCString(aSourceLine)});
      res.AppendElement(EventExtraEntry{
          "context"_ns, nsPromiseFlatCString{*contextIt->second}});
      res.AppendElement(EventExtraEntry{
          "severity"_ns, aIsWarning ? "WARNING"_ns : "ERROR"_ns});

      if (!rvName.IsEmpty()) {
        res.AppendElement(EventExtraEntry{"result"_ns, nsCString{rvName}});
      }

      // The sequence number is currently per-process, and we don't record the
      // thread id. This is ok as long as we only record during storage
      // initialization, and that happens (mostly) from a single thread. It's
      // safe even if errors from multiple threads are interleaved, but the data
      // will be hard to analyze then. In that case, we should record a pair of
      // thread id and thread-local sequence number.
      static Atomic<int32_t> sSequenceNumber{0};

      res.AppendElement(
          EventExtraEntry{"seq"_ns, IntToCString(++sSequenceNumber)});

      return res;
    }());

    Telemetry::RecordEvent(Telemetry::EventID::DomQuotaTry_Error_Step,
                           Nothing(), extra);
  }
#  endif
#endif
}

#ifdef DEBUG
Result<bool, nsresult> WarnIfFileIsUnknown(nsIFile& aFile,
                                           const char* aSourceFile,
                                           const int32_t aSourceLine) {
  nsString leafName;
  nsresult rv = aFile.GetLeafName(leafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  bool isDirectory;
  rv = aFile.IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return Err(rv);
  }

  if (!isDirectory) {
    // Don't warn about OS metadata files. These files are only used in
    // different platforms, but the profile can be shared across different
    // operating systems, so we check it on all platforms.
    if (leafName.Equals(kDSStoreFileName) ||
        leafName.Equals(kDesktopFileName) ||
        leafName.Equals(kDesktopIniFileName,
                        nsCaseInsensitiveStringComparator) ||
        leafName.Equals(kThumbsDbFileName, nsCaseInsensitiveStringComparator)) {
      return false;
    }

    // Don't warn about files starting with ".".
    if (leafName.First() == char16_t('.')) {
      return false;
    }
  }

  NS_DebugBreak(
      NS_DEBUG_WARNING,
      nsPrintfCString("Something (%s) in the directory that doesn't belong!",
                      NS_ConvertUTF16toUTF8(leafName).get())
          .get(),
      nullptr, aSourceFile, aSourceLine);

  return true;
}
#endif

}  // namespace mozilla::dom::quota
