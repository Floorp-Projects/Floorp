/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotacommon_h__
#define mozilla_dom_quota_quotacommon_h__

#include "mozilla/ipc/ProtocolUtils.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsTArray.h"

// Proper use of unique variable names can be tricky (especially if nesting of
// the final macro is required).
// See https://lifecs.likai.org/2016/07/c-preprocessor-hygienic-macros.html
#define MOZ_UNIQUE_VAR(base) MOZ_CONCAT(base, __COUNTER__)

// See
// https://stackoverflow.com/questions/24481810/how-to-remove-the-enclosing-parentheses-with-macro
#define MOZ_REMOVE_PAREN(X) MOZ_REMOVE_PAREN_HELPER2(MOZ_REMOVE_PAREN_HELPER X)
#define MOZ_REMOVE_PAREN_HELPER(...) MOZ_REMOVE_PAREN_HELPER __VA_ARGS__
#define MOZ_REMOVE_PAREN_HELPER2(...) MOZ_REMOVE_PAREN_HELPER3(__VA_ARGS__)
#define MOZ_REMOVE_PAREN_HELPER3(...) MOZ_REMOVE_PAREN_HELPER4_##__VA_ARGS__
#define MOZ_REMOVE_PAREN_HELPER4_MOZ_REMOVE_PAREN_HELPER

