/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
*/

#ifndef NS_MSGUTF7UTILS_H
#define NS_MSGUTF7UTILS_H

#include "msgCore.h"
#include "nsError.h"

NS_MSG_BASE char* CreateUtf7ConvertedString(const char * aSourceString, PRBool aConvertToUtf7Imap);

NS_MSG_BASE nsresult CreateUnicodeStringFromUtf7(const char *aSourceString, PRUnichar **result);

NS_MSG_BASE char * CreateUtf7ConvertedStringFromUnicode(const PRUnichar *aSourceString);

#endif