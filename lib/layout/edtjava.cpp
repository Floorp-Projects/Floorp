/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


//
// Java-related editor code.
//

#ifdef EDITOR

#include "editor.h"

// Local header
#ifdef EDITOR_JAVA

#include "java.h"
#include "libi18n.h"
#include "xp_file.h"

#define JRI_NO_CPLUSPLUS
#ifdef XP_MAC
#include "n_p_composer_PluginManager.h"
#include "n_plugin_composer_Composer.h"
#include "n_p_composer_MozillaCallback.h"
#else
#include "netscape_plugin_composer_PluginManager.h"
#include "netscape_plugin_composer_Composer.h"
#include "netscape_plugin_composer_MozillaCallback.h"
#endif


// This C header file doesn't have C++ linkage #ifdefs.
extern "C" {
#include "zip.h"
};

struct netscape_plugin_composer_PluginManager;
#include "jri.h"

// Interface for calling editor plugins
#define PLUGIN_FAIL 0
#define PLUGIN_CANCEL 1
#define PLUGIN_OK 2
#define PLUGIN_NEWTEXT 3

typedef void (*ComposerPluginCallbackFn)(jint, jint, struct java_lang_Object*);
void EDT_ComposerPluginCallback(jint, jint, struct java_lang_Object*);

#ifdef XP_WIN16
const int32 MAX_STRING=65530L;
#else
// const int32 MAX_STRING=500; // For testing
const int32 MAX_STRING=1L<<30;
#endif

class CEditorPluginInterface {
public:
    static XP_Bool IsValidInterface(CEditorPluginInterface* pInterface);

    CEditorPluginInterface(MWContext* pContext, ComposerPluginCallbackFn callback);
    ~CEditorPluginInterface();

    static void Register(char* plugin);
    static int32 GetNumberOfCategories();
    static int32 GetNumberOfPlugins(int32 type);
    static char* GetCategoryName(int32 type);
    static char* GetPluginName(int32 type, int32 index);
    static char* GetPluginHint(int32 type, int32 index);
    
    static int32 GetNumberOfEncoders();
    static char* GetEncoderName(int32 index);
    static XP_Bool GetEncoderNeedsQuantizedSource(int32 index);
    static char* GetEncoderHint(int32 index);
    static char* GetEncoderFileExtension(int32 index);
    static char* GetEncoderFileType(int32 index);
    XP_Bool StartEncoder(int32 index, int32 width, int32 height, char** pixels,
        char* fileName, EDT_ImageEncoderCallbackFn doneFunction, void* doneFunctionArgument);

    XP_Bool Perform(int32 type, int32 index, EDT_ImageEncoderCallbackFn doneFunction, void* hook,netscape_javascript_JSObject *jsobject);
    XP_Bool Perform(char* pClassName, EDT_ImageEncoderCallbackFn doneFunction, void* hook,netscape_javascript_JSObject *jsobject);
    void Perform(char* pEvent, char* pDocURL, XP_Bool bCanChangeDocument, XP_Bool bCanCancel,
        EDT_ImageEncoderCallbackFn doneFunction, void* hook,netscape_javascript_JSObject *jsobject);
    XP_Bool IsPluginActive();
    void StopPlugin();
    void PluginCallback(int32 action, struct java_lang_Object* pArgument);
    static XP_Bool PluginsExist();

protected:
    struct java_lang_String* GetBaseURL();
    struct java_lang_String* GetDocument();
    void GetWorkDirectoryAndURL(struct java_lang_String*& workDirectory,
        struct java_lang_String*& workDirectoryURL);
    XP_Bool Perform2(char* pClassName, int32 type, int32 index, XP_Bool bCanChangeDocument, XP_Bool bCanCancel,
        EDT_ImageEncoderCallbackFn doneFunction, void* hook, netscape_javascript_JSObject *jsobject);
    void PluginFailed(struct java_lang_String* pWhy);
    void PluginSucceeded();
    void NewText(struct java_lang_String* pText);

    CEditBuffer* GetBuffer();

private:
    void ImageEncoderFinished(EDT_ImageEncoderStatus status);
    void DocumentIsTooLarge(int32 docLengthInBytes, int msgid);
    static netscape_plugin_composer_PluginManager* Manager();
    struct netscape_plugin_composer_Composer* Composer();
    static JRIEnv* Env();
    static XP_Bool g_bJavaInitialized;
    static JRIEnv* g_pEnv;
    static JRIGlobalRef g_pManager;
    static TXP_GrowableArray_pChar g_Plugin;
    static TXP_GrowableArray_CEditorPluginInterface g_Interface;
    XP_Bool m_bPluginActive; // True if an image encoder or a plugin is active.
    XP_Bool m_bImageEncoder; // True if we're exectuing an image encoder.
    XP_Bool m_bCanChangeDocument;
    XP_Bool m_bCanCancel;
    XP_Bool m_bChangedDocument; // True if the current plug-in has changed the document.
    JRIGlobalRef m_pComposer;
    EDT_ImageEncoderCallbackFn m_doneFunction;
    void* m_doneFunctionArgument;
    MWContext* m_pContext;
    ComposerPluginCallbackFn m_callback;
};

#endif // EDITOR_JAVA


// End of local header


// Editor Plugin edt.h API functions.

void EDT_RegisterPlugin(char* csFileSpec){
#ifdef EDITOR_JAVA
    CEditorPluginInterface::Register(csFileSpec);
#else
    XP_TRACE(("EDT_RegisterPlugin(\"%s\")"));
#endif
}

int32 EDT_NumberOfPluginCategories(){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetNumberOfCategories();
    }
#endif
    return 0;
}

int32 EDT_NumberOfPlugins(int32 category){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetNumberOfPlugins(category);
    }
#endif
    return 0;
}

char* EDT_GetPluginCategoryName(int32 pluginIndex){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetCategoryName(pluginIndex);
    }
#endif
    return 0;
}

char* EDT_GetPluginName(int32 category, int32 index){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetPluginName(category, index);
    }
#endif
    return 0;
}

char* EDT_GetPluginMenuHelp(int32 category, int32 index){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetPluginHint(category, index);
    }
#endif
    return 0;
}

XP_Bool EDT_PerformPlugin(MWContext *pContext, int32 category, int32 index, EDT_ImageEncoderCallbackFn doneFunction, void* hook){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        XP_Bool saveJS = pContext->forceJSEnabled;
        pContext->forceJSEnabled = TRUE;
        netscape_javascript_JSObject *t_object=LJ_GetMochaWindow(pContext);
        XP_Bool bReturn = ((CEditorPluginInterface*) pEditBuffer->GetPlugins())->Perform(category, index, doneFunction, hook, t_object);
        pContext->forceJSEnabled = saveJS;
        return bReturn;
    }
