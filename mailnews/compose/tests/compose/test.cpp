#include <stdio.h>
#include "nsRepository.h" 
#include "nsMsgCompCID.h"
#include "nsIMsgCompose.h"
#include "nsIMsgCompFields.h"

static NS_DEFINE_IID(kIMsgComposeIID, NS_IMSGCOMPOSE_IID); 
static NS_DEFINE_CID(kMsgComposeCID, NS_MSGCOMPOSE_CID); 

static NS_DEFINE_CID(kIMsgCompFieldsIID, NS_IMSGCOMPFIELDS_IID); 
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 

/* This is just a testing stub added by mscott. All I'm doing is loading a component,
   and querying it for a particular interface.

   It is its only purpose / use....

 */

 int main(int argc, char *argv[]) 
 { 
	nsIMsgCompose *pMsgCompose; 
	nsIMsgCompFields *pMsgCompFields;
	nsresult res;

	// register our dll
	nsRepository::RegisterFactory(kMsgComposeCID, "msgcompose.dll", PR_FALSE, PR_FALSE);
	nsRepository::RegisterFactory(kMsgCompFieldsCID, "msgcompose.dll", PR_FALSE, PR_FALSE);
   
	res = nsRepository::CreateInstance(kMsgCompFieldsCID, 
												NULL, 
												kIMsgCompFieldsIID, 
												(void **) &pMsgCompFields); 

	if (res == NS_OK && pMsgCompFields) { 
		printf("We succesfully obtained a nsIMsgCompFields interface....\n");
		pMsgCompFields->Test();

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
		res = nsRepository::CreateInstance(kMsgCompFieldsCID, 
												NULL, 
												kIMsgCompFieldsIID, 
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



/*
	NS_IMETHOD GetBcc(char **_retval);
	NS_IMETHOD GetFcc(char **_retval);
	NS_IMETHOD GetNewsFcc(char **_retval);
	NS_IMETHOD GetNewsBcc(char **_retval);
	NS_IMETHOD GetNewsgroups(char **_retval);
	NS_IMETHOD GetFollowupTo(char **_retval);
	NS_IMETHOD GetSubject(char **_retval);
	NS_IMETHOD GetAttachments(char **_retval);
	NS_IMETHOD GetOrganization(char **_retval);
	NS_IMETHOD GetReferences(char **_retval);
	NS_IMETHOD GetOtherRandomHeaders(char **_retval);
	NS_IMETHOD GetNewspostUrl(char **_retval);
	NS_IMETHOD GetDefaultBody(char **_retval);
	NS_IMETHOD GetPriority(char **_retval);
	NS_IMETHOD GetMessageEncoding(char **_retval);
	NS_IMETHOD GetCharacterSet(char **_retval);
	NS_IMETHOD GetMessageId(char **_retval);
	NS_IMETHOD GetHTMLPart(char **_retval);
	NS_IMETHOD GetTemplateName(char **_retval);
	NS_IMETHOD GetReturnReceipt(PRBool *_retval);
	NS_IMETHOD GetAttachVCard(PRBool *_retval);
*/

	 printf("Releasing the interface now...\n");
     pMsgCompFields->Release(); 
   } 

    res = nsRepository::CreateInstance(kMsgComposeCID, 
                                               NULL, 
                                               kIMsgComposeIID, 
                                               (void **) &pMsgCompose); 

   if (res == NS_OK && pMsgCompose) { 
	 printf("We succesfully obtained a nsIMsgCompose interface....\n");
	 pMsgCompose->Test();

	 printf("Releasing the interface now...\n");
     pMsgCompose->Release(); 
   } 
  return 0; 
 }
