/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
#include <windows.h>
#include <winnls.h>

#include "nscore.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kICharsetConverterManagerIID, NS_ICHARSETCONVERTERMANAGER_IID);

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void usage()
{
  printf(
"convperf -f fromcode -t tocode [file]\n"
  );
}
int fromcodeind = 0;
int tocodeind = 0;
FILE* infile = 0;
#define INBUFSIZE (1024*16)
#define MEDBUFSIZE (1024*16*2)
#define OUTBUFSIZE (1024*16*8)
char inbuffer[INBUFSIZE];
char outbuffer[OUTBUFSIZE];
PRUnichar  medbuffer[MEDBUFSIZE];
nsIUnicodeEncoder* encoder = nsnull;
nsIUnicodeDecoder* decoder = nsnull;
UINT incp = 932;
UINT outcp = 932;

void memcpyDecode(const char* src, PRInt32 srclen, char* dest)
{
   ::memcpy(dest, src, srclen);
}
void memcpyEncode(const char* src, PRInt32 srclen, char* dest)
{
   ::memcpy(dest, src, srclen);
}

void WideDecode(const char* src, 
              PRInt32 srclen, PRUnichar *dest, PRInt32 *destLen)
{
   const char* end = src+srclen ;
   while(src < end)
     *dest++ = (PRUnichar) *src++;
   *destLen = srclen;
}
void NarrowEncode(const PRUnichar *src, 
              PRInt32 srclen, char* dest, PRInt32* destLen)
{
   const PRUnichar* end = src+srclen ;
   while(src < end)
     *dest++ = (char) *src++;
   *destLen = srclen;
}
void msDecode(UINT cp, const char* src, 
              PRInt32 srclen, PRUnichar *dest, PRInt32 *destLen)
{
   *destLen = ::MultiByteToWideChar(cp, 0,src, srclen, (LPWSTR)dest, *destLen);
   if(*destLen <= 0)
      fprintf(stderr, "problem in ::MultiByteToWideChar\n");
}
void msEncode(UINT cp, const PRUnichar *src, 
              PRInt32 srcLen, char* dest, PRInt32* destLen)
{
   *destLen = ::WideCharToMultiByte(cp, 0, src, srcLen, (LPSTR)dest, *destLen, 
                (LPCSTR)" ", FALSE);
   if(*destLen <= 0)
      fprintf(stderr, "problem in ::WideCharToMultiByte\n");
}
     
int main(int argc, const char** argv)
{
   nsresult res= NS_OK;
   nsICharsetConverterManager* ccMain=nsnull;
   // get ccMain;
   nsServiceManager::GetService(kCharsetConverterManagerCID,
       kICharsetConverterManagerIID, (nsISupports**) &ccMain);
   if(NS_FAILED(res))
   {
	 fprintf(stderr, "Cannot get Character Converter Manager %x\n", res);
   }
   int i;
   if(argc > 5)
   {
     for(i =0; i < argc; i++)
     {
       if(strcmp(argv[i], "-f") == 0)
       {
             tocodeind = i+1;
             nsAutoString str; str.AssignWithConversion(argv[tocodeind]);
             res = ccMain->GetUnicodeDecoder(&str, &decoder);
             if(NS_FAILED(res)) {
	     	fprintf(stderr, "Cannot get Unicode decoder %s %x\n", 
			argv[tocodeind],res);
  		return -1;
	     }

       }
       if(strcmp(argv[i], "-t") == 0)
       {
             fromcodeind = i+1;
             nsAutoString str; str.AssignWithConversion(argv[fromcodeind]);
             res = ccMain->GetUnicodeEncoder(&str, &encoder);
             if(NS_FAILED(res)) {
	     	fprintf(stderr, "Cannot get Unicode encoder %s %x\n", 
			argv[fromcodeind],res);
  		return -1;
	     }
       }
     }
     if(argc == 6)
     {
        infile = fopen(argv[5], "rb");
        if(NULL == infile) 
        {  
           usage();
           fprintf(stderr,"cannot open file %s\n", argv[5]);
           return -1; 
        }
     }
     else
     {
        infile = stdin;
     }
    
     PRInt32 insize,medsize,outsize;
     while((insize=fread(inbuffer, 1,INBUFSIZE,infile)) > 0)
     {
        medsize=MEDBUFSIZE;
        
	res = decoder->Convert(inbuffer,&insize, medbuffer, &medsize);
        if(NS_FAILED(res)) {
            fprintf(stderr, "failed in decoder->Convert %x\n",res);
	    return -1;
	}
        outsize = OUTBUFSIZE;
	res = encoder->Convert(medbuffer, &medsize, outbuffer,&outsize);
        if(NS_FAILED(res)) {
            fprintf(stderr, "failed in encoder->Convert %x\n",res);
	    return -1;
	}
        fwrite(outbuffer, 1, outsize, stdout);

        memcpyDecode(inbuffer, insize, outbuffer);

        memcpyEncode(inbuffer, insize, outbuffer);

        medsize = MEDBUFSIZE;
        msDecode(incp, inbuffer, insize, medbuffer, &medsize);

        outsize = OUTBUFSIZE;
        msEncode(outcp, medbuffer, medsize, outbuffer, &outsize);

        medsize = MEDBUFSIZE;
        WideDecode( inbuffer, insize, medbuffer, &medsize);

        outsize = OUTBUFSIZE;
        NarrowEncode( medbuffer, medsize, outbuffer, &outsize);
     }
     
     fclose(infile);
     fclose(stdout);
     fprintf(stderr, "Done!\n");
     return 0;
   }
   usage();
   return -1;
}
