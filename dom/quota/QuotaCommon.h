/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotacommon_h__
#define mozilla_dom_quota_quotacommon_h__

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsTArray.h"

#define MOZ_UNIQUE_VAR(base) MOZ_CONCAT(base, __LINE__)

#define MOZ_REMOVE_PAREN(X) MOZ_REMOVE_PAREN_HELPER2(MOZ_REMOVE_PAREN_HELPER X)
#define MOZ_REMOVE_PAREN_HELPER(...) MOZ_REMOVE_PAREN_HELPER __VA_ARGS__
#define MOZ_REMOVE_PAREN_HELPER2(...) MOZ_REMOVE_PAREN_HELPER3(__VA_ARGS__)
#define MOZ_REMOVE_PAREN_HELPER3(...) MOZ_REMOVE_PAREN_HELPER4_##__VA_ARGS__
#define MOZ_REMOVE_PAREN_HELPER4_MOZ_REMOVE_PAREN_HELPER

#define BEGIN_QUOTA_NAMESPACE \
  namespace mozilla {         \
  namespace dom {             \
  namespace quota {
#define END_QUOTA_NAMESPACE \
  } /* namespace quota */   \
  } /* namespace dom */     \
  } /* namespace mozilla */
#define USING_QUOTA_NAMESPACE using namespace mozilla::dom::quota;

#define DSSTORE_FILE_NAME ".DS_Store"
#define DESKTOP_FILE_NAME ".desktop"
#define DESKTOP_INI_FILE_NAME "desktop.ini"
#define THUMBS_DB_FILE_NAME "thumbs.db"

#define QM_WARNING(...)                                                      \
  do {                                                                       \
    nsPrintfCString str(__VA_ARGS__);                                        \
    mozilla::dom::quota::ReportInternalError(__FILE__, __LINE__, str.get()); \
    NS_WARNING(str.get());                                                   \
  } while (0)

#define UNKNOWN_FILE_WARNING(_leafName)                                       \
  NS_WARNING(                                                                 \
      nsPrintfCString("Something (%s) in the directory that doesn't belong!", \
                      NS_ConvertUTF16toUTF8(_leafName).get())                 \
          .get())

// This macro should be used in directory traversals for files or directories
// that are unknown for given directory traversal. It should only be called
// after all known (directory traversal specific) files or directories have
// been checked and handled.
#ifdef DEBUG
#  define WARN_IF_FILE_IS_UNKNOWN(_file) \
    mozilla::dom::quota::WarnIfFileIsUnknown(_file, __FILE__, __LINE__)
#else
#  define WARN_IF_FILE_IS_UNKNOWN(_file) Result<bool, nsresult>(false)
#endif

