/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   name.h --- what are we calling it today.
   Created: Jamie Zawinski <jwz@netscape.com>, 3-Feb-95.
 */


#define XFE_NAME      Netscape
#define XFE_PROGNAME  netscape
#define XFE_PROGCLASS Netscape
#define XFE_LEGALESE "(c) 1995-1998 Netscape Communications Corp."

/* I don't pretend to understand this. */
#define cpp_stringify_noop_helper(x)#x
#define cpp_stringify(x) cpp_stringify_noop_helper(x)

#define XFE_NAME_STRING      cpp_stringify(XFE_NAME)
#define XFE_PROGNAME_STRING  cpp_stringify(XFE_PROGNAME)
#define XFE_PROGCLASS_STRING cpp_stringify(XFE_PROGCLASS)
