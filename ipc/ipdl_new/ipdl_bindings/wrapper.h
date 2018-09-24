/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef dom_base_ipdl_bindings_wrapper_h
#define dom_base_ipdl_bindings_wrapper_h

#include "ipdl_ffi_generated.h"
#include "nsStringFwd.h"

namespace mozilla {
namespace ipdl {
namespace wrapper {

// Parse the given ipdl file and return a Rust-owned AST.
const ffi::AST*
Parse(const nsACString& aIPDLFile, nsACString& errorString);

// Free the AST from Rust code.
void
FreeAST(const ffi::AST* aAST);

// Get the translation unit with the given tuid from the given AST.
const ffi::TranslationUnit*
GetTU(const ffi::AST* aAST, ffi::TUId aTUID);

// Get the tuid of the main translation unit of the AST.
ffi::TUId
GetMainTUId(const ffi::AST* aAST);

} // wrapper
} // ipdl
} // mozilla

#endif // dom_base_ipdl_bindings_wrapper_h
