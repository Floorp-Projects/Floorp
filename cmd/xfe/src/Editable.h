/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   Editable.h -- interface definition for editable views
   Created: Dora Hsu<dora@netscape.com>, 27-Sept-96.
 */



#ifndef _xfe_editable_h
#define _xfe_editable_h

class XFE_Editable //Mix-in class - only can have pure virtual methods
{
public:
  virtual char *getPlainText() = 0; // Must override to provide content
				    // of msg in plain text
  virtual void insertMessageCompositionText(const char* text, 
					    XP_Bool leaveCursorBeginning, XP_Bool isHTML) = 0;

  virtual void getMessageBody(char **pBody, uint32 *body_size, 
			      MSG_FontCode **font_changes) = 0;

  virtual void doneWithMessageBody(char* pBody) = 0;

  virtual Boolean isModified() = 0; // Override to indicate if text area has been modified
};

#endif /* _xfe_editable_h */
