// All the definition here after are temporary and must be removed in the future when be defined somewhere else...

#ifndef _MsgCompGlue_H_
#define _MsgCompGlue_H_

#include "mimeenc.h"
#include "xpgetstr.h"
#include "xp_qsort.h"
#include "msgcom.h"
#include "rosetta_mailnews.h"


// These are some defines that we used to get out of allxpstr. But we can't do that anymore...

#define MK_SMTP_SERVER_ERROR		-234
#define MK_TCP_READ_ERROR			-252
#define MK_COULD_NOT_LOGIN_TO_SMTP_SERVER -229
#define MK_COULD_NOT_GET_USERS_MAIL_ADDRESS -235
#define MK_POP3_PASSWORD_UNDEFINED	-313
#define MK_ERROR_SENDING_FROM_COMMAND -230
#define MK_ERROR_SENDING_RCPT_COMMAND -231
#define MK_ERROR_SENDING_DATA_COMMAND -232
#define MK_ERROR_SENDING_MESSAGE	  -233
#define MK_MIME_NO_RECIPIENTS		  -267
#define MK_OUT_OF_MEMORY			  -207
#define MK_MSG_ASSEMBLING_MSG			 1
#define MK_MSG_ASSEMB_DONE_MSG			 1
#define MK_MSG_LOAD_ATTACHMNT			 1
#define MK_MSG_LOAD_ATTACHMNTS			 1
#define MK_MSG_DELIV_MAIL				 1
#define MK_MSG_DELIV_MAIL_DONE			 1
#define MK_MSG_DELIV_NEWS				 1
#define MK_MSG_DELIV_NEWS_DONE			 1
#define MK_MSG_QUEUEING					 1
#define MK_MSG_WRITING_TO_FCC			 1
#define MK_MSG_QUEUED					 1
#define MK_MIME_ERROR_WRITING_FILE		 1
#define MK_MIME_MULTIPART_BLURB			 1
#define MK_MIME_NO_SENDER				 1
#define MK_MSG_COULDNT_OPEN_FCC_FILE	 1
#define MK_UNABLE_TO_OPEN_TMP_FILE	  -253
#define MK_MSG_CANT_CREATE_FOLDER	  -404
#define MK_MSG_SAVE_DRAFT			     1
#define MK_ADDR_BOOK_CARD			   882
#define MK_MSG_MAC_PROMPT_UUENCODE		 1
#define MK_MSG_SAVING_AS_DRAFT			 1
#define MK_MSG_SAVING_AS_TEMPLATE	     1
#define MK_MSG_UNABLE_TO_SAVE_DRAFT      1
#define MK_MSG_UNABLE_TO_SAVE_TEMPLATE   1
#define MK_UNABLE_TO_OPEN_FILE           1
#define MK_IMAP_UNABLE_TO_SAVE_MESSAGE   1
#define MK_IMAP_NO_ONLINE_FOLDER         1
#define MK_MSG_FAILED_COPY_OPERATION     1
#define MK_MSG_INVALID_NEWS_HEADER		 1
#define MK_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS 1

#define MK_MSG_MSG_COMPOSITION			1
#define MK_COMMUNICATIONS_ERROR			1
#define MK_MSG_EMPTY_MESSAGE			1
#define MK_MSG_DOUBLE_INCLUDE			1
#define MK_MSG_WHY_QUEUE_SPECIAL		1
#define MK_MSG_WHY_QUEUE_SPECIAL_OLD	1
#define MK_MSG_NOT_AS_SENT_FOLDER		1
#define MK_MSG_MISSING_SUBJECT			1
#define MK_MSG_SEND						1
#define MK_MSG_SEND_LATER				1
#define MK_MSG_ATTACH_ETC				1
#define MK_MSG_QUOTE_MESSAGE			1
#define MK_MSG_FROM						1
#define MK_MSG_REPLY_TO					1
#define MK_MSG_MAIL_TO					1
#define MK_MSG_MAIL_CC					1
#define MK_MSG_MAIL_BCC					1
#define MK_MSG_FILE_CC					1
#define MK_MSG_POST_TO					1
#define MK_MSG_FOLLOWUPS_TO				1
#define MK_MSG_SUBJECT					1
#define MK_MSG_ATTACHMENT				1
#define MK_MSG_ATTACH_AS_TEXT			1
#define MK_MSG_SAVE_DRAFT				1
#define MK_MSG_SAVE_TEMPLATE			1
#define MK_MSG_ASK_HTML_MAIL			1
#define MK_MSG_ASK_HTML_MAIL_TITLE		1
#define MK_MSG_HTML_RECIPIENTS			1
#define MK_MSG_HTML_RECIPIENTS_TITLE	1
#define MK_MSG_EVERYONE					1
#define MK_MSG_ENTER_NAME_FOR_TEMPLATE	1
#define MK_MSG_INVALID_NEWS_HEADER		1
#define MK_MSG_INVALID_FOLLOWUP_TO_HEADER	1
#define MK_MSG_OUTBOX_L10N_NAME_OLD		1
#define MK_MSG_SAVE_TEMPLATE 1
#define MK_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS 1
#define MK_MSG_FAILED_COPY_OPERATION		1
#define MK_MSG_INVALID_NEWS_HEADER 1

