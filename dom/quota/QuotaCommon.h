/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_quotacommon_h__
#define mozilla_dom_quota_quotacommon_h__

#include "mozilla/dom/quota/Config.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include "mozIStorageStatement.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/MacroArgs.h"
#include "mozilla/Maybe.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Try.h"
#if defined(QM_LOG_ERROR_ENABLED) && defined(QM_ERROR_STACKS_ENABLED)
#  include "mozilla/Variant.h"
#endif
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/FirstInitializationAttemptsImpl.h"
#include "mozilla/dom/quota/RemoveParen.h"
#include "mozilla/dom/quota/ScopedLogExtraInfo.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIEventTarget.h"
#include "nsIFile.h"
#include "nsLiteralString.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTLiteralString.h"

namespace mozilla {
template <typename T>
class NotNull;
}

#define MOZ_ARGS_AFTER_3(a1, a2, a3, ...) __VA_ARGS__

#define MOZ_ADD_ARGS2(...) , ##__VA_ARGS__
#define MOZ_ADD_ARGS(...) MOZ_ADD_ARGS2(__VA_ARGS__)

// Proper use of unique variable names can be tricky (especially if nesting of
// the final macro is required).
// See https://lifecs.likai.org/2016/07/c-preprocessor-hygienic-macros.html
#define MOZ_UNIQUE_VAR(base) MOZ_CONCAT(base, __COUNTER__)

// See https://florianjw.de/en/passing_overloaded_functions.html
// TODO: Add a test for this macro.
#define MOZ_SELECT_OVERLOAD(func)                         \
  [](auto&&... aArgs) -> decltype(auto) {                 \
    return func(std::forward<decltype(aArgs)>(aArgs)...); \
  }

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

#define QM_LOG_TEST() MOZ_LOG_TEST(GetQuotaManagerLogger(), LogLevel::Info)
#define QM_LOG(_args) MOZ_LOG(GetQuotaManagerLogger(), LogLevel::Info, _args)

#ifdef DEBUG
#  define UNKNOWN_FILE_WARNING(_leafName)                                  \
    NS_WARNING(nsPrintfCString(                                            \
                   "Something (%s) in the directory that doesn't belong!", \
                   NS_ConvertUTF16toUTF8(_leafName).get())                 \
                   .get())
#else
#  define UNKNOWN_FILE_WARNING(_leafName) (void)(_leafName);
#endif

// This macro should be used in directory traversals for files or directories
// that are unknown for given directory traversal. It should only be called
// after all known (directory traversal specific) files or directories have
// been checked and handled.
// XXX Consider renaming the macro to QM_LOG_UNKNOWN_DIR_ENTRY.
#ifdef DEBUG
#  define WARN_IF_FILE_IS_UNKNOWN(_file) \
    mozilla::dom::quota::WarnIfFileIsUnknown(_file, __FILE__, __LINE__)
#else
#  define WARN_IF_FILE_IS_UNKNOWN(_file) Result<bool, nsresult>(false)
#endif

/**
 * There are multiple ways to handle unrecoverable conditions (note that the
 * patterns are put in reverse chronological order and only the first pattern
 * QM_TRY/QM_TRY_UNWRAP/QM_TRY_INSPECT/QM_TRY_RETURN/QM_FAIL should be used in
 * new code):
 *
 * 1. Using QM_TRY/QM_TRY_UNWRAP/QM_TRY_INSPECT/QM_TRY_RETURN/QM_FAIL macros
 *    (Quota manager specific, defined below)
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
 * QM_TRY/QM_TRY_UNWRAP/QM_TRY_INSPECT is like MOZ_TRY/MOZ_TRY_VAR but if an
 * error occurs it additionally calls a generic function HandleError to handle
 * the error and it can be used to return custom return values as well and even
 * call an additional cleanup function.
 * HandleError currently only warns in debug builds, it will report to the
 * browser console and telemetry in the future.
 * The other advantage of QM_TRY/QM_TRY_UNWRAP/QM_TRY_INSPECT is that a local
 * nsresult is not needed at all in all cases, all calls can be wrapped
 * directly. If an error occurs, the warning contains a concrete call instead
 * of the rv local variable. For example:
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
 * QM_TRY_RETURN is a supplementary macro for cases when the result's success
 * value can be directly returned (instead of assigning to a variable as in the
 * QM_TRY_UNWRAP/QM_TRY_INSPECT case).
 *
 * QM_FAIL is a supplementary macro for cases when an error needs to be
 * returned without evaluating an expression. It's possible to write
 * QM_TRY(OkIf(false), NS_ERROR_FAILURE), but QM_FAIL(NS_ERROR_FAILURE) looks
 * more straightforward.
 *
 * It's highly recommended to use
 * QM_TRY/QM_TRY_UNWRAP/QM_TRY_INSPECT/QM_TRY_RETURN/QM_FAIL in new code for
 * quota manager and quota clients. Existing code should be incrementally
 * converted as needed.
 *
 * QM_TRY_VOID/QM_TRY_UNWRAP_VOID/QM_TRY_INSPECT_VOID/QM_FAIL_VOID is not
 * defined on purpose since it's possible to use
 * QM_TRY/QM_TRY_UNWRAP/QM_TRY_INSPECT/QM_FAIL even in void functions.
 * However, QM_TRY(Task(), ) would look odd so it's recommended to use a dummy
 * define QM_VOID that evaluates to nothing instead: QM_TRY(Task(), QM_VOID)
 *
 * Custom return values can be static or dynamically generated using functions
 * with one of these signatures:
 *   auto(const char* aFunc, const char* aExpr);
 *   auto(const char* aFunc, const T& aRv);
 *   auto(const T& aRc);
 */

#define QM_VOID

#define QM_PROPAGATE Err(tryTempError)

namespace mozilla::dom::quota::detail {

struct IpcFailCustomRetVal {
  explicit IpcFailCustomRetVal(
      mozilla::NotNull<mozilla::ipc::IProtocol*> aActor)
      : mActor(aActor) {}

  template <size_t NFunc, size_t NExpr>
  auto operator()(const char (&aFunc)[NFunc],
                  const char (&aExpr)[NExpr]) const {
    return mozilla::Err(mozilla::ipc::IPCResult::Fail(mActor, aFunc, aExpr));
  }

  mozilla::NotNull<mozilla::ipc::IProtocol*> mActor;
};

}  // namespace mozilla::dom::quota::detail

#define QM_IPC_FAIL(actor) \
  mozilla::dom::quota::detail::IpcFailCustomRetVal(mozilla::WrapNotNull(actor))

#ifdef DEBUG
#  define QM_ASSERT_UNREACHABLE                                               \
    [](const char*, const char*) -> ::mozilla::GenericErrorResult<nsresult> { \
      MOZ_CRASH("Should never be reached.");                                  \
    }

#  define QM_ASSERT_UNREACHABLE_VOID \
    [](const char*, const char*) { MOZ_CRASH("Should never be reached."); }
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
#  define QM_DIAGNOSTIC_ASSERT_UNREACHABLE                                    \
    [](const char*, const char*) -> ::mozilla::GenericErrorResult<nsresult> { \
      MOZ_CRASH("Should never be reached.");                                  \
    }

#  define QM_DIAGNOSTIC_ASSERT_UNREACHABLE_VOID \
    [](const char*, const char*) { MOZ_CRASH("Should never be reached."); }
#endif

#define QM_NO_CLEANUP [](const auto&) {}

