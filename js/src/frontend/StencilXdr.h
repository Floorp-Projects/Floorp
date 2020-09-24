/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_StencilXdr_h
#define frontend_StencilXdr_h

#include "frontend/CompilationInfo.h"  // CompilationInfo
#include "frontend/ObjLiteral.h"       // ObjLiteralStencil
#include "frontend/Stencil.h"          // Stencil
#include "vm/Scope.h"                  // Scope, ScopeKindString
#include "vm/Xdr.h"                    // XDRMode, XDRResult, XDREncoder

namespace js {
namespace frontend {

// This is just a namespace class that can be used in friend declarations,
// so that the statically declared XDR methods within have access to the
// relevant struct internals.
class StencilXDR {
 public:
  template <XDRMode mode>
  static XDRResult Scope(XDRState<mode>* xdr, ScopeStencil& stencil);

  template <XDRMode mode>
  static XDRResult ObjLiteral(XDRState<mode>* xdr, ObjLiteralStencil& stencil);

  template <XDRMode mode>
  static XDRResult BigInt(XDRState<mode>* xdr, BigIntStencil& stencil);

  template <XDRMode mode>
  static XDRResult RegExp(XDRState<mode>* xdr, RegExpStencil& stencil);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_StencilXdr_h */
