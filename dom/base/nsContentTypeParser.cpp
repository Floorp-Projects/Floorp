/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentTypeParser.h"
#include "nsContentUtils.h"

nsContentTypeParser::nsContentTypeParser(const nsAString& aString)
  : mString(aString)
  , mService(nullptr)
{
  CallGetService("@mozilla.org/network/mime-hdrparam;1", &mService);
}

nsContentTypeParser::~nsContentTypeParser()
{
  NS_IF_RELEASE(mService);
}

nsresult
nsContentTypeParser::GetParameter(const char* aParameterName,
                                  nsAString& aResult) const
{
  NS_ENSURE_TRUE(mService, NS_ERROR_FAILURE);
  return mService->GetParameterHTTP(
    mString, aParameterName, EmptyCString(), false, nullptr, aResult);
}

nsresult
nsContentTypeParser::GetType(nsAString& aResult) const
{
  nsresult rv = GetParameter(nullptr, aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsContentUtils::ASCIIToLower(aResult);
  return NS_OK;
}