// See https://florianjw.de/en/passing_overloaded_functions.html
// TODO: Add a test for this macro.
#define MOZ_SELECT_OVERLOAD(func)                         \
  [](auto&&... aArgs) -> decltype(auto) {                 \
    return func(std::forward<decltype(aArgs)>(aArgs)...); \
  }

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
 * There are multiple ways to handle unrecoverable conditions (note that the
 * patterns are put in reverse chronological order and only the first pattern
 * QM_TRY/QM_TRY_VAR/QM_FAIL should be used in new code):
 *
 * 1. Using QM_TRY/QM_TRY_VAR/QM_FAIL macros (Quota manager specific, defined
 *    below)
 *
 * Typical use cases:
 *
 * nsresult MyFunc1(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(aFile.Exists(&exists));
 *   QM_TRY(OkIf(exists), NS_ERROR_FAILURE);
 *
 *   // The file exists, and data could be read from it here.
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc2(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(aFile.Exists(&exists), NS_ERROR_UNEXPECTED);
 *   QM_TRY(OkIf(exists), NS_ERROR_UNEXPECTED);
 *
 *   // The file exists, and data could be read from it here.
 *
 *   return NS_OK;
 * }
 *
 * void MyFunc3(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(aFile.Exists(&exists), QM_VOID);
 *   QM_TRY(OkIf(exists), QM_VOID);
 *
 *   // The file exists, and data could be read from it here.
 * }
 *
 * nsresult MyFunc4(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(storageFile->Exists(&exists), QM_PROPAGATE,
 *          []() { NS_WARNING("The Exists call failed!"); });
 *   QM_TRY(OkIf(exists), NS_ERROR_FAILURE,
 *          []() { NS_WARNING("The file doesn't exist!"); });
 *
 *   // The file exists, and data could be read from it here.
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc5(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(aFile.Exists(&exists));
 *   if (exists) {
 *     // The file exists, and data could be read from it here.
 *   } else {
 *     QM_FAIL(NS_ERROR_FAILURE);
 *   }
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc6(nsIFile& aFile) {
 *   bool exists;
 *   QM_TRY(aFile.Exists(&exists));
 *   if (exists) {
 *     // The file exists, and data could be read from it here.
 *   } else {
 *     QM_FAIL(NS_ERROR_FAILURE,
 *             []() { NS_WARNING("The file doesn't exist!"); });
 *   }
 *
 *   return NS_OK;
 * }
 *
 * 2. Using MOZ_TRY/MOZ_TRY_VAR macros
 *
 * Typical use cases:
 *
 * nsresult MyFunc1(nsIFile& aFile) {
 *   // MOZ_TRY can't return a custom return value
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
 *   // MOZ_TRY can't return a custom return value, "void" in this case
 * }
 *
 * nsresult MyFunc4(nsIFile& aFile) {
 *   // MOZ_TRY can't return a custom return value and run an additional
 *   // cleanup function
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc5(nsIFile& aFile) {
 *   // There's no MOZ_FAIL, MOZ_TRY can't return a custom return value
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc6(nsIFile& aFile) {
 *   // There's no MOZ_FAIL, MOZ_TRY can't return a custom return value and run
 *   // an additional cleanup function
 *
 *   return NS_OK;
 * }
 *
 * 3. Using NS_WARN_IF and NS_WARNING macro with own control flow handling
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
 *   // The file exists, and data could be read from it here.
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
 *   // The file exists, and data could be read from it here.
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
 *
 *   // The file exists, and data could be read from it here.
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
 *   // The file exists, and data could be read from it here.
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc5(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   if (NS_WARN_IF(NS_FAILED(rv)) {
 *     return rv;
 *   }
 *   if (exists) {
 *     // The file exists, and data could be read from it here.
 *   } else {
 *     return NS_ERROR_FAILURE;
 *   }
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc6(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   if (NS_WARN_IF(NS_FAILED(rv)) {
 *     return rv;
 *   }
 *   if (exists) {
 *     // The file exists, and data could be read from it here.
 *   } else {
 *     NS_WARNING("The file doesn't exist!");
 *     return NS_ERROR_FAILURE;
 *   }
 *
 *   return NS_OK;
 * }
 *
 * 4. Using NS_ENSURE_* macros
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
 *   // The file exists, and data could be read from it here.
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
 *   // The file exists, and data could be read from it here.
 *
 *   return NS_OK;
 * }
 *
 * void MyFunc3(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   NS_ENSURE_SUCCESS_VOID(rv);
 *   NS_ENSURE_TRUE_VOID(exists);
 *
 *   // The file exists, and data could be read from it here.
 * }
 *
 * nsresult MyFunc4(nsIFile& aFile) {
 *   // NS_ENSURE_SUCCESS/NS_ENSURE_TRUE can't run an additional cleanup
 *   // function
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc5(nsIFile& aFile) {
 *   bool exists;
 *   nsresult rv = aFile.Exists(&exists);
 *   NS_ENSURE_SUCCESS(rv, rv);
 *   if (exists) {
 *     // The file exists, and data could be read from it here.
 *   } else {
 *     NS_ENSURE_TRUE(false, NS_ERROR_FAILURE);
 *   }
 *
 *   return NS_OK;
 * }
 *
 * nsresult MyFunc6(nsIFile& aFile) {
 *   // NS_ENSURE_TRUE can't run an additional cleanup function
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
 * QM_FAIL is a supplementary macro for cases when an error needs to be
 * returned without evaluating an expression. It's possible to write
 * QM_TRY(OkIf(false), NS_ERROR_FAILURE), but QM_FAIL(NS_ERROR_FAILURE) looks
 * more straightforward.
 *
 * It's highly recommended to use QM_TRY/QM_TRY_VAR/QM_FAIL in new code for
 * quota manager and quota clients. Existing code should be incrementally
 * converted as needed.
 *
 * QM_TRY_VOID/QM_TRY_VAR_VOID/QM_FAIL_VOID is not defined on purpose since
 * it's possible to use QM_TRY/QM_TRY_VAR/QM_FAIL even in void functions.
 * However, QM_TRY(Task(), ) would look odd so it's recommended to use a dummy
 * define QM_VOID that evaluates to nothing instead: QM_TRY(Task(), QM_VOID)
 */

#define QM_VOID

#define QM_PROPAGATE tryTempResult.propagateErr()

// QM_MISSING_ARGS and QM_HANDLE_ERROR macros are implementation details of
// QM_TRY/QM_TRY_VAR/QM_FAIL and shouldn't be used directly.

#define QM_MISSING_ARGS(...)                           \
  do {                                                 \
    static_assert(false, "Did you forget arguments?"); \
  } while (0)

#ifdef DEBUG
#  define QM_HANDLE_ERROR(expr) HandleError(#  expr, __FILE__, __LINE__)
#else
#  define QM_HANDLE_ERROR(expr) HandleError("Unavailable", __FILE__, __LINE__)
#endif

// QM_TRY_PROPAGATE_ERR, QM_TRY_CUSTOM_RET_VAL,
// QM_TRY_CUSTOM_RET_VAL_WITH_CLEANUP and QM_TRY_GLUE macros are implementation
// details of QM_TRY and shouldn't be used directly.

// Handles the three arguments case when the error is propagated.
#define QM_TRY_PROPAGATE_ERR(ns, tryResult, expr) \
  auto tryResult = ::mozilla::ToResult(expr);     \
  if (MOZ_UNLIKELY(tryResult.isErr())) {          \
    ns::QM_HANDLE_ERROR(expr);                    \
    return tryResult.propagateErr();              \
  }

// Handles the four arguments case when a custom return value needs to be
// returned
#define QM_TRY_CUSTOM_RET_VAL(ns, tryResult, expr, customRetVal) \
  auto tryResult = ::mozilla::ToResult(expr);                    \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                         \
    auto tryTempResult MOZ_MAYBE_UNUSED = std::move(tryResult);  \
    ns::QM_HANDLE_ERROR(expr);                                   \
    return customRetVal;                                         \
  }

// Handles the five arguments case when a cleanup function needs to be called
// before a custom return value is returned
#define QM_TRY_CUSTOM_RET_VAL_WITH_CLEANUP(ns, tryResult, expr, customRetVal, \
                                           cleanup)                           \
  auto tryResult = ::mozilla::ToResult(expr);                                 \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                      \
    auto tryTempResult MOZ_MAYBE_UNUSED = std::move(tryResult);               \
    ns::QM_HANDLE_ERROR(expr);                                                \
    cleanup(tryTempResult);                                                   \
    return customRetVal;                                                      \
  }

// Chooses the final implementation macro for given argument count.
// It can be used by other modules to define module specific error handling.
// This could use MOZ_PASTE_PREFIX_AND_ARG_COUNT, but explicit named suffxes
// read slightly better than plain numbers.
// See also
// https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros
#define QM_TRY_META(...)                                                       \
  {                                                                            \
    MOZ_ARG_7(, ##__VA_ARGS__,                                                 \
              QM_TRY_CUSTOM_RET_VAL_WITH_CLEANUP(__VA_ARGS__),                 \
              QM_TRY_CUSTOM_RET_VAL(__VA_ARGS__),                              \
              QM_TRY_PROPAGATE_ERR(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__), \
              QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__))      \
  }

// Specifies the namespace and generates unique variable name. This extra
// internal macro (along with __COUNTER__) allows nesting of the final macro.
#define QM_TRY_GLUE(...) \
  QM_TRY_META(mozilla::dom::quota, MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)

/**
 * QM_TRY(expr[, customRetVal, cleanup]) is the C++ equivalent of Rust's
 * `try!(expr);`. First, it evaluates expr, which must produce a Result value.
 * On success, it discards the result altogether. On error, it calls
 * HandleError and an additional cleanup function (if the third argument was
 * passed) and finally returns an error Result from the enclosing function or a
 * custom return value (if the second argument was passed).
 */
#define QM_TRY(...) QM_TRY_GLUE(__VA_ARGS__)

/**
 * QM_DEBUG_TRY works like QM_TRY in debug builds, it has has no effect in
 * non-debug builds.
 */
#ifdef DEBUG
#  define QM_DEBUG_TRY(...) QM_TRY(__VA_ARGS__)
#else
#  define QM_DEBUG_TRY(...)
#endif

// QM_TRY_VAR_PROPAGATE_ERR, QM_TRY_VAR_CUSTOM_RET_VAL,
// QM_TRY_VAR_CUSTOM_RET_VAL_WITH_CLEANUP and QM_TRY_VAR_GLUE macros are
// implementation details of QM_TRY_VAR and shouldn't be used directly.

// Handles the four arguments case when the error is propagated.
#define QM_TRY_VAR_PROPAGATE_ERR(ns, tryResult, target, expr) \
  auto tryResult = (expr);                                    \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                      \
    ns::QM_HANDLE_ERROR(expr);                                \
    return tryResult.propagateErr();                          \
  }                                                           \
  MOZ_REMOVE_PAREN(target) = tryResult.unwrap();

// Handles the five arguments case when a custom return value needs to be
// returned
#define QM_TRY_VAR_CUSTOM_RET_VAL(ns, tryResult, target, expr, customRetVal) \
  auto tryResult = (expr);                                                   \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    auto tryTempResult MOZ_MAYBE_UNUSED = std::move(tryResult);              \
    ns::QM_HANDLE_ERROR(expr);                                               \
    return customRetVal;                                                     \
  }                                                                          \
  MOZ_REMOVE_PAREN(target) = tryResult.unwrap();

// Handles the six arguments case when a cleanup function needs to be called
// before a custom return value is returned
#define QM_TRY_VAR_CUSTOM_RET_VAL_WITH_CLEANUP(ns, tryResult, target, expr, \
                                               customRetVal, cleanup)       \
  auto tryResult = (expr);                                                  \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                    \
    auto tryTempResult MOZ_MAYBE_UNUSED = std::move(tryResult);             \
    ns::QM_HANDLE_ERROR(expr);                                              \
    cleanup(tryTempResult);                                                 \
    return customRetVal;                                                    \
  }                                                                         \
  MOZ_REMOVE_PAREN(target) = tryResult.unwrap();

// Chooses the final implementation macro for given argument count.
// It can be used by other modules to define module specific error handling.
// See also the comment for QM_TRY_META.
#define QM_TRY_VAR_META(...)                                            \
  MOZ_ARG_8(, ##__VA_ARGS__,                                            \
            QM_TRY_VAR_CUSTOM_RET_VAL_WITH_CLEANUP(__VA_ARGS__),        \
            QM_TRY_VAR_CUSTOM_RET_VAL(__VA_ARGS__),                     \
            QM_TRY_VAR_PROPAGATE_ERR(__VA_ARGS__),                      \
            QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__), \
            QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__))

