// All the definition here after are temporary and must be removed in the future when be defined somewhere else...

#include "xpgetstr.h"
#include "xp_qsort.h"

class MSG_ZapIt
{
	void* unused;
};

class MSG_Prefs : public MSG_ZapIt 
{
public:
	PRBool         GetDefaultBccSelf(PRBool newsBcc) {return PR_FALSE;}
	const char *    GetDefaultHeaderContents(MSG_HEADER_SET header) {return NULL;}
	char *          MagicFolderName(PRUint32 flag, int *pStatus = 0) {return NULL;}
	PRBool         GetAutoQuoteReply() {return PR_FALSE;}
};

class MSG_Master
{
public:
    PRInt32 GetFolderTree(void) {return 0;}
	HJ97882
//	msg_HostTable* GetHostTable() {return m_hosttable;}

};

class MSG_Pane
{
public:
	void* unused;
	MSG_Prefs* m_prefs;
	MWContext* m_context;
	MSG_Master* m_master;

	MSG_Pane()	{;}
	virtual ~MSG_Pane()	{;}

	void MSG_PaneCreate(MWContext* context, MSG_Master* master)	{;}
	MSG_Prefs* GetPrefs() {return m_prefs;}
	PRInt32	GetCommandStatus(PRInt32 command,
										 const unsigned long * indices,
										 PRInt32 numindices,
										 PRBool* selectable_p,
										 void* selected_p,
										 const char **display_string,
										 PRBool * plural_p) {return 0;};
	PRInt32 DoCommand(PRInt32 command, const unsigned long* indices, PRInt32 numIndices)  {return 0;}
	virtual void InterruptContext(PRBool safetoo)	{;}
	const char * GetHTMLPart()	{return NULL;}
	MWContext* GetContext() {return m_context;}
	MSG_Pane* FindPane(MWContext* context, PRInt32 type, PRBool contextMustMatch  = PR_FALSE) {return NULL;}
	const char * GetQuoteUrl()	{return NULL;}
	const char * GetQuoteHtmlPart()	{return NULL;}
	void SetQuoteHtmlPart(const char * urlString) {/*NYI*/;}
	void SetQuoteUrl(const char * urlString) {/*NYI*/;}
	MSG_Master* GetMaster() {return m_master;}
};

class MSG_FolderInfoMail
{
	void* unused;
};

const int MK_MSG_OUTBOX_L10N_NAME_OLD = 0;
#define QUEUE_FOLDER_NAME_OLD	MSG_GetSpecialFolderName(MK_MSG_OUTBOX_L10N_NAME_OLD)


#define NotifyCode					PRInt32

#define PREF_GetBoolPref(a, b)		(*b) = PR_FALSE
#define PREF_GetIntPref(a, b)		(*b) = 0
#define PREF_CopyCharPref(a, b)		/*NYI*/
#define PREF_SetBoolPref(a, b)		/*NYI*/
#define PREF_SetCharPref(a, b)		/*NYI*/
#define	PREF_SavePrefFile()			/*NYI*/


#define NET_parse_news_url(a, b, c, d, e, f)	-1

#define FE_Alert(a, b)				/*NYI*/

/* The three ways to deliver a message.
 */
typedef enum
{
  MSG_DeliverNow,
  MSG_QueueForLater,
  MSG_Save,
  MSG_SaveAs,
  MSG_SaveAsDraft,
  MSG_SaveAsTemplate
} MSG_Deliver_Mode;

#define msg_InterruptContext(a, b)		/*NYI*/
#define msg_generate_message_id()		NULL
#define	msg_StartMessageDeliveryWithAttachments(a,b,c,d,e,f,g,h,i,j,k,l)	/*NYI*/


#define AB_UserHasVCard()				PR_TRUE
#define AB_InsertOrUpdateABookEntry(a, b, c, d)	PR_TRUE
#define AB_LoadIdentityVCard(a)			0
#define AB_ExportVCardToTempFile(a, b)	0
const int kMaxFullNameLength 			= 256;
#define vCardMimeFormat					"text/x-vcard"
#define AB_GetHTMLCapability(a, b)		PR_TRUE

#define MSG_FORWARD_COOKIE  "$forward_quoted$"

#define EDT_PasteQuoteBegin(a, b)		PR_FALSE
#define EDT_PasteQuote(a, b)			/*NYI*/
#define EDT_PasteQuoteEnd(a)			/*NYI*/

#define msg_LineBuffer(a, b, c, d, e, f, g, h)		NULL

#define eOUT_OF_MEMORY			0xFF7F

#define XL_InitializeTextSetup(a)		/*NYI*/

#define FindFolderOfType(a)			NULL

#define XP_CopyDialogString(a, b, c)	/*NYI*/



/*QUESTION: can we remove #ifdef XP_OS2 ?*/
/*REMARK: as #define MOZ_NEWADDR, we should remove it when used*/

