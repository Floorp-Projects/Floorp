/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla XMLHttpRequest Tests.
 *
 * The Initial Developer of the Original Code is
 * Adam Barth
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

  rv = xhr->Init(systemPrincipal, nsnull, nsnull, nsnull);
  TEST_ENSURE_SUCCESS(rv, "Couldn't initialize the XHR!");

  rv = xhr->Open(getString, aURL, false, empty, empty);
  TEST_ENSURE_SUCCESS(rv, "OpenRequest failed!");

  rv = xhr->Send(nsnull, nsnull);
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