// QM_MISSING_ARGS and QM_HANDLE_ERROR macros are implementation details of
// QM_TRY/QM_TRY_UNWRAP/QM_TRY_INSPECT/QM_FAIL and shouldn't be used directly.

#define QM_MISSING_ARGS(...)                           \
  do {                                                 \
    static_assert(false, "Did you forget arguments?"); \
  } while (0)

#ifdef DEBUG
#  define QM_HANDLE_ERROR(expr, error, severity) \
    HandleError(#expr, error, __FILE__, __LINE__, severity)
#else
#  define QM_HANDLE_ERROR(expr, error, severity) \
    HandleError("Unavailable", error, __FILE__, __LINE__, severity)
#endif

#ifdef DEBUG
#  define QM_HANDLE_ERROR_RETURN_NOTHING(expr, error, severity) \
    HandleErrorReturnNothing(#expr, error, __FILE__, __LINE__, severity)
#else
#  define QM_HANDLE_ERROR_RETURN_NOTHING(expr, error, severity) \
    HandleErrorReturnNothing("Unavailable", error, __FILE__, __LINE__, severity)
#endif

#ifdef DEBUG
#  define QM_HANDLE_ERROR_WITH_CLEANUP_RETURN_NOTHING(expr, error, severity, \
                                                      cleanup)               \
    HandleErrorWithCleanupReturnNothing(#expr, error, __FILE__, __LINE__,    \
                                        severity, cleanup)
#else
#  define QM_HANDLE_ERROR_WITH_CLEANUP_RETURN_NOTHING(expr, error, severity, \
                                                      cleanup)               \
    HandleErrorWithCleanupReturnNothing("Unavailable", error, __FILE__,      \
                                        __LINE__, severity, cleanup)
#endif

// Handles the case when QM_VOID is passed as a custom return value.
#define QM_HANDLE_CUSTOM_RET_VAL_HELPER0(func, expr, error)

#define QM_HANDLE_CUSTOM_RET_VAL_HELPER1(func, expr, error, customRetVal) \
  mozilla::dom::quota::HandleCustomRetVal(func, #expr, error, customRetVal)

#define QM_HANDLE_CUSTOM_RET_VAL_GLUE(a, b) a b

#define QM_HANDLE_CUSTOM_RET_VAL(...)                                 \
  QM_HANDLE_CUSTOM_RET_VAL_GLUE(                                      \
      MOZ_PASTE_PREFIX_AND_ARG_COUNT(QM_HANDLE_CUSTOM_RET_VAL_HELPER, \
                                     MOZ_ARGS_AFTER_3(__VA_ARGS__)),  \
      (MOZ_ARG_1(__VA_ARGS__), MOZ_ARG_2(__VA_ARGS__),                \
       MOZ_ARG_3(__VA_ARGS__) MOZ_ADD_ARGS(MOZ_ARGS_AFTER_3(__VA_ARGS__))))

// QM_TRY_PROPAGATE_ERR, QM_TRY_CUSTOM_RET_VAL,
// QM_TRY_CUSTOM_RET_VAL_WITH_CLEANUP and QM_TRY_GLUE macros are implementation
// details of QM_TRY and shouldn't be used directly.

// Handles the two arguments case when the error is propagated.
#define QM_TRY_PROPAGATE_ERR(tryResult, expr)                                \
  auto tryResult = (expr);                                                   \
  static_assert(std::is_empty_v<typename decltype(tryResult)::ok_type>);     \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                    \
        expr, tryResult.inspectErr(), mozilla::dom::quota::Severity::Error); \
    return tryResult.propagateErr();                                         \
  }

// Handles the three arguments case when a custom return value needs to be
// returned
#define QM_TRY_CUSTOM_RET_VAL(tryResult, expr, customRetVal)                 \
  auto tryResult = (expr);                                                   \
  static_assert(std::is_empty_v<typename decltype(tryResult)::ok_type>);     \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    auto tryTempError MOZ_MAYBE_UNUSED = tryResult.unwrapErr();              \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                    \
        expr, tryTempError, mozilla::dom::quota::Severity::Error);           \
    constexpr const auto& func MOZ_MAYBE_UNUSED = __func__;                  \
    return QM_HANDLE_CUSTOM_RET_VAL(func, expr, tryTempError, customRetVal); \
  }

// Handles the four arguments case when a cleanup function needs to be called
// before a custom return value is returned
#define QM_TRY_CUSTOM_RET_VAL_WITH_CLEANUP(tryResult, expr, customRetVal,    \
                                           cleanup)                          \
  auto tryResult = (expr);                                                   \
  static_assert(std::is_empty_v<typename decltype(tryResult)::ok_type>);     \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    auto tryTempError = tryResult.unwrapErr();                               \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                    \
        expr, tryTempError, mozilla::dom::quota::Severity::Error);           \
    cleanup(tryTempError);                                                   \
    constexpr const auto& func MOZ_MAYBE_UNUSED = __func__;                  \
    return QM_HANDLE_CUSTOM_RET_VAL(func, expr, tryTempError, customRetVal); \
  }

#define QM_TRY_CUSTOM_RET_VAL_WITH_CLEANUP_AND_PREDICATE(                    \
    tryResult, expr, customRetVal, cleanup, predicate)                       \
  auto tryResult = (expr);                                                   \
  static_assert(std::is_empty_v<typename decltype(tryResult)::ok_type>);     \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    auto tryTempError = tryResult.unwrapErr();                               \
    if (predicate()) {                                                       \
      mozilla::dom::quota::QM_HANDLE_ERROR(                                  \
          expr, tryTempError, mozilla::dom::quota::Severity::Error);         \
    }                                                                        \
    cleanup(tryTempError);                                                   \
    constexpr const auto& func MOZ_MAYBE_UNUSED = __func__;                  \
    return QM_HANDLE_CUSTOM_RET_VAL(func, expr, tryTempError, customRetVal); \
  }

// Chooses the final implementation macro for given argument count.
// This could use MOZ_PASTE_PREFIX_AND_ARG_COUNT, but explicit named suffxes
// read slightly better than plain numbers.
// See also
// https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros
#define QM_TRY_META(...)                                                       \
  {                                                                            \
    MOZ_ARG_7(, ##__VA_ARGS__,                                                 \
              QM_TRY_CUSTOM_RET_VAL_WITH_CLEANUP_AND_PREDICATE(__VA_ARGS__),   \
              QM_TRY_CUSTOM_RET_VAL_WITH_CLEANUP(__VA_ARGS__),                 \
              QM_TRY_CUSTOM_RET_VAL(__VA_ARGS__),                              \
              QM_TRY_PROPAGATE_ERR(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__), \
              QM_MISSING_ARGS(__VA_ARGS__))                                    \
  }

// Generates unique variable name. This extra internal macro (along with
// __COUNTER__) allows nesting of the final macro.
#define QM_TRY_GLUE(...) QM_TRY_META(MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)

/**
 * QM_TRY(expr[, customRetVal, cleanup, predicate]) is the C++ equivalent of
 * Rust's `try!(expr);`. First, it evaluates expr, which must produce a Result
 * value with empty ok_type. On Success, it does nothing else. On error, it
 * calls HandleError (conditionally if the fourth argument was passed) and an
 * additional cleanup function (if the third argument was passed) and finally
 * returns an error Result from the enclosing function or a custom return value
 * (if the second argument was passed).
 */
#define QM_TRY(...) QM_TRY_GLUE(__VA_ARGS__)

