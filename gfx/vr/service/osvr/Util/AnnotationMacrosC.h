/** @file
    @brief Header containing macros for source-level annotation.

    In theory, supporting MSVC SAL, as well as compatible GCC and
    Clang attributes. In practice, expanded as time allows and requires.

    Must be c-safe!

    @date 2014

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

/*
// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef INCLUDED_AnnotationMacrosC_h_GUID_48538D9B_35E3_4E9A_D2B0_D83D51DD5900
#define INCLUDED_AnnotationMacrosC_h_GUID_48538D9B_35E3_4E9A_D2B0_D83D51DD5900

#ifndef OSVR_DISABLE_ANALYSIS

#if defined(_MSC_VER) && (_MSC_VER >= 1700)
/* Visual C++ (2012 and newer) */
/* Using SAL attribute format:
 * http://msdn.microsoft.com/en-us/library/ms182032(v=vs.120).aspx */

#include <sal.h>

#define OSVR_IN _In_
#define OSVR_IN_PTR _In_
#define OSVR_IN_OPT _In_opt_
#define OSVR_IN_STRZ _In_z_
#define OSVR_IN_READS(NUM_ELEMENTS) _In_reads_(NUM_ELEMENTS)

#define OSVR_OUT _Out_
#define OSVR_OUT_PTR _Outptr_
#define OSVR_OUT_OPT _Out_opt_

#define OSVR_INOUT _Inout_
#define OSVR_INOUT_PTR _Inout_

#define OSVR_RETURN_WARN_UNUSED _Must_inspect_result_
#define OSVR_RETURN_SUCCESS_CONDITION(X) _Return_type_success_(X)

/* end of msvc section */
#elif defined(__GNUC__) && (__GNUC__ >= 4)
/* section for GCC and GCC-alikes */

#if defined(__clang__)
/* clang-specific section */
#endif

#define OSVR_FUNC_NONNULL(X) __attribute__((__nonnull__ X))
#define OSVR_RETURN_WARN_UNUSED __attribute__((warn_unused_result))

/* end of gcc section and compiler detection */
#endif

/* end of ndef disable analysis */
#endif

/* Fallback declarations */
/**
@defgroup annotation_macros Static analysis annotation macros
@brief Wrappers for Microsoft's SAL annotations and others
@ingroup Util

Use of these is optional, but recommended particularly for C APIs,
as well as any methods handling a buffer with a length.
@{
*/
/** @name Parameter annotations

    These indicate the role and valid values for parameters to functions.

    At most one of these should be placed before a parameter's type name in the
   function parameter list, in both the declaration and definition. (They must
   match!)
   @{
*/
/** @def OSVR_IN
    @brief Indicates a required function parameter that serves only as input.
*/
#ifndef OSVR_IN
#define OSVR_IN
#endif

/** @def OSVR_IN_PTR
    @brief Indicates a required pointer (non-null) function parameter that
    serves only as input.
*/
#ifndef OSVR_IN_PTR
#define OSVR_IN_PTR
#endif

/** @def OSVR_IN_OPT
    @brief Indicates a function parameter (pointer) that serves only as input,
   but is optional and might be NULL.
*/
#ifndef OSVR_IN_OPT
#define OSVR_IN_OPT
#endif

/** @def OSVR_IN_STRZ
    @brief Indicates a null-terminated string function parameter that serves
   only as input.
*/
#ifndef OSVR_IN_STRZ
#define OSVR_IN_STRZ
#endif

/** @def OSVR_IN_READS(NUM_ELEMENTS)
    @brief Indicates a buffer containing input with the specified number of
   elements.

    The specified number of elements is typically the name of another parameter.
*/
#ifndef OSVR_IN_READS
#define OSVR_IN_READS(NUM_ELEMENTS)
#endif

/** @def OSVR_OUT
    @brief Indicates a required function parameter that serves only as output.
    In C code, since this usually means "pointer", you probably want
   OSVR_OUT_PTR instead.
*/
#ifndef OSVR_OUT
#define OSVR_OUT
#endif

/** @def OSVR_OUT_PTR
    @brief Indicates a required pointer (non-null) function parameter that
    serves only as output.
*/
#ifndef OSVR_OUT_PTR
#define OSVR_OUT_PTR
#endif

/** @def OSVR_OUT_OPT
    @brief Indicates a function parameter (pointer) that serves only as output,
   but is optional and might be NULL
*/
#ifndef OSVR_OUT_OPT
#define OSVR_OUT_OPT
#endif

/** @def OSVR_INOUT
    @brief Indicates a required function parameter that is both read and written
   to.

    In C code, since this usually means "pointer", you probably want
   OSVR_INOUT_PTR instead.
*/
#ifndef OSVR_INOUT
#define OSVR_INOUT
#endif

/** @def OSVR_INOUT_PTR
    @brief Indicates a required pointer (non-null) function parameter that is
    both read and written to.
*/
#ifndef OSVR_INOUT_PTR
#define OSVR_INOUT_PTR
#endif

/* End of parameter annotations. */
/** @} */

/** @name Function annotations

    These indicate particular relevant aspects about a function. Some
    duplicate the effective meaning of parameter annotations: applying both
    allows the fullest extent of static analysis tools to analyze the code,
    and in some compilers, generate warnings.

   @{
*/
/** @def OSVR_FUNC_NONNULL(X)
    @brief Indicates the parameter(s) that must be non-null.

    @param X A parenthesized list of parameters by number (1-based index)

    Should be placed after a function declaration (but before the
   semicolon). Repeating in the definition is not needed.
*/
#ifndef OSVR_FUNC_NONNULL
#define OSVR_FUNC_NONNULL(X)
#endif

/** @def OSVR_RETURN_WARN_UNUSED
    @brief Indicates the function has a return value that must be used (either a
   security problem or an obvious bug if not).

    Should be placed before the return value (and virtual keyword, if
   applicable) in both declaration and definition.
*/
#ifndef OSVR_RETURN_WARN_UNUSED
#define OSVR_RETURN_WARN_UNUSED
#endif
/* End of function annotations. */
/** @} */

/** @def OSVR_RETURN_SUCCESS_CONDITION
    @brief Applied to a typedef, indicates the condition for `return` under
   which a function returning it should be considered to have succeeded (thus
   holding certain specifications).

    Should be placed before the typename in a typedef, with the parameter
   including the keyword `return` to substitute for the return value.
*/
#ifndef OSVR_RETURN_SUCCESS_CONDITION
#define OSVR_RETURN_SUCCESS_CONDITION(X)
#endif

/* End of annotation group. */
/** @} */
#endif
