/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

const int unescape = 0;
const int escape = 1;
const int parseFloat = 2;
const int parseInt = 3;
const int isFinite = 4;
const int isNaN = 5;
const int encodeURIComponent = 6;
const int encodeURI = 7;
const int decodeURIComponent = 8;
const int decodeURI = 9;
const int Object_private__hasOwnProperty = 11;
const int Object_private__propertyIsEnumerable = 12;
const int Object_protected__setPropertyIsEnumerable = 13;
const int Object_private__isPrototypeOf = 14;
const int Object_private__toString = 15;
const int abcclass_Object = 0;
const int Class_prototype_get = 29;
const int abcclass_Class = 1;
const int Function_prototype_get = 37;
const int Function_prototype_set = 38;
const int Function_length_get = 39;
const int Function_AS3_call = 40;
const int Function_AS3_apply = 41;
const int abcclass_Function = 2;
const int Namespace_prefix_get = 46;
const int Namespace_uri_get = 47;
const int abcclass_Namespace = 3;
const int abcclass_Boolean = 4;
const int Number_private__toString = 63;
const int Number_private__convert = 64;
const int abcclass_Number = 5;
const int abcclass_int = 6;
const int abcclass_uint = 7;
const int String_AS3_fromCharCode = 114;
const int String_private__match = 115;
const int String_private__replace = 116;
const int String_private__search = 117;
const int String_private__split = 118;
const int String_length_get = 119;
const int String_private__indexOf = 120;
const int String_AS3_indexOf = 121;
const int String_private__lastIndexOf = 122;
const int String_AS3_lastIndexOf = 123;
const int String_private__charAt = 124;
const int String_AS3_charAt = 125;
const int String_private__charCodeAt = 126;
const int String_AS3_charCodeAt = 127;
const int String_AS3_localeCompare = 129;
const int String_private__slice = 133;
const int String_AS3_slice = 134;
const int String_private__substring = 136;
const int String_AS3_substring = 137;
const int String_private__substr = 138;
const int String_AS3_substr = 139;
const int String_AS3_toLowerCase = 140;
const int String_AS3_toUpperCase = 142;
const int abcclass_String = 8;
const int Array_private__pop = 169;
const int Array_private__reverse = 170;
const int Array_private__concat = 171;
const int Array_private__shift = 172;
const int Array_private__slice = 173;
const int Array_private__splice = 174;
const int Array_private__sort = 175;
const int Array_private__sortOn = 176;
const int Array_private__indexOf = 177;
const int Array_private__lastIndexOf = 178;
const int Array_private__every = 179;
const int Array_private__filter = 180;
const int Array_private__forEach = 181;
const int Array_private__map = 182;
const int Array_private__some = 183;
const int Array_length_get = 184;
const int Array_length_set = 185;
const int Array_AS3_pop = 188;
const int Array_AS3_push = 189;
const int Array_AS3_unshift = 194;
const int abcclass_Array = 9;
const int builtin_as_0_MethodClosure_length_get = 208;
const int abcclass_builtin_as_0_MethodClosure = 10;
const int abcpackage_builtin_as = 0;
const int Math_private__min = 212;
const int Math_private__max = 213;
const int Math_abs = 214;
const int Math_acos = 215;
const int Math_asin = 216;
const int Math_atan = 217;
const int Math_ceil = 218;
const int Math_cos = 219;
const int Math_exp = 220;
const int Math_floor = 221;
const int Math_log = 222;
const int Math_round = 223;
const int Math_sin = 224;
const int Math_sqrt = 225;
const int Math_tan = 226;
const int Math_atan2 = 227;
const int Math_pow = 228;
const int Math_max = 229;
const int Math_min = 230;
const int Math_random = 231;
const int abcclass_Math = 11;
const int abcpackage_Math_as = 1;
const int Error_getErrorMessage = 236;
const int Error_getStackTrace = 240;
const int abcclass_Error = 12;
const int abcclass_DefinitionError = 13;
const int abcclass_EvalError = 14;
const int abcclass_RangeError = 15;
const int abcclass_ReferenceError = 16;
const int abcclass_SecurityError = 17;
const int abcclass_SyntaxError = 18;
const int abcclass_TypeError = 19;
const int abcclass_URIError = 20;
const int abcclass_VerifyError = 21;
const int abcclass_UninitializedError = 22;
const int abcclass_ArgumentError = 23;
const int abcpackage_Error_as = 2;
const int RegExp_source_get = 269;
const int RegExp_global_get = 270;
const int RegExp_ignoreCase_get = 271;
const int RegExp_multiline_get = 272;
const int RegExp_lastIndex_get = 273;
const int RegExp_lastIndex_set = 274;
const int RegExp_dotall_get = 275;
const int RegExp_extended_get = 276;
const int RegExp_AS3_exec = 277;
const int abcclass_RegExp = 24;
const int abcpackage_RegExp_as = 3;
const int Date_parse = 323;
const int Date_UTC = 324;
const int Date_AS3_valueOf = 325;
const int Date_private__toString = 326;
const int Date_private__setTime = 327;
const int Date_private__get = 328;
const int Date_AS3_getUTCFullYear = 337;
const int Date_AS3_getUTCMonth = 338;
const int Date_AS3_getUTCDate = 339;
const int Date_AS3_getUTCDay = 340;
const int Date_AS3_getUTCHours = 341;
const int Date_AS3_getUTCMinutes = 342;
const int Date_AS3_getUTCSeconds = 343;
const int Date_AS3_getUTCMilliseconds = 344;
const int Date_AS3_getFullYear = 345;
const int Date_AS3_getMonth = 346;
const int Date_AS3_getDate = 347;
const int Date_AS3_getDay = 348;
const int Date_AS3_getHours = 349;
const int Date_AS3_getMinutes = 350;
const int Date_AS3_getSeconds = 351;
const int Date_AS3_getMilliseconds = 352;
const int Date_AS3_getTimezoneOffset = 353;
const int Date_AS3_getTime = 354;
const int Date_AS3_setFullYear = 355;
const int Date_AS3_setMonth = 356;
const int Date_AS3_setDate = 357;
const int Date_AS3_setHours = 358;
const int Date_AS3_setMinutes = 359;
const int Date_AS3_setSeconds = 360;
const int Date_AS3_setMilliseconds = 361;
const int Date_AS3_setUTCFullYear = 362;
const int Date_AS3_setUTCMonth = 363;
const int Date_AS3_setUTCDate = 364;
const int Date_AS3_setUTCHours = 365;
const int Date_AS3_setUTCMinutes = 366;
const int Date_AS3_setUTCSeconds = 367;
const int Date_AS3_setUTCMilliseconds = 368;
const int abcclass_Date = 25;
const int abcpackage_Date_as = 4;
const int isXMLName = 404;
const int XML_ignoreComments_get = 447;
const int XML_ignoreComments_set = 448;
const int XML_ignoreProcessingInstructions_get = 449;
const int XML_ignoreProcessingInstructions_set = 450;
const int XML_ignoreWhitespace_get = 451;
const int XML_ignoreWhitespace_set = 452;
const int XML_prettyPrinting_get = 453;
const int XML_prettyPrinting_set = 454;
const int XML_prettyIndent_get = 455;
const int XML_prettyIndent_set = 456;
const int XML_AS3_toString = 460;
const int XML_AS3_hasOwnProperty = 461;
const int XML_AS3_propertyIsEnumerable = 462;
const int XML_AS3_addNamespace = 463;
const int XML_AS3_appendChild = 464;
const int XML_AS3_attribute = 465;
const int XML_AS3_attributes = 466;
const int XML_AS3_child = 467;
const int XML_AS3_childIndex = 468;
const int XML_AS3_children = 469;
const int XML_AS3_comments = 470;
const int XML_AS3_contains = 471;
const int XML_AS3_copy = 472;
const int XML_AS3_descendants = 473;
const int XML_AS3_elements = 474;
const int XML_AS3_hasComplexContent = 475;
const int XML_AS3_hasSimpleContent = 476;
const int XML_AS3_inScopeNamespaces = 477;
const int XML_AS3_insertChildAfter = 478;
const int XML_AS3_insertChildBefore = 479;
const int XML_AS3_localName = 481;
const int XML_AS3_name = 482;
const int XML_AS3_namespace = 483;
const int XML_AS3_namespaceDeclarations = 484;
const int XML_AS3_nodeKind = 485;
const int XML_AS3_normalize = 486;
const int XML_AS3_parent = 487;
const int XML_AS3_processingInstructions = 488;
const int XML_AS3_prependChild = 489;
const int XML_AS3_removeNamespace = 490;
const int XML_AS3_replace = 491;
const int XML_AS3_setChildren = 492;
const int XML_AS3_setLocalName = 493;
const int XML_AS3_setName = 494;
const int XML_AS3_setNamespace = 495;
const int XML_AS3_text = 496;
const int XML_AS3_toXMLString = 497;
const int XML_AS3_notification = 498;
const int XML_AS3_setNotification = 499;
const int abcclass_XML = 26;
const int XMLList_AS3_toString = 541;
const int XMLList_AS3_hasOwnProperty = 543;
const int XMLList_AS3_propertyIsEnumerable = 544;
const int XMLList_AS3_attribute = 545;
const int XMLList_AS3_attributes = 546;
const int XMLList_AS3_child = 547;
const int XMLList_AS3_children = 548;
const int XMLList_AS3_comments = 549;
const int XMLList_AS3_contains = 550;
const int XMLList_AS3_copy = 551;
const int XMLList_AS3_descendants = 552;
const int XMLList_AS3_elements = 553;
const int XMLList_AS3_hasComplexContent = 554;
const int XMLList_AS3_hasSimpleContent = 555;
const int XMLList_AS3_length = 556;
const int XMLList_AS3_name = 557;
const int XMLList_AS3_normalize = 558;
const int XMLList_AS3_parent = 559;
const int XMLList_AS3_processingInstructions = 560;
const int XMLList_AS3_text = 561;
const int XMLList_AS3_toXMLString = 562;
const int XMLList_AS3_addNamespace = 563;
const int XMLList_AS3_appendChild = 564;
const int XMLList_AS3_childIndex = 565;
const int XMLList_AS3_inScopeNamespaces = 566;
const int XMLList_AS3_insertChildAfter = 567;
const int XMLList_AS3_insertChildBefore = 568;
const int XMLList_AS3_nodeKind = 569;
const int XMLList_AS3_namespace = 570;
const int XMLList_AS3_localName = 571;
const int XMLList_AS3_namespaceDeclarations = 572;
const int XMLList_AS3_prependChild = 573;
const int XMLList_AS3_removeNamespace = 574;
const int XMLList_AS3_replace = 575;
const int XMLList_AS3_setChildren = 576;
const int XMLList_AS3_setLocalName = 577;
const int XMLList_AS3_setName = 578;
const int XMLList_AS3_setNamespace = 579;
const int abcclass_XMLList = 27;
const int QName_localName_get = 583;
const int QName_uri_get = 584;
const int abcclass_QName = 28;
const int abcpackage_XML_as = 5;
const int builtin_abc_length = 26281;
const int builtin_abc_method_count = 585;
const int builtin_abc_class_count = 29;
const int builtin_abc_script_count = 6;
extern const unsigned char builtin_abc_data[];
