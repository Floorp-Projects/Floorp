/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIURL.h"
#include "nsPop3URL.h"
#include "nsPop3Protocol.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsCRT.h"

nsPop3URL::nsPop3URL(): nsMsgMailNewsUrl()
{
}
 
nsPop3URL::~nsPop3URL()
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsPop3URL, nsMsgMailNewsUrl, nsIPop3URL)  
  

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIPop3URL specific support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsPop3URL::SetPop3Sink(nsIPop3Sink* aPop3Sink)
{
    if (aPop3Sink)
        m_pop3Sink = dont_QueryInterface(aPop3Sink);
    return NS_OK;
}

nsresult nsPop3URL::GetPop3Sink(nsIPop3Sink** aPop3Sink)
{
    if (aPop3Sink)
	{
        *aPop3Sink = m_pop3Sink;
		NS_IF_ADDREF(*aPop3Sink);
	}
    return NS_OK;
}

NS_IMETHODIMP
nsPop3URL::GetMessageUri(char ** aMessageUri)
{
    if(!aMessageUri || m_messageUri.Length() == 0)
        return NS_ERROR_NULL_POINTER;
    *aMessageUri = ToNewCString(m_messageUri);
    return NS_OK;
}

NS_IMETHODIMP
nsPop3URL::SetMessageUri(const char *aMessageUri)
{
    if (aMessageUri)
        m_messageUri = aMessageUri;
    return NS_OK;
}

nsresult nsPop3URL::ParseUrl(const nsString& aSpec)
{
	// mscott - i don't believe I'm going to need this
	// method anymore..
	NS_ASSERTION(0, "we shouldn't need to call this method anymore");
    return NS_OK;
}



