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
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIUnicharInputStream.h"
#include "nsString.h"

static void Usage(void)
{
  printf("usage: TestCSSParser [-v] url\n");
}

int main(int argc, char** argv)
{
  if (argc < 2 || argc > 3) {
    Usage();
    return -1;
  }

  PRBool verbose = PR_FALSE;
  char* urlName;
  if (argc == 3) {
    if (strcmp(argv[1], "-v") == 0) {
      verbose = PR_TRUE;
    } else {
      Usage();
      return -1;
    }
    urlName = argv[2];
  } else {
    urlName = argv[1];
  }

  // Create url object
  nsIURL* url;
  nsresult rv = NS_NewURL(&url, nsnull, urlName);
  if (NS_OK != rv) {
    printf("invalid URL: '%s'\n", urlName);
    return -1;
  }

  // Get an input stream from the url
  PRInt32 ec;
  nsIInputStream* in = url->Open(&ec);
  if (nsnull == in) {
    printf("open of url('%s') failed: error=%x\n", urlName, ec);
    return -1;
  }

  // Translate the input using the argument character set id into unicode
  nsIUnicharInputStream* uin;
  rv = NS_NewConverterStream(&uin, nsnull, in);
  if (NS_OK != rv) {
    printf("can't create converter input stream: %d\n", rv);
    return -1;
  }

  // Create parser and set it up to process the input file
  nsICSSParser* css;
  rv = NS_NewCSSParser(&css);
  if (NS_OK != rv) {
    printf("can't create css parser: %d\n", rv);
    return -1;
  }
  printf("CSS parser supports %x\n", css->GetInfoMask());

  // Parse the input and produce a style set
  nsIStyleSheet* sheet = css->Parse(&ec, uin, url);
  if (verbose) {
    sheet->List();
  }

  url->Release();
  in->Release();
  uin->Release();
  css->Release();
  sheet->Release();

  return 0;
}
