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

#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsAddbookUrl.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsAbBaseCID.h"
#include "nsEscape.h"

/////////////////////////////////////////////////////////////////////////////////////
// addbook url definition
/////////////////////////////////////////////////////////////////////////////////////
nsAddbookUrl::nsAddbookUrl()
{
  m_baseURL = do_CreateInstance(NS_SIMPLEURI_CONTRACTID);

  mOperationType = nsIAddbookUrlOperation::InvalidUrl; 
}

nsAddbookUrl::~nsAddbookUrl()
{
}

NS_IMPL_ISUPPORTS2(nsAddbookUrl, nsIAddbookUrl, nsIURI)

NS_IMETHODIMP 
nsAddbookUrl::SetSpec(const nsACString &aSpec)
{
  m_baseURL->SetSpec(aSpec);
	return ParseUrl();
}

nsresult nsAddbookUrl::ParseUrl()
{
  nsCAutoString pathStr;

  nsresult rv = m_baseURL->GetPath(pathStr);
  NS_ENSURE_SUCCESS(rv,rv);

  if (strstr(pathStr.get(), "?action=print"))
    mOperationType = nsIAddbookUrlOperation::PrintAddressBook;
  else if (strstr(pathStr.get(), "?action=add"))
    mOperationType = nsIAddbookUrlOperation::AddVCard;
  else
    mOperationType = nsIAddbookUrlOperation::InvalidUrl;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIURI support
////////////////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP nsAddbookUrl::GetSpec(nsACString &aSpec)
{
	return m_baseURL->GetSpec(aSpec);
}

NS_IMETHODIMP nsAddbookUrl::GetPrePath(nsACString &aPrePath)
{
	return m_baseURL->GetPrePath(aPrePath);
}

NS_IMETHODIMP nsAddbookUrl::GetScheme(nsACString &aScheme)
{
	return m_baseURL->GetScheme(aScheme);
}

NS_IMETHODIMP nsAddbookUrl::SetScheme(const nsACString &aScheme)
{
	return m_baseURL->SetScheme(aScheme);
}

NS_IMETHODIMP nsAddbookUrl::GetUserPass(nsACString &aUserPass)
{
	return m_baseURL->GetUserPass(aUserPass);
}

NS_IMETHODIMP nsAddbookUrl::SetUserPass(const nsACString &aUserPass)
{
	return m_baseURL->SetUserPass(aUserPass);
}

NS_IMETHODIMP nsAddbookUrl::GetUsername(nsACString &aUsername)
{
	return m_baseURL->GetUsername(aUsername);
}

NS_IMETHODIMP nsAddbookUrl::SetUsername(const nsACString &aUsername)
{
	return m_baseURL->SetUsername(aUsername);
}

NS_IMETHODIMP nsAddbookUrl::GetPassword(nsACString &aPassword)
{
	return m_baseURL->GetPassword(aPassword);
}

NS_IMETHODIMP nsAddbookUrl::SetPassword(const nsACString &aPassword)
{
	return m_baseURL->SetPassword(aPassword);
}

NS_IMETHODIMP nsAddbookUrl::GetHostPort(nsACString &aHostPort)
{
	return m_baseURL->GetHostPort(aHostPort);
}

NS_IMETHODIMP nsAddbookUrl::SetHostPort(const nsACString &aHostPort)
{
	return m_baseURL->SetHostPort(aHostPort);
}

NS_IMETHODIMP nsAddbookUrl::GetHost(nsACString &aHost)
{
	return m_baseURL->GetHost(aHost);
}

NS_IMETHODIMP nsAddbookUrl::SetHost(const nsACString &aHost)
{
	return m_baseURL->SetHost(aHost);
}

NS_IMETHODIMP nsAddbookUrl::GetPort(PRInt32 *aPort)
{
	return m_baseURL->GetPort(aPort);
}

NS_IMETHODIMP nsAddbookUrl::SetPort(PRInt32 aPort)
{
	return m_baseURL->SetPort(aPort);
}

NS_IMETHODIMP nsAddbookUrl::GetPath(nsACString &aPath)
{
	return m_baseURL->GetPath(aPath);
}

NS_IMETHODIMP nsAddbookUrl::SetPath(const nsACString &aPath)
{
	return m_baseURL->SetPath(aPath);
}

NS_IMETHODIMP nsAddbookUrl::GetAsciiHost(nsACString &aHostA)
{
	return m_baseURL->GetAsciiHost(aHostA);
}

NS_IMETHODIMP nsAddbookUrl::GetAsciiSpec(nsACString &aSpecA)
{
	return m_baseURL->GetAsciiSpec(aSpecA);
}

NS_IMETHODIMP nsAddbookUrl::GetOriginCharset(nsACString &aOriginCharset)
{
    return m_baseURL->GetOriginCharset(aOriginCharset);
}

NS_IMETHODIMP nsAddbookUrl::SchemeIs(const char *aScheme, PRBool *_retval)
{
	return m_baseURL->SchemeIs(aScheme, _retval);
}

NS_IMETHODIMP nsAddbookUrl::Equals(nsIURI *other, PRBool *_retval)
{
	return m_baseURL->Equals(other, _retval);
}

NS_IMETHODIMP nsAddbookUrl::Clone(nsIURI **_retval)
{
	return m_baseURL->Clone(_retval);
}	

NS_IMETHODIMP nsAddbookUrl::Resolve(const nsACString &relativePath, nsACString &result) 
{
	return m_baseURL->Resolve(relativePath, result);
}

//
// Specific nsAddbookUrl operations
//
NS_IMETHODIMP 
nsAddbookUrl::GetAddbookOperation(PRInt32 *_retval)
{
  *_retval = mOperationType;
  return NS_OK;
}