#endif
    return FALSE;
}

XP_Bool EDT_PerformPluginByClassName(MWContext *pContext, char* pClassName, EDT_ImageEncoderCallbackFn doneFunction, void* hook){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) FALSE;
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        XP_Bool saveJS = pContext->forceJSEnabled;
        pContext->forceJSEnabled = TRUE;
        netscape_javascript_JSObject *t_object=LJ_GetMochaWindow(pContext);
        XP_Bool bReturn = ((CEditorPluginInterface*) pEditBuffer->GetPlugins())->Perform(pClassName, doneFunction, hook, t_object);
        pContext->forceJSEnabled = saveJS;
        return bReturn;
    }
#endif
    return FALSE;
}

void EDT_PerformEvent(MWContext *pContext, char* pEvent, char* pDocURL, XP_Bool bCanChangeDocument, XP_Bool bCanCancel,
                      EDT_ImageEncoderCallbackFn doneFunction, void* hook){
    GET_WRITABLE_EDIT_BUF_OR_RETURN(pContext, pEditBuffer);
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        // We must force JavaScript on with this, else we fail to
        //   get t_object because JS is normally OFF for editor
        XP_Bool saveJS = pContext->forceJSEnabled;
        pContext->forceJSEnabled = TRUE;
        netscape_javascript_JSObject *t_object=LJ_GetMochaWindow(pContext);
        ((CEditorPluginInterface*) pEditBuffer->GetPlugins())->Perform(pEvent, pDocURL, bCanChangeDocument, bCanCancel,
            doneFunction, hook, t_object);
        pContext->forceJSEnabled = saveJS;
        return;
    }
#endif
    if ( doneFunction){
        (*doneFunction)(ED_IMAGE_ENCODER_OK,  hook);
    }
}

XP_Bool EDT_IsPluginActive(MWContext* pContext){
    GET_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer) FALSE;
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return ((CEditorPluginInterface*) pEditBuffer->GetPlugins())->IsPluginActive();
    }
#endif
    return FALSE;
}

void EDT_StopPlugin(MWContext* pContext){
    GET_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer);
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        ((CEditorPluginInterface*) pEditBuffer->GetPlugins())->StopPlugin();
    }
#endif
}

#ifdef EDITOR_JAVA
void EDT_ComposerPluginCallback(jint hook1, jint action1, struct java_lang_Object* pArg){
    CEditorPluginInterface* pInterface = (CEditorPluginInterface*) hook1;
    if ( ! CEditorPluginInterface::IsValidInterface(pInterface )) {
        XP_ASSERT(FALSE); // A spurrious callback.
        return;
    }
    int32 action = (int32) action1;
    if ( pInterface ) {
        pInterface->PluginCallback(action, pArg);
    }
}
#endif

int32 EDT_NumberOfEncoders(){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetNumberOfEncoders();
    }
#endif
    return 0;

}

char* EDT_GetEncoderName(int32 index){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetEncoderName(index);
    }
#endif
    return 0;
}

char* EDT_GetEncoderFileExtension(int32 index){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetEncoderFileExtension(index);
    }
#endif
    return 0;
}

char* EDT_GetEncoderFileType(int32 index){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetEncoderFileType(index);
    }
#endif
    return 0;
}

char* EDT_GetEncoderMenuHelp(int32 index){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetEncoderHint(index);
    }
#endif
    return 0;
}

XP_Bool EDT_GetEncoderNeedsQuantizedSource(int32 index){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        return CEditorPluginInterface::GetEncoderNeedsQuantizedSource(index);
    }
#endif
    return FALSE;
}


XP_Bool EDT_StartEncoder(MWContext* pContext, int32 index, int32 width, int32 height, char** pixels,
    char* csFileName, EDT_ImageEncoderCallbackFn doneFunction,
    void* doneArgument){
#ifdef EDITOR_JAVA
    GET_EDIT_BUF_OR_RETURN_READY_OR_NOT(pContext, pEditBuffer) FALSE;
    if ( EditorPluginManager_PluginsExist() ) {
        return ((CEditorPluginInterface*) pEditBuffer->GetPlugins())->StartEncoder(index, width, height, pixels,
            csFileName, doneFunction, doneArgument);
    }
#endif
    return FALSE;
}

void EDT_StopEncoder(MWContext* pContext){
#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        EDT_StopPlugin(pContext);
    }
#endif
}


#ifdef EDITOR_JAVA

struct PreOpenCallbackHook {
    CEditorPluginInterface* pInterface;
    EDT_PreOpenCallbackFn doneFunction;
    void* hook;
    char* pURL;
};

static void edt_SetURL(void* hook3, char* pURL){
    PreOpenCallbackHook* hook2 = (PreOpenCallbackHook*) hook3;
    char* pOldURL = hook2->pURL;
    hook2->pURL = pURL ? XP_STRDUP(pURL) : 0;
    XP_FREE(pOldURL);
}

#ifndef XP_OS2
static
#else
extern "OPTLINK"
#endif
void edt_PreOpenDoneFunction(EDT_ImageEncoderStatus status, void* hook3){
    PreOpenCallbackHook* hook2 = (PreOpenCallbackHook*) hook3;
    EDT_PreOpenCallbackFn doneFunction = hook2->doneFunction;
    void* hook = hook2->hook;
    char* pURL = hook2->pURL;
    if ( doneFunction ) {
        (*doneFunction)(status == ED_IMAGE_ENCODER_USER_CANCELED, pURL, hook);
    }
    CEditorPluginInterface* epi = hook2->pInterface;
    if (pURL){
        XP_FREE(pURL);
    }
    delete epi;
    delete hook2;
}

#endif

