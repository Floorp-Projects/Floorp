#include <stdio.h>
#include "nsRepository.h" 
#include "nsMsgCID.h"
#include "nsIMsgRFC822Parser.h"

static NS_DEFINE_IID(kIMsgRFC822ParserIID, NS_IMSGRFC822PARSER_IID); 
static NS_DEFINE_CID(kMsgRFC822ParserCID, NS_MSGRFC822PARSER_CID); 

/* This is just a testing stub added by mscott. All I'm doing is loading a component,
   and querying it for a particular interface.

   It is its only purpose / use....

 */

 int main(int argc, char *argv[]) 
 { 
   nsIMsgRFC822Parser *sample; 

   // register our dll
   nsRepository::RegisterFactory(kMsgRFC822ParserCID, "msg.dll", PR_FALSE, PR_FALSE);
   
   nsresult res = nsRepository::CreateInstance(kMsgRFC822ParserCID, 
                                               NULL, 
                                               kIMsgRFC822ParserIID, 
                                               (void **) &sample); 

   if (res == NS_OK && sample) { 
	 printf("We succesfully obtained a nsIMsgRFC822Parser interface....\n");
	 char * names = NULL;
	 char * addresses = NULL;
	 sample->ParseRFC822Addresses("Scott MacGregor <mscott@netscape.com>", &names, &addresses);
	 if (names)
	 {
		 printf(names);
		 printf("\n");
	 }

	 if (addresses)
	 {
		printf(addresses);
		printf("\n");
	 }

	 printf("Releasing the interface now...\n");
     sample->Release(); 
   } 

   return 0; 
 }