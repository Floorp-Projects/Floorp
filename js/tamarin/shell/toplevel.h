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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

const int avmplus_System_exit = 7;
const int avmplus_System_exec = 8;
const int avmplus_System_getAvmplusVersion = 9;
const int avmplus_System_trace = 10;
const int avmplus_System_write = 11;
const int avmplus_System_debugger = 12;
const int avmplus_System_isDebugger = 13;
const int avmplus_System_getTimer = 14;
const int avmplus_System_private_getArgv = 15;
const int avmplus_System_readLine = 16;
const int abcclass_avmplus_System = 0;
const int avmplus_File_exists = 19;
const int avmplus_File_read = 20;
const int avmplus_File_write = 21;
const int abcclass_avmplus_File = 1;
const int abcclass_flash_system_Capabilities = 2;
const int abcpackage_toplevel_as = 0;
const int avmplus_Domain_currentDomain_get = 29;
const int avmplus_Domain_Domain = 30;
const int avmplus_Domain_loadBytes = 31;
const int avmplus_Domain_getClass = 32;
const int abcclass_avmplus_Domain = 3;
const int abcpackage_Domain_as = 1;
const int avmplus_StringBuilder_append = 37;
const int avmplus_StringBuilder_capacity_get = 38;
const int avmplus_StringBuilder_charAt = 39;
const int avmplus_StringBuilder_charCodeAt = 40;
const int avmplus_StringBuilder_ensureCapacity = 41;
const int avmplus_StringBuilder_indexOf = 42;
const int avmplus_StringBuilder_insert = 43;
const int avmplus_StringBuilder_lastIndexOf = 44;
const int avmplus_StringBuilder_length_get = 45;
const int avmplus_StringBuilder_length_set = 46;
const int avmplus_StringBuilder_remove = 47;
const int avmplus_StringBuilder_removeCharAt = 48;
const int avmplus_StringBuilder_replace = 49;
const int avmplus_StringBuilder_reverse = 50;
const int avmplus_StringBuilder_setCharAt = 51;
const int avmplus_StringBuilder_substring = 52;
const int avmplus_StringBuilder_toString = 53;
const int avmplus_StringBuilder_trimToSize = 54;
const int abcclass_avmplus_StringBuilder = 4;
const int abcpackage_StringBuilder_as = 2;
const int flash_utils_ByteArray_readFile = 57;
const int flash_utils_ByteArray_writeFile = 58;
const int flash_utils_ByteArray_readBytes = 59;
const int flash_utils_ByteArray_writeBytes = 60;
const int flash_utils_ByteArray_writeBoolean = 61;
const int flash_utils_ByteArray_writeByte = 62;
const int flash_utils_ByteArray_writeShort = 63;
const int flash_utils_ByteArray_writeInt = 64;
const int flash_utils_ByteArray_writeUnsignedInt = 65;
const int flash_utils_ByteArray_writeFloat = 66;
const int flash_utils_ByteArray_writeDouble = 67;
const int flash_utils_ByteArray_writeUTF = 68;
const int flash_utils_ByteArray_writeUTFBytes = 69;
const int flash_utils_ByteArray_readBoolean = 70;
const int flash_utils_ByteArray_readByte = 71;
const int flash_utils_ByteArray_readUnsignedByte = 72;
const int flash_utils_ByteArray_readShort = 73;
const int flash_utils_ByteArray_readUnsignedShort = 74;
const int flash_utils_ByteArray_readInt = 75;
const int flash_utils_ByteArray_readUnsignedInt = 76;
const int flash_utils_ByteArray_readFloat = 77;
const int flash_utils_ByteArray_readDouble = 78;
const int flash_utils_ByteArray_readUTF = 79;
const int flash_utils_ByteArray_readUTFBytes = 80;
const int flash_utils_ByteArray_length_get = 81;
const int flash_utils_ByteArray_length_set = 82;
const int flash_utils_ByteArray_compress = 83;
const int flash_utils_ByteArray_uncompress = 84;
const int flash_utils_ByteArray_toString = 85;
const int flash_utils_ByteArray_bytesAvailable_get = 86;
const int flash_utils_ByteArray_position_get = 87;
const int flash_utils_ByteArray_position_set = 88;
const int flash_utils_ByteArray_endian_get = 89;
const int flash_utils_ByteArray_endian_set = 90;
const int abcclass_flash_utils_ByteArray = 5;
const int abcpackage_ByteArray_as = 3;
const int flash_utils_IntArray_length_get = 94;
const int flash_utils_IntArray_length_set = 95;
const int abcclass_flash_utils_IntArray = 6;
const int abcpackage_IntArray_as = 4;
const int flash_utils_UIntArray_length_get = 99;
const int flash_utils_UIntArray_length_set = 100;
const int abcclass_flash_utils_UIntArray = 7;
const int abcpackage_UIntArray_as = 5;
const int flash_utils_DoubleArray_length_get = 104;
const int flash_utils_DoubleArray_length_set = 105;
const int abcclass_flash_utils_DoubleArray = 8;
const int abcpackage_DoubleArray_as = 6;
const int flash_utils_FloatArray_length_get = 109;
const int flash_utils_FloatArray_length_set = 110;
const int abcclass_flash_utils_FloatArray = 9;
const int abcpackage_FloatArray_as = 7;
const int flash_utils_ShortArray_length_get = 114;
const int flash_utils_ShortArray_length_set = 115;
const int abcclass_flash_utils_ShortArray = 10;
const int abcpackage_ShortArray_as = 8;
const int flash_utils_UShortArray_length_get = 119;
const int flash_utils_UShortArray_length_set = 120;
const int abcclass_flash_utils_UShortArray = 11;
const int abcpackage_UShortArray_as = 9;
const int flash_utils_Dictionary_Dictionary = 124;
const int abcclass_flash_utils_Dictionary = 12;
const int abcpackage_Dictionary_as = 10;
const int abcclass_flash_utils_Endian = 13;
const int abcpackage_Endian_as = 11;
const int avmplus_JObject_create = 130;
const int avmplus_JObject_createArray = 131;
const int avmplus_JObject_toArray = 132;
const int avmplus_JObject_constructorSignature = 133;
const int avmplus_JObject_methodSignature = 134;
const int avmplus_JObject_fieldSignature = 135;
const int avmplus_JObject_toString = 136;
const int abcclass_avmplus_JObject = 14;
const int abcpackage_Java_as = 12;