void EDT_PreOpen(MWContext *pErrorContext,char* _pURL, 
                 EDT_PreOpenCallbackFn doneFunction, void* hook){
  // Make local copy, so we can change it if we want.
  char *pURL = _pURL ? XP_STRDUP(_pURL) : 0;
  int urlType = NET_URL_Type(pURL);
  
  // Always ignore target or query appendages
  edt_StripAtHashOrQuestionMark(pURL);
  XP_Bool bDeleteContext = FALSE;
  XP_Bool bValid = FALSE;
  
  //if( !pErrorContext )
  {
    //Primarily for the Mac, which doesn't create
    //  a context before calling EDT_PreOpen
    // (Windows and UNIX create a new Composer window + MWContext
    //  first or pass in parent MWContext here )
    pErrorContext = XP_NewContext();
    if( !pErrorContext )
        goto ERROR_EXIT;
    
    pErrorContext->type = MWContextBrowser;
    pErrorContext->is_editor = TRUE;
    bDeleteContext = TRUE;
  }
 
  XP_ASSERT(pErrorContext);
   
  if ( !pURL || !*pURL ) {
ERROR_EXIT:
    // Error or user Canceled.
    if (doneFunction) {
      (*doneFunction)(TRUE, _pURL, hook);
    }
    goto PRE_OPEN_EXIT;
  }

  // Only allow editing of appropriate types of URLs.
  if (urlType == FILE_TYPE_URL ||
      urlType == FTP_TYPE_URL ||
      urlType == HTTP_TYPE_URL ||
      urlType == SECURE_HTTP_TYPE_URL) {
    bValid = TRUE;
  }

  // Test for platform-specific filename if URL type is unknown.
  if (urlType == 0) {
    // don't make assumption that the user wants a local file
    // If it's not a valid URL, then the user probably wants a remote file
    // the following behavior is consistent with the browser location field
    bValid = TRUE;
  }


  if (!bValid) {
    // Tell user that we are rejecting their URL.
    char *tmplate = XP_GetString(XP_EDT_CANT_EDIT_URL);
    char *msg = NULL;
    if (tmplate) {
        msg = PR_smprintf(tmplate,pURL);
    }

    if (msg) {
        FE_Alert(pErrorContext,msg);
        XP_FREE(msg);
    }
    else {
        XP_ASSERT(0);
    }
    goto ERROR_EXIT;
  }


#ifdef EDITOR_JAVA
    if ( EditorPluginManager_PluginsExist() ) {
        CEditorPluginInterface* epi = new CEditorPluginInterface(0, &EDT_ComposerPluginCallback);
        char temp[500];
        XP_SPRINTF(temp, "edit:%s", pURL);
        PreOpenCallbackHook* hook2 = new PreOpenCallbackHook;
        hook2->pInterface = epi;
        hook2->doneFunction = doneFunction;
        hook2->pURL = pURL ? XP_STRDUP(pURL) : 0;
        hook2->hook = hook;

        XP_Bool saveJS = pErrorContext->forceJSEnabled;
        pErrorContext->forceJSEnabled = TRUE;
        netscape_javascript_JSObject *t_object=LJ_GetMochaWindow(pErrorContext);
        // Note: doneFunction will be called via hook2->doneFunction if following is successful
        XP_Bool bResult = epi->Perform(temp, &edt_PreOpenDoneFunction, hook2, t_object);
        pErrorContext->forceJSEnabled = saveJS;
        if ( bResult )
            goto PRE_OPEN_EXIT;

        // Failed, so clean up
        delete epi;
        delete hook2;
    }
#endif
    if ( doneFunction ){
        // FALSE means don't cancel.
        (*doneFunction)(FALSE, pURL, hook);
    }
    
PRE_OPEN_EXIT:
    XP_FREEIF(pURL);
    if( bDeleteContext )
      XP_DeleteContext(pErrorContext);
}

struct PreCloseCallbackHook {
    EDT_PreCloseCallbackFn doneFunction;
    void* hook;
};

#ifndef XP_OS2
static 
#else
extern "OPTLINK"
#endif
void edt_PreCloseDoneFunction(EDT_ImageEncoderStatus, void* hook2){
  // Ignore status.
  if (!hook2) {
    XP_ASSERT(0);
    return;
  }
 
  PreCloseCallbackHook *preClose = (PreCloseCallbackHook *)hook2;
  if ( preClose->doneFunction )
	  (*preClose->doneFunction)(preClose->hook);
  delete preClose;  
}

void EDT_PreClose(MWContext *pMWContext,char* pURL, 
                  EDT_PreCloseCallbackFn doneFunction, void* hook) {
  PreCloseCallbackHook *hook2 = new PreCloseCallbackHook;
  if (!hook2) {
    XP_ASSERT(0);
    return;
  }
  hook2->doneFunction = doneFunction;
  hook2->hook = hook;

  // EDT_PerformEvent takes care of #ifdef EDITOR_JAVA.
  EDT_PerformEvent(pMWContext,"close",pURL,FALSE,FALSE,edt_PreCloseDoneFunction,(void *)hook2);
}

// End of edt.h API functions

#ifdef EDITOR_JAVA

#define JAVA_EE_OR_RETURN JRIEnv* ee = Env(); if ( ! ee ) return

// Helper class for calling editor plugins

extern "C" {
    JRIEnv* npn_getJavaEnv();
}

EditorPluginManager EditorPluginManager_new(MWContext* pContext) {
    return new CEditorPluginInterface(pContext, &EDT_ComposerPluginCallback);
}

void EditorPluginManager_delete(EditorPluginManager a){
    CEditorPluginInterface* b = (CEditorPluginInterface*) a;
    delete b;
}

XP_Bool EditorPluginManager_IsPluginActive(EditorPluginManager a) {
    CEditorPluginInterface* b = (CEditorPluginInterface*) a;
    if ( CEditorPluginInterface::PluginsExist() && b ) {
        return b->IsPluginActive();
    }
    return FALSE;
}

XP_Bool EditorPluginManager_PluginsExist() {
    return CEditorPluginInterface::PluginsExist();
}

// This method is a modified version of intl_makeJavaString in modules/edtplug/lj_init.c
// Really would like this to be standardized.
PRIVATE PRBool onlyascii2(char* str, int len)
{
    int i;
    for(i = 0 ; i < len ; i++, str++)
    {
        if((*str & 0x80) != 0x00)
            return PR_FALSE;
    }
    return PR_TRUE;
}

