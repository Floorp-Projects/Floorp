#include <stdio.h>
#include "nsIComponentManager.h" 
#include "nsMsgBaseCID.h"
#include "nsIMsgHeaderParser.h"

static NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID); 

/* This is just a testing stub added by mscott. All I'm doing is loading a component,
   and querying it for a particular interface.

   It is its only purpose / use....

 */

 int main(int argc, char *argv[]) 
 { 
   nsIMsgHeaderParser *sample; 

   // register our dll
   nsComponentManager::RegisterComponent(kMsgHeaderParserCID, NULL, NULL, "mailnews.dll", PR_FALSE, PR_FALSE);
   
   nsresult res = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                               NULL, 
                                               nsIMsgHeaderParser::GetIID(), 
                                               (void **) &sample); 

   if (res == NS_OK && sample) { 
	 printf("We succesfully obtained a nsIMsgHeaderParser interface....\n");
	 char * names = NULL;
	 char * addresses = NULL;
	 PRUint32 numAddresses = 0; 
	 sample->ParseHeaderAddresses(NULL, "Scott MacGregor <mscott@netscape.com>", &names, &addresses, numAddresses);
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
