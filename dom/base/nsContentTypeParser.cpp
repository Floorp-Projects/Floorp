/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentTypeParser.h"
#include "nsNetUtil.h"

nsContentTypeParser::nsContentTypeParser(const nsAString& aString)
  : mString(aString)
{
}

nsresult
nsContentTypeParser::GetParameter(const char* aParameterName,
                                  nsAString& aResult) const
{
  return net::GetParameterHTTP(mString, aParameterName, aResult);
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