// QM_TRY_ASSIGN_PROPAGATE_ERR, QM_TRY_ASSIGN_CUSTOM_RET_VAL,
// QM_TRY_ASSIGN_CUSTOM_RET_VAL_WITH_CLEANUP and QM_TRY_ASSIGN_GLUE macros are
// implementation details of QM_TRY_UNWRAP/QM_TRY_INSPECT and shouldn't be used
// directly.

// Handles the four arguments case when the error is propagated.
#define QM_TRY_ASSIGN_PROPAGATE_ERR(tryResult, accessFunction, target, expr) \
  auto tryResult = (expr);                                                   \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                    \
        expr, tryResult.inspectErr(), mozilla::dom::quota::Severity::Error); \
    return tryResult.propagateErr();                                         \
  }                                                                          \
  MOZ_REMOVE_PAREN(target) = tryResult.accessFunction();

// Handles the five arguments case when a custom return value needs to be
// returned
#define QM_TRY_ASSIGN_CUSTOM_RET_VAL(tryResult, accessFunction, target, expr, \
                                     customRetVal)                            \
  auto tryResult = (expr);                                                    \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                      \
    auto tryTempError MOZ_MAYBE_UNUSED = tryResult.unwrapErr();               \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                     \
        expr, tryTempError, mozilla::dom::quota::Severity::Error);            \
    constexpr const auto& func MOZ_MAYBE_UNUSED = __func__;                   \
    return QM_HANDLE_CUSTOM_RET_VAL(func, expr, tryTempError, customRetVal);  \
  }                                                                           \
  MOZ_REMOVE_PAREN(target) = tryResult.accessFunction();

// Handles the six arguments case when a cleanup function needs to be called
// before a custom return value is returned
#define QM_TRY_ASSIGN_CUSTOM_RET_VAL_WITH_CLEANUP(                           \
    tryResult, accessFunction, target, expr, customRetVal, cleanup)          \
  auto tryResult = (expr);                                                   \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    auto tryTempError = tryResult.unwrapErr();                               \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                    \
        expr, tryTempError, mozilla::dom::quota::Severity::Error);           \
    cleanup(tryTempError);                                                   \
    constexpr const auto& func MOZ_MAYBE_UNUSED = __func__;                  \
    return QM_HANDLE_CUSTOM_RET_VAL(func, expr, tryTempError, customRetVal); \
  }                                                                          \
  MOZ_REMOVE_PAREN(target) = tryResult.accessFunction();

// Chooses the final implementation macro for given argument count.
// See also the comment for QM_TRY_META.
#define QM_TRY_ASSIGN_META(...)                                         \
  MOZ_ARG_8(, ##__VA_ARGS__,                                            \
            QM_TRY_ASSIGN_CUSTOM_RET_VAL_WITH_CLEANUP(__VA_ARGS__),     \
            QM_TRY_ASSIGN_CUSTOM_RET_VAL(__VA_ARGS__),                  \
            QM_TRY_ASSIGN_PROPAGATE_ERR(__VA_ARGS__),                   \
            QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__), \
            QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__))

// Generates unique variable name. This extra internal macro (along with
// __COUNTER__) allows nesting of the final macro.
#define QM_TRY_ASSIGN_GLUE(accessFunction, ...) \
  QM_TRY_ASSIGN_META(MOZ_UNIQUE_VAR(tryResult), accessFunction, ##__VA_ARGS__)

/**
 * QM_TRY_UNWRAP(target, expr[, customRetVal, cleanup]) is the C++ equivalent of
 * Rust's `target = try!(expr);`. First, it evaluates expr, which must produce
 * a Result value. On success, the result's success value is unwrapped and
 * assigned to target. On error, it calls HandleError and an additional cleanup
 * function (if the fourth argument was passed) and finally returns the error
 * result or a custom return value (if the third argument was passed). |target|
 * must be an lvalue.
 */
#define QM_TRY_UNWRAP(...) QM_TRY_ASSIGN_GLUE(unwrap, __VA_ARGS__)

/**
 * QM_TRY_INSPECT is similar to QM_TRY_UNWRAP, but it does not unwrap a success
 * value, but inspects it and binds it to the target. It can therefore only be
 * used when the target declares a const&. In general,
 *
 *   QM_TRY_INSPECT(const auto &target, DoSomething())
 *
 * should be preferred over
 *
 *   QM_TRY_UNWRAP(const auto target, DoSomething())
 *
 * as it avoids unnecessary moves/copies.
 */
#define QM_TRY_INSPECT(...) QM_TRY_ASSIGN_GLUE(inspect, __VA_ARGS__)

// QM_TRY_RETURN_PROPAGATE_ERR, QM_TRY_RETURN_CUSTOM_RET_VAL,
// QM_TRY_RETURN_CUSTOM_RET_VAL_WITH_CLEANUP and QM_TRY_RETURN_GLUE macros are
// implementation details of QM_TRY_RETURN and shouldn't be used directly.

// Handles the two arguments case when the error is (also) propagated.
// Note that this deliberately uses a single return statement without going
// through unwrap/unwrapErr/propagateErr, so that this does not prevent NRVO or
// tail call optimizations when possible.
#define QM_TRY_RETURN_PROPAGATE_ERR(tryResult, expr)                         \
  auto tryResult = (expr);                                                   \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                    \
        expr, tryResult.inspectErr(), mozilla::dom::quota::Severity::Error); \
  }                                                                          \
  return tryResult;

// Handles the three arguments case when a custom return value needs to be
// returned
#define QM_TRY_RETURN_CUSTOM_RET_VAL(tryResult, expr, customRetVal)          \
  auto tryResult = (expr);                                                   \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    auto tryTempError MOZ_MAYBE_UNUSED = tryResult.unwrapErr();              \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                    \
        expr, tryResult.inspectErr(), mozilla::dom::quota::Severity::Error); \
    constexpr const auto& func MOZ_MAYBE_UNUSED = __func__;                  \
    return QM_HANDLE_CUSTOM_RET_VAL(func, expr, tryTempError, customRetVal); \
  }                                                                          \
  return tryResult.unwrap();

// Handles the four arguments case when a cleanup function needs to be called
// before a custom return value is returned
#define QM_TRY_RETURN_CUSTOM_RET_VAL_WITH_CLEANUP(tryResult, expr,           \
                                                  customRetVal, cleanup)     \
  auto tryResult = (expr);                                                   \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                     \
    auto tryTempError = tryResult.unwrapErr();                               \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                    \
        expr, tryTempError, mozilla::dom::quota::Severity::Error);           \
    cleanup(tryTempError);                                                   \
    constexpr const auto& func MOZ_MAYBE_UNUSED = __func__;                  \
    return QM_HANDLE_CUSTOM_RET_VAL(func, expr, tryTempError, customRetVal); \
  }                                                                          \
  return tryResult.unwrap();

// Chooses the final implementation macro for given argument count.
// See also the comment for QM_TRY_META.
#define QM_TRY_RETURN_META(...)                                           \
  {                                                                       \
    MOZ_ARG_6(, ##__VA_ARGS__,                                            \
              QM_TRY_RETURN_CUSTOM_RET_VAL_WITH_CLEANUP(__VA_ARGS__),     \
              QM_TRY_RETURN_CUSTOM_RET_VAL(__VA_ARGS__),                  \
              QM_TRY_RETURN_PROPAGATE_ERR(__VA_ARGS__),                   \
              QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__)) \
  }

