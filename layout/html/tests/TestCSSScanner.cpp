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
#include "nsCSSScanner.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIUnicharInputStream.h"
#include "nsString.h"

int main(int argc, char** argv)
{
  if (2 != argc) {
    printf("usage: TestCSSScanner url\n");
    return -1;
  }

  // Create url object
  char* urlName = argv[1];
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

  // Create scanner and set it up to process the input file
  nsCSSScanner* css = new nsCSSScanner();
  css->Init(uin);

  // Scan the file and dump out the tokens
  nsCSSToken tok;
  for (;;) {
    char buf[20];
    if (!css->Next(&ec, &tok)) {
      break;
    }
    printf("%02d: ", tok.mType);
    switch (tok.mType) {
    case eCSSToken_Ident:
    case eCSSToken_AtKeyword:
    case eCSSToken_String:
    case eCSSToken_WhiteSpace:
    case eCSSToken_URL:
    case eCSSToken_InvalidURL:
    case eCSSToken_ID:
      fputs(tok.mIdent, stdout);
      fputs("\n", stdout);
      break;

    case eCSSToken_Number:
      printf("%g\n", tok.mNumber);
      break;
    case eCSSToken_Percentage:
      printf("%g%%\n", tok.mNumber * 100.0f);
      break;
    case eCSSToken_Dimension:
      tok.mIdent.ToCString(buf, sizeof(buf));
      printf("%g%s\n", tok.mNumber, buf);
      break;
    case eCSSToken_Symbol:
      printf("%c (%x)\n", tok.mSymbol, tok.mSymbol);
      break;
    }
  }

  delete css;
  uin->Release();
  in->Release();
  url->Release();

  return 0;
}
