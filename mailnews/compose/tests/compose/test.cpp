#include <stdio.h>
#include "nsIComponentManager.h" 
#include "nsMsgCompCID.h"
#include "nsIMsgCompose.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"

static NS_DEFINE_IID(kIMsgComposeIID, NS_IMSGCOMPOSE_IID); 
static NS_DEFINE_CID(kMsgComposeCID, NS_MSGCOMPOSE_CID); 

static NS_DEFINE_IID(kIMsgCompFieldsIID, NS_IMSGCOMPFIELDS_IID); 
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 

static NS_DEFINE_IID(kIMsgSendIID, NS_IMSGSEND_IID); 
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID); 

/* This is just a testing stub added by mscott. All I'm doing is loading a component,
   and querying it for a particular interface.

   It is its only purpose / use....

 */

 int main(int argc, char *argv[]) 
 { 
	nsIMsgCompose *pMsgCompose; 
	nsIMsgCompFields *pMsgCompFields;
	nsIMsgSend *pMsgSend;
	nsresult res;

	// register our dll
	nsComponentManager::RegisterComponent(kMsgComposeCID, NULL, NULL, "msgcompose.dll", PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kMsgCompFieldsCID, NULL, NULL, "msgcompose.dll", PR_FALSE, PR_FALSE);
	nsComponentManager::RegisterComponent(kMsgSendCID, NULL, NULL, "msgcompose.dll", PR_FALSE, PR_FALSE);

	res = nsComponentManager::CreateInstance(kMsgCompFieldsCID, 
                                           NULL, 
                                           nsIMsgCompFields::GetIID(), 
                                           (void **) &pMsgCompFields); 

	if (res == NS_OK && pMsgCompFields) { 
		printf("We succesfully obtained a nsIMsgCompFields interface....\n");

/*
		const char * value;
		pMsgCompFields->SetFrom("field #01 FROM", NULL);
		pMsgCompFields->SetReplyTo("field #02 REPLY_TO", NULL);
		pMsgCompFields->SetTo("field #03 TO", NULL);
		pMsgCompFields->SetCc("field #04 CC", NULL);
		pMsgCompFields->SetBcc("field #05 BCC", NULL);
		pMsgCompFields->SetFcc("field #06 FCC", NULL);
		pMsgCompFields->SetNewsFcc("field #07 NEWS_FCC", NULL);
		pMsgCompFields->SetNewsBcc("field #08 NEWS_BCC", NULL);
		pMsgCompFields->SetNewsgroups("field #09 NEWSGROUPS", NULL);
		pMsgCompFields->SetFollowupTo("field #10 FOLLOWUP_TO", NULL);
		pMsgCompFields->SetSubject("field #11 SUBJECT", NULL);
		pMsgCompFields->SetAttachments("field #12 ATTACHMENTS", NULL);
		pMsgCompFields->SetOrganization("field #13 ORGANIZATION", NULL);
		pMsgCompFields->SetReferences("field #14 REFERENCES", NULL);
		pMsgCompFields->SetOtherRandomHeaders("field #15 OTHET_RANDOM_HEADEARS", NULL);
		pMsgCompFields->SetNewspostUrl("field #16 NEWS_POST_URL", NULL);
		pMsgCompFields->SetDefaultBody("field #17 DEFAULT_BODY", NULL);
		pMsgCompFields->SetPriority("field #18 PRIORITY", NULL);
		pMsgCompFields->SetMessageEncoding("field #19 MESSAGE_ENCODING", NULL);
		pMsgCompFields->SetCharacterSet("field #20 CHARACTER_SET", NULL);
		pMsgCompFields->SetMessageId("field #21 MESSAGE_ID", NULL);
		pMsgCompFields->SetHTMLPart("field #22 HTML_PART", NULL);
		pMsgCompFields->SetTemplateName("field #23 TEMPLATE_NAME", NULL);
		pMsgCompFields->SetReturnReceipt(PR_TRUE, NULL);
		pMsgCompFields->SetAttachVCard(PR_TRUE, NULL);

		nsIMsgCompFields * pCopyFields;
		res = nsComponentManager::CreateInstance(kMsgCompFieldsCID, 
												NULL, 
												nsIMsgCompFields::GetIID(), 
												(void **) &pCopyFields); 

		if (res == NS_OK && pCopyFields) { 
			printf("We succesfully obtained a second nsIMsgCompFields interface....\n");

			pCopyFields->Copy(pMsgCompFields);
			pMsgCompFields->GetFrom((char **)&value);
			printf("%s\n", value);
			pMsgCompFields->GetReplyTo((char **)&value);
			printf("%s\n", value);
			pMsgCompFields->GetTo((char **)&value);
			printf("%s\n", value);
			pMsgCompFields->GetCc((char **)&value);
			printf("%s\n", value);
			pCopyFields->Release();
		}
*/

	 printf("Releasing the interface now...\n");
     pMsgCompFields->Release(); 
   } 

    res = nsComponentManager::CreateInstance(kMsgComposeCID, 
                                               NULL, 
                                               nsIMsgCompose::GetIID(), 
                                               (void **) &pMsgCompose); 

   if (res == NS_OK && pMsgCompose) {
	 printf("We succesfully obtained a nsIMsgCompose interface....\n");
#if 0
	 pMsgCompose->CreateAndInitialize(NULL, NULL, NULL, NULL, NULL);
#endif
	 printf("Releasing the interface now...\n");
     pMsgCompose->Release(); 
   } 
 
	res = nsComponentManager::CreateInstance(kMsgSendCID, 
                                               NULL, 
                                               kIMsgSendIID, 
                                               (void **) &pMsgSend); 

	if (res == NS_OK && pMsgSend) { 
		printf("We succesfully obtained a nsIMsgSend interface....\n");

		res = nsComponentManager::CreateInstance(kMsgCompFieldsCID, 
													NULL, 
													kIMsgCompFieldsIID, 
													(void **) &pMsgCompFields); 
		if (res == NS_OK && pMsgCompFields) { 
			pMsgCompFields->SetFrom(", qatest02@netscape.com, ", NULL);
			pMsgCompFields->SetTo("qatest02@netscape.com", NULL);
			pMsgCompFields->SetSubject("[spam] test", NULL);
			pMsgCompFields->SetBody("Sample message sent with Mozilla\n\nPlease do not reply, thanks\n\nJean-Francois\n", NULL);

			pMsgSend->SendMessage(pMsgCompFields, "");

			pMsgCompFields->Release(); 
		}

		printf("Releasing the interface now...\n");
		pMsgSend->Release(); 
	}

	return 0; 
 }
