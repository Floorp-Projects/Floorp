/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
*/

#include "nsProxiedService.h"
#include "nsIEventQueueService.h"
#include "nsPSMUICallbacks.h"
#include "nsINetSupportDialogService.h"
#include "nsIFilePicker.h"

#include "nsAppShellCIDs.h"
#include "prprf.h"
#include "prmem.h"

#include "nsSSLIOLayer.h" // for SSMSTRING_PADDED_LENGTH
#include "ssmdefs.h"
#include "rsrcids.h"

// Interfaces Needed
#include "nsIAppShellService.h"
#include "nsIDocShell.h"
#include "nsIDOMWindowInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIPrompt.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#include "nsIXULWindow.h"

static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);


// Happy callbacks
static char * PromptUserCallback(void *arg, char *prompt, int isPasswd);
static char * FilePathPromptCallback(void *arg, char *prompt, char *fileRegEx, CMUint32 shouldFileExist);
static void   ApplicationFreeCallback(char *userInput);

static void * CartmanUIHandler(uint32 resourceID, void* clientContext, uint32 width, uint32 height, 
								CMBool isModal, char* urlStr, void *data);

extern "C" void CARTMAN_UIEventLoop(void *data);


/* nsISupports Implementation for the class */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsPSMUIHandlerImpl, nsIPSMUIHandler)

NS_METHOD
nsPSMUIHandlerImpl::DisplayURI(PRInt32 width, PRInt32 height, PRBool modal, const char *urlStr, nsIDOMWindow * win)
{
    nsresult rv;
    nsCOMPtr<nsIDOMWindowInternal> parentWindow;
    JSContext *jsContext;
	jsval	*argv = NULL;

	if (win) {
		// Get script global object for the window.
		nsCOMPtr<nsIScriptGlobalObject> sgo;
		sgo = do_QueryInterface(win);
		if (!sgo) { rv = NS_ERROR_FAILURE; goto loser; }

		// Get script context from that.
		nsCOMPtr<nsIScriptContext> scriptContext;
		sgo->GetContext( getter_AddRefs( scriptContext ) );
		if (!scriptContext) { rv = NS_ERROR_FAILURE; goto loser; }

		// Get JSContext from the script context.
		jsContext = (JSContext*)scriptContext->GetNativeContext();
		if (!jsContext) { rv = NS_ERROR_FAILURE; goto loser; }

		parentWindow = do_QueryInterface(win);
	} else {
		NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
		if (NS_FAILED(rv)) {
			goto loser;
		}
		rv = appShell->GetHiddenWindowAndJSContext( getter_AddRefs( parentWindow ),
				                                        &jsContext );
		if ( NS_FAILED( rv ) ) {
			goto loser;
		}
	}

    // Set up arguments for "window.open"
    void *stackPtr;
    char params[36];

    if (modal) {  // if you change this, remember to change the buffer size above.
	    strcpy(params, "menubar=no,height=%d,width=%d,modal");
    } else {
		strcpy(params, "menubar=no,height=%d,width=%d");
	}

	char buffer[256];
    PR_snprintf(buffer,
		sizeof(buffer),
        params,
        height,
        width );

	argv = JS_PushArguments(jsContext, &stackPtr, "sss", urlStr, "_blank", buffer);
	if (argv) {
		// open the window
		nsIDOMWindowInternal	*newWindow;
		parentWindow->Open(jsContext, argv, 3, &newWindow);
        newWindow->ResizeTo(width, height);
        JS_PopArguments(jsContext, stackPtr);
	}
loser:
    return rv;
}