PRIVATE struct java_lang_String*
intl_makeJavaString2(JRIEnv* env, int16 encoding, char* str, int len)
{
    // First, convert from encoding to win_csid
    encoding &= ~CS_AUTO;

    if ((encoding == 0) || 
		((STATEFUL != (encoding & STATEFUL) ) && onlyascii2(str,len) )
       )
	{
        return JRI_NewStringUTF(env, str, len);
	}

    int16 win_csid = INTL_DocToWinCharSetID(encoding) & ~CS_AUTO;

    char *str_in_win_csid = NULL;
    int len_in_win_csid = 0;
    XP_Bool needConvertToWincsid = (encoding != win_csid);
    XP_Bool needToFreeMem = needConvertToWincsid;	
    
    if(needConvertToWincsid)
    {
		str_in_win_csid = (char*)INTL_ConvertLineWithoutAutoDetect(
			(int16)encoding, win_csid, (unsigned char*) str, (uint32) len);
		if(NULL == str_in_win_csid )
		{
			str_in_win_csid = str;
			len_in_win_csid = len;
			needToFreeMem = FALSE;
			
		}
		else
		{
			len_in_win_csid = (int)XP_STRLEN( str_in_win_csid );
		}
    }	
    else
    {
		str_in_win_csid = str;
        len_in_win_csid = len;
    }
    // Second, convert from win_csid to Unicode
    INTL_Unicode* q;
    int unicodelen, realunicodelen;
    unicodelen = INTL_TextToUnicodeLen(win_csid, 
		(unsigned char*)str_in_win_csid, (int32)len_in_win_csid);
    
    struct java_lang_String* result;
    if ((q = (INTL_Unicode*) XP_ALLOC(sizeof(INTL_Unicode) * unicodelen)) != NULL) {
        realunicodelen = INTL_TextToUnicode(win_csid, 
			(unsigned char*)str_in_win_csid, len_in_win_csid, q, unicodelen );
	
        result = JRI_NewString(env, (const jchar*)q, realunicodelen);
        XP_FREE(q);
    } else {
        XP_ASSERT(q);
        result = JRI_NewStringUTF(env, str, len);
    }

    // Finally, let's free some memory
    if(needToFreeMem && str_in_win_csid)
	{
		XP_FREE(str_in_win_csid);
	}

    return result;
}

// Returns length accurately.
// returns NULL if the string is too large for the platform.
PRIVATE char*
intl_makeStringFromJava(JRIEnv* env, int16 encoding, struct java_lang_String* jstr, int32& length)
{
    const jchar * juc = JRI_GetStringChars(env, jstr);
    uint32 jucLen = JRI_GetStringLength(env, jstr);
    if ( jucLen > (uint32) MAX_STRING ) {
        length = jucLen;
        return 0;
    }

	encoding &= ~CS_AUTO;
	int16 win_csid = INTL_DocToWinCharSetID(encoding) & ~CS_AUTO;

    length = INTL_UnicodeToStrLen(win_csid, (INTL_Unicode*) juc, jucLen);
    if ( length > MAX_STRING ) {
        return 0;
    }
    char* text_in_win_csid = (char*) XP_ALLOC(length + 1);
    if ( ! text_in_win_csid ) {
        return 0;
    }
    INTL_UnicodeToStr(win_csid, (INTL_Unicode*) juc, jucLen, (unsigned char*) text_in_win_csid, length + 1);

	char* text_in_encoding = NULL;
	if(encoding == win_csid)
	{
		text_in_encoding = text_in_win_csid;
	}
	else
	{
		text_in_encoding = (char*)INTL_ConvertLineWithoutAutoDetect(
			win_csid, (int16)encoding, (unsigned char*) text_in_win_csid, 
			(uint32) XP_STRLEN(text_in_win_csid));
        if ( ! text_in_encoding ){
            text_in_encoding = text_in_win_csid;
        }
        else {
		    XP_FREE(text_in_win_csid);
        }
	}
    return text_in_encoding;

}

XP_Bool CEditorPluginInterface::PluginsExist(){
    return g_Plugin.Size() > 0;
}

JRIGlobalRef CEditorPluginInterface::g_pManager;
JRIEnv* CEditorPluginInterface::g_pEnv;
XP_Bool CEditorPluginInterface::g_bJavaInitialized;

JRIEnv* CEditorPluginInterface::Env(){
    if ( ! g_bJavaInitialized ) {
        g_bJavaInitialized = TRUE;
        g_pEnv = npn_getJavaEnv();
        if ( g_pEnv == NULL ) {
            XP_TRACE(("CEditorPluginInterface::Env() - Could not start Java."));
            return NULL;
        }
        JRIEnv* ee = g_pEnv;
        JRI_ExceptionClear(ee);
        use_netscape_plugin_composer_PluginManager(ee);
        use_netscape_plugin_composer_Composer(ee);
        use_netscape_plugin_composer_MozillaCallback(ee);
        if ( JRI_ExceptionOccurred(ee)){
            JRI_ExceptionDescribe(ee);
            XP_TRACE(("Couldn't find PluginManager class."));
            return g_pEnv;
        }
        g_pManager = JRI_NewGlobalRef(ee, netscape_plugin_composer_PluginManager_new(ee,
            class_netscape_plugin_composer_PluginManager(ee)));

        if ( ! Manager() || JRI_ExceptionOccurred(ee)){
            JRI_ExceptionDescribe(ee);
            XP_TRACE(("Couldn't create PluginManager instance."));
        }

        // Tell the plugin manager about the plugins
        int size = g_Plugin.Size();
        for(int i = 0; i < size; i++ ){
            char* pPlugin = g_Plugin[i];
            // Until we get java based local file i/o working, help out the plugin by
            // reading the ini text here.
            static const char* kComposerIni = "netscape_plugin_composer.ini";

            zip_t* pZip = zip_open(pPlugin);
            if ( pZip ) {
                struct stat status;
                if ( zip_stat(pZip, kComposerIni, &status)){
                    int32_t len = status.st_size;
                    if ( len > 0 ) {
                        char* pBuffer = new char[len];
                        if ( zip_get(pZip, kComposerIni, pBuffer, len) ){
                            struct java_lang_String* pluginINI = (struct java_lang_String*)
                                JRI_NewStringUTF(ee, pBuffer, len);
                            struct java_lang_String* pluginFileName = (struct java_lang_String*)
                                JRI_NewStringUTF(ee, pPlugin, XP_STRLEN(pPlugin));
                            JRI_ExceptionClear(ee);
                            netscape_plugin_composer_PluginManager_registerPlugin(ee, Manager(),
                                pluginFileName, pluginINI);
                            if ( JRI_ExceptionOccurred(ee)){
                                JRI_ExceptionDescribe(ee);
                                XP_TRACE(("Couldn't registerPlugin %s", pPlugin));
                            }
                        }
                    }
                }
                zip_close(pZip);
            }
        }
    }
    return g_pEnv;
}

netscape_plugin_composer_PluginManager* CEditorPluginInterface::Manager(){
    JAVA_EE_OR_RETURN 0;
    return (netscape_plugin_composer_PluginManager*) JRI_GetGlobalRef(ee, g_pManager);
}

