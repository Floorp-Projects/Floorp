/*  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/* -*- Mode: C; tab-width: 8 -*-
 * Copyright (C) 1998-1999 Netscape Communications Corporation, All Rights Reserved.
 */

/*
	Error messages for LiveConnect. See JSJMSG.def for format.

*/

MSG_DEF(JSJMSG_NOT_AN_ERROR,         0, 0, "<Error #0 is reserved>") 
MSG_DEF(JSJMSG_CANT_LOAD_JSOBJECT,   1, 0, "Couldn't convert JavaScript object to an "
									       "instance of netscape.javascript.JSObject "
									       "because that class could not be loaded.")
MSG_DEF(JSJMSG_CANT_CONVERT_JS,      2, 2, "Unable to convert JavaScript value {0} to "
								  	       "Java value of type {1}")
MSG_DEF(JSJMSG_BAD_OP_JARRAY,        3, 0, "illegal operation on JavaArray prototype object")
MSG_DEF(JSJMSG_CANT_WRITE_JARRAY,    4, 1, "Attempt to write to invalid Java array element \"{0}\"")
MSG_DEF(JSJMSG_BAD_INDEX_EXPR,       5, 0, "invalid Java array index expression")
MSG_DEF(JSJMSG_BAD_JARRAY_INDEX,     6, 1, "Java array index {0} out of range")
MSG_DEF(JSJMSG_JARRAY_PROP_DEFINE,   7, 0, "Cannot define a new property in a JavaArray")
MSG_DEF(JSJMSG_JARRAY_PROP_DELETE,   8, 0, "Properties of JavaArray objects may not be deleted")
MSG_DEF(JSJMSG_JARRAY_PROP_WATCH,    9, 0, "Cannot place watchpoints on JavaArray object properties")
MSG_DEF(JSJMSG_JARRAY_PROP_EXPORT,  10, 0, "Cannot export a JavaArray object's properties")
MSG_DEF(JSJMSG_BAD_JCLASS_EXPR,     11, 0, "invalid JavaClass property expression. "
				                           "(methods and fields of a JavaClass object can only be identified by their name)")
MSG_DEF(JSJMSG_MISSING_NAME,        12, 2, "Java class {0} has no public static field or method named \"{1}\"")
MSG_DEF(JSJMSG_MISSING_STATIC,      13, 2, "No static field named \"{0}\" in Java class {1}")
MSG_DEF(JSJMSG_JCLASS_PROP_DEFINE,  14, 0, "Cannot define a new property in a JavaClass")
MSG_DEF(JSJMSG_JCLASS_PROP_DELETE,  15, 0, "Properties of JavaClass objects may not be deleted")
MSG_DEF(JSJMSG_JCLASS_PROP_WATCH,   16, 0, "Cannot place watchpoints on JavaClass object properties")
MSG_DEF(JSJMSG_JCLASS_PROP_EXPORT,  17, 0, "Cannot export a JavaClass object's properties")
MSG_DEF(JSJMSG_BAD_OP_JCLASS,       18, 0, "illegal operation on JavaClass prototype object")
MSG_DEF(JSJMSG_BAD_OP_PROTO,        19, 0, "illegal operation on prototype object")
MSG_DEF(JSJMSG_NEED_JOBJECT_ARG,    20, 0, "getClass expects a Java object argument")
MSG_DEF(JSJMSG_PROTO_GETCLASS,      21, 0, "getClass called on prototype object")
MSG_DEF(JSJMSG_BAD_OP_JOBJECT,      22, 0, "illegal operation on JavaObject prototype object")
MSG_DEF(JSJMSG_BAD_JOBJECT_EXPR,    23, 0, "invalid JavaObject property expression. "
				                           "(methods and field properties of a JavaObject object can only be strings)")
MSG_DEF(JSJMSG_NO_INSTANCE_NAME,    24, 2, "Java class {0} has no public field or method named \"{1}\"")
MSG_DEF(JSJMSG_NO_NAME_IN_CLASS,    25, 2, "No instance field named \"{0}\" in Java class {1}")
MSG_DEF(JSJMSG_JOBJECT_PROP_DEFINE, 26, 0, "Cannot define a new property in a JavaObject")
MSG_DEF(JSJMSG_JOBJECT_PROP_DELETE, 27, 0, "Properties of JavaObject objects may not be deleted")
MSG_DEF(JSJMSG_JOBJECT_PROP_WATCH,  28, 0, "Cannot place watchpoints on JavaObject object properties")
MSG_DEF(JSJMSG_JOBJECT_PROP_EXPORT, 29, 0, "Cannot export a JavaObject object's properties")
MSG_DEF(JSJMSG_CONVERT_TO_FUNC,     30, 0, "can't convert Java object to function")
MSG_DEF(JSJMSG_BAD_ADD_TO_PACKAGE,  31, 0, "illegal attempt to add property to JavaPackage prototype object")
MSG_DEF(JSJMSG_DONT_ADD_TO_PACKAGE, 32, 0, "You may not add properties to a JavaPackage object")
MSG_DEF(JSJMSG_MISSING_PACKAGE,     33, 1, "No Java system package with name \"{0}\" was identified "
                                           "and no Java class with that name exists either")
MSG_DEF(JSJMSG_DOUBLE_SHIPPING,     34, 1, "Package {0} defined twice ?")
MSG_DEF(JSJMSG_BAD_PACKAGE_PREDEF,  35, 1, "Illegal predefined package definition for {0}")
MSG_DEF(JSJMSG_NULL_MEMBER_NAME,    36, 0, "illegal null member name")
MSG_DEF(JSJMSG_NULL_FUNCTION_NAME,  37, 0, "illegal null JavaScript function name")
MSG_DEF(JSJMSG_NULL_EVAL_ARG,       38, 0, "illegal null string eval argument")
MSG_DEF(JSJMSG_MULTIPLE_JTHREADS,   39, 0, "[Obsolete error message]")
MSG_DEF(JSJMSG_ABSTRACT_JCLASS,     40, 1, "Java class {0} is abstract and therefore may not be instantiated")
MSG_DEF(JSJMSG_IS_INTERFACE,        41, 1, "{0} is a Java interface and therefore may not be instantiated")
MSG_DEF(JSJMSG_NOT_PUBLIC,          42, 1, "Java class {0} is not public and therefore may not be instantiated")
MSG_DEF(JSJMSG_NO_CONSTRUCTORS,     43, 1, "No public constructors defined for Java class {0}")
MSG_DEF(JSJMSG_BAD_PROTO_ASSIGNMENT,44, 0, "Only objects can be assigned as the value of the __proto__ property")
MSG_DEF(JSJMSG_NEED_JCLASS_ARG,     45, 0, "The JavaClass constructor expects an instance of java.lang.Class as an argument")
