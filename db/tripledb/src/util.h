/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is
 * Geocast Network Systems.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Weissman <terry@mozilla.org>
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

#ifndef _util_h_
#define _util_h_ 1

extern TDBInt32 tdbGetInt16(char** ptr, char* endptr);
extern TDBInt32 tdbGetInt32(char** ptr, char* endptr);
extern TDBInt64 tdbGetInt64(char** ptr, char* endptr);
extern TDBInt8 tdbGetInt8(char** ptr, char* endptr);
extern TDBPtr tdbGetPtr(char** ptr, char* endptr);
extern TDBUint16 tdbGetUInt16(char** ptr, char* endptr);
extern TDBUint32 tdbGetUInt32(char** ptr, char* endptr);
extern TDBUint64 tdbGetUInt64(char** ptr, char* endptr);
extern TDBUint8 tdbGetUInt8(char** ptr, char* endptr);

extern TDBStatus tdbPutInt16(char** ptr, TDBInt16 value, char* endptr);
extern TDBStatus tdbPutInt32(char** ptr, TDBInt32 value, char* endptr);
extern TDBStatus tdbPutInt64(char** ptr, TDBInt64 value, char* endptr);
extern TDBStatus tdbPutInt8(char** ptr, TDBInt8 value, char* endptr);
extern TDBStatus tdbPutPtr(char** ptr, TDBPtr value, char* endptr);
extern TDBStatus tdbPutUInt16(char** ptr, TDBUint16 value, char* endptr);
extern TDBStatus tdbPutUInt32(char** ptr, TDBUint32 value, char* endptr);
extern TDBStatus tdbPutUInt64(char** ptr, TDBUint64 value, char* endptr);
extern TDBStatus tdbPutUInt8(char** ptr, TDBUint8 value, char* endptr);

/* tdbPutString() puts the string and its trailing NULL into the given
   buffer. */
extern TDBStatus tdbPutString(char** ptr, const char* str, char* endptr);

#endif /* _util_h_ */
