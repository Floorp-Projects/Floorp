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


#if defined(AVMPLUS_PROFILE) || defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
const char *opNames[] = {
    "OP_0x00",
    "bkpt",
    "nop",
    "throw",
    "getsuper",
    "setsuper",
    "dxns",
    "dxnslate",
    "kill",
    "label",
    "OP_0x0A",
    "OP_0x0B",
    "ifnlt",
    "ifnle",
    "ifngt",
    "ifnge",
    "jump",
    "iftrue",
    "iffalse",
    "ifeq",
    "ifne",
    "iflt",
    "ifle",
    "ifgt",
    "ifge",
    "ifstricteq",
    "ifstrictne",
    "lookupswitch",
    "pushwith",
    "popscope",
    "nextname",
    "hasnext",
    "pushnull",
    "pushundefined",
    "OP_0x22",
    "nextvalue",
    "pushbyte",
    "pushshort",
    "pushtrue",
    "pushfalse",
    "pushnan",
    "pop",
    "dup",
    "swap",
    "pushstring",
    "pushint",
    "pushuint",
    "pushdouble",
    "pushscope",
    "pushnamespace",
    "hasnext2",
    "OP_0x33",
    "OP_0x34",
    "OP_0x35",
    "OP_0x36",
    "OP_0x37",
    "OP_0x38",
    "OP_0x39",
    "OP_0x3A",
    "OP_0x3B",
    "OP_0x3C",
    "OP_0x3D",
    "OP_0x3E",
    "OP_0x3F",
    "newfunction",
    "call",
    "construct",
    "callmethod",
    "callstatic",
    "callsuper",
    "callproperty",
    "returnvoid",
    "returnvalue",
    "constructsuper",
    "constructprop",
    "callsuperid",
    "callproplex",
    "callinterface",
    "callsupervoid",
    "callpropvoid",
    "OP_0x50",
    "OP_0x51",
    "OP_0x52",
    "OP_0x53",
    "OP_0x54",
    "newobject",
    "newarray",
    "newactivation",
    "newclass",
    "getdescendants",
    "newcatch",
    "OP_0x5B",
    "OP_0x5C",
    "findpropstrict",
    "findproperty",
    "finddef",
    "getlex",
    "setproperty",
    "getlocal",
    "setlocal",
    "getglobalscope",
    "getscopeobject",
    "getproperty",
    "OP_0x67",
    "initproperty",
    "OP_0x69",
    "deleteproperty",
    "OP_0x6B",
    "getslot",
    "setslot",
    "getglobalslot",
    "setglobalslot",
    "convert_s",
    "esc_xelem",
    "esc_xattr",
    "convert_i",
    "convert_u",
    "convert_d",
    "convert_b",
    "convert_o",
    "checkfilter",
    "OP_0x79",
    "OP_0x7A",
    "OP_0x7B",
    "OP_0x7C",
    "OP_0x7D",
    "OP_0x7E",
    "OP_0x7F",
    "coerce",
    "coerce_b",
    "coerce_a",
    "coerce_i",
    "coerce_d",
    "coerce_s",
    "astype",
    "astypelate",
    "coerce_u",
    "coerce_o",
    "OP_0x8A",
    "OP_0x8B",
    "OP_0x8C",
    "OP_0x8D",
    "OP_0x8E",
    "OP_0x8F",
    "negate",
    "increment",
    "inclocal",
    "decrement",
    "declocal",
    "typeof",
    "not",
    "bitnot",
    "OP_0x98",
    "OP_0x99",
    "concat",
    "add_d",
    "OP_0x9C",
    "OP_0x9D",
    "OP_0x9E",
    "OP_0x9F",
    "add",
    "subtract",
    "multiply",
    "divide",
    "modulo",
    "lshift",
    "rshift",
    "urshift",
    "bitand",
    "bitor",
    "bitxor",
    "equals",
    "strictequals",
    "lessthan",
    "lessequals",
    "greaterthan",
    "greaterequals",
    "instanceof",
    "istype",
    "istypelate",
    "in",
    "OP_0xB5",
    "OP_0xB6",
    "OP_0xB7",
    "OP_0xB8",
    "OP_0xB9",
    "OP_0xBA",
    "OP_0xBB",
    "OP_0xBC",
    "OP_0xBD",
    "OP_0xBE",
    "OP_0xBF",
    "increment_i",
    "decrement_i",
    "inclocal_i",
    "declocal_i",
    "negate_i",
    "add_i",
    "subtract_i",
    "multiply_i",
    "OP_0xC8",
    "OP_0xC9",
    "OP_0xCA",
    "OP_0xCB",
    "OP_0xCC",
    "OP_0xCD",
    "OP_0xCE",
    "OP_0xCF",
    "getlocal0",
    "getlocal1",
    "getlocal2",
    "getlocal3",
    "setlocal0",
    "setlocal1",
    "setlocal2",
    "setlocal3",
    "OP_0xD8",
    "OP_0xD9",
    "OP_0xDA",
    "OP_0xDB",
    "OP_0xDC",
    "OP_0xDD",
    "OP_0xDE",
    "OP_0xDF",
    "OP_0xE0",
    "OP_0xE1",
    "OP_0xE2",
    "OP_0xE3",
    "OP_0xE4",
    "OP_0xE5",
    "OP_0xE6",
    "OP_0xE7",
    "OP_0xE8",
    "OP_0xE9",
    "OP_0xEA",
    "OP_0xEB",
    "OP_0xEC",
    "OP_0xED",
    "abs_jump",
    "debug",
    "debugline",
    "debugfile",
    "bkptline",
    "timestamp",
    "OP_0xF4",
    "verifypass",
    "alloc",
    "mark",
    "wb",
    "prologue",
    "sendenter",
    "doubletoatom",
    "sweep",
    "codegenop",
    "verifyop",
    "decode"
};
#endif