NS_IMETHODIMP
nsPSMUIHandlerImpl::PromptForFile(const PRUnichar *prompt, 
                                  const char *fileRegEx, 
                                  PRBool shouldFileExist, char **outFile)
{
    NS_ENSURE_ARG_POINTER(outFile);
    nsCOMPtr<nsIFilePicker> fp = do_CreateInstance("component://mozilla/filepicker");
    
    if (!fp)
        return NS_ERROR_NULL_POINTER;

    if (shouldFileExist) {
        fp->Init(nsnull, prompt, nsIFilePicker::modeOpen);
    } else {
        fp->Init(nsnull, prompt, nsIFilePicker::modeSave);
    }
    fp->AppendFilter(NS_ConvertASCIItoUCS2(fileRegEx).GetUnicode(), NS_ConvertASCIItoUCS2(fileRegEx).GetUnicode());  
    fp->AppendFilters(nsIFilePicker::filterAll);
    PRInt16 mode;
    nsresult rv = fp->Show(&mode);

    if (NS_FAILED(rv) || (mode == nsIFilePicker::returnCancel))
         return rv;

    nsCOMPtr<nsILocalFile> file;
    rv = fp->GetFile(getter_AddRefs(file));

    if (file)
      file->GetPath(outFile);

    return rv;
}

NS_METHOD      
nsPSMUIHandlerImpl::CreatePSMUIHandler(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                                  
    nsresult rv = NS_OK; 
    if ( aResult ) 
    { 
        /* Allocate new find component object. */
        nsPSMUIHandlerImpl *component = new nsPSMUIHandlerImpl(); 
        if ( component ) 
        { 
            /* Allocated OK, do query interface to get proper */
            /* pointer and increment refcount.                */
            rv = component->QueryInterface( aIID, aResult ); 
            if ( NS_FAILED( rv ) ) 
            { 
                /* refcount still at zero, delete it here. */
                delete component; 
            } 
        } 
        else 
        { 
            rv = NS_ERROR_OUT_OF_MEMORY; 
        } 
    } 
    else 
    { 
        rv = NS_ERROR_NULL_POINTER; 
    } 
    return rv; 
} 



extern "C" void CARTMAN_UIEventLoop(void *data)
{
   CMT_EventLoop((PCMT_CONTROL)data);
}

PRStatus InitPSMEventLoop(PCMT_CONTROL control)
{
    PR_CreateThread(PR_USER_THREAD,
                    CARTMAN_UIEventLoop,
                    control, 
                    PR_PRIORITY_NORMAL, 
                    PR_GLOBAL_THREAD, 
                    PR_UNJOINABLE_THREAD,
                    0);  

    return PR_SUCCESS;
}

PRStatus InitPSMUICallbacks(PCMT_CONTROL control)
{
    if (!control)
        return PR_FAILURE;  
    
    CMT_SetPromptCallback(control, (promptCallback_fn)PromptUserCallback, nsnull);
    CMT_SetAppFreeCallback(control, (applicationFreeCallback_fn) ApplicationFreeCallback);
    CMT_SetFilePathPromptCallback(control, (filePathPromptCallback_fn) FilePathPromptCallback, nsnull);

    if (CMT_SetUIHandlerCallback(control, (uiHandlerCallback_fn) CartmanUIHandler, NULL) != CMTSuccess) 
            return PR_FAILURE;

    return PR_SUCCESS;
}