// Generates unique variable name. This extra internal macro (along with
// __COUNTER__) allows nesting of the final macro.
#define QM_TRY_RETURN_GLUE(...) \
  QM_TRY_RETURN_META(MOZ_UNIQUE_VAR(tryResult), ##__VA_ARGS__)

/**
 * QM_TRY_RETURN(expr[, customRetVal, cleanup]) evaluates expr, which must
 * produce a Result value. On success, the result's success value is returned.
 * On error, it calls HandleError and an additional cleanup function (if the
 * third argument was passed) and finally returns the error result or a custom
 * return value (if the second argument was passed).
 */
#define QM_TRY_RETURN(...) QM_TRY_RETURN_GLUE(__VA_ARGS__)

// QM_FAIL_RET_VAL and QM_FAIL_RET_VAL_WITH_CLEANUP macros are implementation
// details of QM_FAIL and shouldn't be used directly.

// Handles the one argument case when just an error is returned
#define QM_FAIL_RET_VAL(retVal)                                               \
  mozilla::dom::quota::QM_HANDLE_ERROR(Failure, 0,                            \
                                       mozilla::dom::quota::Severity::Error); \
  return retVal;

// Handles the two arguments case when a cleanup function needs to be called
// before a return value is returned
#define QM_FAIL_RET_VAL_WITH_CLEANUP(retVal, cleanup)                         \
  mozilla::dom::quota::QM_HANDLE_ERROR(Failure, 0,                            \
                                       mozilla::dom::quota::Severity::Error); \
  cleanup();                                                                  \
  return retVal;

// Chooses the final implementation macro for given argument count.
// See also the comment for QM_TRY_META.
#define QM_FAIL_META(...)                                               \
  MOZ_ARG_4(, ##__VA_ARGS__, QM_FAIL_RET_VAL_WITH_CLEANUP(__VA_ARGS__), \
            QM_FAIL_RET_VAL(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__))

// This extra internal macro allows nesting of the final macro.
#define QM_FAIL_GLUE(...) QM_FAIL_META(__VA_ARGS__)

/**
 * QM_FAIL(retVal[, cleanup]) calls HandleError and an additional cleanup
 * function (if the second argument was passed) and returns a return value.
 */
#define QM_FAIL(...) QM_FAIL_GLUE(__VA_ARGS__)

// QM_REPORTONLY_TRY, QM_REPORTONLY_TRY_WITH_CLEANUP, QM_REPORTONLY_TRY_GLUE
// macros are implementation details of QM_WARNONLY_TRY/QM_INFOONLY_TRY and
// shouldn't be used directly.

// Handles the three arguments case when only a warning/info is reported.
#define QM_REPORTONLY_TRY(tryResult, severity, expr)                           \
  auto tryResult = (expr);                                                     \
  static_assert(std::is_empty_v<typename decltype(tryResult)::ok_type>);       \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                       \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                      \
        expr, tryResult.unwrapErr(), mozilla::dom::quota::Severity::severity); \
  }

// Handles the four arguments case when a cleanup function needs to be called
#define QM_REPORTONLY_TRY_WITH_CLEANUP(tryResult, severity, expr, cleanup) \
  auto tryResult = (expr);                                                 \
  static_assert(std::is_empty_v<typename decltype(tryResult)::ok_type>);   \
  if (MOZ_UNLIKELY(tryResult.isErr())) {                                   \
    auto tryTempError = tryResult.unwrapErr();                             \
    mozilla::dom::quota::QM_HANDLE_ERROR(                                  \
        expr, tryTempError, mozilla::dom::quota::Severity::severity);      \
    cleanup(tryTempError);                                                 \
  }

// Chooses the final implementation macro for given argument count.
// See also the comment for QM_TRY_META.
#define QM_REPORTONLY_TRY_META(...)                                         \
  {                                                                         \
    MOZ_ARG_6(, ##__VA_ARGS__, QM_REPORTONLY_TRY_WITH_CLEANUP(__VA_ARGS__), \
              QM_REPORTONLY_TRY(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__), \
              QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__))   \
  }

// Generates unique variable name. This extra internal macro (along with
// __COUNTER__) allows nesting of the final macro.
#define QM_REPORTONLY_TRY_GLUE(severity, ...) \
  QM_REPORTONLY_TRY_META(MOZ_UNIQUE_VAR(tryResult), severity, ##__VA_ARGS__)

/**
 * QM_WARNONLY_TRY(expr[, cleanup]) evaluates expr, which must produce a
 * Result value with empty ok_type. On Success, it does nothing else. On error,
 * it calls HandleError and an additional cleanup function (if the second
 * argument was passed). This macro never returns and failures are always
 * reported using a lower level of severity relative to failures reported by
 * QM_TRY. The macro is intended for failures that should not be propagated.
 */
#define QM_WARNONLY_TRY(...) QM_REPORTONLY_TRY_GLUE(Warning, __VA_ARGS__)

/**
 * QM_INFOONLY_TRY is like QM_WARNONLY_TRY. The only difference is that
 * failures are reported using a lower level of severity relative to failures
 * reported by QM_WARNONLY_TRY.
 */
#define QM_INFOONLY_TRY(...) QM_REPORTONLY_TRY_GLUE(Info, __VA_ARGS__)

// QM_REPORTONLY_TRY_ASSIGN, QM_REPORTONLY_TRY_ASSIGN_WITH_CLEANUP,
// QM_REPORTONLY_TRY_ASSIGN_GLUE macros are implementation details of
// QM_WARNONLY_TRY_UNWRAP/QM_INFOONLY_TRY_UNWRAP and shouldn't be used
// directly.

// Handles the four arguments case when only a warning/info is reported.
#define QM_REPORTONLY_TRY_ASSIGN(tryResult, severity, target, expr) \
  auto tryResult = (expr);                                          \
  MOZ_REMOVE_PAREN(target) =                                        \
      MOZ_LIKELY(tryResult.isOk())                                  \
          ? Some(tryResult.unwrap())                                \
          : mozilla::dom::quota::QM_HANDLE_ERROR_RETURN_NOTHING(    \
                expr, tryResult.unwrapErr(),                        \
                mozilla::dom::quota::Severity::severity);

// Handles the five arguments case when a cleanup function needs to be called
#define QM_REPORTONLY_TRY_ASSIGN_WITH_CLEANUP(tryResult, severity, target,    \
                                              expr, cleanup)                  \
  auto tryResult = (expr);                                                    \
  MOZ_REMOVE_PAREN(target) =                                                  \
      MOZ_LIKELY(tryResult.isOk())                                            \
          ? Some(tryResult.unwrap())                                          \
          : mozilla::dom::quota::QM_HANDLE_ERROR_WITH_CLEANUP_RETURN_NOTHING( \
                expr, tryResult.unwrapErr(),                                  \
                mozilla::dom::quota::Severity::severity, cleanup);

// Chooses the final implementation macro for given argument count.
// See also the comment for QM_TRY_META.
#define QM_REPORTONLY_TRY_ASSIGN_META(...)                              \
  MOZ_ARG_7(, ##__VA_ARGS__,                                            \
            QM_REPORTONLY_TRY_ASSIGN_WITH_CLEANUP(__VA_ARGS__),         \
            QM_REPORTONLY_TRY_ASSIGN(__VA_ARGS__),                      \
            QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__), \
            QM_MISSING_ARGS(__VA_ARGS__), QM_MISSING_ARGS(__VA_ARGS__))

