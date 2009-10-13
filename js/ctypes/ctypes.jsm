/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is js-ctypes.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Witte <dwitte@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let EXPORTED_SYMBOLS = [ "ctypes" ];

/**
 * This is the js module for ctypes. Import it like so:
 *   Components.utils.import("resource://gre/modules/ctypes.jsm");
 *
 * This will create a 'ctypes' object, which provides an interface to describe
 * C types and call C functions from a dynamic library. It has the following
 * properties and functions:
 *
 *   ABI constants that specify the calling convention to use.
 *   ctypes.default_abi corresponds to the cdecl convention, and in almost all
 *   cases is the correct choice. ctypes.stdcall is provided for calling
 *   functions in the Microsoft Win32 API.
 *
 *   ctypes.default_abi    // corresponds to cdecl
 *   ctypes.stdcall_abi    // for calling Win32 API functions
 *
 *   Types available for arguments and return values, representing
 *   their C counterparts.
 *
 *   ctypes.void_t         // Only allowed for return types.
 *   ctypes.bool           // _Bool type (assumed 8 bits wide).
 *   ctypes.int8_t         // int8_t (signed char) type.
 *   ctypes.int16_t        // int16_t (short) type.
 *   ctypes.int32_t        // int32_t (int) type.
 *   ctypes.int64_t        // int64_t (long long) type.
 *   ctypes.uint8_t        // uint8_t (unsigned char) type.
 *   ctypes.uint16_t       // uint16_t (unsigned short) type.
 *   ctypes.uint32_t       // uint32_t (unsigned int) type.
 *   ctypes.uint64_t       // uint64_t (unsigned long long) type.
 *   ctypes.float          // float type.
 *   ctypes.double         // double type.
 *   ctypes.string         // C string (char *).
 *   ctypes.ustring        // 16-bit string (char16_t *).
 *
 * Library ctypes.open(name)
 *
 *   Attempts to dynamically load the specified library. Returns a Library
 *   object on success.
 *     @name        A string or nsILocalFile representing the name and path of
 *                  the library to open.
 *     @returns     A Library object.
 *
 * Library.close()
 *
 *   Unloads the currently loaded library. Any subsequent attempts to call
 *   functions on this interface will fail.
 *
 * function Library.declare(name, abi, returnType, argType1, argType2, ...)
 *
 *   Declares a C function in a library.
 *     @name        Function name. This must be a valid symbol in the library.
 *     @abi         The calling convention to use. Must be an ABI constant
 *                  from ctypes.
 *     @returnType  The return type of the function. Must be a type constant
 *                  from ctypes.
 *     @argTypes    Argument types. Must be a type constant (other than void_t)
 *                  from ctypes.
 *     @returns     A function object.
 *
 *   A function object can then be used to call the C function it represents
 *   like so:
 *
 *     const myFunction = myLibrary.declare("myFunction", ctypes.default_abi,
 *       ctypes.double, ctypes.int32_t, ctypes.int32_t, ...);
 *
 *     var result = myFunction(5, 10, ...);
 *
 *   Arguments will be checked against the types supplied at declaration, and
 *   some attempt to convert values (e.g. boolean true/false to integer 0/1)
 *   will be made. Otherwise, if types do not match, or conversion fails,
 *   an exception will be thrown.
 */

// Initialize the ctypes object. You do not need to do this yourself.
const init = Components.classes["@mozilla.org/jsctypes;1"].createInstance();
init();

