/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ValueUsage_h
#define frontend_ValueUsage_h

namespace js {
namespace frontend {

// Used to control whether JSOP_CALL_IGNORES_RV is emitted for function calls.
enum class ValueUsage {
  // Assume the value of the current expression may be used. This is always
  // correct but prohibits JSOP_CALL_IGNORES_RV.
  WantValue,

  // Pass this when emitting an expression if the expression's value is
  // definitely unused by later instructions. You must make sure the next
  // instruction is JSOP_POP, a jump to a JSOP_POP, or something similar.
  IgnoreValue
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_ValueUsage_h */
