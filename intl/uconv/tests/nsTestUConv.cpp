/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#define NS_IMPL_IDS

#include <stdio.h>
#include "nsRepository.h"
#include "nsISupports.h"
#include "nsICharsetConverterManager.h"
#include "nsConverterCID.h"

#ifdef XP_UNIX
#define UCONV_DLL       "libuconv.so"
#else
#ifdef XP_MAC
#define UCONV_DLL       "UCONV_DLL"
#else /* XP_MAC */
#define UCONV_DLL       "uconv.dll"
#endif
#endif
#define TABLE_SIZE1     5

nsICharsetConverterManager * ccMan = NULL;

nsresult setupRegistry()
{
  nsresult res;

  res = nsRepository::RegisterFactory(kCharsetConverterManagerCID, UCONV_DLL, PR_FALSE, PR_FALSE);
  if (NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) return res;

  res = nsRepository::RegisterFactory(kAscii2UnicodeCID, UCONV_DLL, PR_FALSE, PR_FALSE);
  return res;
}

nsresult init()
{
  nsresult res;

  res = setupRegistry();
  if (NS_FAILED(res)  && (NS_ERROR_FACTORY_EXISTS != res)) {
    printf("Error setting up registry: 0x%x",res);
    return res;
  }

  return NS_OK;
}

nsresult done()
{
  if (ccMan != NULL) NS_RELEASE(ccMan);
  return NS_OK;
}

nsresult testCharsetConverterManager()
{
  printf("\n[T1] CharsetConverterManager\n");

  nsresult res = nsRepository::CreateInstance(kCharsetConverterManagerCID,
      NULL, kICharsetConverterManagerIID, (void **)&ccMan);
  if (NS_FAILED(res)) {
    printf("ERROR 0x%x: Cannot instantiate.\n",res);
    return res;
  } else {
    printf("Instantiated.\n");
  }

  nsString ** cs;
  PRInt32 ct;

  res = ccMan->GetEncodableCharsets(&cs, &ct);
  if (NS_FAILED(res)) {
    printf("ERROR 0x%x: GetEncodableCharsets()\n",res);
  } else {
    printf("Unicode encoders (%d): ", ct);
    for (int i=0;i<ct;i++) {
      char * cstr = cs[i]->ToNewCString();
      printf("%s ",cstr);
      delete [] cstr;
    }
    printf("\n");
  }
  delete [] cs;

  res = ccMan->GetDecodableCharsets(&cs, &ct);
  if (NS_FAILED(res)) {
    printf("ERROR 0x%x: GetDecodableCharsets()\n",res);
  } else {
    printf("Unicode decoders (%d): ", ct);
    for (int i=0;i<ct;i++) {
      char * cstr = cs[i]->ToNewCString();
      printf("%s ",cstr);
      delete [] cstr;
    }
    printf("\n");
  }
  delete [] cs;

  return NS_OK;
}

nsresult testAsciiDecoder()
{
  printf("\n[T2] Ascii2Unicode\n");

  // create converter
  nsIUnicodeDecoder * dec;
  nsAutoString str("Ascii");
  nsresult res = ccMan->GetUnicodeDecoder(&str,&dec);
  if (NS_FAILED(res)) {
    printf("ERROR 0x%x: Cannot instantiate.\n",res);
    return res;
  } else {
    printf("Instantiated.\n");
  }

  //test converter

  PRInt32 srcL = TABLE_SIZE1;
  PRInt32 destL = TABLE_SIZE1;
  char src [TABLE_SIZE1] = {(char)0,(char)255,(char)13,(char)127,(char)128,};
  PRUnichar dest [TABLE_SIZE1];

  res=dec->Convert(dest, 0, &destL, src, 0, &srcL);
  if (NS_FAILED(res)) {
    printf("ERROR 0x%x: Convert().\n",res);
  } else {
    printf("Read %d, write %d.\n",srcL,destL);
    printf("Converted:");

    PRBool failed = PR_FALSE;
    for (int i=0;i<TABLE_SIZE1;i++) {
      printf(" %d->%x", src[i], dest[i]);
      if (dest[i] != ((PRUint8)src[i])) failed = PR_TRUE;
    }
    printf("\n");

    if (failed) {
      printf("Test FAILED!!!\n");
    } else {
      printf("Test Passed.\n");
    }
  }

  NS_RELEASE(dec);

  return NS_OK;
}

nsresult run()
{
  nsresult res;

  res = testCharsetConverterManager();
  if (NS_FAILED(res)) return res;

  res = testAsciiDecoder();

  return NS_OK;
}

int main(int argc, char **argv)
{
  nsresult res;

  printf("*** Unicode Converters Test ***\n");

  res = init();
  if (NS_FAILED(res)) return -1;
  run();
  done();

  printf("\n***---------  Done  --------***\n");
  return 0;
}
