/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"    // precompiled header...
#include "nsMimeHeaders.h"

nsMimeHeaders::nsMimeHeaders() :
	mHeaders(nsnull)
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_ISUPPORTS();
}

nsMimeHeaders::~nsMimeHeaders()
{
	if (mHeaders)
		MimeHeaders_free(mHeaders);
}

NS_IMPL_ISUPPORTS1(nsMimeHeaders, nsIMimeHeaders)

nsresult nsMimeHeaders::Initialize(const char * aAllHeaders, PRInt32 allHeadersSize)
{
	/* just in case we want to reuse the object, cleanup...*/
	if (mHeaders)
		MimeHeaders_free(mHeaders);

	mHeaders = MimeHeaders_new();
	if (mHeaders)
		return MimeHeaders_parse_line(aAllHeaders, allHeadersSize, mHeaders);
	
	return NS_ERROR_OUT_OF_MEMORY;
}

nsresult nsMimeHeaders::ExtractHeader(const char *headerName, PRBool getAllOfThem, char **_retval)
{
	if (! mHeaders)
		return NS_ERROR_NOT_INITIALIZED;
	
	*_retval = MimeHeaders_get(mHeaders, headerName, PR_FALSE, getAllOfThem);
	return NS_OK;
}

