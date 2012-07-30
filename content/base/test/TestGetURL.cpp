/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

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

nsresult TestGetURL(const nsCString& aURL)
{
  nsresult rv;

  nsCOMPtr<nsIXMLHttpRequest> xhr =
    do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  TEST_ENSURE_SUCCESS(rv, "Couldn't create nsIXMLHttpRequest instance!");

  NS_NAMED_LITERAL_CSTRING(getString, "GET");
  const nsAString& empty = EmptyString();
  
  nsCOMPtr<nsIScriptSecurityManager> secman =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  TEST_ENSURE_SUCCESS(rv, "Couldn't get script security manager!");

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  rv = secman->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  TEST_ENSURE_SUCCESS(rv, "Couldn't get system principal!");

  rv = xhr->Init(systemPrincipal, nullptr, nullptr, nullptr);
  TEST_ENSURE_SUCCESS(rv, "Couldn't initialize the XHR!");

  rv = xhr->Open(getString, aURL, false, empty, empty);
  TEST_ENSURE_SUCCESS(rv, "OpenRequest failed!");

  rv = xhr->Send(nullptr);
  TEST_ENSURE_SUCCESS(rv, "Send failed!");

  nsAutoString response;
  rv = xhr->GetResponseText(response);
  TEST_ENSURE_SUCCESS(rv, "GetResponse failed!");

  nsCAutoString responseUTF8 = NS_ConvertUTF16toUTF8(response);
  printf("#BEGIN\n");
  printf("%s", responseUTF8.get());
  printf("\n#EOF\n");

  return NS_OK;
}

int main(int argc, char** argv)
{
  if (argc <  2) {
    printf("Usage: TestGetURL <url>\n");
    exit(0);
  }

  ScopedXPCOM xpcom("XMLHttpRequest");
  if (xpcom.failed())
    return 1;

  nsCAutoString targetURL(argv[1]);

  int retval = 0;
  if (NS_FAILED(TestGetURL(targetURL))) {
    retval = 1;
  }

  return retval;
}
