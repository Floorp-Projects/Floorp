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
"nsconv -f fromcode -t tocode [file]\n"
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
       if(strcmp(argv[i], "-t") == 0)
       {
             tocodeind = i+1;
             nsAutoString str(argv[tocodeind]);
             res = ccMain->GetUnicodeDecoder(&str, &decoder);
             if(NS_FAILED(res)) {
	     	fprintf(stderr, "Cannot get Unicode decoder %s %x\n", 
			argv[tocodeind],res);
  		return -1;
	     }

       }
       if(strcmp(argv[i], "-f") == 0)
       {
             fromcodeind = i+1;
             nsAutoString str(argv[fromcodeind]);
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
        
	res = decoder->Convert(medbuffer, 0, &medsize,inbuffer,0,&insize);
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

     }
     
     fclose(infile);
     fclose(stdout);
     fprintf(stderr, "Done!\n");
     return 0;
   }
   usage();
   return -1;
}
