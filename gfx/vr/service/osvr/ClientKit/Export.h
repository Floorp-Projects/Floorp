/** @file
    @brief Automatically-generated export header - do not edit!

    @date 2016

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2016 Sensics, Inc.
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

#ifndef OSVR_CLIENTKIT_EXPORT_H
#define OSVR_CLIENTKIT_EXPORT_H

#ifdef OSVR_CLIENTKIT_STATIC_DEFINE
#  define OSVR_CLIENTKIT_EXPORT
#  define OSVR_CLIENTKIT_NO_EXPORT
#endif

/* Per-compiler advance preventative definition */
#if defined(__BORLANDC__) || defined(__CODEGEARC__) || defined(__HP_aCC) ||    \
    defined(__PGI) || defined(__WATCOMC__)
/* Compilers that don't support deprecated, according to CMake. */
#  ifndef OSVR_CLIENTKIT_DEPRECATED
#    define OSVR_CLIENTKIT_DEPRECATED
#  endif
#endif

/* Check for attribute support */
#if defined(__INTEL_COMPILER)
/* Checking before GNUC because Intel implements GNU extensions,
 * so it chooses to define __GNUC__ as well. */
#  if __INTEL_COMPILER >= 1200
/* Intel compiler 12.0 or newer can handle these attributes per CMake */
#    define OSVR_CLIENTKIT_EXPORT_HEADER_SUPPORTS_ATTRIBUTES
#  endif

#elif defined(__GNUC__)
#  if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
/* GCC 4.2+ */
#    define OSVR_CLIENTKIT_EXPORT_HEADER_SUPPORTS_ATTRIBUTES
#  endif
#endif

/* Per-platform defines */
#if defined(_MSC_VER)
/* MSVC on Windows */

#ifndef OSVR_CLIENTKIT_EXPORT
#  ifdef osvrClientKit_EXPORTS
      /* We are building this library */
#    define OSVR_CLIENTKIT_EXPORT __declspec(dllexport)
#  else
      /* We are using this library */
#    define OSVR_CLIENTKIT_EXPORT __declspec(dllimport)
#  endif
#endif

#ifndef OSVR_CLIENTKIT_DEPRECATED
#  define OSVR_CLIENTKIT_DEPRECATED __declspec(deprecated)
#endif

#elif defined(_WIN32) && defined(__GNUC__)
/* GCC-compatible on Windows */

#ifndef OSVR_CLIENTKIT_EXPORT
#  ifdef osvrClientKit_EXPORTS
      /* We are building this library */
#    define OSVR_CLIENTKIT_EXPORT __attribute__((dllexport))
#  else
      /* We are using this library */
#    define OSVR_CLIENTKIT_EXPORT __attribute__((dllimport))
#  endif
#endif

#ifndef OSVR_CLIENTKIT_DEPRECATED
#  define OSVR_CLIENTKIT_DEPRECATED __attribute__((__deprecated__))
#endif

#elif defined(OSVR_CLIENTKIT_EXPORT_HEADER_SUPPORTS_ATTRIBUTES) ||         \
    (defined(__APPLE__) && defined(__MACH__))
/* GCC4.2+ compatible (assuming something *nix-like) and Mac OS X */
/* (The first macro is defined at the top of the file, if applicable) */
/* see https://gcc.gnu.org/wiki/Visibility */

#ifndef OSVR_CLIENTKIT_EXPORT
  /* We are building/using this library */
#  define OSVR_CLIENTKIT_EXPORT __attribute__((visibility("default")))
#endif

#ifndef OSVR_CLIENTKIT_NO_EXPORT
#  define OSVR_CLIENTKIT_NO_EXPORT __attribute__((visibility("hidden")))
#endif

#ifndef OSVR_CLIENTKIT_DEPRECATED
#  define OSVR_CLIENTKIT_DEPRECATED __attribute__((__deprecated__))
#endif

#endif
/* End of platform ifdefs */

/* fallback def */
#ifndef OSVR_CLIENTKIT_EXPORT
#  define OSVR_CLIENTKIT_EXPORT
#endif

/* fallback def */
#ifndef OSVR_CLIENTKIT_NO_EXPORT
#  define OSVR_CLIENTKIT_NO_EXPORT
#endif

/* fallback def */
#ifndef OSVR_CLIENTKIT_DEPRECATED_EXPORT
#  define OSVR_CLIENTKIT_DEPRECATED_EXPORT OSVR_CLIENTKIT_EXPORT OSVR_CLIENTKIT_DEPRECATED
#endif

/* fallback def */
#ifndef OSVR_CLIENTKIT_DEPRECATED_NO_EXPORT
#  define OSVR_CLIENTKIT_DEPRECATED_NO_EXPORT OSVR_CLIENTKIT_NO_EXPORT OSVR_CLIENTKIT_DEPRECATED
#endif

/* Clean up after ourselves */
#undef OSVR_CLIENTKIT_EXPORT_HEADER_SUPPORTS_ATTRIBUTES

#endif