/**
 * There are multiple ways to handle unrecoverable conditions:
 * 1. Using NS_ENSURE_* macros
 *
 * Mainly:
 * - NS_ENSURE_SUCCESS
 * - NS_ENSURE_SUCCESS_VOID
 * - NS_ENSURE_TRUE
 * - NS_ENSURE_TRUE_VOID
 *
 * Typical use cases:
 *
 * nsresult MyFunc1(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   NS_ENSURE_SUCCESS(rv, rv);
 *   NS_ENSURE_TRUE(exists, NS_ERROR_FAILURE);
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc2(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);
 *   NS_ENSURE_TRUE(exists, NS_ERROR_UNEXPECTED);
 *
 *   return NS_OK;
 * }
 *
 * void MyFunc3(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   NS_ENSURE_SUCCESS_VOID(rv);
 *   NS_ENSURE_TRUE_VOID(exists);
 * }
 *
 * nsresult MyFunc4(nsIFile& aFile) {
 *   // NS_ENSURE_SUCCESS can't run an additional cleanup function
 *
 *   return NS_OK;
 * }
 *
 * 2. Using NS_WARN_IF macro with own control flow handling
 *
 * Typical use cases:
 *
 * nsresult MyFunc1(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   if (NS_WARN_IF(NS_FAILED(rv)) {
 *     return rv;
 *   }
 *   if (NS_WARN_IF(!exists) {
 *     return NS_ERROR_FAILURE;
 *   }
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc2(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   if (NS_WARN_IF(NS_FAILED(rv)) {
 *     return NS_ERROR_UNEXPECTED;
 *   }
 *   if (NS_WARN_IF(!exists) {
 *     return NS_ERROR_UNEXPECTED;
 *   }
 *
 *   return NS_OK;
 * }
 *
 * void MyFunc3(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   if (NS_WARN_IF(NS_FAILED(rv)) {
 *     return;
 *   }
 *   if (NS_WARN_IF(!exists) {
 *     return;
 *   }
 * }
 *
 * nsresult MyFunc4(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   if (NS_WARN_IF(NS_FAILED(rv)) {
 *     NS_WARNING("The Exists call failed!");
 *     return rv;
 *   }
 *   if (NS_WARN_IF(!exists) {
 *     NS_WARNING("The file doesn't exist!");
 *     return NS_ERROR_FAILURE;
 *   }
 *
 *   return NS_OK;
 * }
 *
 * 3. Using MOZ_TRY/MOZ_TRY_VAR macros
 *
 * Typical use cases:
 *
 * nsresult MyFunc1(nsIFile& aFile) {
 *   bool exists;
 *   MOZ_TRY(aFile.Exists(&exists));
 *   // Note, exists can't be checked directly using MOZ_TRY without the Result
 *   // extension defined in quota manager
 *   MOZ_TRY(OkIf(exists), NS_ERROR_FAILURE);
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc2(nsIFile& aFile) {
 *   // MOZ_TRY can't return a custom return value
 *
 *   return NS_OK;
 * }
 *
 * void MyFunc3(nsIFile& aFile) {
 *   // MOZ_TRY can't return void
 * }
 *
 * nsresult MyFunc4(nsIFile& aFile) {
 *   // MOZ_TRY can't run an additional cleanup function
 *
 *   return NS_OK;
 * }
 *
 * 4. Using QM_TRY/QM_TRY_VAR macros (Quota manager specific, defined below)
 *
 * Typical use cases:
 *
 * nsresult MyFunc1(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(aFile.Exists(&exists));
 *   QM_TRY(OkIf(exists), NS_ERROR_FAILURE);
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc2(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(aFile.Exists(&exists), NS_ERROR_UNEXPECTED);
 *   QM_TRY(OkIf(exists), NS_ERROR_UNEXPECTED);
 *
 *   return NS_OK;
 * }
 *
 * void MyFunc3(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(aFile.Exists(&exists), QM_VOID);
 *   QM_TRY(OkIf(exists), QM_VOID);
 * }
 *
 * nsresult MyFunc4(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(storageFile->Exists(&exists), QM_PROPAGATE,
 *          []() { NS_WARNING("The Exists call failed!"); });
 *   QM_TRY(OkIf(exists), NS_ERROR_FAILURE,
 *          []() { NS_WARNING("The file doesn't exist!"); });
 *
 *   return NS_OK;
 * }
 *
 * QM_TRY/QM_TRY_VAR is like MOZ_TRY/MOZ_TRY_VAR but if an error occurs it
 * additionally calls a generic function HandleError to handle the error and it
 * can be used to return custom return values as well and even call an
 * additional cleanup function.
 * HandleError currently only warns in debug builds, it will report to the
 * browser console and telemetry in the future.
 * The other advantage of QM_TRY/QM_TRY_VAR is that a local nsresult is not
 * needed at all in all cases, all calls can be wrapped directly. If an error
 * occurs, the warning contains a concrete call instead of the rv local
 * variable. For example:
 *
 * 1. WARNING: NS_ENSURE_SUCCESS(rv, rv) failed with result 0x80004005
 * (NS_ERROR_FAILURE): file XYZ, line N
 *
 * 2. WARNING: 'NS_FAILED(rv)', file XYZ, line N
 *
 * 3. Nothing (MOZ_TRY doesn't warn)
 *
 * 4. WARNING: Error: 'aFile.Exists(&exists)', file XYZ, line N
 *
 * It's highly recommended to use QM_TRY/QM_TRY_VAR in new code for quota
 * manager and quota clients. Existing code should be incrementally converted
 * as needed.
 *
 * QM_TRY_VOID/QM_TRY_VAR_VOID is not defined on purpose since it's possible to
 * use QM_TRY/QM_TRY_VAR even in void functions. However, QM_TRY(Task(), )
 * would look odd so it's recommended to use a dummy define QM_VOID that
 * evaluates to nothing instead: QM_TRY(Task(), QM_VOID)
 */

#define QM_VOID

#define QM_PROPAGATE MOZ_UNIQUE_VAR(tryResult).propagateErr()