// Specifies the namespace and generates unique variable name. This extra
// internal macro (along with __COUNTER__) allows nesting of the final macro.
#define QM_TRY_VAR_GLUE(...) \
  QM_TRY_VAR_META(mozilla::dom::quota, MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)

/**
 * QM_TRY_VAR(target, expr[, customRetVal, cleanup]) is the C++ equivalent of
 * Rust's `target = try!(expr);`. First, it evaluates expr, which must produce
 * a Result value. On success, the result's success value is assigned to
 * target. On error, it calls HandleError and an additional cleanup function (
 * if the fourth argument was passed) and finally returns the error result or a
 * custom return value (if the third argument was passed). |target| must be an
 * lvalue.
 */
#define QM_TRY_VAR(...) QM_TRY_VAR_GLUE(__VA_ARGS__)

/**
 * QM_DEBUG_VAR_TRY works like QM_TRY_VAR in debug builds, it has has no effect
 * in non-debug builds.
 */
#ifdef DEBUG
#  define QM_DEBUG_TRY_VAR(...) QM_TRY_VAR(__VA_ARGS__)
#else
#  define QM_DEBUG_TRY_VAR(...)
#endif

// QM_FAIL_RET_VAL and QM_FAIL_RET_VAL_WITH_CLEANUP macros are implementation
// details of QM_FAIL and shouldn't be used directly.

