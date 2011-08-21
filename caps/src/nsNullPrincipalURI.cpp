/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsNullPrincipalURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsCRT.h"

////////////////////////////////////////////////////////////////////////////////
//// nsNullPrincipalURI

nsNullPrincipalURI::nsNullPrincipalURI(const nsCString &aSpec)
{
  PRInt32 dividerPosition = aSpec.FindChar(':');
  NS_ASSERTION(dividerPosition != -1, "Malformed URI!");

  PRInt32 n = aSpec.Left(mScheme, dividerPosition);
  NS_ASSERTION(n == dividerPosition, "Storing the scheme failed!");

  PRInt32 count = aSpec.Length() - dividerPosition - 1;
  n = aSpec.Mid(mPath, dividerPosition + 1, count);
  NS_ASSERTION(n == count, "Storing the path failed!");

  ToLowerCase(mScheme);
}

static NS_DEFINE_CID(kNullPrincipalURIImplementationCID,
                     NS_NULLPRINCIPALURI_IMPLEMENTATION_CID);

NS_IMPL_THREADSAFE_ADDREF(nsNullPrincipalURI)
NS_IMPL_THREADSAFE_RELEASE(nsNullPrincipalURI)

NS_INTERFACE_MAP_BEGIN(nsNullPrincipalURI)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  if (aIID.Equals(kNullPrincipalURIImplementationCID))
    foundInterface = static_cast<nsIURI *>(this);
  else
  NS_INTERFACE_MAP_ENTRY(nsIURI)
NS_INTERFACE_MAP_END

////////////////////////////////////////////////////////////////////////////////
//// nsIURI

NS_IMETHODIMP
nsNullPrincipalURI::GetAsciiHost(nsACString &_host)
{
  _host.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetAsciiSpec(nsACString &_spec)
{
  nsCAutoString buffer;
  (void)GetSpec(buffer);
  NS_EscapeURL(buffer, esc_OnlyNonASCII | esc_AlwaysCopy, _spec);
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetHost(nsACString &_host)
{
  _host.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetHost(const nsACString &aHost)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetHostPort(nsACString &_host)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetHostPort(const nsACString &aHost)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetOriginCharset(nsACString &_charset)
{
  _charset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetPassword(nsACString &_password)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetPassword(const nsACString &aPassword)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetPath(nsACString &_path)
{
  _path = mPath;
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetPath(const nsACString &aPath)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetRef(nsACString &_ref)
{
  _ref.Truncate();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetRef(const nsACString &aRef)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetPrePath(nsACString &_prePath)
{
  _prePath = mScheme + NS_LITERAL_CSTRING(":");
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetPort(PRInt32 *_port)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetPort(PRInt32 aPort)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetScheme(nsACString &_scheme)
{
  _scheme = mScheme;
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetScheme(const nsACString &aScheme)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetSpec(nsACString &_spec)
{
  _spec = mScheme + NS_LITERAL_CSTRING(":") + mPath;
  return NS_OK;
}

// result may contain unescaped UTF-8 characters
NS_IMETHODIMP
nsNullPrincipalURI::GetSpecIgnoringRef(nsACString &result)
{
  return GetSpec(result);
}

NS_IMETHODIMP
nsNullPrincipalURI::GetHasRef(PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetSpec(const nsACString &aSpec)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetUsername(nsACString &_username)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetUsername(const nsACString &aUsername)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::GetUserPass(nsACString &_userPass)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetUserPass(const nsACString &aUserPass)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::Clone(nsIURI **_newURI)
{
  nsCOMPtr<nsIURI> uri =
    new nsNullPrincipalURI(mScheme + NS_LITERAL_CSTRING(":") + mPath);
  uri.forget(_newURI);
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::CloneIgnoringRef(nsIURI **_newURI)
{
  // GetRef/SetRef not supported by nsNullPrincipalURI, so
  // CloneIgnoringRef() is the same as Clone().
  return Clone(_newURI);
}

NS_IMETHODIMP
nsNullPrincipalURI::Equals(nsIURI *aOther, PRBool *_equals)
{
  *_equals = PR_FALSE;
  nsNullPrincipalURI *otherURI;
  nsresult rv = aOther->QueryInterface(kNullPrincipalURIImplementationCID,
                                       (void **)&otherURI);
  if (NS_SUCCEEDED(rv)) {
    *_equals = (mScheme == otherURI->mScheme && mPath == otherURI->mPath);
    NS_RELEASE(otherURI);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::EqualsExceptRef(nsIURI *aOther, PRBool *_equals)
{
  // GetRef/SetRef not supported by nsNullPrincipalURI, so
  // EqualsExceptRef() is the same as Equals().
  return Equals(aOther, _equals);
}

NS_IMETHODIMP
nsNullPrincipalURI::Resolve(const nsACString &aRelativePath,
                            nsACString &_resolvedURI)
{
  _resolvedURI = aRelativePath;
  return NS_OK;
}

NS_IMETHODIMP
nsNullPrincipalURI::SchemeIs(const char *aScheme, PRBool *_schemeIs)
{
  *_schemeIs = (0 == nsCRT::strcasecmp(mScheme.get(), aScheme));
  return NS_OK;
}