class MailMessageHdr
{
public:
	MailMessageHdr() {return;}
	~MailMessageHdr() {return;}
	uint32		OrFlags(uint32 flags) {return 0;}
	void		SetMessageSize(uint32 messageSize) {return;}
	PRBool		SetMessageKey(char* articleNumber) {return PR_FALSE;}
};

class MailDB
{
public:
	static nsresult			Open(const char * dbName, PRBool create, 
								MailDB** pMessageDB,
								PRBool upgrading = PR_FALSE) {return NS_ERROR_NOT_IMPLEMENTED;}
	nsresult Close(void) {return 0;}
	virtual nsresult		AddHdrToDB(void *newHdr, PRBool *newThread, PRBool notify = PR_FALSE) {return PR_FALSE;}
	unsigned long GetUnusedFakeId() {return 0;}
};

class ParseOutgoingMessage
{
public:
	ParseOutgoingMessage() {return;}
	virtual ~ParseOutgoingMessage() {return;}
	void			SetOutFile(XP_File out_file) {m_out_file = out_file;}
	XP_File			GetOutFile() {return m_out_file;}
	void			SetWriteMozillaStatus(XP_Bool writeMozStatus) 
						{m_writeMozillaStatus = writeMozStatus;}
	virtual int		StartNewEnvelope(const char *line, uint32 lineLength) {return 0;}
	virtual void	FinishHeader() {return;}
	virtual int32	ParseFolderLine(const char *line, uint32 lineLength) {return 0;}
	virtual int32	ParseBlock(const char *block, uint32 lineLength) {return 0;}
	virtual void	Clear() {return;}
	void			AdvanceOutPosition(uint32 amountToAdvance) {return;}
	void			SetWriteToOutFile(XP_Bool writeToOutFile) {m_writeToOutFile = writeToOutFile;}

	void			FlushOutputBuffer() {return;}
	void			SetMailDB(MailDB *mailDB) {return;}
	MailDB *		GetMailDB() {return NULL;}
	void			Init(uint32 fileposition) {return;}
	uint32			m_bytes_written;
	MailMessageHdr *m_newMsgHdr;		/* current message header we're building */
protected:
	static int32	LineBufferCallback(char *line, uint32 lineLength, void *closure);
	XP_Bool			m_wroteXMozillaStatus;
	XP_Bool			m_writeMozillaStatus;
	XP_Bool			m_writeToOutFile;
	XP_Bool			m_lastBodyLineEmpty;
	XP_File			m_out_file;
	uint32			m_ouputBufferSize;
	char			*m_outputBuffer;
	uint32			m_outputBufferIndex;
};

class MSG_ZapIt
{
	void* unused;
};

class MSG_Prefs : public MSG_ZapIt 
{
public:
	PRBool         GetDefaultBccSelf(PRBool newsBcc) {return PR_FALSE;}
	const char *    GetDefaultHeaderContents(MSG_HEADER_SET header) {return NULL;}
	char *          MagicFolderName(uint32 flag, int *pStatus = 0) {return NULL;}
	PRBool         GetAutoQuoteReply() {return PR_FALSE;}
	PRBool         GetMailServerIsIMAP4() {return PR_FALSE;}
};

class MSG_FolderInfoMail
{
public:
	char * GetPathname() {return "";}
};

class MSG_IMAPFolderInfoMail : public MSG_FolderInfoMail
{ 
public:
	virtual MSG_IMAPFolderInfoMail	*GetIMAPFolderInfoMail() {return NULL;}
};

