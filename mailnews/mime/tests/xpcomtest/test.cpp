#include <stdio.h>
#include "nsRepository.h" 
#include "nsRFC822toHTMLStreamConverter.h"

static NS_DEFINE_IID(kIStreamConverterIID, NS_ISTREAM_CONVERTER_IID);
static NS_DEFINE_CID(kRFC822toHTMLStreamConverterCID, NS_RFC822_HTML_STREAM_CONVERTER_CID); 

/* This is just a testing stub added by mscott. All I'm doing is loading a component,
   and querying it for a particular interface.

   It is its only purpose / use....

 */

 int main(int argc, char *argv[]) 
 { 
   nsRFC822toHTMLStreamConverter *sample; 

   // register our dll
   nsRepository::RegisterFactory(kRFC822toHTMLStreamConverterCID, "mime.dll", PR_FALSE, PR_FALSE);
   
   nsresult res = nsRepository::CreateInstance(kRFC822toHTMLStreamConverterCID, 
                                               NULL, 
                                               kIStreamConverterIID, 
                                               (void **) &sample); 

   if (res == NS_OK && sample) 
   { 
      printf("We succesfully obtained a nsRFC822toHTMLStreamConverter interface....\n");

      // sample->ParseRFC822Addresses("Scott MacGregor <mscott@netscape.com>", &names, &addresses);

      printf("Releasing the interface now...\n");
      sample->Release(); 
   } 

   return 0; 
 }
