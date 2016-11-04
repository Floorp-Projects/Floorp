/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsIDOMDocument.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIXMLHttpRequest.h"


#define TEST_ENSURE_BASE(_test, _msg)       \
  PR_BEGIN_MACRO                            \
    if (_test) {                            \
      fail(_msg);                           \
      return NS_ERROR_FAILURE;              \
    }                                       \
  PR_END_MACRO

#define TEST_ENSURE_SUCCESS(_rv, _msg)      \
  TEST_ENSURE_BASE(NS_FAILED(_rv), _msg)

#define TEST_ENSURE_FAILED(_rv, _msg)       \
  TEST_ENSURE_BASE(NS_SUCCEEDED(_rv), _msg)

#define TEST_URL_PREFIX                     \
  "data:text/xml,"
#define TEST_URL_CONTENT                    \
  "<foo><bar></bar></foo>"

#define TEST_URL                            \
  TEST_URL_PREFIX TEST_URL_CONTENT

nsresult TestNativeXMLHttpRequest()
{
  nsresult rv;

  nsCOMPtr<nsIXMLHttpRequest> xhr =
    do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  TEST_ENSURE_SUCCESS(rv, "Couldn't create nsIXMLHttpRequest instance!");

  NS_NAMED_LITERAL_CSTRING(getString, "GET");
  NS_NAMED_LITERAL_CSTRING(testURL, TEST_URL);
  const nsAString& empty = EmptyString();

  nsCOMPtr<nsIScriptSecurityManager> secman =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  TEST_ENSURE_SUCCESS(rv, "Couldn't get script security manager!");

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  rv = secman->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  TEST_ENSURE_SUCCESS(rv, "Couldn't get system principal!");

  rv = xhr->Init(systemPrincipal, nullptr, nullptr, nullptr);
  TEST_ENSURE_SUCCESS(rv, "Couldn't initialize the XHR!");

  rv = xhr->Open(getString, testURL, false, empty, empty);
  TEST_ENSURE_SUCCESS(rv, "Open failed!");

  rv = xhr->Send(nullptr);
  TEST_ENSURE_SUCCESS(rv, "Send failed!");

  nsAutoString response;
  rv = xhr->GetResponseText(response);
  TEST_ENSURE_SUCCESS(rv, "GetResponse failed!");

  if (!response.EqualsLiteral(TEST_URL_CONTENT)) {
    fail("Response text does not match!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMDocument> dom;
  rv = xhr->GetResponseXML(getter_AddRefs(dom));
  TEST_ENSURE_SUCCESS(rv, "GetResponseXML failed!");

  if (!dom) {
    fail("No DOM document constructed!");
    return NS_ERROR_FAILURE;
  }

  passed("Native XMLHttpRequest");
  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("XMLHttpRequest");
  if (xpcom.failed())
    return 1;

  int retval = 0;
  if (NS_FAILED(TestNativeXMLHttpRequest())) {
    retval = 1;
  }

  return retval;
}