signed char opOperandCount[] = {
    -1,	// "OP_0x00"
    0,	// "bkpt"
    0,	// "nop"
    0,	// "throw"
    1,	// "getsuper"
    1,	// "setsuper"
    1,	// "dxns"
    0,	// "dxnslate"
    1,	// "kill"
    0,	// "label"
    -1,	// "OP_0x0A"
    -1,	// "OP_0x0B"
    1,	// "ifnlt"
    1,	// "ifnle"
    1,	// "ifngt"
    1,	// "ifnge"
    1,	// "jump"
    1,	// "iftrue"
    1,	// "iffalse"
    1,	// "ifeq"
    1,	// "ifne"
    1,	// "iflt"
    1,	// "ifle"
    1,	// "ifgt"
    1,	// "ifge"
    1,	// "ifstricteq"
    1,	// "ifstrictne"
    2,	// "lookupswitch"
    0,	// "pushwith"
    0,	// "popscope"
    0,	// "nextname"
    0,	// "hasnext"
    0,	// "pushnull"
    0,	// "pushundefined"
    -1,	// "OP_0x22"
    0,	// "nextvalue"
    1,	// "pushbyte"
    1,	// "pushshort"
    0,	// "pushtrue"
    0,	// "pushfalse"
    0,	// "pushnan"
    0,	// "pop"
    0,	// "dup"
    0,	// "swap"
    1,	// "pushstring"
    1,	// "pushint"
    1,	// "pushuint"
    1,	// "pushdouble"
    0,	// "pushscope"
    1,	// "pushnamespace"
    2,	// "hasnext2"
    -1,	// "OP_0x33"
    -1,	// "OP_0x34"
    -1,	// "OP_0x35"
    -1,	// "OP_0x36"
    -1,	// "OP_0x37"
    -1,	// "OP_0x38"
    -1,	// "OP_0x39"
    -1,	// "OP_0x3A"
    -1,	// "OP_0x3B"
    -1,	// "OP_0x3C"
    -1,	// "OP_0x3D"
    -1,	// "OP_0x3E"
    -1,	// "OP_0x3F"
    1,	// "newfunction"
    1,	// "call"
    1,	// "construct"
    2,	// "callmethod"
    2,	// "callstatic"
    2,	// "callsuper"
    2,	// "callproperty"
    0,	// "returnvoid"
    0,	// "returnvalue"
    1,	// "constructsuper"
    2,	// "constructprop"
    -1,	// "callsuperid"
    2,	// "callproplex"
    -1,	// "callinterface"
    2,	// "callsupervoid"
    2,	// "callpropvoid"
    -1,	// "OP_0x50"
    -1,	// "OP_0x51"
    -1,	// "OP_0x52"
    -1,	// "OP_0x53"
    -1,	// "OP_0x54"
    1,	// "newobject"
    1,	// "newarray"
    0,	// "newactivation"
    1,	// "newclass"
    1,	// "getdescendants"
    1,	// "newcatch"
    -1,	// "OP_0x5B"
    -1,	// "OP_0x5C"
    1,	// "findpropstrict"
    1,	// "findproperty"
    1,	// "finddef"
    1,	// "getlex"
    1,	// "setproperty"
    1,	// "getlocal"
    1,	// "setlocal"
    0,	// "getglobalscope"
    1,	// "getscopeobject"
    1,	// "getproperty"
    -1,	// "OP_0x67"
    1,	// "initproperty"
    -1,	// "OP_0x69"
    1,	// "deleteproperty"
    -1,	// "OP_0x6B"
    1,	// "getslot"
    1,	// "setslot"
    1,	// "getglobalslot"
    1,	// "setglobalslot"
    0,	// "convert_s"
    0,	// "esc_xelem"
    0,	// "esc_xattr"
    0,	// "convert_i"
    0,	// "convert_u"
    0,	// "convert_d"
    0,	// "convert_b"
    0,	// "convert_o"
    0,	// "checkfilter"
    -1,	// "OP_0x79"
    -1,	// "OP_0x7A"
    -1,	// "OP_0x7B"
    -1,	// "OP_0x7C"
    -1,	// "OP_0x7D"
    -1,	// "OP_0x7E"
    -1,	// "OP_0x7F"
    1,	// "coerce"
    0,	// "coerce_b"
    0,	// "coerce_a"
    0,	// "coerce_i"
    0,	// "coerce_d"
    0,	// "coerce_s"
    1,	// "astype"
    0,	// "astypelate"
    0,	// "coerce_u"
    0,	// "coerce_o"
    -1,	// "OP_0x8A"
    -1,	// "OP_0x8B"
    -1,	// "OP_0x8C"
    -1,	// "OP_0x8D"
    -1,	// "OP_0x8E"
    -1,	// "OP_0x8F"
    0,	// "negate"
    0,	// "increment"
    1,	// "inclocal"
    0,	// "decrement"
    1,	// "declocal"
    0,	// "typeof"
    0,	// "not"
    0,	// "bitnot"
    -1,	// "OP_0x98"
    -1,	// "OP_0x99"
    -1,	// "concat"
    -1,	// "add_d"
    -1,	// "OP_0x9C"
    -1,	// "OP_0x9D"
    -1,	// "OP_0x9E"
    -1,	// "OP_0x9F"
    0,	// "add"
    0,	// "subtract"
    0,	// "multiply"
    0,	// "divide"
    0,	// "modulo"
    0,	// "lshift"
    0,	// "rshift"
    0,	// "urshift"
    0,	// "bitand"
    0,	// "bitor"
    0,	// "bitxor"
    0,	// "equals"
    0,	// "strictequals"
    0,	// "lessthan"
    0,	// "lessequals"
    0,	// "greaterthan"
    0,	// "greaterequals"
    0,	// "instanceof"
    1,	// "istype"
    0,	// "istypelate"
    0,	// "in"
    -1,	// "OP_0xB5"
    -1,	// "OP_0xB6"
    -1,	// "OP_0xB7"
    -1,	// "OP_0xB8"
    -1,	// "OP_0xB9"
    -1,	// "OP_0xBA"
    -1,	// "OP_0xBB"
    -1,	// "OP_0xBC"
    -1,	// "OP_0xBD"
    -1,	// "OP_0xBE"
    -1,	// "OP_0xBF"
    0,	// "increment_i"
    0,	// "decrement_i"
    1,	// "inclocal_i"
    1,	// "declocal_i"
    0,	// "negate_i"
    0,	// "add_i"
    0,	// "subtract_i"
    0,	// "multiply_i"
    -1,	// "OP_0xC8"
    -1,	// "OP_0xC9"
    -1,	// "OP_0xCA"
    -1,	// "OP_0xCB"
    -1,	// "OP_0xCC"
    -1,	// "OP_0xCD"
    -1,	// "OP_0xCE"
    -1,	// "OP_0xCF"
    0,	// "getlocal0"
    0,	// "getlocal1"
    0,	// "getlocal2"
    0,	// "getlocal3"
    0,	// "setlocal0"
    0,	// "setlocal1"
    0,	// "setlocal2"
    0,	// "setlocal3"
    -1,	// "OP_0xD8"
    -1,	// "OP_0xD9"
    -1,	// "OP_0xDA"
    -1,	// "OP_0xDB"
    -1,	// "OP_0xDC"
    -1,	// "OP_0xDD"
    -1,	// "OP_0xDE"
    -1,	// "OP_0xDF"
    -1,	// "OP_0xE0"
    -1,	// "OP_0xE1"
    -1,	// "OP_0xE2"
    -1,	// "OP_0xE3"
    -1,	// "OP_0xE4"
    -1,	// "OP_0xE5"
    -1,	// "OP_0xE6"
    -1,	// "OP_0xE7"
    -1,	// "OP_0xE8"
    -1,	// "OP_0xE9"
    -1,	// "OP_0xEA"
    -1,	// "OP_0xEB"
    -1,	// "OP_0xEC"
    -1,	// "OP_0xED"
    2,	// "abs_jump"
    4,	// "debug"
    1,	// "debugline"
    1,	// "debugfile"
    1,	// "bkptline"
    0,	// "timestamp"
    -1,	// "OP_0xF4"
    -1,	// "verifypass"
    -1,	// "alloc"
    -1,	// "mark"
    -1,	// "wb"
    -1,	// "prologue"
    -1,	// "sendenter"
    -1,	// "doubletoatom"
    -1,	// "sweep"
    -1,	// "codegenop"
    -1,	// "verifyop"
    -1,	// "decode"

};