// Handles the two arguments case when just an error is returned
#define QM_FAIL_RET_VAL(ns, retVal) \
  ns::QM_HANDLE_ERROR(Failure);     \
  return retVal;

// Handles the three arguments case when a cleanup function needs to be called
// before a return value is returned
#define QM_FAIL_RET_VAL_WITH_CLEANUP(ns, retVal, cleanup) \
  ns::QM_HANDLE_ERROR(Failure);                           \
  cleanup();                                              \
  return retVal;

// Chooses the final implementation macro for given argument count.
// It can be used by other modules to define module specific error handling.
// See also the comment for QM_TRY_META.
#define QM_FAIL_META(...)                                               \
  MOZ_ARG_5(, ##__VA_ARGS__, QM_FAIL_RET_VAL_WITH_CLEANUP(__VA_ARGS__), \
            QM_FAIL_RET_VAL(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__))

// Specifies the namespace. This extra internal macro allows nesting of the
// final macro.
#define QM_FAIL_GLUE(...) QM_FAIL_META(mozilla::dom::quota, ##__VA_ARGS__)

/**
 * QM_FAIL(retVal[, cleanup]) calls HandleError and an additional cleanup
 * function (if the second argument was passed) and returns a return value.
 */
#define QM_FAIL(...) QM_FAIL_GLUE(__VA_ARGS__)

/**
 * QM_DEBUG_FAIL works like QM_FAIL in debug builds, it has has no effect in
 * non-debug builds.
 */
#ifdef DEBUG
#  define QM_DEBUG_FAIL(...) QM_FAIL(__VA_ARGS__)
#else
#  define QM_DEBUG_FAIL(...)
#endif

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

