/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EXTENSIONS_SPELLCHECK_HUNSPELL_GLUE_RLBOXHUNSPELLTYPES_H_
#define EXTENSIONS_SPELLCHECK_HUNSPELL_GLUE_RLBOXHUNSPELLTYPES_H_

#include <stddef.h>
#include "mozilla/rlbox/rlbox_types.hpp"
#include "hunspell_csutil.hxx"

#ifdef MOZ_WASM_SANDBOXING_HUNSPELL
RLBOX_DEFINE_BASE_TYPES_FOR(hunspell, wasm2c)
#else
RLBOX_DEFINE_BASE_TYPES_FOR(hunspell, noop)
#endif

#define sandbox_fields_reflection_hunspell_class_cs_info(f, g, ...) \
  f(unsigned char, ccase, FIELD_NORMAL, ##__VA_ARGS__) g()          \
      f(unsigned char, clower, FIELD_NORMAL, ##__VA_ARGS__) g()     \
          f(unsigned char, cupper, FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_hunspell_allClasses(f, ...) \
  f(cs_info, hunspell, ##__VA_ARGS__)

#endif
