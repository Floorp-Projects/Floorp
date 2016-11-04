/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsContentUtils.h"
#include "nsIDOMDocument.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXMLHttpRequest.h"

#define TEST_URL_PREFIX  "data:text/xml,"
#define TEST_URL_CONTENT "<foo><bar></bar></foo>"
#define TEST_URL         TEST_URL_PREFIX TEST_URL_CONTENT

TEST(NativeXMLHttpRequest, Test)
{
  nsresult rv;

  nsCOMPtr<nsIXMLHttpRequest> xhr =
    do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Couldn't create nsIXMLHttpRequest instance";

  NS_NAMED_LITERAL_CSTRING(getString, "GET");
  NS_NAMED_LITERAL_CSTRING(testURL, TEST_URL);
  const nsAString& empty = EmptyString();

  nsCOMPtr<nsIScriptSecurityManager> secman =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Couldn't get script security manager";

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  rv = secman->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Couldn't get system principal";

  rv = xhr->Init(systemPrincipal, nullptr, nullptr, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Couldn't initialize the XHR";

  rv = xhr->Open(getString, testURL, false, empty, empty);
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Open failed";

  rv = xhr->Send(nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "Send failed";

  nsAutoString response;
  rv = xhr->GetResponseText(response);
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "GetResponse failed";
  ASSERT_TRUE(response.EqualsLiteral(TEST_URL_CONTENT)) <<
    "Response text does not match";

  nsCOMPtr<nsIDOMDocument> dom;
  rv = xhr->GetResponseXML(getter_AddRefs(dom));
  ASSERT_TRUE(NS_SUCCEEDED(rv)) << "GetResponseXML failed";
  ASSERT_TRUE(dom) << "No DOM document constructed";
}