netscape_plugin_composer_Composer* CEditorPluginInterface::Composer(){
    JAVA_EE_OR_RETURN 0;
    return (netscape_plugin_composer_Composer*) JRI_GetGlobalRef(ee, m_pComposer);
}


TXP_GrowableArray_pChar CEditorPluginInterface::g_Plugin;
TXP_GrowableArray_CEditorPluginInterface CEditorPluginInterface::g_Interface;

CEditorPluginInterface::CEditorPluginInterface(MWContext* pContext, ComposerPluginCallbackFn callback){
    m_pContext = pContext;
    m_callback = callback;
    m_bPluginActive = FALSE;
    m_doneFunction = 0;
    m_pComposer = 0;
    m_bImageEncoder = FALSE;
    JAVA_EE_OR_RETURN;
    m_pComposer = JRI_NewGlobalRef(ee, netscape_plugin_composer_Composer_new(ee,
        class_netscape_plugin_composer_Composer(ee),
        (jint) this, (jint) m_callback, (jint) ee));
    g_Interface.Add(this);
}

CEditorPluginInterface::~CEditorPluginInterface(){
    int size = g_Interface.Size();
    int i = 0;
    for(i = 0; i < size; i++ ){
        if ( g_Interface[i] == this ) {
            break;
        }
    }
    if ( i < size ) {
        for(i++; i < size; i++){
            g_Interface[i-1] = g_Interface[i];
        }
        g_Interface.SetSize(size-1);
    }
    else {
        XP_ASSERT(FALSE); // We didn't find ourselves in the interface registry.
    }
    JAVA_EE_OR_RETURN;
    JRI_DisposeGlobalRef(ee, m_pComposer);
#if 0
    netscape_plugin_composer_PluginManager::_unuse(ee);
#endif
}

XP_Bool CEditorPluginInterface::IsValidInterface(CEditorPluginInterface* pInterface){
    int i = 0;
    int size = g_Interface.Size();
    for(i = 0; i < size; i++ ){
        if ( g_Interface[i] == pInterface ) {
            return TRUE;
        }
    }
    return FALSE;
}

void CEditorPluginInterface::Register(char* plugin){
    // This method is called before Java is initialized.
    // It is designed so that it doesn't need Java to run.
    // The names of the plugin files are saved until later.
    //
    // This way, we don't start Java until the Composer is started.

    int size = g_Plugin.Size();
    for(int i = 0; i < size; i++ ){
        if ( XP_STRCMP(g_Plugin[i], plugin) == 0 ) {
            return;
        }
    }

    // If We've started Java, it's too late to add the plug-in.
    if ( g_bJavaInitialized ) {
        // to do: Allow Composer plug-ins to be added after starting.
        return;
    }
    // We don't have to add this plugin to the class path because
    // LJ_AddToClassPath puts every .zip or .jar file in
    // the plug-ins directory into the class path.

    // Save the plugin name.
    int len = XP_STRLEN(plugin);
    char* pCopy = (char*) XP_ALLOC(len+1);
    if ( pCopy ) {
        XP_STRCPY(pCopy, plugin);
        g_Plugin.Add(pCopy);
    }
}

int32 CEditorPluginInterface::GetNumberOfCategories(){
    if ( ! PluginsExist() ) return 0;
    JAVA_EE_OR_RETURN 0;
    if ( ! Manager() ) {
        return 0;
    }
    JRI_ExceptionClear(ee);
    int numPlugins = netscape_plugin_composer_PluginManager_getNumberOfCategories(ee, Manager());
    if ( JRI_ExceptionOccurred(ee)){
        JRI_ExceptionDescribe(ee);
        XP_TRACE(("Couldn't getNumberOfCategories."));
        return 0;
    }
    return numPlugins;
}

int32 CEditorPluginInterface::GetNumberOfPlugins(int32 type){
    if ( ! PluginsExist() ) return 0;
    JAVA_EE_OR_RETURN 0;
    if ( ! Manager() ) {
        return 0;
    }
    JRI_ExceptionClear(ee);
    int numPlugins = netscape_plugin_composer_PluginManager_getNumberOfPlugins(ee, Manager(), type);
    if ( JRI_ExceptionOccurred(ee)){
        JRI_ExceptionDescribe(ee);
        XP_TRACE(("Couldn't getNumberOfPlugins."));
        return 0;
    }
    return numPlugins;
}

static char*
getPlatformString(JRIEnv* ee, java_lang_String* name)
{
    if (name == NULL || JRI_ExceptionOccurred(ee)){
#ifdef DEBUG
        JRI_ExceptionDescribe(ee);
#endif
        XP_TRACE(("Couldn't get composer plugin field."));
        return NULL;
    }
	/* Get the platform encoding of the string based on the browser's default csid */
    char* result = (char*) JRI_GetStringPlatformChars(ee, name, NULL, 0);
	if (result == NULL) {
        XP_TRACE(("Couldn't convert unicode string to platform string."));
		return NULL;
	}
    else if (JRI_ExceptionOccurred(ee)) {
#ifdef DEBUG
		JRI_ExceptionDescribe(ee);
#endif
        XP_TRACE(("Couldn't convert unicode string to platform string."));
		return NULL;
    }
    return result;
}

char* CEditorPluginInterface::GetCategoryName(int32 type){
    char* result = 0;
    if ( ! PluginsExist() ) return result;
    JAVA_EE_OR_RETURN result;
    if ( ! Manager() ) {
        return result;
    }
    JRI_ExceptionClear(ee);
    java_lang_String* name = netscape_plugin_composer_PluginManager_getCategoryName(ee, Manager(), type);
	return getPlatformString(ee, name);
}

char* CEditorPluginInterface::GetPluginName(int32 type, int32 index){
    char* result = 0;
    if ( ! PluginsExist() ) return result;
    JAVA_EE_OR_RETURN result;
    if ( ! Manager() ) {
        return result;
    }
    JRI_ExceptionClear(ee);
    java_lang_String* name = netscape_plugin_composer_PluginManager_getPluginName(ee, Manager(), type, index);
	return getPlatformString(ee, name);
}

char* CEditorPluginInterface::GetPluginHint(int32 type, int32 index){
    char* result = 0;
    if ( ! PluginsExist() ) return result;
    JAVA_EE_OR_RETURN result;
    if ( ! Manager() ) {
        return result;
    }
    JRI_ExceptionClear(ee);
    java_lang_String* hint = netscape_plugin_composer_PluginManager_getPluginHint(ee, Manager(), type, index);
	return getPlatformString(ee, hint);
}

