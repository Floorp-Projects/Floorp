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
#include "nsIUnicharInputStream.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsString.h"
#include "prprf.h"
#include "prtime.h"

static nsCharSetID ConvertCharacterSetName(const char* aName)
{
  if (nsCRT::strcasecmp(aName, "iso-latin-1") == 0) {
    return eCharSetID_IsoLatin1;
  }
  return (nsCharSetID) -1;
}

int main(int argc, char** argv)
{
  if (3 != argc) {
    printf("usage: CvtURL url character-set-name\n");
    return -1;
  }

  char* characterSetName = argv[2];
  nsCharSetID cset = ConvertCharacterSetName(characterSetName);
  if (PRInt32(cset) < 0) {
    printf("illegal character set name: '%s'\n", characterSetName);
    return -1;
  }

  // Create url object
  char* urlName = argv[1];
  nsIURL* url;
  nsresult rv = NS_NewURL(&url, urlName);
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
  rv = NS_NewConverterStream(&uin, nsnull, in, 0, cset);
  if (NS_OK != rv) {
    printf("can't create converter input stream: %d\n", rv);
    return -1;
  }

  // Read the input and write some output
  PRTime start = PR_Now();
  PRInt32 count = 0;
  for (;;) {
    PRUnichar buf[1000];
    PRInt32 nb = uin->Read(&ec, buf, 0, 1000);
    if (nb <= 0) {
      if (nb < 0) {
        printf("i/o error: %d\n", ec);
      }
      break;
    }
    count += nb;
  }
  PRTime end = PR_Now();
  PRTime conversion, ustoms;
  LL_I2L(ustoms, 1000);
  LL_SUB(conversion, end, start);
  LL_DIV(conversion, conversion, ustoms);
  char buf[500];
  PR_snprintf(buf, sizeof(buf),
              "converting and discarding %d bytes took %lldms",
              count, conversion);
  puts(buf);

  // Release the objects
  in->Release();
  uin->Release();
  url->Release();

  return 0;
}
