/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include <stdio.h>
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIStyleRule.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIUnicharInputStream.h"
#include "nsString.h"

// XXX begin bad code
#include "plevent.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"

#include "nsLayoutCID.h" 
#include "nsCOMPtr.h"

#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define LAYOUT_DLL    "gkhtml.dll"
#else
#ifdef XP_MAC
#include "nsMacRepository.h"
#else
#define NETLIB_DLL "libnecko"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define LAYOUT_DLL    "libgklayout"MOZ_DLL_SUFFIX
#endif
#endif

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCSSParserCID, NS_CSSPARSER_CID);
// XXX end bad code

static void Usage(void)
{
  printf("usage: TestCSSParser [-v] [-s string | url1 ...]\n");
}

int main(int argc, char** argv)
{
  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kIOServiceCID, NULL, NULL, NETLIB_DLL, PR_FALSE, PR_FALSE);

  nsComponentManager::RegisterComponent(kCSSParserCID, NULL, NULL, LAYOUT_DLL, PR_FALSE, PR_FALSE);

  nsresult rv;
  PRBool verbose = PR_FALSE;
  nsString* string = nsnull;
  int i;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (strcmp(argv[i], "-v") == 0) {
        verbose = PR_TRUE;
      }
      else if (strcmp(argv[i], "-s") == 0) {
        if ((nsnull != string) || (i == argc - 1)) {
          Usage();
          return -1;
        }
        string = new nsString;
        string->AssignWithConversion(argv[++i]);
      }
      else {
        Usage();
        return -1;
      }
    }
    else
      break;
  }

  // Create the Event Queue for this thread...
  nsIEventQueueService* pEventQService = nsnull;
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    NS_GET_IID(nsIEventQueueService),
                                    (nsISupports **)&pEventQService);
  if (NS_SUCCEEDED(rv)) {
    // XXX: What if this fails?
    rv = pEventQService->CreateThreadEventQueue();
  }

  // Create parser
  nsCOMPtr<nsICSSParser> css;
  rv = nsComponentManager::CreateInstance(kCSSParserCID,
	nsnull,
	NS_GET_IID(nsICSSParser),
	getter_AddRefs(css));
	
  if (NS_FAILED(rv)) {
    printf("can't create css parser: %d\n", rv);
    return -1;
  }
  PRUint32 infoMask;
  css->GetInfoMask(infoMask);
  printf("CSS parser supports %x\n", infoMask);

  if (nsnull != string) {
    nsIStyleRule* rule;
    rv = css->ParseStyleAttribute(*string, nsnull, &rule);
    if (NS_OK == rv) {
      if (verbose && (nsnull != rule)) {
        rule->List();
      }
    }
    else {
      printf("ParseStyleAttribute failed: rv=%d\n", rv);
    }
  }
  else {
    for (; i < argc; i++) {
      char* urlName = argv[i];
      // Create url object
      nsIURI* url;
      nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID, &rv));
      if (NS_FAILED(rv)) return -1;

      nsIURI *uri = nsnull;
      rv = service->NewURI(urlName, nsnull, &uri);
      if (NS_FAILED(rv)) return -1;

      rv = uri->QueryInterface(NS_GET_IID(nsIURI), (void**)&url);
      NS_RELEASE(uri);
      if (NS_OK != rv) {
        printf("invalid URL: '%s'\n", urlName);
        return -1;
      }

      // Get an input stream from the url
      nsIInputStream* in;
      rv = NS_OpenURI(&in, url);
      if (rv != NS_OK) {
        printf("open of url('%s') failed: error=%x\n", urlName, rv);
        continue;
      }

      // Translate the input using the argument character set id into unicode
      nsIUnicharInputStream* uin;
      rv = NS_NewConverterStream(&uin, nsnull, in);
      if (NS_OK != rv) {
        printf("can't create converter input stream: %d\n", rv);
        return -1;
      }

      // Parse the input and produce a style set
      nsICSSStyleSheet* sheet;
      rv = css->Parse(uin, url, sheet);
      if (NS_OK == rv) {
        if (verbose) {
          sheet->List();
        }
      }
      else {
        printf("parse failed: %d\n", rv);
      }

      url->Release();
      in->Release();
      uin->Release();
      sheet->Release();
    }
  }

  /* Release the event queue for the thread */
  if (nsnull != pEventQService) {
    pEventQService->DestroyThreadEventQueue();
    nsServiceManager::ReleaseService(kEventQueueServiceCID, pEventQService);
  }

  return 0;
}
