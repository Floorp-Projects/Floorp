/* $Id: name.h,v 1.1 1998/09/25 18:01:36 ramiro%netscape.com Exp $
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#define QTFE_NAME      QtMozilla
#define QTFE_PROGNAME  qtMozilla
#define QTFE_PROGCLASS QtMozilla
#define QTFE_LEGALESE "(c) 1998 Troll Tech AS"

/* I don't pretend to understand this. */
#define cpp_stringify_noop_helper(x)#x
#define cpp_stringify(x) cpp_stringify_noop_helper(x)

#define QTFE_NAME_STRING      cpp_stringify(QTFE_NAME)
#define QTFE_PROGNAME_STRING  cpp_stringify(QTFE_PROGNAME)
#define QTFE_PROGCLASS_STRING cpp_stringify(QTFE_PROGCLASS)