class MSG_PostDeliveryActionInfo : public MSG_ZapIt
{
public:
	MSG_PostDeliveryActionInfo(MSG_FolderInfo *folderInfo) {;}

  MSG_FolderInfo *m_folderInfo;
//  XPDWordArray m_msgKeyArray;		/* origianl message keyArray */
  uint32 m_flags;
	char *          MagicFolderName(PRUint32 flag, int *pStatus = 0) {return NULL;}
	PRBool         GetAutoQuoteReply() {return PR_FALSE;}
};

class TIMAPNamespace
{

public:
	void *	unsused;
};

class MSG_Master
{
public:
	MSG_Prefs* GetPrefs() {return NULL;}
    PRInt32 GetFolderTree(void) {return 0;}
	HJ97882
	PRBool	IsUserAuthenticated() {return PR_FALSE;}
	//	msg_HostTable* GetHostTable() {return m_hosttable;}
	MSG_FolderInfoMail *FindMailFolder(const char* pathname, PRBool createIfMissing) {return NULL;}
	MSG_IMAPFolderInfoMail *FindImapMailFolder(const char *hostName, const char* onlineServerName, const char *owner, PRBool createIfMissing) {return NULL;}
	MSG_IMAPHost *GetIMAPHost(const char *hostName) {return NULL;}
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
	virtual MSG_PaneType GetPaneType() {return MSG_PANE;}
	void SetRequestForReturnReceipt(PRBool isNeeded) {return;}
	MSG_PostDeliveryActionInfo *GetPostDeliveryActionInfo () {return NULL;}
	void SetPostDeliveryActionInfo(MSG_PostDeliveryActionInfo *) {;}
	virtual void SetIMAPListMailbox(const char *name) {;}
	virtual XP_Bool IMAPListMailboxExist() {return PR_FALSE;}
};

#define QUEUE_FOLDER_NAME_OLD	MSG_GetSpecialFolderName(MK_MSG_OUTBOX_L10N_NAME_OLD)


#define NotifyCode					PRInt32

#define PREF_GetBoolPref(a, b)		(*b) = PR_FALSE
#define PREF_GetIntPref(a, b)		(*b) = 0
#define PREF_CopyCharPref(a, b)		/*NYI*/
#define PREF_SetBoolPref(a, b)		/*NYI*/
#define PREF_SetCharPref(a, b)		/*NYI*/
#define	PREF_SavePrefFile()			/*NYI*/
#define PREF_GetCharPref(a, b, c)	/*NIY*/


#define NET_parse_news_url(a, b, c, d, e, f)	-1

#define FE_Alert(a, b)				printf("ALERT: %s", b)

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
#define msg_GetDummyEnvelope()	NULL
#define msg_IsSummaryValid(a, b)		PR_FALSE
#define msg_ConfirmMailFile(a, b)		PR_FALSE
#define msg_SetSummaryValid(a, b, c)	/*NYI*/

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

#define MAX_WRITE_READY (((unsigned) (~0) << 1) >> 1)   /* must be <= than MAXINT!!!!! */

#define APPLICATION_DIRECTORY				"application/directory" /* text/x-vcard is synonym */

extern char *msg_MagicFolderName(MSG_Prefs* prefs, uint32 flag, int *pStatus);

#define	kIsRead			0x00000001

#define MIME_MakeFromField(a)  PL_strdup("testmsg@netscape.com")

#define DRAFTS_FOLDER_NAME  "Draft"
#define SENT_FOLDER_NAME	"Sent"
#define TEMPLATES_FOLDER_NAME	"Template"

#define CreateIMAPSubscribeMailboxURL(a, b, c) ""
#define IMAPNS_GenerateFullFolderNameWithDefaultNamespace(a, b, c, d, e)	NULL
#define IMAPNS_GetDelimiterForNamespace(a)	'/'
#define CreateImapMailboxCreateUrl(a, b,c)	NULL

#define MSG_CommandType		PRUint32

#undef FE_Progress
#define FE_Progress(cx,msg) printf("Progress: %s\n", msg)

extern int MISC_ValidateReturnAddress (MWContext *,const char *);
extern History_entry * SHIST_GetCurrent(History *);



/*QUESTION: can we remove #ifdef XP_OS2 ?*/
/*REMARK: as #define MOZ_NEWADDR is always true, we should remove it when used*/

#endif //_MsgCompGlue_H_
