/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 * Contributors:
 *  Joe Hewitt <hewitt@netscape.com>
 */

#include "inBitmapURI.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"

static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);
#define DEFAULT_IMAGE_SIZE          16

////////////////////////////////////////////////////////////////////////////////
 
NS_IMPL_THREADSAFE_ISUPPORTS2(inBitmapURI, inIBitmapURI, nsIURI)

#define NS_BITMAP_SCHEME           "moz-bitmap:"
#define NS_BITMAP_DELIMITER        '?'

inBitmapURI::inBitmapURI()
{
  NS_INIT_ISUPPORTS();
}
 
inBitmapURI::~inBitmapURI()
{
}

////////////////////////////////////////////////////////////////////////////////
// inIBitmapURI

NS_IMETHODIMP
inBitmapURI::GetBitmapName(PRUnichar** aBitmapName)
{
  *aBitmapName = ToNewUnicode(mBitmapName);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
inBitmapURI::FormatSpec(nsACString &result)
{
  result = NS_LITERAL_CSTRING(NS_BITMAP_SCHEME "//") + mBitmapName;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIURI

NS_IMETHODIMP
inBitmapURI::GetSpec(nsACString &aSpec)
{
  return FormatSpec(aSpec);
}

NS_IMETHODIMP
inBitmapURI::SetSpec(const nsACString &aSpec)
{
  nsresult rv;
  nsCOMPtr<nsIIOService> ioService (do_GetService(kIOServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString scheme;
  rv = ioService->ExtractScheme(aSpec, scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  if (strcmp("moz-bitmap", scheme.get()) != 0)
    return NS_ERROR_MALFORMED_URI;

  nsACString::const_iterator end, colon, delim;
  aSpec.BeginReading(colon);
  aSpec.EndReading(end);

  if (!FindCharInReadable(':', colon, end))
    return NS_ERROR_MALFORMED_URI;
  
  delim = ++colon;
  if (!FindCharInReadable(NS_BITMAP_DELIMITER, delim, end))
    delim = end;

  mBitmapName = Substring(colon, delim);
  // TODO: parse out other parameters

  if (mBitmapName.IsEmpty())
    return NS_ERROR_MALFORMED_URI;
  
  return NS_OK;
}

NS_IMETHODIMP
inBitmapURI::GetPrePath(nsACString &prePath)
{
  prePath = NS_BITMAP_SCHEME;
  return NS_OK;
}

NS_IMETHODIMP
inBitmapURI::GetScheme(nsACString &aScheme)
{
  aScheme = "moz-bitmap";
  return NS_OK;
}

NS_IMETHODIMP
inBitmapURI::SetScheme(const nsACString &aScheme)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::GetUsername(nsACString &aUsername)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::SetUsername(const nsACString &aUsername)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::GetPassword(nsACString &aPassword)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::SetPassword(const nsACString &aPassword)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::GetUserPass(nsACString &aUserPass)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::SetUserPass(const nsACString &aUserPass)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::GetHostPort(nsACString &aHostPort)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::SetHostPort(const nsACString &aHostPort)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::GetHost(nsACString &aHost)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::SetHost(const nsACString &aHost)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::GetPort(PRInt32 *aPort)
{
  return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP
inBitmapURI::SetPort(PRInt32 aPort)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::GetPath(nsACString &aPath)
{
  aPath.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
inBitmapURI::SetPath(const nsACString &aPath)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
inBitmapURI::GetAsciiSpec(nsACString &aSpec)
{
  return GetSpec(aSpec);
}

NS_IMETHODIMP
inBitmapURI::GetAsciiHost(nsACString &aHost)
{
  return GetHost(aHost);
}

NS_IMETHODIMP
inBitmapURI::GetOriginCharset(nsACString &result)
{
  result.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
inBitmapURI::Equals(nsIURI *other, PRBool *result)
{
  nsCAutoString spec1;
  nsCAutoString spec2;

  other->GetSpec(spec2);
  GetSpec(spec1);

  *result = !nsCRT::strcasecmp(spec1.get(), spec2.get());
  return NS_OK;
}

NS_IMETHODIMP
inBitmapURI::SchemeIs(const char *i_Scheme, PRBool *o_Equals)
{
  NS_ENSURE_ARG_POINTER(o_Equals);
  if (!i_Scheme) return NS_ERROR_INVALID_ARG;
  
  *o_Equals = PL_strcasecmp("moz-bitmap", i_Scheme) ? PR_FALSE : PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
inBitmapURI::Clone(nsIURI **result)
{
  return NS_OK;
}

NS_IMETHODIMP
inBitmapURI::Resolve(const nsACString &relativePath, nsACString &result)
{
  return NS_OK;
}

