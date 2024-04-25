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

// Define some ID types to identify various parts of the UDL interfaces.  Using
// these IDs, allow this .webidl file to remain static, but support new
// interfaces.  Using IDs is that the C++ and JS code need to agree on their
// meaning, which is handled by
// toolkit/components/uniffi-bindgen-gecko-js/src/ci_list.rs.

// Identifies a scaffolding function.
typedef unsigned long long UniFFIFunctionId;

// Identifies a pointer type
typedef unsigned long long UniFFIPointerId;

// Identifies a callback interface
typedef unsigned long UniFFICallbackInterfaceId;

// Identifies a method of a callback interface
typedef unsigned long UniFFICallbackMethodId;

// Handle for a callback interface instance
typedef unsigned long long UniFFICallbackObjectHandle;

// Opaque type used to represent a pointer from Rust
[ChromeOnly, Exposed=Window]
interface UniFFIPointer { };

// Types that can be passed or returned from scaffolding functions
//
// - double is used for all numeric types and types which the JS code coerces
//   to an int including Boolean and CallbackInterface.
// - ArrayBuffer is used for RustBuffer
// - UniFFIPointer is used for Arc pointers
typedef (double or ArrayBuffer or UniFFIPointer) UniFFIScaffoldingValue;

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
    UniFFIScaffoldingValue data;
    // For internal-error, this will be a utf-8 string describing the error
    ByteString internalErrorMessage;
};

// JS handler for calling a CallbackInterface method.
// The arguments and return value are always packed into an ArrayBuffer
callback UniFFICallbackHandler = undefined (UniFFICallbackObjectHandle objectId, UniFFICallbackMethodId methodId, ArrayBuffer aArgs);

// Functions to facilitate UniFFI scaffolding calls
[ChromeOnly, Exposed=Window]
namespace UniFFIScaffolding {
  // Call a scaffolding function on the worker thread.
  //
  // id is a unique identifier for the function, known to both the C++ and JS code
  [Throws]
  Promise<UniFFIScaffoldingCallResult> callAsync(UniFFIFunctionId id, UniFFIScaffoldingValue... args);

  // Call a scaffolding function on the main thread
  //
  // id is a unique identifier for the function, known to both the C++ and JS code
  [Throws]
  UniFFIScaffoldingCallResult callSync(UniFFIFunctionId id, UniFFIScaffoldingValue... args);

  // Read a UniFFIPointer from an ArrayBuffer
  //
  // id is a unique identifier for the pointer type, known to both the C++ and JS code
  [Throws]
  UniFFIPointer readPointer(UniFFIPointerId id, ArrayBuffer buff, long position);

  // Write a UniFFIPointer to an ArrayBuffer
  //
  // id is a unique identifier for the pointer type, known to both the C++ and JS code
  [Throws]
  undefined writePointer(UniFFIPointerId id, UniFFIPointer ptr, ArrayBuffer buff, long position);

  // Register the global calblack handler
  //
  // This will be used to invoke all calls for a CallbackInterface.
  // interfaceId is a unique identifier for the callback interface, known to both the C++ and JS code
  [Throws]
  undefined registerCallbackHandler(UniFFICallbackInterfaceId interfaceId, UniFFICallbackHandler handler);

  // Deregister the global calblack handler
  //
  // This is called at shutdown to clear out the reference to the JS function.
  [Throws]
  undefined deregisterCallbackHandler(UniFFICallbackInterfaceId interfaceId);
};
