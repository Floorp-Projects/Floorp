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

/**
 * This file defines the constants available on the ctypes.types object (e.g.
 * ctypes.types.VOID). They do not have any interesting properties; they simply
 * exist as unique identifiers for the type they represent.
 */

/**
 * ABI constants that specify the calling convention to use.
 * DEFAULT corresponds to the cdecl convention, and in almost all
 * cases is the correct choice. STDCALL is provided for calling
 * functions in the Microsoft Win32 API.
 */
DEFINE_ABI(default_abi)    // corresponds to cdecl
DEFINE_ABI(stdcall_abi)    // for calling Win32 API functions

/**
 * Types available for arguments and return values, representing
 * their C counterparts.
 */
DEFINE_TYPE(void_t)        // Only allowed for return types.
DEFINE_TYPE(bool)          // _Bool type (assumed 8 bits wide).
DEFINE_TYPE(int8_t)        // int8_t (signed char) type.
DEFINE_TYPE(int16_t)       // int16_t (short) type.
DEFINE_TYPE(int32_t)       // int32_t (int) type.
DEFINE_TYPE(int64_t)       // int64_t (long long) type.
DEFINE_TYPE(uint8_t)       // uint8_t (unsigned char) type.
DEFINE_TYPE(uint16_t)      // uint16_t (unsigned short) type.
DEFINE_TYPE(uint32_t)      // uint32_t (unsigned int) type.
DEFINE_TYPE(uint64_t)      // uint64_t (unsigned long long) type.
DEFINE_TYPE(float)         // float type.
DEFINE_TYPE(double)        // double type.
DEFINE_TYPE(string)        // C string (char *).
DEFINE_TYPE(ustring)       // 16-bit string (char16_t *).

