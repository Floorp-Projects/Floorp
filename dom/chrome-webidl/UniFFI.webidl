/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Interface for making UniFFI scaffolding calls
//
// Gecko uses UniFFI to generate privileged JS bindings for Rust components.
// UniFFI defines a C-ABI FFI layer for calling into Rust, called the
// scaffolding. This interface is a bridge that allows the JS code to make
// scaffolding calls
//
// See https://mozilla.github.io/uniffi-rs/ for details.

// Opaque type used to represent a pointer from Rust
[ChromeOnly, Exposed=Window]
interface UniFFIPointer {};

// Types that can be passed or returned from scaffolding functions
//
// - double is used for all numeric types (including bool, which the JS code
//   coerces to an int)
// - ArrayBuffer is used for RustBuffer
// - UniFFIPointer is used for Arc pointers
typedef (double or ArrayBuffer or UniFFIPointer) UniFFIScaffoldingType;

// The result of a call into UniFFI scaffolding call
enum UniFFIScaffoldingCallCode {
   "success",         // Successful return
   "error",           // Rust Err return
   "internal-error",  // Internal/unexpected error
};

dictionary UniFFIScaffoldingCallResult {
    required UniFFIScaffoldingCallCode code;
    // For success, this will be the return value for non-void returns
    // For error, this will be an ArrayBuffer storing the serialized error value
    UniFFIScaffoldingType data;
    // For internal-error, this will be a utf-8 string describing the error
    ByteString internalErrorMessage;
};

// Functions to facilitate UniFFI scaffolding calls
//
// These should only be called by the generated code from UniFFI.
[ChromeOnly, Exposed=Window]
namespace UniFFIScaffolding {
  [Throws]
  Promise<UniFFIScaffoldingCallResult> callAsync(unsigned long long id, UniFFIScaffoldingType... args);

  [Throws]
  UniFFIScaffoldingCallResult callSync(unsigned long long id, UniFFIScaffoldingType... args);

  [Throws]
  UniFFIPointer readPointer(unsigned long long id, ArrayBuffer buff, long position);

  [Throws]
  void writePointer(unsigned long long id, UniFFIPointer ptr, ArrayBuffer buff, long position);
};
