// All the definition here after are temporary and must be removed in the future when be defined somewhere else...


#define NS_IMPL_IDS
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIPref.h"
#include "nsIMimeConverter.h"
  
#include "rosetta_mailnews.h"
#include "nsMsgCompose.h"

#include "msgCore.h"


static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);

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
int16				INTL_DefaultWinCharSetID(MWContext *) {return 2;}
int16				INTL_DefaultMailCharSetID(int16 csid) {return 2;}
int16				INTL_DefaultNewsCharSetID(int16 csid) {return 2;}
void				INTL_MessageSendToNews(XP_Bool toNews) {return;}

//
// Begin international functions.
//

// Convert an unicode string to a C string with a given charset.
nsresult ConvertFromUnicode(const nsString aCharset, 
                            const nsString inString,
                            char** outCString)
{
  nsICharsetConverterManager * ccm = nsnull;
  nsresult res;

  res = nsServiceManager::GetService(kCharsetConverterManagerCID, 
                                     kICharsetConverterManagerIID, 
                                     (nsISupports**)&ccm);
  if(NS_SUCCEEDED(res) && (nsnull != ccm)) {
    nsIUnicodeEncoder* encoder = nsnull;

    // get an unicode converter
    res = ccm->GetUnicodeEncoder(&aCharset, &encoder);
    if(NS_SUCCEEDED(res) && (nsnull != encoder)) {
      const PRUnichar *unichars = inString.GetUnicode();
      PRInt32 unicharLength = inString.Length();
      PRInt32 dstLength;
      res = encoder->GetMaxLength(unichars, unicharLength, &dstLength);
      // allocale an output buffer
      *outCString = (char *) PR_Malloc(dstLength + 1);
      if (*outCString != nsnull) {
        // convert from unicode
        res = encoder->Convert(unichars, &unicharLength, *outCString, &dstLength);
        (*outCString)[dstLength] = '\0';
      }
      else {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      NS_IF_RELEASE(encoder);
    }    
    nsServiceManager::ReleaseService(kCharsetConverterManagerCID, ccm);
  }
  return res;
}

// Convert a C string to an unicode string.
nsresult ConvertToUnicode(const nsString aCharset, 
                          const char *inCString, 
                          nsString &outString)
{
  nsICharsetConverterManager * ccm = nsnull;
  nsresult res;

  res = nsServiceManager::GetService(kCharsetConverterManagerCID, 
                                     kICharsetConverterManagerIID, 
                                     (nsISupports**)&ccm);
  if(NS_SUCCEEDED(res) && (nsnull != ccm)) {
    nsIUnicodeDecoder* decoder = nsnull;
    PRUnichar *unichars;
    PRInt32 unicharLength;

    // get an unicode converter
    res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
    if(NS_SUCCEEDED(res) && (nsnull != decoder)) {
      PRInt32 srcLen = PL_strlen(inCString);
      res = decoder->Length(inCString, 0, srcLen, &unicharLength);
      // allocale an output buffer
      unichars = (PRUnichar *) PR_Malloc(unicharLength * sizeof(PRUnichar));
      if (unichars != nsnull) {
        // convert to unicode
        res = decoder->Convert(unichars, 0, &unicharLength, inCString, 0, &srcLen);
        outString.SetString(unichars, unicharLength);
      }
      else {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      NS_IF_RELEASE(decoder);
    }    
    nsServiceManager::ReleaseService(kCharsetConverterManagerCID, ccm);
  }  
  return res;
}

// Charset to be used for the internatl processing.
const char *msgCompHeaderInternalCharset()
{
  // UTF-8 is a super set of us-ascii. 
  // We can use the same string manipulation methods as us-ascii without breaking non us-ascii characters. 
  return "UTF-8";
}

// MIME encoder, output string should be freed by PR_FREE
char * INTL_EncodeMimePartIIStr(const char *header, const char *charset, PRBool bUseMime) 
{
  nsString aCharset(msgCompHeaderInternalCharset());
  nsString aString;
  char *outCString = (char *) header; // initialize
  nsresult res;
#if 0 // Enable this when appcore hooks up the converter.
  // utf-8 to ucs2 this conversion is inexpensive.
  res = ConvertToUnicode(aCharset, header, aString);
  if (NS_SUCCEEDED(res)) {
    aCharset.SetString(charset);
    // Convert to the mail charset
    res = ConvertFromUnicode(aCharset, aString, &outCString);
  }
#endif

  // No MIME, just duplicate the string.
  if (PR_FALSE == bUseMime) {
    return PL_strdup(header);
  }

  char *encodedString = nsnull;
  nsIMimeConverter *converter;
  res = nsComponentManager::CreateInstance(kCMimeConverterCID, nsnull, 
                                           nsIMimeConverter::GetIID(), (void **)&converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) {
    res = converter->EncodeMimePartIIStr(outCString, charset, kMIME_ENCODED_WORD_SIZE, &encodedString);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? encodedString : nsnull;
}

// Get a default mail character set.
char * INTL_GetDefaultMailCharset()
{
  char * retVal = nsnull;
	nsIPref* prefs;
	nsresult res = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
  if (nsnull != prefs && NS_SUCCEEDED(res))
	{
	  char prefValue[kMAX_CSNAME+1];
  	PRInt32 prefLength = kMAX_CSNAME;
		
    prefs->Startup("prefs.js");
		res = prefs->GetCharPref("intl.charactesr_set_name", prefValue, &prefLength);
    if (NS_SUCCEEDED(res) && prefLength > 0) {
      //TODO: map to mail charset (e.g. Shift_JIS -> ISO-2022-JP) bug#3941.
			retVal = PL_strdup(prefValue);
    }
    else {
			retVal = PL_strdup("us-ascii");
    }
		
		nsServiceManager::ReleaseService(kPrefCID, prefs);
	}

  return (nsnull != retVal) ? retVal : PL_strdup("us-ascii");
}

// Return True if a charset is stateful (e.g. JIS).
PRBool INTL_stateful_charset(const char *charset)
{
  //TODO: use charset manager's service
  return (PL_strcasecmp(charset, "iso-2022-jp") == 0);
}

// Obsolescent
int					INTL_IsLeadByte(int charSetID,unsigned char ch) {return 0;}
// Obsolescent
CCCDataObject		INTL_CreateDocToMailConverter(iDocumentContext context, XP_Bool isHTML, unsigned char *buffer,uint32 buffer_size) {return NULL;}
// Obsolescent
char *				INTL_GetAcceptLanguage() {return "en";}
// Obsolescent
void				INTL_CharSetIDToName(int16 charSetID,char *charset_return) {PL_strcpy (charset_return,"us-ascii"); return;}

//
// End international functions.
//

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
XP_Bool				isMacFile(char* filename) {return PR_FALSE;}

HJ10196
History_entry *		SHIST_GetCurrent(History *) {return NULL;}
int					MISC_ValidateReturnAddress (MWContext *,const char *) {return nsnull;}
char *				msg_MagicFolderName(MSG_Prefs* prefs, uint32 flag, int *pStatus) {return NULL;}

time_t 				GetTimeMac()	{return 0;}


extern "C" {

void FE_MsgShowHeaders(MSG_Pane *, MSG_HEADER_SET) {return;}
HJ98703
}

#include "allxpstr.h"
extern "C" {
        int MK_MSG_SAVE_TEMPLATE = XP_MSG_BASE + 1389;
        int MK_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS = XP_MSG_BASE + 1455;
        int MK_MSG_FAILED_COPY_OPERATION = XP_MSG_BASE + 0;
        int MK_MSG_INVALID_FOLLOWUP_TO_HEADER = XP_MSG_BASE + 0;
        int MK_MSG_INVALID_NEWS_HEADER = XP_MSG_BASE + 0;
}