// TODO: Maybe move this to mfbt/ResultExtensions.h
template <auto SuccessValue>
auto OkToOk(Ok) -> Result<decltype(SuccessValue), nsresult> {
  return SuccessValue;
}

template <nsresult ErrorValue, auto SuccessValue,
          typename V = decltype(SuccessValue)>
auto ErrToOkOrErr(nsresult aValue) -> Result<V, nsresult> {
  if (aValue == ErrorValue) {
    return V{SuccessValue};
  }
  return Err(aValue);
}

template <nsresult ErrorValue, typename V>
auto ErrToDefaultOkOrErr(nsresult aValue) -> Result<V, nsresult> {
  if (aValue == ErrorValue) {
    return V{};
  }
  return Err(aValue);
}

// TODO: Maybe move this to mfbt/ResultExtensions.h
template <typename R, typename Func, typename... Args>
Result<R, nsresult> ToResultGet(const Func& aFunc, Args&&... aArgs) {
  nsresult rv;
  R res = aFunc(std::forward<Args>(aArgs)..., &rv);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  return res;
}

// Like Rust's collect with a step function, not a generic iterator/range.
//
// Cond must be a function type with a return type to Result<V, E>, where
// V is convertible to bool
// - success converts to true indicates that collection shall continue
// - success converts to false indicates that collection is completed
// - error indicates that collection shall stop, propagating the error
//
// Body must a function type accepting a V xvalue with a return type convertible
// to Result<empty, E>.
template <typename Step, typename Body>
auto CollectEach(const Step& aStep, const Body& aBody)
    -> Result<mozilla::Ok, typename std::result_of_t<Step()>::err_type> {
  using StepResultType = typename std::result_of_t<Step()>::ok_type;

  static_assert(std::is_empty_v<
                typename std::result_of_t<Body(StepResultType &&)>::ok_type>);

  while (true) {
    StepResultType element;
    MOZ_TRY_VAR(element, aStep());

    if (!static_cast<bool>(element)) {
      break;
    }

    MOZ_TRY(aBody(std::move(element)));
  }

  return mozilla::Ok{};
}

// Like Rust's collect with a while loop, not a generic iterator/range.
//
// Cond must be a function type accepting no parameters with a return type
// convertible to Result<bool, E>, where
// - success true indicates that collection shall continue
// - success false indicates that collection is completed
// - error indicates that collection shall stop, propagating the error
//
// Body must a function type accepting no parameters with a return type
// convertible to Result<empty, E>.
template <typename Cond, typename Body>
auto CollectWhile(const Cond& aCond, const Body& aBody)
    -> Result<mozilla::Ok, typename std::result_of_t<Cond()>::err_type> {
  return CollectEach(aCond, [&aBody](bool) { return aBody(); });
}

template <>
class MOZ_MUST_USE_TYPE GenericErrorResult<mozilla::ipc::IPCResult> {
  mozilla::ipc::IPCResult mErrorValue;

  template <typename V, typename E2>
  friend class Result;

 public:
  explicit GenericErrorResult(mozilla::ipc::IPCResult aErrorValue)
      : mErrorValue(aErrorValue) {
    MOZ_ASSERT(!aErrorValue);
  }

  operator mozilla::ipc::IPCResult() const { return mErrorValue; }
};

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

MOZ_COLD void ReportInternalError(const char* aFile, uint32_t aLine,
                                  const char* aStr);

LogModule* GetQuotaManagerLogger();

void AnonymizeCString(nsACString& aCString);

inline auto AnonymizedCString(const nsACString& aCString) {
  nsAutoCString result{aCString};
  AnonymizeCString(result);
  return result;
}

void AnonymizeOriginString(nsACString& aOriginString);

inline auto AnonymizedOriginString(const nsACString& aOriginString) {
  nsAutoCString result{aOriginString};
  AnonymizeOriginString(result);
  return result;
}

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

// IntString is deprecated, use GetIntString instead.
class IntString : public nsAutoString {
 public:
  explicit IntString(int64_t aInteger) { AppendInt(aInteger); }
};

// IntCString is deprecated, use GetIntCString instead.
class IntCString : public nsAutoCString {
 public:
  explicit IntCString(int64_t aInteger) { AppendInt(aInteger); }
};

nsAutoString GetIntString(const int64_t aInteger);