// Generates unique variable name. This extra internal macro (along with
// __COUNTER__) allows nesting of the final macro.
#define QM_REPORTONLY_TRY_ASSIGN_GLUE(severity, ...)                 \
  QM_REPORTONLY_TRY_ASSIGN_META(MOZ_UNIQUE_VAR(tryResult), severity, \
                                ##__VA_ARGS__)

/**
 * QM_WARNONLY_TRY_UNWRAP(target, expr[, cleanup]) evaluates expr, which must
 * produce a Result value. On success, the result's success value is first
 * unwrapped from the Result, then wrapped in a Maybe and finally assigned to
 * target. On error, it calls HandleError and an additional cleanup
 * function (if the third argument was passed) and finally assigns Nothing to
 * target. |target| must be an lvalue. This macro never returns and failures
 * are always reported using a lower level of severity relative to failures
 * reported by QM_TRY_UNWRAP/QM_TRY_INSPECT. The macro is intended for failures
 * that should not be propagated.
 */
#define QM_WARNONLY_TRY_UNWRAP(...) \
  QM_REPORTONLY_TRY_ASSIGN_GLUE(Warning, __VA_ARGS__)

// QM_WARNONLY_TRY_INSPECT doesn't make sense.

/**
 * QM_INFOONLY_TRY_UNWRAP is like QM_WARNONLY_TRY_UNWRAP. The only difference is
 * that failures are reported using a lower level of severity relative to
 * failures reported by QM_WARNONLY_TRY_UNWRAP.
 */
#define QM_INFOONLY_TRY_UNWRAP(...) \
  QM_REPORTONLY_TRY_ASSIGN_GLUE(Info, __VA_ARGS__)

// QM_INFOONLY_TRY_INSPECT doesn't make sense.

// QM_OR_ELSE_REPORT macro is an implementation detail of
// QM_OR_ELSE_WARN/QM_OR_ELSE_INFO/QM_OR_ELSE_LOG_VERBOSE and shouldn't be used
// directly.

#define QM_OR_ELSE_REPORT(severity, expr, fallback)                \
  (expr).orElse([&](const auto& firstRes) {                        \
    mozilla::dom::quota::QM_HANDLE_ERROR(                          \
        #expr, firstRes, mozilla::dom::quota::Severity::severity); \
    return fallback(firstRes);                                     \
  })

/*
 * QM_OR_ELSE_WARN(expr, fallback) evaluates expr, which must produce a Result
 * value. On Success, it just moves the success over. On error, it calls
 * HandleError (with the Warning severity) and a fallback function (passed as
 * the second argument) which produces a new result. Failed expr is always
 * reported as a warning (the macro essentially wraps the fallback function
 * with a warning). QM_OR_ELSE_WARN is a sub macro and is intended to be used
 * along with one of the main macros such as QM_TRY.
 */
#define QM_OR_ELSE_WARN(...) QM_OR_ELSE_REPORT(Warning, __VA_ARGS__)

/**
 * QM_OR_ELSE_INFO is like QM_OR_ELSE_WARN. The only difference is that
 * failures are reported using a lower level of severity relative to failures
 * reported by QM_OR_ELSE_WARN.
 */
#define QM_OR_ELSE_INFO(...) QM_OR_ELSE_REPORT(Info, __VA_ARGS__)

/**
 * QM_OR_ELSE_LOG_VERBOSE is like QM_OR_ELSE_WARN. The only difference is that
 * failures are reported using the lowest severity which is currently ignored
 * in LogError, so nothing goes to the console, browser console and telemetry.
 * Since nothing goes to the telemetry, the macro can't signal the end of the
 * underlying error stack or change the type of the error stack in the
 * telemetry. For that reason, the expression shouldn't contain nested QM_TRY
 * macro uses.
 */
#define QM_OR_ELSE_LOG_VERBOSE(...) QM_OR_ELSE_REPORT(Verbose, __VA_ARGS__)

namespace mozilla::dom::quota {

// XXX Support orElseIf directly in mozilla::Result
template <typename V, typename E, typename P, typename F>
auto OrElseIf(Result<V, E>&& aResult, P&& aPred, F&& aFunc) -> Result<V, E> {
  return MOZ_UNLIKELY(aResult.isErr())
             ? (std::forward<P>(aPred)(aResult.inspectErr()))
                   ? std::forward<F>(aFunc)(aResult.unwrapErr())
                   : aResult.propagateErr()
             : aResult.unwrap();
}

}  // namespace mozilla::dom::quota

// QM_OR_ELSE_REPORT_IF macro is an implementation detail of
// QM_OR_ELSE_WARN_IF/QM_OR_ELSE_INFO_IF/QM_OR_ELSE_LOG_VERBOSE_IF and
// shouldn't be used directly.

#define QM_OR_ELSE_REPORT_IF(severity, expr, predicate, fallback) \
  mozilla::dom::quota::OrElseIf(                                  \
      (expr),                                                     \
      [&](const auto& firstRes) {                                 \
        bool res = predicate(firstRes);                           \
        mozilla::dom::quota::QM_HANDLE_ERROR(                     \
            #expr, firstRes,                                      \
            res ? mozilla::dom::quota::Severity::severity         \
                : mozilla::dom::quota::Severity::Error);          \
        return res;                                               \
      },                                                          \
      fallback)

/*
 * QM_OR_ELSE_WARN_IF(expr, predicate, fallback) evaluates expr first, which
 * must produce a Result value. On Success, it just moves the success over.
 * On error, it calls a predicate function (passed as the second argument) and
 * then it either calls HandleError (with the Warning severity) and a fallback
 * function (passed as the third argument) which produces a new result if the
 * predicate returned true. Or it calls HandleError (with the Error severity)
 * and propagates the error result if the predicate returned false. So failed
 * expr can be reported as a warning or as an error depending on the predicate.
 * QM_OR_ELSE_WARN_IF is a sub macro and is intended to be used along with one
 * of the main macros such as QM_TRY.
 */
#define QM_OR_ELSE_WARN_IF(...) QM_OR_ELSE_REPORT_IF(Warning, __VA_ARGS__)

/**
 * QM_OR_ELSE_INFO_IF is like QM_OR_ELSE_WARN_IF. The only difference is that
 * failures are reported using a lower level of severity relative to failures
 * reported by QM_OR_ELSE_WARN_IF.
 */
#define QM_OR_ELSE_INFO_IF(...) QM_OR_ELSE_REPORT_IF(Info, __VA_ARGS__)

/**
 * QM_OR_ELSE_LOG_VERBOSE_IF is like QM_OR_ELSE_WARN_IF. The only difference is
 * that failures are reported using the lowest severity which is currently
 * ignored in LogError, so nothing goes to the console, browser console and
 * telemetry. Since nothing goes to the telemetry, the macro can't signal the
 * end of the underlying error stack or change the type of the error stack in
 * the telemetry. For that reason, the expression shouldn't contain nested
 * QM_TRY macro uses.
 */
#define QM_OR_ELSE_LOG_VERBOSE_IF(...) \
  QM_OR_ELSE_REPORT_IF(Verbose, __VA_ARGS__)

// Telemetry probes to collect number of failure during the initialization.
#ifdef NIGHTLY_BUILD
#  define RECORD_IN_NIGHTLY(_recorder, _status) \
    do {                                        \
      if (NS_SUCCEEDED(_recorder)) {            \
        _recorder = _status;                    \
      }                                         \
    } while (0)

#  define OK_IN_NIGHTLY_PROPAGATE_IN_OTHERS \
    Ok {}

#  define RETURN_STATUS_OR_RESULT(_status, _rv) \
    return Err(NS_FAILED(_status) ? (_status) : (_rv))
#else
#  define RECORD_IN_NIGHTLY(_dummy, _status) \
    {}

#  define OK_IN_NIGHTLY_PROPAGATE_IN_OTHERS QM_PROPAGATE

#  define RETURN_STATUS_OR_RESULT(_status, _rv) return Err(_rv)
#endif

class mozIStorageConnection;
class mozIStorageStatement;
class nsIFile;

namespace mozilla {

class LogModule;

struct CreateIfNonExistent {};

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

template <nsresult ErrorValue, typename V = mozilla::Ok>
auto ErrToDefaultOkOrErr(nsresult aValue) -> Result<V, nsresult> {
  if (aValue == ErrorValue) {
    return V{};
  }
  return Err(aValue);
}

// Helper template function so that QM_TRY predicates checking for a specific
// error can be concisely written as IsSpecificError<NS_SOME_ERROR> instead of
// as a more verbose lambda.
template <nsresult ErrorValue>
bool IsSpecificError(const nsresult aValue) {
  return aValue == ErrorValue;
}

#ifdef QM_ERROR_STACKS_ENABLED
template <nsresult ErrorValue>
bool IsSpecificError(const QMResult& aValue) {
  return aValue.NSResult() == ErrorValue;
}
#endif

// Helper template function so that QM_TRY fallback functions that are
// converting errors into specific in-band success values can be concisely
// written as ErrToOk<SuccessValueToReturn> (with the return type inferred).
// For example, many file-related APIs that access information about a file may
// return an nsresult error code if the file does not exist. From an
// application perspective, the file not existing is not actually exceptional
// and can instead be handled by the success case.
template <auto SuccessValue, typename V = decltype(SuccessValue)>
auto ErrToOk(const nsresult aValue) -> Result<V, nsresult> {
  return V{SuccessValue};
}

template <auto SuccessValue, typename V = decltype(SuccessValue)>
auto ErrToOkFromQMResult(const QMResult& aValue) -> Result<V, QMResult> {
  return V{SuccessValue};
}

// Helper template function so that QM_TRY fallback functions that are
// suppressing errors by converting them into (generic) success can be
// concisely written as ErrToDefaultOk<>.
template <typename V = mozilla::Ok>
auto ErrToDefaultOk(const nsresult aValue) -> Result<V, nsresult> {
  return V{};
}

template <typename MozPromiseType, typename RejectValueT = nsresult>
auto CreateAndRejectMozPromise(const char* aFunc, const RejectValueT& aRv)
    -> decltype(auto) {
  if constexpr (std::is_same_v<RejectValueT, nsresult>) {
    return MozPromiseType::CreateAndReject(aRv, aFunc);
  } else if constexpr (std::is_same_v<RejectValueT, QMResult>) {
    return MozPromiseType::CreateAndReject(aRv.NSResult(), aFunc);
  }
}

RefPtr<BoolPromise> CreateAndRejectBoolPromise(const char* aFunc, nsresult aRv);

RefPtr<Int64Promise> CreateAndRejectInt64Promise(const char* aFunc,
                                                 nsresult aRv);

RefPtr<BoolPromise> CreateAndRejectBoolPromiseFromQMResult(const char* aFunc,
                                                           const QMResult& aRv);

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
auto CollectEach(Step aStep, const Body& aBody)
    -> Result<mozilla::Ok, typename std::invoke_result_t<Step>::err_type> {
  using StepResultType = typename std::invoke_result_t<Step>::ok_type;

  static_assert(
      std::is_empty_v<
          typename std::invoke_result_t<Body, StepResultType&&>::ok_type>);

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

// This is like std::reduce with a to-be-defined execution policy (we don't want
// to std::terminate on an error, but probably it's fine to just propagate any
// error that occurred), operating not on a pair of iterators but rather a
// generator function.
template <typename InputGenerator, typename T, typename BinaryOp>
auto ReduceEach(InputGenerator aInputGenerator, T aInit,
                const BinaryOp& aBinaryOp)
    -> Result<T, typename std::invoke_result_t<InputGenerator>::err_type> {
  T res = std::move(aInit);

  // XXX This can be done in parallel!
  MOZ_TRY(CollectEach(
      std::move(aInputGenerator),
      [&res, &aBinaryOp](const auto& element)
          -> Result<Ok,
                    typename std::invoke_result_t<InputGenerator>::err_type> {
        MOZ_TRY_VAR(res, aBinaryOp(std::move(res), element));

        return Ok{};
      }));

  return std::move(res);
}

// This is like std::reduce with a to-be-defined execution policy (we don't want
// to std::terminate on an error, but probably it's fine to just propagate any
// error that occurred).
template <typename Range, typename T, typename BinaryOp>
auto Reduce(Range&& aRange, T aInit, const BinaryOp& aBinaryOp) {
  using std::begin;
  using std::end;
  return ReduceEach(
      [it = begin(aRange), end = end(aRange)]() mutable {
        auto res = ToMaybeRef(it != end ? &*it++ : nullptr);
        return Result<decltype(res), typename std::invoke_result_t<
                                         BinaryOp, T, decltype(res)>::err_type>(
            res);
      },
      aInit, aBinaryOp);
}

template <typename Range, typename Body>
auto CollectEachInRange(Range&& aRange, const Body& aBody)
    -> Result<mozilla::Ok, nsresult> {
  for (auto&& element : aRange) {
    MOZ_TRY(aBody(element));
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
    -> Result<mozilla::Ok, typename std::invoke_result_t<Cond>::err_type> {
  return CollectEach(aCond, [&aBody](bool) { return aBody(); });
}

template <>
class [[nodiscard]] GenericErrorResult<mozilla::ipc::IPCResult> {
  mozilla::ipc::IPCResult mErrorValue;

  template <typename V, typename E2>
  friend class Result;

 public:
  explicit GenericErrorResult(mozilla::ipc::IPCResult aErrorValue)
      : mErrorValue(aErrorValue) {
    MOZ_ASSERT(!aErrorValue);
  }

  GenericErrorResult(mozilla::ipc::IPCResult aErrorValue,
                     const ErrorPropagationTag&)
      : GenericErrorResult(aErrorValue) {}

  operator mozilla::ipc::IPCResult() const { return mErrorValue; }
};

namespace dom::quota {

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
  nsCOMPtr<nsISerialEventTarget> mOwningThread;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  nsISerialEventTarget* OwningThread() const;

 protected:
  BackgroundThreadObject();

  explicit BackgroundThreadObject(nsISerialEventTarget* aOwningThread);
};

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

#ifdef XP_WIN
void CacheUseDOSDevicePathSyntaxPrefValue();
#endif

Result<nsCOMPtr<nsIFile>, nsresult> QM_NewLocalFile(const nsAString& aPath);

nsDependentCSubstring GetLeafName(const nsACString& aPath);

Result<nsCOMPtr<nsIFile>, nsresult> CloneFileAndAppend(
    nsIFile& aDirectory, const nsAString& aPathElement);

enum class nsIFileKind {
  ExistsAsDirectory,
  ExistsAsFile,
  DoesNotExist,
};

// XXX We can use this outside of QM and its clients as well, probably. Maybe it
// could be moved to xpcom/io?
Result<nsIFileKind, nsresult> GetDirEntryKind(nsIFile& aFile);

Result<nsCOMPtr<mozIStorageStatement>, nsresult> CreateStatement(
    mozIStorageConnection& aConnection, const nsACString& aStatementString);

enum class SingleStepResult { AssertHasResult, ReturnNullIfNoResult };

template <SingleStepResult ResultHandling>
using SingleStepSuccessType =
    std::conditional_t<ResultHandling == SingleStepResult::AssertHasResult,
                       NotNull<nsCOMPtr<mozIStorageStatement>>,
                       nsCOMPtr<mozIStorageStatement>>;

template <SingleStepResult ResultHandling>
Result<SingleStepSuccessType<ResultHandling>, nsresult> ExecuteSingleStep(
    nsCOMPtr<mozIStorageStatement>&& aStatement);

// Creates a statement with the specified aStatementString, executes a single
// step, and returns the statement.
// Depending on the value ResultHandling,
// - it is asserted that there is a result (default resp.
//   SingleStepResult::AssertHasResult), and the success type is
//   MovingNotNull<nsCOMPtr<mozIStorageStatement>>
// - it is asserted that there is no result, and the success type is Ok
// - in case there is no result, nullptr is returned, and the success type is
//   nsCOMPtr<mozIStorageStatement>
// Any other errors are always propagated.
template <SingleStepResult ResultHandling = SingleStepResult::AssertHasResult>
Result<SingleStepSuccessType<ResultHandling>, nsresult>
CreateAndExecuteSingleStepStatement(mozIStorageConnection& aConnection,
                                    const nsACString& aStatementString);

namespace detail {

// Determine the absolute path of the root of our built source tree so we can
// derive source-relative paths for non-exported header files in
// MakeSourceFileRelativePath. Exported header files end up in the objdir and
// we have GetObjdirDistIncludeTreeBase for that.
nsDependentCSubstring GetSourceTreeBase();

// Determine the absolute path of the root of our built OBJDIR/dist/include
// directory. The aQuotaCommonHPath argument cleverly defaults to __FILE__
// initialized in our exported header; no argument should ever be provided to
// this method. GetSourceTreeBase handles identifying the root of the source
// tree.
nsDependentCSubstring GetObjdirDistIncludeTreeBase(
    const nsLiteralCString& aQuotaCommonHPath = nsLiteralCString(__FILE__));

nsDependentCSubstring MakeSourceFileRelativePath(
    const nsACString& aSourceFilePath);

}  // namespace detail

enum class Severity {
  Error,
  Warning,
  Info,
  Verbose,
};

#ifdef QM_LOG_ERROR_ENABLED
#  ifdef QM_ERROR_STACKS_ENABLED
using ResultType = Variant<QMResult, nsresult, Nothing>;

void LogError(const nsACString& aExpr, const ResultType& aResult,
              const nsACString& aSourceFilePath, int32_t aSourceFileLine,
              Severity aSeverity)
#  else
void LogError(const nsACString& aExpr, Maybe<nsresult> aMaybeRv,
              const nsACString& aSourceFilePath, int32_t aSourceFileLine,
              Severity aSeverity)
#  endif
    ;
#endif

#ifdef DEBUG
Result<bool, nsresult> WarnIfFileIsUnknown(nsIFile& aFile,
                                           const char* aSourceFilePath,
                                           int32_t aSourceFileLine);
#endif

// As HandleError is a function that will only be called in error cases, it is
// marked with MOZ_COLD to avoid bloating the code of calling functions, if it's
// not empty.
//
// For the same reason, the string-ish parameters are of type const char* rather
// than any ns*String type, to minimize the code at each call site. This
// deliberately de-optimizes runtime performance, which is uncritical during
// error handling.
//
// This functions are not intended to be called
// directly, they should only be called from the QM_* macros.
#ifdef QM_LOG_ERROR_ENABLED
template <typename T>
MOZ_COLD MOZ_NEVER_INLINE void HandleError(const char* aExpr, const T& aRv,
                                           const char* aSourceFilePath,
                                           int32_t aSourceFileLine,
                                           const Severity aSeverity) {
#  ifdef QM_ERROR_STACKS_ENABLED
  if constexpr (std::is_same_v<T, QMResult> || std::is_same_v<T, nsresult>) {
    mozilla::dom::quota::LogError(nsDependentCString(aExpr), ResultType(aRv),
                                  nsDependentCString(aSourceFilePath),
                                  aSourceFileLine, aSeverity);
  } else {
    mozilla::dom::quota::LogError(
        nsDependentCString(aExpr), ResultType(Nothing{}),
        nsDependentCString(aSourceFilePath), aSourceFileLine, aSeverity);
  }
#  else
  if constexpr (std::is_same_v<T, nsresult>) {
    mozilla::dom::quota::LogError(nsDependentCString(aExpr), Some(aRv),
                                  nsDependentCString(aSourceFilePath),
                                  aSourceFileLine, aSeverity);
  } else {
    mozilla::dom::quota::LogError(nsDependentCString(aExpr), Nothing{},
                                  nsDependentCString(aSourceFilePath),
                                  aSourceFileLine, aSeverity);
  }
#  endif
}
#else
template <typename T>
MOZ_ALWAYS_INLINE constexpr void HandleError(const char* aExpr, const T& aRv,
                                             const char* aSourceFilePath,
                                             int32_t aSourceFileLine,
                                             const Severity aSeverity) {}
#endif

template <typename T>
Nothing HandleErrorReturnNothing(const char* aExpr, const T& aRv,
                                 const char* aSourceFilePath,
                                 int32_t aSourceFileLine,
                                 const Severity aSeverity) {
  HandleError(aExpr, aRv, aSourceFilePath, aSourceFileLine, aSeverity);
  return Nothing();
}

template <typename T, typename CleanupFunc>
Nothing HandleErrorWithCleanupReturnNothing(const char* aExpr, const T& aRv,
                                            const char* aSourceFilePath,
                                            int32_t aSourceFileLine,
                                            const Severity aSeverity,
                                            CleanupFunc&& aCleanupFunc) {
  HandleError(aExpr, aRv, aSourceFilePath, aSourceFileLine, aSeverity);
  std::forward<CleanupFunc>(aCleanupFunc)(aRv);
  return Nothing();
}

template <size_t NFunc, size_t NExpr, typename T, typename CustomRetVal>
auto HandleCustomRetVal(const char (&aFunc)[NFunc], const char (&aExpr)[NExpr],
                        const T& aRv, CustomRetVal&& aCustomRetVal) {
  if constexpr (std::is_invocable<CustomRetVal, const char[NFunc],
                                  const char[NExpr]>::value) {
    return std::forward<CustomRetVal>(aCustomRetVal)(aFunc, aExpr);
  } else if constexpr (std::is_invocable<CustomRetVal, const char[NFunc],
                                         const T&>::value) {
    return aCustomRetVal(aFunc, aRv);
  } else if constexpr (std::is_invocable<CustomRetVal, const T&>::value) {
    return aCustomRetVal(aRv);
  } else {
    return std::forward<CustomRetVal>(aCustomRetVal);
  }
}

template <SingleStepResult ResultHandling = SingleStepResult::AssertHasResult,
          typename BindFunctor>
Result<SingleStepSuccessType<ResultHandling>, nsresult>
CreateAndExecuteSingleStepStatement(mozIStorageConnection& aConnection,
                                    const nsACString& aStatementString,
                                    BindFunctor aBindFunctor) {
  QM_TRY_UNWRAP(auto stmt, CreateStatement(aConnection, aStatementString));

  QM_TRY(aBindFunctor(*stmt));

  return ExecuteSingleStep<ResultHandling>(std::move(stmt));
}

template <typename StepFunc>
Result<Ok, nsresult> CollectWhileHasResult(mozIStorageStatement& aStmt,
                                           StepFunc&& aStepFunc) {
  return CollectWhile(
      [&aStmt] {
        QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_MEMBER(aStmt, ExecuteStep));
      },
      [&aStmt, &aStepFunc] { return aStepFunc(aStmt); });
}

template <typename StepFunc,
          typename ArrayType = nsTArray<typename std::invoke_result_t<
              StepFunc, mozIStorageStatement&>::ok_type>>
auto CollectElementsWhileHasResult(mozIStorageStatement& aStmt,
                                   StepFunc&& aStepFunc)
    -> Result<ArrayType, nsresult> {
  ArrayType res;

  QM_TRY(CollectWhileHasResult(
      aStmt, [&aStepFunc, &res](auto& stmt) -> Result<Ok, nsresult> {
        QM_TRY_UNWRAP(auto element, aStepFunc(stmt));
        res.AppendElement(std::move(element));
        return Ok{};
      }));

  return std::move(res);
}

template <typename ArrayType, typename StepFunc>
auto CollectElementsWhileHasResultTyped(mozIStorageStatement& aStmt,
                                        StepFunc&& aStepFunc) {
  return CollectElementsWhileHasResult<StepFunc, ArrayType>(
      aStmt, std::forward<StepFunc>(aStepFunc));
}

namespace detail {
template <typename Cancel, typename Body>
Result<mozilla::Ok, nsresult> CollectEachFile(nsIFile& aDirectory,
                                              const Cancel& aCancel,
                                              const Body& aBody) {
  QM_TRY_INSPECT(const auto& entries, MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                                          nsCOMPtr<nsIDirectoryEnumerator>,
                                          aDirectory, GetDirectoryEntries));

  return CollectEach(
      [&entries, &aCancel]() -> Result<nsCOMPtr<nsIFile>, nsresult> {
        if (aCancel()) {
          return nsCOMPtr<nsIFile>{};
        }

        QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsCOMPtr<nsIFile>,
                                                        entries, GetNextFile));
      },
      aBody);
}
}  // namespace detail

template <typename Body>
Result<mozilla::Ok, nsresult> CollectEachFile(nsIFile& aDirectory,
                                              const Body& aBody) {
  return detail::CollectEachFile(
      aDirectory, [] { return false; }, aBody);
}

template <typename Body>
Result<mozilla::Ok, nsresult> CollectEachFileAtomicCancelable(
    nsIFile& aDirectory, const Atomic<bool>& aCanceled, const Body& aBody) {
  return detail::CollectEachFile(
      aDirectory, [&aCanceled] { return static_cast<bool>(aCanceled); }, aBody);
}

template <typename T, typename Body>
auto ReduceEachFileAtomicCancelable(nsIFile& aDirectory,
                                    const Atomic<bool>& aCanceled, T aInit,
                                    const Body& aBody) -> Result<T, nsresult> {
  QM_TRY_INSPECT(const auto& entries, MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                                          nsCOMPtr<nsIDirectoryEnumerator>,
                                          aDirectory, GetDirectoryEntries));

  return ReduceEach(
      [&entries, &aCanceled]() -> Result<nsCOMPtr<nsIFile>, nsresult> {
        if (aCanceled) {
          return nsCOMPtr<nsIFile>{};
        }

        QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsCOMPtr<nsIFile>,
                                                        entries, GetNextFile));
      },
      std::move(aInit), aBody);
}

