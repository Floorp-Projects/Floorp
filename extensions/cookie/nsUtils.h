/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
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

#ifndef UTILS_H
#define UTILS_H

#include "nsString.h"
#include "nsFileStream.h"

//#define GET_ALL_PARTS      127
#define GET_PASSWORD_PART     64
#define GET_USERNAME_PART     32
#define GET_PROTOCOL_PART     16
#define GET_HOST_PART          8
#define GET_PATH_PART          4
#define GET_HASH_PART          2
#define GET_SEARCH_PART        1

extern PRInt32 CKutil_GetLine(nsInputFileStream& strm, nsString& aLine);
extern char * CKutil_ParseURL (const char *url, int parts_requested);
extern PRUnichar* CKutil_Localize(const PRUnichar *genericString);
extern nsresult CKutil_ProfileDirectory(nsFileSpec& dirSpec);
extern char * CKutil_StrAllocCopy(char *&destination, const char *source);
extern char * CKutil_StrAllocCat(char *&destination, const char *source);

#endif /* UTILS_H */
