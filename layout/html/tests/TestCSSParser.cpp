/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */
#include <stdio.h>
#include "nsICSSParser.h"
#include "nsIStyleSheet.h"
#include "nsIStyleRule.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIUnicharInputStream.h"
#include "nsString.h"

static void Usage(void)
{
  printf("usage: TestCSSParser [-v] [-s string | url1 ...]\n");
}

int main(int argc, char** argv)
{
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
        string = new nsString(argv[++i]);
      }
      else {
        Usage();
        return -1;
      }
    }
    else
      break;
  }

  // Create parser
  nsICSSParser* css;
  nsresult rv = NS_NewCSSParser(&css);
  if (NS_OK != rv) {
    printf("can't create css parser: %d\n", rv);
    return -1;
  }
  PRUint32 infoMask;
  css->GetInfoMask(infoMask);
  printf("CSS parser supports %x\n", infoMask);

  if (nsnull != string) {
    nsIStyleRule* rule;
    rv = css->ParseDeclarations(*string, nsnull, rule);
    if (NS_OK == rv) {
      if (verbose && (nsnull != rule)) {
        rule->List();
      }
    }
    else {
      printf("ParseDeclarations failed: rv=%d\n", rv);
    }
  }
  else {
    for (; i < argc; i++) {
      char* urlName = argv[i];
      // Create url object
      nsIURL* url;
      rv = NS_NewURL(&url, nsnull, urlName);
      if (NS_OK != rv) {
        printf("invalid URL: '%s'\n", urlName);
        return -1;
      }

      // Get an input stream from the url
      PRInt32 ec;
      nsIInputStream* in = url->Open(&ec);
      if (nsnull == in) {
        printf("open of url('%s') failed: error=%x\n", urlName, ec);
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
      nsIStyleSheet* sheet;
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

  css->Release();

  return 0;
}