constexpr bool IsDatabaseCorruptionError(const nsresult aRv) {
  return aRv == NS_ERROR_FILE_CORRUPTED || aRv == NS_ERROR_STORAGE_IOERR;
}

template <typename Func>
auto CallWithDelayedRetriesIfAccessDenied(Func&& aFunc, uint32_t aMaxRetries,
                                          uint32_t aDelayMs)
    -> Result<typename std::invoke_result_t<Func>::ok_type, nsresult> {
  uint32_t retries = 0;

  while (true) {
    auto result = std::forward<Func>(aFunc)();

    if (result.isOk()) {
      return result;
    }

    if (result.inspectErr() != NS_ERROR_FILE_IS_LOCKED &&
        result.inspectErr() != NS_ERROR_FILE_ACCESS_DENIED) {
      return result;
    }

    if (retries++ >= aMaxRetries) {
      return result;
    }

    PR_Sleep(PR_MillisecondsToInterval(aDelayMs));
  }
}

namespace detail {

template <bool flag = false>
void UnsupportedReturnType() {
  static_assert(flag, "Unsupported return type!");
}

}  // namespace detail

template <typename Initialization, typename StringGenerator, typename Func>
auto ExecuteInitialization(
    FirstInitializationAttempts<Initialization, StringGenerator>&
        aFirstInitializationAttempts,
    const Initialization aInitialization, Func&& aFunc)
    -> std::invoke_result_t<Func, const FirstInitializationAttempt<
                                      Initialization, StringGenerator>&> {
  return aFirstInitializationAttempts.WithFirstInitializationAttempt(
      aInitialization, [&aFunc](auto&& firstInitializationAttempt) {
        auto res = std::forward<Func>(aFunc)(firstInitializationAttempt);

        const auto rv = [&res]() -> nsresult {
          using RetType =
              std::invoke_result_t<Func, const FirstInitializationAttempt<
                                             Initialization, StringGenerator>&>;

          if constexpr (std::is_same_v<RetType, nsresult>) {
            return res;
          } else if constexpr (mozilla::detail::IsResult<RetType>::value &&
                               std::is_same_v<typename RetType::err_type,
                                              nsresult>) {
            return res.isOk() ? NS_OK : res.inspectErr();
          } else {
            detail::UnsupportedReturnType();
          }
        }();

        // NS_ERROR_ABORT signals a non-fatal, recoverable problem during
        // initialization. We do not want these kind of failures to count
        // against our overall first initialization attempt telemetry. Thus we
        // just ignore this kind of failure and keep
        // aFirstInitializationAttempts unflagged to stay ready to record a real
        // success or failure on the next attempt.
        if (rv == NS_ERROR_ABORT) {
          return res;
        }

        if (!firstInitializationAttempt.Recorded()) {
          firstInitializationAttempt.Record(rv);
        }

        return res;
      });
}

template <typename Initialization, typename StringGenerator, typename Func>
auto ExecuteInitialization(
    FirstInitializationAttempts<Initialization, StringGenerator>&
        aFirstInitializationAttempts,
    const Initialization aInitialization, const nsACString& aContext,
    Func&& aFunc)
    -> std::invoke_result_t<Func, const FirstInitializationAttempt<
                                      Initialization, StringGenerator>&> {
  return ExecuteInitialization(
      aFirstInitializationAttempts, aInitialization,
      [&](const auto& firstInitializationAttempt) -> decltype(auto) {
#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
        const auto maybeScopedLogExtraInfo =
            firstInitializationAttempt.Recorded()
                ? Nothing{}
                : Some(ScopedLogExtraInfo{ScopedLogExtraInfo::kTagContext,
                                          aContext});
#endif

        return std::forward<Func>(aFunc)(firstInitializationAttempt);
      });
}

}  // namespace dom::quota
}  // namespace mozilla

#endif  // mozilla_dom_quota_quotacommon_h__
