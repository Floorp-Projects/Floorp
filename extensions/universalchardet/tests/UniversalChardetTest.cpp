/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsICharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(XP_WIN) || defined(XP_OS2)
#include <io.h>
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
#include <unistd.h>
#endif

#define MAXBSIZE (1L << 13)

void usage() {
   printf("Usage: DetectFile blocksize\n"
        "    blocksize: 1 ~ %ld\n"
          "  Data are passed in from STDIN\n"
          ,  MAXBSIZE);
}

class nsReporter : public nsICharsetDetectionObserver 
{
   NS_DECL_ISUPPORTS
 public:
   nsReporter() { };
   virtual ~nsReporter() { };

   NS_IMETHOD Notify(const char* aCharset, nsDetectionConfident aConf)
    {
        printf("RESULT CHARSET : %s\n", aCharset);
        printf("RESULT Confident : %d\n", aConf);
        return NS_OK;
    };
};


NS_IMPL_ISUPPORTS1(nsReporter, nsICharsetDetectionObserver)

nsresult GetDetector(const char* key, nsICharsetDetector** det)
{
  char buf[128];
  strcpy(buf, NS_CHARSET_DETECTOR_CONTRACTID_BASE);
  strcat(buf, key);
  return CallCreateInstance(buf, det);
}


nsresult GetObserver(nsICharsetDetectionObserver** aRes)
{
  *aRes = nsnull;
  nsReporter* rep = new nsReporter();
  if(rep) {
     return rep->QueryInterface(NS_GET_IID(nsICharsetDetectionObserver) ,
                                (void**)aRes);
  }
  return NS_ERROR_OUT_OF_MEMORY;
}

int main(int argc, char** argv) {
  char buf[MAXBSIZE];
  PRUint32 bs;
  if( 2 != argc )
  {
    usage();
    printf("Need 1 arguments\n");
    return(-1);
  }
  bs = atoi(argv[1]);
  if((bs <1)||(bs>MAXBSIZE))
  {
    usage();
    printf("blocksize out of range - %s\n", argv[2]);
    return(-1);
  }
  nsresult rev = NS_OK;
  nsICharsetDetector *det = nsnull;
  rev = GetDetector("universal_charset_detector", &det);
  if(NS_FAILED(rev) || (nsnull == det) ){
    usage();
    printf("Error: Could not find Universal Detector\n");
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
      if(! done) {
        rev = det->DoIt( buf, sz, &done);
        if(NS_FAILED(rev))
        {
          printf("XPCOM ERROR CODE = %x\n", rev);
          return(-1);
        }
      }
    }
  } while((sz > 0) &&  (!done) );
  //} while(sz > 0);
  if(!done)
  {
    rev = det->Done();
    if(NS_FAILED(rev))
    {
      printf("XPCOM ERROR CODE = %x\n", rev);
      return(-1);
    }
  }

  NS_IF_RELEASE(det);
  return (0);
}
