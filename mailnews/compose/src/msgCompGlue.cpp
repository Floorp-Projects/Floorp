// All the definition here after are temporary and must be removed in the future when be defined somewhere else...

#include "rosetta_mailnews.h"
#include "nsMsgCompose.h"

#include "msgCore.h"

class MSG_Pane;

void				FE_DestroyMailCompositionContext(MWContext*) {return;}
const char *		FE_UsersSignature() {return NULL;}
void				FE_UpdateCompToolbar(MSG_Pane*) {return;}
void				FE_SetWindowLoading(MWContext *, URL_Struct *,Net_GetUrlExitFunc **) {return;}

XP_Bool				NET_AreThereActiveConnectionsForWindow(MWContext *) {return PR_FALSE;}
int					NET_SilentInterruptWindow(MWContext * window_id) {return 0;}
int					NET_ScanForURLs(MSG_Pane*, const char *, PRInt32,char *, int, PRBool) {return nsnull;}
void				NET_FreeURLStruct (URL_Struct *) {return;}
URL_Struct *		NET_CreateURLStruct (const char *, NET_ReloadMethod) {return NULL;}
char *				NET_UnEscape (char * ) {return NULL;}
char *				NET_EscapeHTML(const char * string) {return NULL;}
char *				NET_ParseURL (const char *, int ) {return NULL;}
int					NET_URL_Type (const char *) {return nsnull;}
XP_Bool				NET_IsLocalFileURL(char *address) {return PR_TRUE;}
char *				NET_Escape (const char * str, int mask) {return PL_strdup(str);}
int					NET_InterruptWindow(MWContext * window_id) {return 0;}
XP_Bool				NET_IsOffline() {return PR_FALSE;}
//char *				NET_ExplainErrorDetails (int code, ...) {return NULL;}
char*				NET_ScanHTMLForURLs(const char* input) {return NULL;}

int					XP_FileRemove(const char *, XP_FileType) {return 0;}
XP_FILE_URL_PATH	XP_PlatformFileToURL (const XP_FILE_NATIVE_PATH ) {return NULL;}
MWContext *			XP_FindContextOfType(MWContext *, MWContextType) {return NULL;}
char *				XP_StripLine (char *) {return NULL;}
XP_File				XP_FileOpen (const char* name, XP_FileType type, const XP_FilePerm permissions) {return NULL;}
int					XP_Stat(const char * name, XP_StatStruct * outStat, XP_FileType type) {return 0;}
int					XP_FileTruncate(const char* name, XP_FileType type, int32 length) {return 0;}
Bool				XP_IsContextBusy(MWContext * context) {return PR_FALSE;}

const char *		MSG_GetSpecialFolderName(int ) {return NULL;}
const char *		MSG_GetQueueFolderName() {return NULL;}
MSG_Pane *			MSG_FindPane(MWContext* , MSG_PaneType ) {return NULL;}
int					MSG_ExplodeHeaderField(MSG_HEADER_SET,const char * ,MSG_HeaderEntry **) {return nsnull;}
char *				MSG_MakeFullAddress (const char* , const char* ) {return NULL;}
void				MSG_MailCompositionAllConnectionsComplete (MSG_Pane* /*pane*/) {return;}


void				INTL_DestroyCharCodeConverter(CCCDataObject) {return;}
unsigned char *		INTL_CallCharCodeConverter(CCCDataObject,const unsigned char *,int32) {return NULL;}
int					INTL_GetCharCodeConverter(int16 ,int16 ,CCCDataObject) {return nsnull;}
CCCDataObject		INTL_CreateCharCodeConverter() {return NULL;}
int16				INTL_GetCSIWinCSID(INTL_CharSetInfo) {return 2;}
INTL_CharSetInfo	LO_GetDocumentCharacterSetInfo(MWContext *) {return NULL;}
int16				INTL_GetCSIDocCSID(INTL_CharSetInfo obj) {return 2;}
int16				INTL_DefaultMailCharSetID(int16 csid) {return 2;}
int16				INTL_DefaultNewsCharSetID(int16 csid) {return 2;}
void				INTL_MessageSendToNews(XP_Bool toNews) {return;}
char *				INTL_EncodeMimePartIIStr(char *header, int16 wincsid, XP_Bool bUseMime) {return NULL;}
int					INTL_IsLeadByte(int charSetID,unsigned char ch) {return 0;}
CCCDataObject		INTL_CreateDocToMailConverter(iDocumentContext context, XP_Bool isHTML, unsigned char *buffer,uint32 buffer_size) {return NULL;}
char *				INTL_GetAcceptLanguage() {return "en";}
void				INTL_CharSetIDToName(int16 charSetID,char *charset_return) {PL_strcpy (charset_return,"us-ascii"); return;}

MimeEncoderData *	MimeB64EncoderInit(int (*output_fn) (const char *buf, int32 size, void *closure), void *closure) {return NULL;}
MimeEncoderData *	MimeQPEncoderInit (int (*output_fn) (const char *buf, int32 size, void *closure), void *closure) {return NULL;}
MimeEncoderData *	MimeUUEncoderInit (char *filename, int (*output_fn) (const char *buf, int32 size, void *closure), void *closure) {return NULL;}
int					MimeEncoderDestroy(MimeEncoderData *data, XP_Bool abort_p) {return 0;}
int					MimeEncoderWrite (MimeEncoderData *data, const char *buffer, int32 size) {return 0;}
char *				MimeGuessURLContentName(MWContext *context, const char *url) {return NULL;}
void				MIME_GetMessageCryptoState(MWContext *,PRBool *,PRBool *,PRBool *,PRBool *) {return;}

int					strcasecomp  (const char *, const char *) {return 0;}
int					strncasecomp (const char *, const char *, int ) {return nsnull;}
char *				strcasestr (const char * str, const char * substr) {return NULL;}

XP_FILE_NATIVE_PATH WH_FileName (const char *, XP_FileType ) {return NULL;}

HJ10196
History_entry *		SHIST_GetCurrent(History *) {return NULL;}
int					MISC_ValidateReturnAddress (MWContext *,const char *) {return nsnull;}
char *				msg_MagicFolderName(MSG_Prefs* prefs, uint32 flag, int *pStatus) {return NULL;}

extern "C" {
int MK_MSG_SAVE_TEMPLATE;
int MK_MSG_INVALID_FOLLOWUP_TO_HEADER;
int MK_MSG_INVALID_NEWS_HEADER;
int	MK_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS;
int MK_MSG_FAILED_COPY_OPERATION;

void FE_MsgShowHeaders(MSG_Pane *, MSG_HEADER_SET) {return;}
HJ98703
}


