#include <stdio.h>
#include "nsIComponentManager.h" 
#include "nsMsgBaseCID.h"
#include "nsIMsgRFC822Parser.h"

static NS_DEFINE_CID(kMsgRFC822ParserCID, NS_MSGRFC822PARSER_CID); 

/* This is just a testing stub added by mscott. All I'm doing is loading a component,
   and querying it for a particular interface.

   It is its only purpose / use....

 */

 int main(int argc, char *argv[]) 
 { 
   nsIMsgRFC822Parser *sample; 

   // register our dll
   nsComponentManager::RegisterComponent(kMsgRFC822ParserCID, NULL, NULL, "mailnews.dll", PR_FALSE, PR_FALSE);
   
   nsresult res = nsComponentManager::CreateInstance(kMsgRFC822ParserCID, 
                                               NULL, 
                                               nsIMsgRFC822Parser::GetIID(), 
                                               (void **) &sample); 

   if (res == NS_OK && sample) { 
	 printf("We succesfully obtained a nsIMsgRFC822Parser interface....\n");
	 char * names = NULL;
	 char * addresses = NULL;
	 PRUint32 numAddresses = 0; 
	 sample->ParseRFC822Addresses(NULL, "Scott MacGregor <mscott@netscape.com>", &names, &addresses, numAddresses);
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
