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


class nsStatis {
public:
    nsStatis() { };
    virtual ~nsStatis() { };
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen) = 0;
    virtual void   DataEnd() = 0;
    virtual void Report()=0;
};

class nsBaseStatis : public nsStatis {
public:
    nsBaseStatis(unsigned char aL, unsigned char aH, float aR) ;
    virtual ~nsBaseStatis() {};
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen);
    virtual void   DataEnd() ;
    virtual void Report();
protected:
    unsigned char mLWordHi;
    unsigned char mLWordLo;
private:
    PRUint32 mNumOf2Bytes;
    PRUint32 mNumOfLChar;
    PRUint32 mNumOfLWord;
    PRUint32 mLWordLength;
    PRUint32 mLWordLen[10]; 
    float    mR;
    PRBool mTailByte;
    PRBool mLastLChar;
};
nsBaseStatis::nsBaseStatis(unsigned char aL, unsigned char aH, float aR)
{
    mNumOf2Bytes = mNumOfLWord = mLWordLength = mNumOfLChar= 0;
    mTailByte = mLastLChar = PR_FALSE;
    for(PRUint32 i =0;i < 20; i++)
       mLWordLen[i] = 0;
    mLWordHi = aH;
    mLWordLo = aL;
    mR = aR;
}
PRBool nsBaseStatis::HandleData(const char* aBuf, PRUint32 aLen)
{
    for(PRUint32 i=0; i < aLen; i++)
    {
       if(mTailByte)
          mTailByte = PR_FALSE;
       else 
       {
          mTailByte = (0x80 == ( aBuf[i] & 0x80));
          if(mTailByte) 
          {
             mNumOf2Bytes++;
             unsigned char a = (unsigned char) aBuf[i];
             PRBool thisLChar = (( mLWordLo <= a) && (a <= mLWordHi));
             if(thisLChar)
             {
                mNumOfLChar++;
                mLWordLength++;
             } else {
                if(mLastLChar) {
                  mNumOfLWord++;
                  mLWordLen[ (mLWordLength > 10) ? 9 : (mLWordLength-1)]++;
                  mLWordLength =0 ;
                }
             }
             mLastLChar = thisLChar;
          } else {
             if(mLastLChar) {
                mNumOfLWord++;
                mLWordLen[ (mLWordLength > 10) ? 9 : (mLWordLength-1)]++;
                mLWordLength =0 ;
                mLastLChar = PR_FALSE;
             }
          }
       }
    }
    return PR_TRUE;
}
void nsBaseStatis::DataEnd()
{
    if(mLastLChar) {
      mNumOfLWord++;
      mLWordLen[ (mLWordLength > 10) ? 9 : (mLWordLength-1)]++;
    }
}
void nsBaseStatis::Report()
{
    if(mNumOf2Bytes > 0)
    {
/*
      printf("LChar Ratio = %d : %d ( %5.3f)\n", 
                         mNumOfLChar,
                         mNumOf2Bytes,
                        ((float)mNumOfLChar / (float)mNumOf2Bytes) * 100);
*/
      float rate = (float) mNumOfLChar / (float) mNumOf2Bytes;
      float delta = (rate - mR) / mR;
      delta *= delta * 1000;
#ifdef EXPERIMENT
      printf("Exp = %f \n",delta);
#endif
    }
    
/*

    if(mNumOfLChar > 0)
      printf("LWord Word = %d : %d (%5.3f)\n", 
                         mNumOfLWord,
                         mNumOfLChar,
                        ((float)mNumOfLWord / (float)mNumOfLChar) * 100);
    if(mNumOfLWord > 0)
    {
      PRUint32 ac =0;
      for(PRUint32 i=0;i<10;i++)
      {
       ac += mLWordLen[i];
       printf("LWord Word Length[%d]= %d -> %5.3f%% %5.3f%%\n", i+1, 
           mLWordLen[i],
           (((float)mLWordLen[i] / (float)mNumOfLWord) * 100),
           (((float)ac / (float)mNumOfLWord) * 100));
      }
    }
*/
}


class nsSimpleStatis : public nsStatis {
public:
    nsSimpleStatis(unsigned char aL, unsigned char aH, float aR,const char* aCharset) ;
    virtual ~nsSimpleStatis() {};
    virtual PRBool HandleData(const char* aBuf, PRUint32 aLen);
    virtual void   DataEnd() ;
    virtual void Report();
protected:
    unsigned char mLWordHi;
    unsigned char mLWordLo;
private:
    PRUint32 mNumOf2Bytes;
    PRUint32 mNumOfLChar;
    float    mR;
    const char* mCharset;
    PRBool mTailByte;
};
nsSimpleStatis::nsSimpleStatis(unsigned char aL, unsigned char aH, float aR, const char* aCharset)
{
    mNumOf2Bytes =  mNumOfLChar= 0;
    mTailByte =  PR_FALSE;
    mLWordHi = aH;
    mLWordLo = aL;
    mR = aR;
    mCharset = aCharset;
}
PRBool nsSimpleStatis::HandleData(const char* aBuf, PRUint32 aLen)
{
    for(PRUint32 i=0; i < aLen; i++)
    {
       if(mTailByte)
          mTailByte = PR_FALSE;
       else 
       {
          mTailByte = (0x80 == ( aBuf[i] & 0x80));
          if(mTailByte) 
          {
             mNumOf2Bytes++;
             unsigned char a = (unsigned char) aBuf[i];
             PRBool thisLChar = (( mLWordLo <= a) && (a <= mLWordHi));
             if(thisLChar)
                mNumOfLChar++;
          }
       }
    }
    return PR_TRUE;
}
void nsSimpleStatis::DataEnd()
{
}
void nsSimpleStatis::Report()
{
    if(mNumOf2Bytes > 0)
    {
      float rate = (float) mNumOfLChar / (float) mNumOf2Bytes;
      float delta = (rate - mR) / mR;
      delta = delta * delta * (float)100;
#ifdef EXPERIMENT
      printf("Exp = %f \n",delta);
      if(delta < 1.0)
         printf("This is %s\n" ,mCharset);
#endif

    }
}
//==========================================================


#define MAXBSIZE (1L << 13)

void usage() {
   printf("Usage: DetectFile detector blocksize\n"
          "     detector: " 
          "japsm,"
          "kopsm,"
          "zhcnpsm,"
          "zhtwpsm,"
          "zhpsm,"
          "cjkpsm,"
          "ruprob,"
          "ukprob,"
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
  nsSimpleStatis  ks(0xb0,0xc8, (float)0.95952, "EUC-KR");
  nsSimpleStatis  js(0xa4,0xa5, (float)0.45006, "EUC-JP");
  nsStatis* stat[2] = {&ks, &js};
  PRUint32 i;
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
      for(i=0;i<2;i++)
        stat[i]->HandleData(buf, sz);
    }
  // } while((sz > 0) &&  (!done) );
  } while(sz > 0);
  if(!done)
  {
    rev = det->Done();
    if(NS_FAILED(rev))
    {
      printf("XPCOM ERROR CODE = %x\n", rev);
      return(-1);
    }
  }
  for(i=0;i<2;i++) {
    stat[i]->DataEnd();
    stat[i]->Report();
  }
printf( "Done\n");
  
  NS_IF_RELEASE(det);
printf( "Done 2\n");
  return (0);
}
