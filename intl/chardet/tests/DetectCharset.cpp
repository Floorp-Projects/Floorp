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
#include <iostream.h>
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef XP_PC
#include <io.h>
#endif
#ifdef XP_UNIX
#include <unistd.h>
#endif


#define MAXBSIZE (1L << 13)

void usage() {
   printf("Usage: DetectFile detector blocksize\n"
          "     detector: " 
          "japsm,"
          "1stblkdbg,"
          "2ndblkdbg,"
          "lastblkdbg"
        "\n     blocksize: 1 ~ %ld\n"
          "  Data are passed in from STDIN\n"
          ,  MAXBSIZE);
}

class nsReporter : public nsICharsetDetectionObserver 
{
   NS_DECL_ISUPPORTS
 public:
   nsReporter() { NS_INIT_REFCNT(); };
   virtual ~nsReporter() { };

   NS_IMETHOD Notify(const char* aCharset, nsDetectionConfident aConf)
    {
        printf("RESULT CHARSET : %s\n", aCharset);
        printf("RESULT Confident : %d\n", aConf);
        return NS_OK;
    };
};


NS_IMPL_ISUPPORTS(nsReporter, nsICharsetDetectionObserver::GetIID())

nsresult GetDetector(const char* key, nsICharsetDetector** det)
{
  char buf[128];
  strcpy(buf, NS_CHARSET_DETECTOR_PROGID_BASE);
  strcat(buf, key);
  return nsComponentManager::CreateInstance(
            buf,
            nsnull,
            nsICharsetDetector::GetIID(),
            (void**)det);
}


nsresult GetObserver(nsICharsetDetectionObserver** aRes)
{
  *aRes = nsnull;
  nsReporter* rep = new nsReporter();
  if(rep) {
     return rep->QueryInterface(nsICharsetDetectionObserver::GetIID() ,
                                (void**)aRes);
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

int main(int argc, char** argv) {
  char buf[MAXBSIZE];
  PRUint32 bs;
  if( 3 != argc )
  {
    usage();
    printf("Need 2 arguments\n");
    return(-1);
  }
  bs = atoi(argv[2]);
  if((bs <1)||(bs>MAXBSIZE))
  {
    usage();
    printf("blocksize out of range - %s\n", argv[2]);
    return(-1);
  }
  nsresult rev = NS_OK;
  nsICharsetDetector *det = nsnull;
  rev = GetDetector(argv[1], &det);
  if(NS_FAILED(rev) || (nsnull == det) ){
    usage();
    printf("Invalid Detector - %s\n", argv[1]);
    printf("XPCOM ERROR CODE = %x\n", rev);
    return(-1);
  }
  nsICharsetDetectionObserver *obs = nsnull;
  rev = GetObserver(&obs);
  if(NS_SUCCEEDED(rev)) {
    rev = det->Init(obs);
    NS_IF_RELEASE(obs);
    if(NS_FAILED(rev))
    {
      printf("XPCOM ERROR CODE = %x\n", rev);
      return(-1);
    }
  } else {
    printf("XPCOM ERROR CODE = %x\n", rev);
    return(-1);
  }

  size_t sz;
  PRBool done = PR_FALSE;
  do
  {
    sz = read(0, buf, bs); 
    if(sz > 0) {
      rev = det->DoIt( buf, sz, &done);
      if(NS_FAILED(rev))
      {
        printf("XPCOM ERROR CODE = %x\n", rev);
        return(-1);
      }
    }
  } while((sz > 0) && (!done));
  rev = det->Done();
  if(NS_FAILED(rev))
  {
    printf("XPCOM ERROR CODE = %x\n", rev);
    return(-1);
  }
  
  NS_IF_RELEASE(det);
  return (0);
}