int32 CEditorPluginInterface::GetNumberOfEncoders(){
    if ( ! PluginsExist() ) return 0;
    JAVA_EE_OR_RETURN 0;
    if ( ! Manager() ) {
        return 0;
    }
    JRI_ExceptionClear(ee);
    int32 result = netscape_plugin_composer_PluginManager_getNumberOfEncoders(ee, Manager());
    if ( JRI_ExceptionOccurred(ee)){
        JRI_ExceptionDescribe(ee);
        XP_TRACE(("Couldn't getNumberOfEncoders."));
        return 0;
    }
    return result;
}

char* CEditorPluginInterface::GetEncoderName(int32 index){
    char* result = 0;
    if ( ! PluginsExist() ) return result;
    JAVA_EE_OR_RETURN result;
    if ( ! Manager() ) {
        return result;
    }
    JRI_ExceptionClear(ee);
    java_lang_String* hint = netscape_plugin_composer_PluginManager_getEncoderName(ee, Manager(), index);
	return getPlatformString(ee, hint);
}

char* CEditorPluginInterface::GetEncoderHint(int32 index){
    char* result = 0;
    if ( ! PluginsExist() ) return result;
    JAVA_EE_OR_RETURN result;
    if ( ! Manager() ) {
        return result;
    }
    JRI_ExceptionClear(ee);
    java_lang_String* hint = netscape_plugin_composer_PluginManager_getEncoderHint(ee, Manager(), index);
	return getPlatformString(ee, hint);
}

char* CEditorPluginInterface::GetEncoderFileExtension(int32 index){
    char* result = 0;
    if ( ! PluginsExist() ) return result;
    JAVA_EE_OR_RETURN result;
    if ( ! Manager() ) {
        return result;
    }
    JRI_ExceptionClear(ee);
    java_lang_String* hint = netscape_plugin_composer_PluginManager_getEncoderFileExtension(ee, Manager(), index);
	return getPlatformString(ee, hint);
}

char* CEditorPluginInterface::GetEncoderFileType(int32 index){
    char* result = 0;
    if ( ! PluginsExist() ) return result;
    JAVA_EE_OR_RETURN result;
    if ( ! Manager() ) {
        return result;
    }
    JRI_ExceptionClear(ee);
    java_lang_String* hint = netscape_plugin_composer_PluginManager_getEncoderFileType(ee, Manager(), index);
	return getPlatformString(ee, hint);
}

XP_Bool CEditorPluginInterface::GetEncoderNeedsQuantizedSource(int32 index){
    XP_Bool result = FALSE;
    if ( ! PluginsExist() ) return result;
    JAVA_EE_OR_RETURN result;
    if ( ! Manager() ) {
        return result;
    }
    JRI_ExceptionClear(ee);
    result = netscape_plugin_composer_PluginManager_getEncoderNeedsQuantizedSource(ee, Manager(), index);
    if ( JRI_ExceptionOccurred(ee)){
        JRI_ExceptionDescribe(ee);
        XP_TRACE(("Couldn't getEncoderNeedsQuantizedSource."));
        return result;
    }
    return result;
}

XP_Bool CEditorPluginInterface::StartEncoder(int32 index, int32 width, int32 height, char** pixels,
    char* csFileName, EDT_ImageEncoderCallbackFn doneFunction,
    void* doneArgument){
    if ( ! PluginsExist() ) return FALSE;
    JAVA_EE_OR_RETURN FALSE;
    if ( ! Manager() ) {
        return FALSE;
    }
    m_bCanChangeDocument = FALSE;
    m_bCanCancel = TRUE;
    m_bImageEncoder = TRUE;
    // Convert the file name
    struct java_lang_String* javaFileName = (struct java_lang_String*)
        JRI_NewStringUTF(ee, csFileName, XP_STRLEN(csFileName));

    jarrayArray javaPixels = (jarrayArray) JRI_NewObjectArray(ee, height,
        JRI_FindClass(ee, JRISigArray(JRISigByte)) , NULL);
    {
        // Convert the pixels to Java data struct
        int32 byteLength = width * 3;
        // Fill it up with the lines
        for(int hh = 0; hh < height; hh++){
            jbyteArray a = (jbyteArray) JRI_NewByteArray(ee, byteLength, pixels[hh]);
            JRI_SetObjectArrayElement(ee, javaPixels, hh, a);
        }
    }
    // Save the callback info.
    m_doneFunction = doneFunction;
    m_doneFunctionArgument = doneArgument;
    m_bPluginActive = TRUE;
    // call the encoder
    JRI_ExceptionClear(ee);
    XP_Bool result = (XP_Bool)
        netscape_plugin_composer_PluginManager_startEncoder(ee, Manager(), Composer(), index, width, height, javaPixels, javaFileName);
    if ( JRI_ExceptionOccurred(ee)){
        JRI_ExceptionDescribe(ee);
        XP_TRACE(("Couldn't start encoder."));
        m_bPluginActive = FALSE;
        result = FALSE;
    }
    return result;
}

XP_Bool CEditorPluginInterface::Perform(int32 type, int32 index, EDT_ImageEncoderCallbackFn doneFunction, void* hook, netscape_javascript_JSObject *jsobject){
    return Perform2(NULL, type, index, TRUE, TRUE, doneFunction, hook, jsobject);
}

XP_Bool CEditorPluginInterface::Perform(char* pPluginName, EDT_ImageEncoderCallbackFn doneFunction, void* hook, netscape_javascript_JSObject *jsobject){
    return Perform2(pPluginName, 0, 0, TRUE, TRUE, doneFunction, hook, jsobject);
}

void CEditorPluginInterface::Perform(char* pEvent, char* pDocURL, XP_Bool bCanChangeDocument, XP_Bool bCanCancel,
                                     EDT_ImageEncoderCallbackFn doneFunction, void* hook, netscape_javascript_JSObject *jsobject){
    char *pEventName = NULL;
    StrAllocCat(pEventName,pEvent);
    StrAllocCat(pEventName,":");
    if ( pDocURL ) {
        StrAllocCat(pEventName,pDocURL);
    }
    XP_Bool result = Perform2(pEventName, 0, 0, bCanChangeDocument, bCanCancel, doneFunction, hook, jsobject);
    XP_FREE(pEventName);
    if ( !result && doneFunction ) {
        (*doneFunction)(ED_IMAGE_ENCODER_EXCEPTION, hook);
    }
}

