/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNullPrincipalURI.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsCRT.h"

////////////////////////////////////////////////////////////////////////////////
//// nsNullPrincipalURI

nsNullPrincipalURI::nsNullPrincipalURI(const nsCString &aSpec)
{
  int32_t dividerPosition = aSpec.FindChar(':');
  NS_ASSERTION(dividerPosition != -1, "Malformed URI!");

  int32_t n = aSpec.Left(mScheme, dividerPosition);
  NS_ASSERTION(n == dividerPosition, "Storing the scheme failed!");

  int32_t count = aSpec.Length() - dividerPosition - 1;
  n = aSpec.Mid(mPath, dividerPosition + 1, count);
  NS_ASSERTION(n == count, "Storing the path failed!");

  ToLowerCase(mScheme);
}

static NS_DEFINE_CID(kNullPrincipalURIImplementationCID,
                     NS_NULLPRINCIPALURI_IMPLEMENTATION_CID);

NS_IMPL_THREADSAFE_ADDREF(nsNullPrincipalURI)
NS_IMPL_THREADSAFE_RELEASE(nsNullPrincipalURI)

NS_INTERFACE_MAP_BEGIN(nsNullPrincipalURI)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURI)
  if (aIID.Equals(kNullPrincipalURIImplementationCID))
    foundInterface = static_cast<nsIURI *>(this);
  else
  NS_INTERFACE_MAP_ENTRY(nsIURI)
  NS_INTERFACE_MAP_ENTRY(nsISizeOf)
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
  nsAutoCString buffer;
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
nsNullPrincipalURI::GetPort(int32_t *_port)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNullPrincipalURI::SetPort(int32_t aPort)
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
nsNullPrincipalURI::GetHasRef(bool *result)
{
  *result = false;
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
nsNullPrincipalURI::Equals(nsIURI *aOther, bool *_equals)
{
  *_equals = false;
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
nsNullPrincipalURI::EqualsExceptRef(nsIURI *aOther, bool *_equals)
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
nsNullPrincipalURI::SchemeIs(const char *aScheme, bool *_schemeIs)
{
  *_schemeIs = (0 == nsCRT::strcasecmp(mScheme.get(), aScheme));
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISizeOf

size_t
nsNullPrincipalURI::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return mScheme.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mPath.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t
nsNullPrincipalURI::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