PRStatus DisplayPSMUIDialog(PCMT_CONTROL control, const char *pickledStatus, const char *hostName, nsIDOMWindow * window)
{
    CMUint32 advRID = 0;
    CMInt32 width = 0;
    CMInt32 height = 0;
    CMTItem urlItem = {0, NULL, 0};
    CMTStatus rv = CMTSuccess;
    CMTItem advisorContext = {0, NULL, 0};
    void * pwin;

    CMTSecurityAdvisorData data;
    memset(&data, '\0', sizeof(CMTSecurityAdvisorData));
    
    if (hostName)
    {
        // if there is a hostName, than this request is about
        // a webpage.  
        data.hostname    = (char*) hostName;
        data.infoContext = SSM_BROWSER;
    
        if (pickledStatus)
        {
            CMTItem pickledResource = {0, NULL, 0};
            CMUint32 socketStatus = 0;
    
            pickledResource.len = *(int*)(pickledStatus);
            pickledResource.data = (unsigned char*) PR_Malloc(SSMSTRING_PADDED_LENGTH(pickledResource.len));
        
            if (! pickledResource.data) return PR_FAILURE;

            memcpy(pickledResource.data, pickledStatus+sizeof(int), pickledResource.len);
        
            /* Unpickle the SSL Socket Status */
            if (CMT_UnpickleResource( control, 
                                      SSM_RESTYPE_SSL_SOCKET_STATUS,
                                      pickledResource, 
                                      &socketStatus) == CMTSuccess)
            {
                data.infoContext = SSM_BROWSER;    
                data.resID = socketStatus;
            }

            PR_FREEIF(pickledResource.data);
        }
    }

    /* Create a Security Advisor context object. */
    rv = CMT_SecurityAdvisor(control, &data, &advRID);

    if (rv != CMTSuccess)
        return PR_FAILURE;

    /* Get the URL, width, height, etc. from the advisor context. */
    rv = CMT_GetStringAttribute(control, 
                                advRID, 
                                SSM_FID_SECADVISOR_URL,
                                &urlItem);

    if ((rv != CMTSuccess) || (!urlItem.data))
        return PR_FAILURE;

    rv = CMT_GetNumericAttribute(control, 
                                 advRID, 
                                 SSM_FID_SECADVISOR_WIDTH,
                                 &width);
    if (rv != CMTSuccess)
        return PR_FAILURE;

    rv = CMT_GetNumericAttribute(control, 
                                 advRID, 
                                 SSM_FID_SECADVISOR_HEIGHT,
                                 &height);
    if (rv != CMTSuccess)
        return PR_FAILURE;

    /* Fire the URL up in a window of its own. */
    pwin = CartmanUIHandler(advRID, nsnull, width, height, CM_TRUE,(char*)urlItem.data, window);
    
    //allocated by cmt, we can free with free:
    free(urlItem.data);
    
    return PR_SUCCESS;
}



void* CartmanUIHandler(uint32 resourceID, void* clientContext, uint32 width, uint32 height, CMBool isModal, char* urlStr, void *data)
{
    nsresult rv = NS_OK;
    
    NS_WITH_PROXIED_SERVICE(nsIPSMUIHandler, handler, nsPSMUIHandlerImpl::GetCID(), NS_UI_THREAD_EVENTQ, &rv);
    
    if(NS_SUCCEEDED(rv))
	    handler->DisplayURI(width, height, isModal, urlStr, (nsIDOMWindow*)data);

    return nsnull;
}

    
    
char * PromptUserCallback(void *arg, char *prompt, int isPasswd)
{

    nsresult rv = NS_OK;
    PRUnichar *password;
    PRBool  value;

    NS_WITH_PROXIED_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, NS_UI_THREAD_EVENTQ, &rv);
    
    if (NS_SUCCEEDED(rv)) {
	    rv = dialog->PromptPassword(nsnull, NS_ConvertASCIItoUCS2(prompt).GetUnicode(),
                                  NS_ConvertASCIItoUCS2(" ").GetUnicode(),      // hostname
                                  nsIPrompt::SAVE_PASSWORD_NEVER, &password, &value);

        if (NS_SUCCEEDED(rv) && value) {
            nsString a(password);
            char* str = a.ToNewCString();
            Recycle(password);
            return str;
        }
    }

    return nsnull;
}

void ApplicationFreeCallback(char *userInput)
{
    nsMemory::Free(userInput);
}

char * FilePathPromptCallback(void *arg, char *prompt, char *fileRegEx, CMUint32 shouldFileExist)
{
    nsresult rv = NS_OK;
    
    char* filePath = nsnull;
    
    NS_WITH_PROXIED_SERVICE(nsIPSMUIHandler, handler, nsPSMUIHandlerImpl::GetCID(), NS_UI_THREAD_EVENTQ, &rv);
    
    if(NS_SUCCEEDED(rv))
	    handler->PromptForFile(NS_ConvertASCIItoUCS2(prompt).GetUnicode(), fileRegEx, (PRBool)shouldFileExist, &filePath);

    return filePath;
}