#define QM_MISSING_ARGS(...)                           \
  do {                                                 \
    static_assert(false, "Did you forget arguments?"); \
  } while (0)

// QM_TRY_PROPAGATE_ERR, QM_TRY_CUSTOM_RET_VAR and
// QM_TRY_CUSTOM_RET_VAR_WITH_CLEANUP macros are implementation details of
// QM_TRY and shouldn't be used directly.

// Handles the two arguments case when the eror is propagated.
#define QM_TRY_PROPAGATE_ERR(ns, expr)                                   \
  auto MOZ_UNIQUE_VAR(tryResult) = ::mozilla::ToResult(expr);            \
  if (MOZ_UNLIKELY(MOZ_UNIQUE_VAR(tryResult).isErr())) {                 \
    ns::HandleError(nsLiteralCString(#expr), nsLiteralCString(__FILE__), \
                    __LINE__);                                           \
    return QM_PROPAGATE;                                                 \
  }

// Handles the three arguments case when a custom return value needs to be
// returned
#define QM_TRY_CUSTOM_RET_VAR(ns, expr, customRetVal)                    \
  auto MOZ_UNIQUE_VAR(tryResult) = ::mozilla::ToResult(expr);            \
  if (MOZ_UNLIKELY(MOZ_UNIQUE_VAR(tryResult).isErr())) {                 \
    ns::HandleError(nsLiteralCString(#expr), nsLiteralCString(__FILE__), \
                    __LINE__);                                           \
    return customRetVal;                                                 \
  }

// Handles the four arguments case when a cleanup function needs to be called
// before a custom return value is returned
#define QM_TRY_CUSTOM_RET_VAR_WITH_CLEANUP(ns, expr, customRetVal, cleanup) \
  auto MOZ_UNIQUE_VAR(tryResult) = ::mozilla::ToResult(expr);               \
  if (MOZ_UNLIKELY(MOZ_UNIQUE_VAR(tryResult).isErr())) {                    \
    ns::HandleError(nsLiteralCString(#expr), nsLiteralCString(__FILE__),    \
                    __LINE__);                                              \
    cleanup();                                                              \
    return customRetVal;                                                    \
  }

// Chooses the final implementation macro for given argument count.
// It can be used by other modules to define module specific error handling.
#define QM_TRY_META(...)                                                      \
  MOZ_ARG_6(, ##__VA_ARGS__, QM_TRY_CUSTOM_RET_VAR_WITH_CLEANUP(__VA_ARGS__), \
            QM_TRY_CUSTOM_RET_VAR(__VA_ARGS__),                               \
            QM_TRY_PROPAGATE_ERR(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__),  \
            QM_MISSING_ARGS(__VA_ARGS__))

/**
 * QM_TRY(expr[, customRetVal, cleanup]) is the C++ equivalent of Rust's
 * `try!(expr);`. First, it evaluates expr, which must produce a Result value.
 * On success, it discards the result altogether. On error, it calls
 * HandleError and an additional cleanup function (if the third argument was
 * passed) and finally returns an error Result from the enclosing function or a
 * custom return value (if the second argument was passed).
 */
#define QM_TRY(...) QM_TRY_META(mozilla::dom::quota, ##__VA_ARGS__)

// QM_TRY_VAR_PROPAGATE_ERR, QM_TRY_VAR_CUSTOM_RET_VAR and
// QM_TRY_VAR_CUSTOM_RET_VAR_WITH_CLEANUP macros are implementation details of
// QM_TRY_VAR and shouldn't be used directly.

// Handles the three arguments case when the eror is propagated.
#define QM_TRY_VAR_PROPAGATE_ERR(ns, target, expr)                       \
  auto MOZ_UNIQUE_VAR(tryResult) = (expr);                               \
  if (MOZ_UNLIKELY(MOZ_UNIQUE_VAR(tryResult).isErr())) {                 \
    ns::HandleError(nsLiteralCString(#expr), nsLiteralCString(__FILE__), \
                    __LINE__);                                           \
    return QM_PROPAGATE;                                                 \
  }                                                                      \
  MOZ_REMOVE_PAREN(target) = MOZ_UNIQUE_VAR(tryResult).unwrap();

// Handles the four arguments case when a custom return value needs to be
// returned
#define QM_TRY_VAR_CUSTOM_RET_VAR(ns, target, expr, customRetVal)        \
  auto MOZ_UNIQUE_VAR(tryResult) = (expr);                               \
  if (MOZ_UNLIKELY(MOZ_UNIQUE_VAR(tryResult).isErr())) {                 \
    ns::HandleError(nsLiteralCString(#expr), nsLiteralCString(__FILE__), \
                    __LINE__);                                           \
    return customRetVal;                                                 \
  }                                                                      \
  MOZ_REMOVE_PAREN(target) = MOZ_UNIQUE_VAR(tryResult).unwrap();

// Handles the five arguments case when a cleanup function needs to be called
// before a custom return value is returned
#define QM_TRY_VAR_CUSTOM_RET_VAR_WITH_CLEANUP(ns, target, expr, customRetVal, \
                                               cleanup)                        \
  auto MOZ_UNIQUE_VAR(tryResult) = (expr);                                     \
  if (MOZ_UNLIKELY(MOZ_UNIQUE_VAR(tryResult).isErr())) {                       \
    ns::HandleError(nsLiteralCString(#expr), nsLiteralCString(__FILE__),       \
                    __LINE__);                                                 \
    cleanup();                                                                 \
    return customRetVal;                                                       \
  }                                                                            \
  MOZ_REMOVE_PAREN(target) = MOZ_UNIQUE_VAR(tryResult).unwrap();

// Chooses the final implementation macro for given argument count.
// It can be used by other modules to define module specific error handling.
#define QM_TRY_VAR_META(...)                                                \
  MOZ_ARG_7(                                                                \
      , ##__VA_ARGS__, QM_TRY_VAR_CUSTOM_RET_VAR_WITH_CLEANUP(__VA_ARGS__), \
      QM_TRY_VAR_CUSTOM_RET_VAR(__VA_ARGS__),                               \
      QM_TRY_VAR_PROPAGATE_ERR(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__),  \
      QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__))

/**
 * QM_TRY_VAR(target, expr[, customRetVal, cleanup]) is the C++ equivalent of
 * Rust's `target = try!(expr);`. First, it evaluates expr, which must produce
 * a Result value. On success, the result's success value is assigned to
 * target. On error, it calls HandleError and an additional cleanup function (
 * if the fourth argument was passed) and finally returns the error result or a
 * custom return value (if the third argument was passed). |target| must be an
 * lvalue.
 */
#define QM_TRY_VAR(...) QM_TRY_VAR_META(mozilla::dom::quota, ##__VA_ARGS__)

// Telemetry probes to collect number of failure during the initialization.
#ifdef NIGHTLY_BUILD
#  define REPORT_TELEMETRY_INIT_ERR(_key, _label)   \
    mozilla::Telemetry::AccumulateCategoricalKeyed( \
        mozilla::dom::quota::_key,                  \
        mozilla::Telemetry::LABELS_QM_INIT_TELEMETRY_ERROR::_label);

#  define REPORT_TELEMETRY_ERR_IN_INIT(_initializing, _key, _label) \
    do {                                                            \
      if (_initializing) {                                          \
        REPORT_TELEMETRY_INIT_ERR(_key, _label)                     \
      }                                                             \
    } while (0)

#  define RECORD_IN_NIGHTLY(_recorder, _status) \
    do {                                        \
      if (NS_SUCCEEDED(_recorder)) {            \
        _recorder = _status;                    \
      }                                         \
    } while (0)

#  define CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(_dummy) continue

#  define RETURN_STATUS_OR_RESULT(_status, _rv) \
    return NS_FAILED(_status) ? _status : _rv
#else
#  define REPORT_TELEMETRY_INIT_ERR(_key, _label) \
    {}

#  define REPORT_TELEMETRY_ERR_IN_INIT(_initializing, _key, _label) \
    {}

#  define RECORD_IN_NIGHTLY(_dummy, _status) \
    {}

#  define CONTINUE_IN_NIGHTLY_RETURN_IN_OTHERS(_rv) return _rv

#  define RETURN_STATUS_OR_RESULT(_status, _rv) return _rv
#endif

class nsIEventTarget;
class nsIFile;

namespace mozilla {

class LogModule;

struct NotOk {};

// Allow MOZ_TRY/QM_TRY to handle `bool` values by wrapping them with OkIf.
// TODO: Maybe move this to mfbt/ResultExtensions.h
inline Result<Ok, NotOk> OkIf(bool aValue) {
  if (aValue) {
    return Ok();
  }
  return Err(NotOk());
}

/**
 * Allow MOZ_TRY/QM_TRY to handle `nsresult` values and optionally enforce
 * success for some special values.
 * This is useful in cases when a specific error shouldn't abort execution of a
 * function and we need to distinguish between a normal success and a forced
 * success.
 *
 * Typical use case:
 *
 * nsresult MyFunc(nsIFile& aDirectory) {
 *   bool exists;
 *   QM_TRY_VAR(exists,
 *              ToResult(aDirectory.Create(nsIFile::DIRECTORY_TYPE, 0755),
 *                       [](auto aValue) {
 *                         return aValue == NS_ERROR_FILE_ALREADY_EXISTS;
 *                       }));
 *
 *   if (exists) {
 *     QM_TRY(aDirectory.IsDirectory(&isDirectory));
 *     QM_TRY(isDirectory, NS_ERROR_UNEXPECTED);
 *   }
 *
 *   return NS_OK;
 * }
 *
 * TODO: Maybe move this to mfbt/ResultExtensions.h
 */

class Okish {
  const bool mEnforced;

 public:
  explicit Okish(bool aEnforced) : mEnforced(aEnforced) {}

  MOZ_IMPLICIT operator bool() const { return mEnforced; }
};

template <typename SuccessEnforcer>
Result<Okish, nsresult> ToResult(nsresult aValue,
                                 const SuccessEnforcer& aSuccessEnforcer) {
  if (NS_SUCCEEDED(aValue)) {
    return Okish(/* aEnforced */ false);
  }
  if (aSuccessEnforcer(aValue)) {
    return Okish(/* aEnforced */ true);
  }
  return Err(aValue);
}

namespace dom {
namespace quota {

extern const char kQuotaGenericDelimiter;

// Telemetry keys to indicate types of errors.
#ifdef NIGHTLY_BUILD
extern const nsLiteralCString kQuotaInternalError;
extern const nsLiteralCString kQuotaExternalError;
#else
// No need for these when we're not collecting telemetry.
#  define kQuotaInternalError
#  define kQuotaExternalError
#endif

class BackgroundThreadObject {
 protected:
  nsCOMPtr<nsIEventTarget> mOwningThread;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  nsIEventTarget* OwningThread() const;

 protected:
  BackgroundThreadObject();

  explicit BackgroundThreadObject(nsIEventTarget* aOwningThread);
};

void AssertIsOnIOThread();

void AssertCurrentThreadOwnsQuotaMutex();

bool IsOnIOThread();

void ReportInternalError(const char* aFile, uint32_t aLine, const char* aStr);

LogModule* GetQuotaManagerLogger();

void AnonymizeCString(nsACString& aCString);

class AnonymizedCString : public nsCString {
 public:
  explicit AnonymizedCString(const nsACString& aCString) : nsCString(aCString) {
    AnonymizeCString(*this);
  }
};

void AnonymizeOriginString(nsACString& aOriginString);

class AnonymizedOriginString : public nsCString {
 public:
  explicit AnonymizedOriginString(const nsACString& aOriginString)
      : nsCString(aOriginString) {
    AnonymizeOriginString(*this);
  }
};

template <typename T>
void StringifyTableKeys(const T& aTable, nsACString& aResult) {
  bool first = true;
  for (auto iter = aTable.ConstIter(); !iter.Done(); iter.Next()) {
    if (first) {
      first = false;
    } else {
      aResult.Append(", "_ns);
    }

    const auto& key = iter.Get()->GetKey();

    aResult.Append(key);
  }
}

#ifdef XP_WIN
void CacheUseDOSDevicePathSyntaxPrefValue();
#endif

Result<nsCOMPtr<nsIFile>, nsresult> QM_NewLocalFile(const nsAString& aPath);

class IntString : public nsAutoString {
 public:
  explicit IntString(int64_t aInteger) { AppendInt(aInteger); }
};

class IntCString : public nsAutoCString {
 public:
  explicit IntCString(int64_t aInteger) { AppendInt(aInteger); }
};

#ifdef DEBUG
Result<bool, nsresult> WarnIfFileIsUnknown(nsIFile& aFile,
                                           const char* aSourceFile,
                                           int32_t aSourceLine);
#endif

void HandleError(const nsLiteralCString& aExpr,
                 const nsLiteralCString& aSourceFile, int32_t aSourceLine);

}  // namespace quota
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_quota_quotacommon_h__
