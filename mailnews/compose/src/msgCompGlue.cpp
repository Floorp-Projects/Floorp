// All the definition here after are temporary and must be removed in the future when be defined somewhere else...

#include "rosetta_mailnews.h"
#include "nsMsgCompose.h"

#include "msgCore.h"

typedef void *MWContext;
class MSG_Pane;
typedef struct URL_Struct _URL_Struct;

void				FE_DestroyMailCompositionContext(MWContext*) {return;}
const char *		FE_UsersSignature() {return NULL;}
const char *		FE_UsersOrganization() {return NULL;}
const char *		FE_UsersFullName() {return NULL;}
const char *		FE_UsersMailAddress() {return NULL;}
void				FE_UpdateCompToolbar(MSG_Pane*) {return;}
void				FE_SetWindowLoading(MWContext *, URL_Struct *,Net_GetUrlExitFunc **) {return;}

PRBool				NET_AreThereActiveConnectionsForWindow(MWContext *) {return PR_FALSE;}
int					NET_SilentInterruptWindow(MWContext * window_id) {return 0;}
int					NET_ScanForURLs(MSG_Pane*, const char *, PRInt32,char *, int, PRBool) {return NULL;}
void				NET_FreeURLStruct (URL_Struct *) {return;}
URL_Struct *		NET_CreateURLStruct (const char *, NET_ReloadMethod) {return NULL;}
char *				NET_UnEscape (char * ) {return NULL;}
char *				NET_SACat  (char **, const char *) {return NULL;}
char *				NET_EscapeHTML(const char * string) {return NULL;}
char *				NET_ParseURL (const char *, int ) {return NULL;}
int					NET_URL_Type (const char *) {return NULL;}

int					XP_FileRemove(const char *, XP_FileType) {return 0;}
XP_FILE_URL_PATH	XP_PlatformFileToURL (const XP_FILE_NATIVE_PATH ) {return NULL;}
MWContext *			XP_FindContextOfType(MWContext *, MWContextType) {return NULL;}
char *				XP_StripLine (char *) {return NULL;}

const char *		MSG_GetSpecialFolderName(int ) {return NULL;}
const char *		MSG_GetQueueFolderName() {return NULL;}
MSG_Pane *			MSG_FindPane(MWContext* , MSG_PaneType ) {return NULL;}
char *				MSG_ExtractRFC822AddressMailboxes (const char *) {return NULL;}
int					MSG_ExplodeHeaderField(MSG_HEADER_SET,const char * ,MSG_HeaderEntry **) {return NULL;}
int					MSG_ParseRFC822Addresses (const char *,char **, char **) {return NULL;}
char *				MSG_MakeFullAddress (const char* , const char* ) {return NULL;}

void				INTL_DestroyCharCodeConverter(CCCDataObject) {return;}
unsigned char *		INTL_CallCharCodeConverter(CCCDataObject,const unsigned char *,PRInt32) {return NULL;}
int					INTL_GetCharCodeConverter(int16 ,int16 ,CCCDataObject) {return NULL;}
CCCDataObject		INTL_CreateCharCodeConverter() {return NULL;}
int16				INTL_GetCSIWinCSID(INTL_CharSetInfo) {return NULL;}
INTL_CharSetInfo	LO_GetDocumentCharacterSetInfo(MWContext *) {return NULL;}

XP_FILE_NATIVE_PATH WH_FileName (const char *, XP_FileType ) {return NULL;}
void				MIME_GetMessageCryptoState(MWContext *,PRBool *,PRBool *,PRBool *,PRBool *) {return;}
History_entry *		SHIST_GetCurrent(History *) {return NULL;}
int					MISC_ValidateReturnAddress (MWContext *,const char *) {return NULL;}

extern "C" {
int MK_MSG_SAVE_TEMPLATE;
int MK_MSG_INVALID_FOLLOWUP_TO_HEADER;
int MK_MSG_INVALID_NEWS_HEADER;
int	MK_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS;

void FE_MsgShowHeaders(MSG_Pane *, MSG_HEADER_SET) {return;}
HJ98703
}