nsAutoCString GetIntCString(const int64_t aInteger);

nsDependentCSubstring GetLeafName(const nsACString& aPath);

void LogError(const nsLiteralCString& aModule, const nsACString& aExpr,
              const nsACString& aSourceFile, int32_t aSourceLine);

#ifdef DEBUG
Result<bool, nsresult> WarnIfFileIsUnknown(nsIFile& aFile,
                                           const char* aSourceFile,
                                           int32_t aSourceLine);
#endif

// XXX Since this uses thread_local, we currently disable it on android, see Bug
// 1324316
#if (defined(EARLY_BETA_OR_EARLIER) || defined(DEBUG)) && \
    !defined(MOZ_WIDGET_ANDROID)
#  define QM_ENABLE_SCOPED_LOG_EXTRA_INFO
#endif

struct MOZ_STACK_CLASS ScopedLogExtraInfo {
  static constexpr const char kTagQuery[] = "query";

#ifdef QM_ENABLE_SCOPED_LOG_EXTRA_INFO
  using ScopedLogExtraInfoMap = std::map<const char*, nsCString>;

  template <size_t N>
  ScopedLogExtraInfo(const char (&aTag)[N], const nsACString& aExtraInfo)
      : mTag{aTag} {
    AddInfo(aTag, aExtraInfo);
  }

  ~ScopedLogExtraInfo() {
    if (mTag) {
      if (mPreviousValue.IsEmpty()) {
        sInfos.erase(mTag);
      } else {
        sInfos.find(mTag)->second = mPreviousValue;
      }
    }
  }

  ScopedLogExtraInfo(ScopedLogExtraInfo&& aOther)
      : mTag(aOther.mTag), mPreviousValue(std::move(aOther.mPreviousValue)) {
    aOther.mTag = nullptr;
  }
  ScopedLogExtraInfo& operator=(ScopedLogExtraInfo&& aOther) = delete;

  ScopedLogExtraInfo(const ScopedLogExtraInfo&) = delete;
  ScopedLogExtraInfo& operator=(const ScopedLogExtraInfo&) = delete;

  static const ScopedLogExtraInfoMap* GetExtraInfoMap() { return &sInfos; }

 private:
  const char* mTag;
  nsCString mPreviousValue;

  inline static thread_local ScopedLogExtraInfoMap sInfos;

  void AddInfo(const char* aTag, const nsACString& aExtraInfo) {
    auto foundIt = sInfos.find(aTag);

    if (foundIt != sInfos.end()) {
      mPreviousValue = std::move(foundIt->second);
      foundIt->second = aExtraInfo;
    } else {
      sInfos.emplace(aTag, aExtraInfo);
    }
  }

#else
  template <size_t N>
  ScopedLogExtraInfo(const char (&aTag)[N], const nsACString& aExtraInfo) {}

  // user-defined to silence unused variable warnings
  ~ScopedLogExtraInfo() {}
#endif
};

#if defined(EARLY_BETA_OR_EARLIER) || defined(DEBUG)
#  define QM_META_HANDLE_ERROR(module)                                     \
    MOZ_COLD inline void HandleError(                                      \
        const char* aExpr, const char* aSourceFile, int32_t aSourceLine) { \
      mozilla::dom::quota::LogError(module, nsDependentCString(aExpr),     \
                                    nsDependentCString(aSourceFile),       \
                                    aSourceLine);                          \
    }
#else
#  define QM_META_HANDLE_ERROR(module)            \
    MOZ_ALWAYS_INLINE constexpr void HandleError( \
        const char* aExpr, const char* aSourceFile, int32_t aSourceLine) {}
#endif

// As this is a function that will only be called in error cases, this is marked
// with MOZ_COLD to avoid bloating the code of calling functions, if it's not
// empty.
//
// For the same reason, the string-ish parameters are of type const char* rather
// than any ns*String type, to minimize the code at each call site. This
// deliberately de-optimizes runtime performance, which is uncritical during
// error handling.
//
// The corresponding functions in the quota clients should be defined using
// QM_META_HANDLE_ERROR, in particular they should have exactly the same
// signature incl. attributes. These functions are not intended to be called
// directly, they should only be called from the QM_* macros.

QM_META_HANDLE_ERROR("QuotaManager"_ns)

}  // namespace quota
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_quota_quotacommon_h__