XP_Bool CEditorPluginInterface::Perform2(char* pPluginName, int32 type, int32 index,
        XP_Bool bCanChangeDocument, XP_Bool bCanCancel,
        EDT_ImageEncoderCallbackFn doneFunction, void* hook, netscape_javascript_JSObject *jsobject){
    m_bCanChangeDocument = bCanChangeDocument;
    m_bCanCancel = bCanCancel;
    m_bImageEncoder = FALSE;
    if ( ! pPluginName && ! PluginsExist() ) {
        return FALSE;
    }
    if ( m_bPluginActive ) {
        return FALSE;
    }
    JAVA_EE_OR_RETURN FALSE;
    if ( ! Manager() ) {
        return FALSE;
    }

    struct java_lang_String* document = GetDocument();
    // It is OK for the document to be NULL if we have
    // no buffer. (This can happen during an edit event.)
    // Otherwise, it is an error condition. NULL is returned
    // when the document is too large for Win16.
    if ( document==NULL && GetBuffer() != NULL){
        return FALSE;
    }
    struct java_lang_String* base = GetBaseURL();
    struct java_lang_String* workDirectoryURL = 0;
    struct java_lang_String* workDirectory = 0;
    GetWorkDirectoryAndURL(workDirectory, workDirectoryURL);
    m_bChangedDocument = FALSE;
    // call the plugin
    JRI_ExceptionClear(ee);
    
    XP_Bool bResult = FALSE;
    m_doneFunction = doneFunction;
    m_doneFunctionArgument = hook;

    if ( pPluginName ) {
        // Call plugin by class name.
        struct java_lang_String* pluginName = (struct java_lang_String*)
            JRI_NewStringUTF(ee, pPluginName, XP_STRLEN(pPluginName));
        JRI_ExceptionClear(ee);
        bResult = netscape_plugin_composer_PluginManager_performPluginByName(ee, Manager(), Composer(),
            pluginName, document, base, workDirectory, workDirectoryURL, jsobject);
    }
    else {
        // Call plugin by menu category and index
        JRI_ExceptionClear(ee);
        bResult = netscape_plugin_composer_PluginManager_performPlugin(ee, Manager(), Composer(),
            type, index, document, base, workDirectory, workDirectoryURL, jsobject);
    }
    if ( JRI_ExceptionOccurred(ee)){
        JRI_ExceptionDescribe(ee);
        bResult = FALSE;
    }

    if ( bResult == FALSE ) {
        XP_TRACE(("Couldn't perform."));
        m_doneFunction = 0;
        m_doneFunctionArgument = 0;
        return FALSE;
    }

    m_bPluginActive = TRUE;
    if ( GetBuffer() ){
        GetBuffer()->m_pContext->waitingMode = 1;
    }
    return TRUE;
}

struct java_lang_String* CEditorPluginInterface::GetBaseURL(){
    if ( GetBuffer() == 0 ) {
        return 0;
    }
    JAVA_EE_OR_RETURN 0;
    char* pBase = LO_GetBaseURL(GetBuffer()->m_pContext); // an alias. We don't own the result.
    int32 iBaseLen = XP_STRLEN(pBase);
    struct java_lang_String* base = (struct java_lang_String*)
        JRI_NewStringUTF(ee, pBase, iBaseLen);
    return base;
}

struct java_lang_String* CEditorPluginInterface::GetDocument(){
    if ( GetBuffer() == 0 ) {
        return 0;
    }
    JAVA_EE_OR_RETURN 0;
    XP_HUGE_CHAR_PTR pDocData = 0;
    int32 docLength = 0;

    docLength = GetBuffer()->WriteToBuffer(&pDocData, TRUE);
    if ( docLength > MAX_STRING ) {
        DocumentIsTooLarge(docLength, XP_EDT_CP_DOCUMENT_TOO_LARGE_READ);
        return 0;
    }

    int32 unicodeByteLength = 2 * (int32) INTL_TextToUnicodeLen(GetBuffer()->GetDocCharSetID(), (unsigned char*)pDocData, docLength);
    if ( unicodeByteLength > MAX_STRING ) {
        DocumentIsTooLarge(unicodeByteLength, XP_EDT_CP_DOCUMENT_TOO_LARGE_READ);
        return 0;
    }

    struct java_lang_String* document = (struct java_lang_String*)
        intl_makeJavaString2(ee, GetBuffer()->GetDocCharSetID(), pDocData, docLength);

    if ( pDocData ) {
        XP_HUGE_FREE(pDocData);
    }
    return document;
}

void CEditorPluginInterface::DocumentIsTooLarge(int32 docLengthInBytes, int msgid){
    // Tell the user that the document is too big.
    char* tmplate = XP_GetString(msgid);
    if (tmplate) {
        char* msg = PR_smprintf(tmplate,docLengthInBytes, MAX_STRING);
        if (msg) {
            FE_Alert(m_pContext,msg);
            XP_FREE(msg);
        }
    }
}

void CEditorPluginInterface::GetWorkDirectoryAndURL(
        struct java_lang_String*& workDirectory,
        struct java_lang_String*& workDirectoryURL){
    workDirectory = 0;
    workDirectoryURL = 0;
    if ( GetBuffer() == 0 ) {
        return;
    }
    JAVA_EE_OR_RETURN;
    // Get the work directory
    char* pWork = NULL; // We own the storage

    char* pBase = LO_GetBaseURL(GetBuffer()->m_pContext); // an alias. We don't own the result.
    int baseURLType = NET_URL_Type(pBase);
    if (baseURLType == FILE_TYPE_URL) {
        // Get the file name out of the base URL
        XP_Bool bValidBase = XP_ConvertUrlToLocalFile(pBase, &pWork);
        XP_FREE(pWork);
        pWork = NULL;
        if ( bValidBase) {
            // OK, the base file exists. Now we need pWork to hold the directory rather than the
            // file.
            char* pTemp = XP_STRDUP(pBase);
            if ( pTemp != NULL ) {
                char* pEnd = pTemp + XP_STRLEN(pTemp) - 1;
                while ( pEnd > pTemp && *pEnd != '/') {
                    pEnd--;
                }
                if (* pEnd == '/') {
                    *pEnd = '\0';
                    XP_ConvertUrlToLocalFile(pTemp, &pWork);
                }
                XP_FREE(pTemp);
            }
        }
    }

    if ( pWork == NULL ) { 
      // If we can't use document current directory, use document temp directory.
      CEditCommandLog *pLog = CGlobalHistoryGroup::GetGlobalHistoryGroup()->GetLog(GetBuffer());
      if (pLog) {
        char *pxpURL = pLog->GetDocTempDir();
        if (pxpURL) {
          pWork = WH_FileName(pxpURL,xpURL);
          XP_FREE(pxpURL);
        }
      }
    }

	if ( pWork != NULL ) {
		int32 iWorkLen = XP_STRLEN(pWork);

		workDirectory = (struct java_lang_String*)
			JRI_NewStringUTF(ee, pWork, iWorkLen);
		// And what's the URL that corresponds to the work directory?
		char* pWorkURL = XP_PlatformFileToURL(pWork);

		int32 iWorkURLLen = XP_STRLEN(pWorkURL);

		workDirectoryURL = (struct java_lang_String*)
			JRI_NewStringUTF(ee, pWorkURL, iWorkURLLen);

#ifdef DEBUG_rhess
		fprintf(stderr, "     workDir::[ %s ]\n", pWork);
		fprintf(stderr, "  workDirURL::[ %s ]\n", pWorkURL);
#endif
		XP_FREE(pWork);
		XP_FREE(pWorkURL);
	}
#ifdef DEBUG_rhess
	else {
		fprintf(stderr, "     workDir::[ NULL ]\n");
		fprintf(stderr, "  workDirURL::[ NULL ]\n");
	}
#endif
}