#if 0 && defined(AVMPLUS_MIR) && defined(_DEBUG)


// C++ note.  the max opSize[] value is 5 (3 bits).  We could pack these tighter.
// no. of bytes in the opcode stream.  0 means unsupported opcode.
unsigned char opSizes[] = {
    0,	// "OP_0x00"
    1,	// "bkpt"
    1,	// "nop"
    1,	// "throw"
    1+2,	// "getsuper"
    1+2,	// "setsuper"
    1+2,	// "dxns"
    1,	// "dxnslate"
    1+2,	// "kill"
    1,	// "label"
    0,	// "OP_0x0A"
    0,	// "OP_0x0B"
    1+3,	// "ifnlt"
    1+3,	// "ifnle"
    1+3,	// "ifngt"
    1+3,	// "ifnge"
    1+3,	// "jump"
    1+3,	// "iftrue"
    1+3,	// "iffalse"
    1+3,	// "ifeq"
    1+3,	// "ifne"
    1+3,	// "iflt"
    1+3,	// "ifle"
    1+3,	// "ifgt"
    1+3,	// "ifge"
    1+3,	// "ifstricteq"
    1+3,	// "ifstrictne"
    1+3+2+0,	// "lookupswitch"
    1,	// "pushwith"
    1,	// "popscope"
    1,	// "nextname"
    1,	// "hasnext"
    1,	// "pushnull"
    1,	// "pushundefined"
    0,	// "OP_0x22"
    1,	// "nextvalue"
    1+1,	// "pushbyte"
    1+2,	// "pushshort"
    1,	// "pushtrue"
    1,	// "pushfalse"
    1,	// "pushnan"
    1,	// "pop"
    1,	// "dup"
    1,	// "swap"
    1+2,	// "pushstring"
    1+2,	// "pushint"
    1+2,	// "pushuint"
    1+2,	// "pushdouble"
    1,	// "pushscope"
    1+2,	// "pushnamespace"
    1,	// "hasnext2"
    0,	// "OP_0x33"
    0,	// "OP_0x34"
    0,	// "OP_0x35"
    0,	// "OP_0x36"
    0,	// "OP_0x37"
    0,	// "OP_0x38"
    0,	// "OP_0x39"
    0,	// "OP_0x3A"
    0,	// "OP_0x3B"
    0,	// "OP_0x3C"
    0,	// "OP_0x3D"
    0,	// "OP_0x3E"
    0,	// "OP_0x3F"
    1+2,	// "newfunction"
    1+2,	// "call"
    1+2,	// "construct"
    1+2+2,	// "callmethod"
    1+2+2,	// "callstatic"
    1+2+2,	// "callsuper"
    1+2+2,	// "callproperty"
    1,	// "returnvoid"
    1,	// "returnvalue"
    1+2,	// "constructsuper"
    1+2+2,	// "constructprop"
    0,	// "callsuperid"
    1+2+2,	// "callproplex"
    0,	// "callinterface"
    1+2+2,	// "callsupervoid"
    1+2+2,	// "callpropvoid"
    0,	// "OP_0x50"
    0,	// "OP_0x51"
    0,	// "OP_0x52"
    0,	// "OP_0x53"
    0,	// "OP_0x54"
    1+2,	// "newobject"
    1+2,	// "newarray"
    1,	// "newactivation"
    1+2,	// "newclass"
    1+2,	// "getdescendants"
    1+2,	// "newcatch"
    0,	// "OP_0x5B"
    0,	// "OP_0x5C"
    1+2,	// "findpropstrict"
    1+2,	// "findproperty"
    1+2,	// "finddef"
    1+2,	// "getlex"
    1+2,	// "setproperty"
    1+2,	// "getlocal"
    1+2,	// "setlocal"
    1,	// "getglobalscope"
    1+1,	// "getscopeobject"
    1+2,	// "getproperty"
    0,	// "OP_0x67"
    1+2,	// "initproperty"
    0,	// "OP_0x69"
    1+2,	// "deleteproperty"
    0,	// "OP_0x6B"
    1+2,	// "getslot"
    1+2,	// "setslot"
    1+2,	// "getglobalslot"
    1+2,	// "setglobalslot"
    1,	// "convert_s"
    1,	// "esc_xelem"
    1,	// "esc_xattr"
    1,	// "convert_i"
    1,	// "convert_u"
    1,	// "convert_d"
    1,	// "convert_b"
    1,	// "convert_o"
    1,	// "checkfilter"
    0,	// "OP_0x79"
    0,	// "OP_0x7A"
    0,	// "OP_0x7B"
    0,	// "OP_0x7C"
    0,	// "OP_0x7D"
    0,	// "OP_0x7E"
    0,	// "OP_0x7F"
    1+2,	// "coerce"
    1,	// "coerce_b"
    1,	// "coerce_a"
    1,	// "coerce_i"
    1,	// "coerce_d"
    1,	// "coerce_s"
    1+2,	// "astype"
    1,	// "astypelate"
    1,	// "coerce_u"
    1,	// "coerce_o"
    0,	// "OP_0x8A"
    0,	// "OP_0x8B"
    0,	// "OP_0x8C"
    0,	// "OP_0x8D"
    0,	// "OP_0x8E"
    0,	// "OP_0x8F"
    1,	// "negate"
    1,	// "increment"
    1+2,	// "inclocal"
    1,	// "decrement"
    1+2,	// "declocal"
    1,	// "typeof"
    1,	// "not"
    1,	// "bitnot"
    0,	// "OP_0x98"
    0,	// "OP_0x99"
    0,	// "concat"
    0,	// "add_d"
    0,	// "OP_0x9C"
    0,	// "OP_0x9D"
    0,	// "OP_0x9E"
    0,	// "OP_0x9F"
    1,	// "add"
    1,	// "subtract"
    1,	// "multiply"
    1,	// "divide"
    1,	// "modulo"
    1,	// "lshift"
    1,	// "rshift"
    1,	// "urshift"
    1,	// "bitand"
    1,	// "bitor"
    1,	// "bitxor"
    1,	// "equals"
    1,	// "strictequals"
    1,	// "lessthan"
    1,	// "lessequals"
    1,	// "greaterthan"
    1,	// "greaterequals"
    1,	// "instanceof"
    1+2,	// "istype"
    1,	// "istypelate"
    1,	// "in"
    0,	// "OP_0xB5"
    0,	// "OP_0xB6"
    0,	// "OP_0xB7"
    0,	// "OP_0xB8"
    0,	// "OP_0xB9"
    0,	// "OP_0xBA"
    0,	// "OP_0xBB"
    0,	// "OP_0xBC"
    0,	// "OP_0xBD"
    0,	// "OP_0xBE"
    0,	// "OP_0xBF"
    1,	// "increment_i"
    1,	// "decrement_i"
    1+2,	// "inclocal_i"
    1+2,	// "declocal_i"
    1,	// "negate_i"
    1,	// "add_i"
    1,	// "subtract_i"
    1,	// "multiply_i"
    0,	// "OP_0xC8"
    0,	// "OP_0xC9"
    0,	// "OP_0xCA"
    0,	// "OP_0xCB"
    0,	// "OP_0xCC"
    0,	// "OP_0xCD"
    0,	// "OP_0xCE"
    0,	// "OP_0xCF"
    1,	// "getlocal0"
    1,	// "getlocal1"
    1,	// "getlocal2"
    1,	// "getlocal3"
    1,	// "setlocal0"
    1,	// "setlocal1"
    1,	// "setlocal2"
    1,	// "setlocal3"
    0,	// "OP_0xD8"
    0,	// "OP_0xD9"
    0,	// "OP_0xDA"
    0,	// "OP_0xDB"
    0,	// "OP_0xDC"
    0,	// "OP_0xDD"
    0,	// "OP_0xDE"
    0,	// "OP_0xDF"
    0,	// "OP_0xE0"
    0,	// "OP_0xE1"
    0,	// "OP_0xE2"
    0,	// "OP_0xE3"
    0,	// "OP_0xE4"
    0,	// "OP_0xE5"
    0,	// "OP_0xE6"
    0,	// "OP_0xE7"
    0,	// "OP_0xE8"
    0,	// "OP_0xE9"
    0,	// "OP_0xEA"
    0,	// "OP_0xEB"
    0,	// "OP_0xEC"
    0,	// "OP_0xED"
    1+2,	// "abs_jump"
    1+1+2+4,	// "debug"
    1+3,	// "debugline"
    1+2,	// "debugfile"
    1+3,	// "bkptline"
    1,	// "timestamp"
    0,	// "OP_0xF4"
    0,	// "verifypass"
    0,	// "alloc"
    0,	// "mark"
    0,	// "wb"
    0,	// "prologue"
    0,	// "sendenter"
    0,	// "doubletoatom"
    0,	// "sweep"
    0,	// "codegenop"
    0,	// "verifyop"
    0,	// "decode"

};
unsigned char opStackPop[] = {
    0,
    0,
    0,
    0,
    1,
    2,
    0,
    1,
    0,
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    0,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    0,
    0,
    2,
    2,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0,
    1,
    -1,
    -1,
    0,
    -1,
    0,
    -1,
    -1,
    0,
    0,
    0,
    0,
    0,
    -1,
    -1,
    -1,
    1,
    1,
    -1,
    0,
    0,
    0,
    0,
    0,
    -1,
    -1,
    0,
    1,
    0,
    0,
    -1,
    0,
    -1,
    0,
    -1,
    0,
    1,
    2,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

unsigned char opStackPush[] = {
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    2,
    2,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};
#endif /* AVMPLUS_MIR && _DEBUG */
unsigned char opCanThrow[] = {
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

