/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _NAB_UTILS_H_
#define _NAB_UTILS_H_

#define   CHAR_VALUE      0
#define   BOOL_VALUE      1
#define   INT_VALUE       2

BOOL        GetLDIFLineForUser(MSG_Pane *abPane, LONG userIndex, 
                           LPSTR outString, NABUserID *userID, 
                           NABUpdateTime *updtTime);

// For creating HTML output file...
BOOL        OpenNABAPIHTMLFile(LPSTR fName, LPSTR title);
BOOL        CloseNABAPIHTMLFile(void);
BOOL        DumpHTMLTableLineForUser(MSG_Pane *abPane, LONG userIndex);
BOOL        SearchABForAttrib(AB_ContainerInfo  *abContainer,
                              LPSTR             searchAttrib,
                              LPSTR             ldifInfo,
                              NABUserID         *userID, 
                              NABUpdateTime     *updtTime);

// For insert entries...
NABError    InsertEntryToAB(AB_ContainerInfo *abContainer, LPSTR newLine, 
                BOOL updateOnly, ABID *updateID);

#define HTML_HEAD1 \
"<HTML>\n\
<HEAD>\n\
   <META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\">\n\
   <TITLE>"

#define HMTL_HEAD2 \
"</TITLE>\n\
</HEAD>\n\
&nbsp;\n\
<TABLE BORDER >\n"

#define HTML_TAIL "\
</TABLE>\n\
&nbsp;\n\
</BODY>\n\
</HTML>\n"

#endif // _NAB_UTILS_H_

/*
stuff...
<TD>AB_attribFamilyName</TD>

<TD>AB_attribGivenName</TD>

<TD>AB_attribEmailAddress</TD>

<TD>AB_attribCompanyName
<BR>AB_attribTitle</TD>

<TD>AB_attribWorkPhone</TD>

<TD>AB_attribFaxPhone</TD>

<TD>AB_attribHomePhone</TD>

<TD>AB_attribPOAddress
<BR>AB_attribStreetAddress
<BR>AB_attribLocality, AB_attribRegion AB_attribZipCode
<BR>AB_attribCountry</TD>
...end of stuff....
*/