XP_Bool CEditorPluginInterface::IsPluginActive(){
    return m_bPluginActive;
}

void CEditorPluginInterface::StopPlugin(){
    if ( ! PluginsExist() ) return;
    JAVA_EE_OR_RETURN;
    JRI_ExceptionClear(ee);
    netscape_plugin_composer_PluginManager_stopPlugin(ee, Manager(), Composer());
    if ( JRI_ExceptionOccurred(ee)){
        JRI_ExceptionDescribe(ee);
        XP_TRACE(("Couldn't stop plugin."));
    }
    m_bPluginActive = FALSE;
    if ( GetBuffer()){
        if ( m_bChangedDocument) {
            GetBuffer()->EndBatchChanges();
        }
        GetBuffer()->m_pContext->waitingMode = 0;
    }
}

void CEditorPluginInterface::PluginCallback(int32 action,
    struct java_lang_Object* pArgument)
{
    if ( ! PluginsExist() ) return;
    switch ( action ) {
    case PLUGIN_FAIL:
    case PLUGIN_CANCEL:
        // The plugin has failed.
        if ( m_bCanCancel ) {
            PluginFailed((struct java_lang_String*) pArgument);
        }
        else {
            PluginSucceeded();
        }

        break;
    case PLUGIN_OK:
        // The plugin has succeeded.
        PluginSucceeded();
        break;
    case PLUGIN_NEWTEXT:
        // The plugin has changed the text.
        NewText((struct java_lang_String*) pArgument);
        break;
    default:
        // Unknown callack
        XP_TRACE(("Unknown Composer Plugin Callback: %d\n", action));
        break;
    }
}

void CEditorPluginInterface::PluginFailed(struct java_lang_String* pWhy){
    XP_TRACE(("CEditorPluginInterface::PluginFailed."));
    m_bPluginActive = FALSE;
    ImageEncoderFinished(pWhy ? ED_IMAGE_ENCODER_EXCEPTION : ED_IMAGE_ENCODER_USER_CANCELED);
    if ( m_bImageEncoder ) return;
    // The user canceled, or the plugin threw an exception. Back out
    // any change the user may have made to the document.
    if ( GetBuffer() ){
        if (m_bChangedDocument) {
            GetBuffer()->EndBatchChanges();
        }
        GetBuffer()->m_pContext->waitingMode = 0;
        if (m_bChangedDocument) {
            GetBuffer()->Undo(); // After this call, GetBuffer() is deleted.
        }
    }
}

void CEditorPluginInterface::PluginSucceeded(){
    XP_TRACE(("CEditorPluginInterface::PluginSucceeded."));
    m_bPluginActive = FALSE;
    ImageEncoderFinished(ED_IMAGE_ENCODER_OK);
    if ( m_bImageEncoder ) return;
    if ( GetBuffer() ){
        if (m_bChangedDocument) {
            GetBuffer()->EndBatchChanges();
        }
        GetBuffer()->m_pContext->waitingMode = 0;
    }
}

void CEditorPluginInterface::ImageEncoderFinished(EDT_ImageEncoderStatus status){
    if ( ! m_doneFunction ) {
        return;
    }
    (*m_doneFunction)(status, m_doneFunctionArgument);
    m_doneFunction = 0;
    m_doneFunctionArgument = 0;
}

void CEditorPluginInterface::NewText(struct java_lang_String* pText){
    XP_TRACE(("CEditorPluginInterface::NewText."));
    JAVA_EE_OR_RETURN;
    
    if ( pText && m_bCanChangeDocument ) {

        int32 length;
        int16 csid = 0;
        if ( GetBuffer() ) {
            csid = GetBuffer()->GetDocCharSetID();
        }
        char* pElementsAsChar =
            intl_makeStringFromJava(ee, csid, pText, length);
        if ( !pElementsAsChar ){
            DocumentIsTooLarge(length, XP_EDT_CP_DOCUMENT_TOO_LARGE_WRITE);
            return;
        }

        if ( m_bChangedDocument == FALSE ) {
            m_bChangedDocument = TRUE;
            if ( GetBuffer() ) {
                GetBuffer()->BeginBatchChanges(kPerformPluginCommandID);
            }
        }

        if ( pElementsAsChar ) {
            if ( GetBuffer() ) {
                length++; // for '\0' at end.
                XP_HUGE_CHAR_PTR pElements = (XP_HUGE_CHAR_PTR) XP_HUGE_ALLOC(length);
                for(int32 i = 0; i < length; i++ ) {
                    pElements[i] = pElementsAsChar[i];
                }
                GetBuffer()->ReadFromBuffer(pElements);
                // At this point GetBuffer() is deleted.
                XP_HUGE_FREE(pElements);
            }
            else { // Call-back hack for setting the URL during preOpen
                edt_SetURL(m_doneFunctionArgument, pElementsAsChar);
            }
            XP_FREE(pElementsAsChar);
        }
    }
}

CEditBuffer* CEditorPluginInterface::GetBuffer(){
    if ( ! m_pContext ) return 0;
    GET_EDIT_BUF_OR_RETURN_READY_OR_NOT(m_pContext, pEditBuffer) NULL;
    return pEditBuffer;
}

#endif // EDITOR_JAVA
#endif // EDITOR
