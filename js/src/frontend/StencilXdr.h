/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_StencilXdr_h
#define frontend_StencilXdr_h

#include "frontend/ObjLiteral.h"  // ObjLiteralStencil
#include "frontend/Stencil.h"     // *Stencil
#include "vm/Scope.h"             // Scope, ScopeKindString
#include "vm/Xdr.h"               // XDRMode, XDRResult, XDREncoder

namespace js {
namespace frontend {

// Check that we can copy data to disk and restore it in another instance of
// the program in a different address space.
template <typename DataT>
struct CanCopyDataToDisk {
  // Check that the object is fully packed, to save disk space.
#ifdef __cpp_lib_has_unique_object_representations
  static constexpr bool unique_repr =
      std::has_unique_object_representations<DataT>();
#else
  static constexpr bool unique_repr = true;
#endif

  // Approximation which assumes that 32bits variant of the class would not
  // have pointers if the 64bits variant does not have pointer.
  static constexpr bool no_pointer =
      alignof(DataT) < alignof(void*) || sizeof(void*) == sizeof(uint32_t);

  static constexpr bool value = unique_repr && no_pointer;
};

// This is just a namespace class that can be used in friend declarations,
// so that the statically declared XDR methods within have access to the
// relevant struct internals.
class StencilXDR {
 public:
  template <XDRMode mode>
  static XDRResult ScopeData(XDRState<mode>* xdr, ScopeStencil& stencil,
                             BaseParserScopeData*& baseScopeData);

  template <XDRMode mode>
  static XDRResult ObjLiteral(XDRState<mode>* xdr, ObjLiteralStencil& stencil);

  template <XDRMode mode>
  static XDRResult BigInt(XDRState<mode>* xdr, BigIntStencil& stencil);

  template <XDRMode mode>
  static XDRResult SharedData(js::XDRState<mode>* xdr,
                              RefPtr<SharedImmutableScriptData>& sisd);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_StencilXdr_h */
