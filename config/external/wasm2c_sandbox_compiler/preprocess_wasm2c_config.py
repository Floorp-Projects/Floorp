# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Souce Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distibuted with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import itertools

# The wasm2c source relies on CMAKE to generate a config file to be used to build the project
# Since we do not use cmake, this script automates the generation of a similar config suitable
# for firefox builds
# The script has a list of known variables it can replace and throws an error if it encounters a
# new variable (for instance when the in-tree source is updated)

# This python script knows how to replace the following variables normally configured by cmake for
# the wasm2c source
known_vars = [
    '#cmakedefine WABT_VERSION_STRING "@WABT_VERSION_STRING@"',
    "#cmakedefine WABT_DEBUG @WABT_DEBUG@",
    "#cmakedefine01 HAVE_ALLOCA_H",
    "#cmakedefine01 HAVE_UNISTD_H",
    "#cmakedefine01 HAVE_SNPRINTF",
    "#cmakedefine01 HAVE_SSIZE_T",
    "#cmakedefine01 HAVE_STRCASECMP",
    "#cmakedefine01 HAVE_WIN32_VT100",
    "#cmakedefine01 WABT_BIG_ENDIAN",
    "#cmakedefine01 HAVE_OPENSSL_SHA_H",
    "#cmakedefine01 COMPILER_IS_CLANG",
    "#cmakedefine01 COMPILER_IS_GNU",
    "#cmakedefine01 COMPILER_IS_MSVC",
    "#cmakedefine01 WITH_EXCEPTIONS",
    "#define SIZEOF_SIZE_T @SIZEOF_SIZE_T@",
]

# The above variables are replaced with the code shown below
replaced_variables = """
// mozilla-config.h defines the following which is used
// - HAVE_ALLOCA_H
// - HAVE_UNISTD_H
#include "mozilla-config.h"

#define WABT_VERSION_STRING "Firefox-in-tree-version"

#define WABT_DEBUG 0

/* We don't require color printing of wasm2c errors on any platform */
#define HAVE_WIN32_VT100 0

#ifdef _WIN32
  // Ignore whatever is set in mozilla-config.h wrt alloca because it is
  // wrong when cross-compiling on Windows.
  #undef HAVE_ALLOCA_H
  // It is wrong when cross-compiling on Windows.
  #undef HAVE_UNISTD_H
  /* Whether ssize_t is defined by stddef.h */
  #define HAVE_SSIZE_T 0
  /* Whether strcasecmp is defined by strings.h */
  #define HAVE_STRCASECMP 0
#else
  #define HAVE_SSIZE_T 1
  #define HAVE_STRCASECMP 1
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define WABT_BIG_ENDIAN 0
#  elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define WABT_BIG_ENDIAN 1
#  else
#    error "Can't handle mixed-endian architectures"
#  endif
#else
#  error "Don't know how to determine endianness"
#endif

/* Use internal Pico-SHA. Never use OpenSSL */
#define HAVE_OPENSSL_SHA_H 0

/* Whether snprintf is defined by stdio.h */
#define HAVE_SNPRINTF 1

#if defined(_MSC_VER)
  #define COMPILER_IS_GNU 0
  #define COMPILER_IS_CLANG 0
  #define COMPILER_IS_MSVC 1
#elif defined(__GNUC__)
  #if defined(__clang__)
    #define COMPILER_IS_GNU 0
    #define COMPILER_IS_CLANG 1
    #define COMPILER_IS_MSVC 0
  #else
    #define COMPILER_IS_GNU 1
    #define COMPILER_IS_CLANG 0
    #define COMPILER_IS_MSVC 0
  #endif
#else
  #error "Unknown compiler"
#endif

#define WITH_EXCEPTIONS 0

#if SIZE_MAX == 0xffffffffffffffff
  #define SIZEOF_SIZE_T 8
#elif SIZE_MAX == 0xffffffff
  #define SIZEOF_SIZE_T 4
#else
  #error "Unknown size of size_t"
#endif
"""


def generate_config(output, config_h_in):
    file_config_h_in = open(config_h_in, "r")
    lines = file_config_h_in.readlines()

    # Remove the known cmake variables
    for known_var in known_vars:
        lines = [x for x in lines if not x.startswith(known_var)]

    # Do a sanity check to make sure there are no unknown variables
    remaining_vars = [x for x in lines if x.startswith("#cmakedefine") or "@" in x]
    if len(remaining_vars) > 0:
        raise BaseException("Unknown cmake variables: " + str(remaining_vars))

    pos = lines.index("#define WABT_CONFIG_H_\n")
    skipped = itertools.takewhile(
        lambda x: not (x.strip()) or x.startswith("#include "), lines[pos + 1 :]
    )
    pos += len(list(skipped))
    pre_include_lines = lines[0:pos]
    post_include_lines = lines[pos:]
    output_str = (
        "".join(pre_include_lines)
        + "\n"
        + replaced_variables
        + "\n"
        + "".join(post_include_lines)
    )
    output.write(output_str)
