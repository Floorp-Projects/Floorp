//
// All the definition here after are temporary and must be removed in the future when 
// be defined somewhere else...
//

#define NS_IMPL_IDS
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIPref.h"
#include "nsIMimeConverter.h"
#include "msgCore.h"
#include "rosetta_mailnews.h"

#include "nsMsgTransition.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);

class MSG_Pane;

void				FE_DestroyMailCompositionContext(MWContext*) {return;}
const char  *FE_UsersSignature() {return NULL;}
void				FE_UpdateCompToolbar(MSG_Pane*) {return;}
void				FE_SetWindowLoading(MWContext *, URL_Struct *,Net_GetUrlExitFunc **) {return;}
XP_Bool			NET_AreThereActiveConnectionsForWindow(MWContext *) {return PR_FALSE;}
int					NET_SilentInterruptWindow(MWContext * window_id) {return 0;}
void				NET_FreeURLStruct (URL_Struct *) {return;}
URL_Struct  *NET_CreateURLStruct (const char *, NET_ReloadMethod) {return NULL;}
char *			NET_ParseURL (const char *, int ) {return NULL;}
int					NET_URL_Type (const char *) {return nsnull;}
XP_Bool			NET_IsLocalFileURL(char *address) {return PR_TRUE;}
int					NET_InterruptWindow(MWContext * window_id) {return 0;}
XP_Bool			NET_IsOffline() {return PR_FALSE;}

XP_FILE_URL_PATH	XP_PlatformFileToURL (const XP_FILE_NATIVE_PATH ) {return NULL;}
MWContext   *XP_FindContextOfType(MWContext *, MWContextType) {return NULL;}
XP_FILE_NATIVE_PATH WH_FileName (const char *, XP_FileType ) {return NULL;}


int					XP_Stat(const char * name, XP_StatStruct * outStat, XP_FileType type) {return 0;}
int					XP_FileTruncate(const char* name, XP_FileType type, int32 length) {return 0;}
Bool				XP_IsContextBusy(MWContext * context) {return PR_FALSE;}

const char *		MSG_GetSpecialFolderName(int ) {return NULL;}
const char *		MSG_GetQueueFolderName() {return NULL;}
MSG_Pane *			MSG_FindPane(MWContext* , MSG_PaneType ) {return NULL;}
int					MSG_ExplodeHeaderField(MSG_HEADER_SET,const char * ,MSG_HeaderEntry **) {return nsnull;}
char *				MSG_MakeFullAddress (const char* , const char* ) {return NULL;}
void				MSG_MailCompositionAllConnectionsComplete (MSG_Pane* /*pane*/) {return;}

char        *MimeGuessURLContentName(MWContext *context, const char *url) {return NULL;}
void				MIME_GetMessageCryptoState(MWContext *,PRBool *,PRBool *,PRBool *,PRBool *) {return;}

XP_Bool			isMacFile(char* filename) {return PR_FALSE;}

HJ10196
History_entry *		SHIST_GetCurrent(History *) {return NULL;}
int					MISC_ValidateReturnAddress (MWContext *,const char *) {return nsnull;}
char        *msg_MagicFolderName(MSG_Prefs* prefs, uint32 flag, int *pStatus) {return NULL;}

//time_t 			GetTimeMac()	{return 0;}

extern "C" {
  void FE_MsgShowHeaders(MSG_Pane *, MSG_HEADER_SET) {return;}
  HJ98703
}

