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
#include "nsIFileSpecWithUI.h"



#include "nsAppShellCIDs.h"
#include "prprf.h"

// Interfaces Needed
#include "nsIAppShellService.h"
#include "nsIDocShell.h"
#include "nsIDOMWindow.h"
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
static void * CartmanUIHandler(uint32 resourceID, void* clientContext, uint32 width, uint32 height, char* urlStr, void *data);
extern "C" void CARTMAN_UIEventLoop(void *data);


/* nsISupports Implementation for the class */
NS_IMPL_ISUPPORTS1(nsPSMUIHandlerImpl, nsIPSMUIHandler)

NS_METHOD
nsPSMUIHandlerImpl::DisplayURI(PRInt32 width, PRInt32 height, const char *urlStr)
{
    nsresult rv;
    
    NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
	if (NS_SUCCEEDED(rv))
	{
		// get a parent window for the new browser window
		nsCOMPtr<nsIXULWindow>	parent;
		appShell->GetHiddenWindow(getter_AddRefs(parent));

		// convert it to a DOMWindow
		nsCOMPtr<nsIDocShell>	docShell;
		if (parent)
		{
			parent->GetDocShell(getter_AddRefs(docShell));
		}
		nsCOMPtr<nsIDOMWindow>	domParent(do_GetInterface(docShell));
		nsCOMPtr<nsIScriptGlobalObject>	sgo(do_QueryInterface(domParent));

		nsCOMPtr<nsIScriptContext>	context;
		if (sgo)
		{
			sgo->GetContext(getter_AddRefs(context));
		}
		if (context)
		{
			JSContext *jsContext = (JSContext*)context->GetNativeContext();
			if (jsContext)
			{
				void	*stackPtr;

                char buffer[256];
                PR_snprintf(buffer,
                            sizeof(buffer),
                            "menubar=no,height=%d,width=%d",
                            height,
                            width );

				jsval	*argv = JS_PushArguments(jsContext, &stackPtr, "sss", urlStr, "_blank", buffer);
				if (argv)
				{
					// open the window
					nsIDOMWindow	*newWindow;
					domParent->Open(jsContext, argv, 3, &newWindow);
                    newWindow->ResizeTo(width, height);
					JS_PopArguments(jsContext, stackPtr);
                }
			}
	    }
    }
    return rv;
}

NS_IMETHODIMP
nsPSMUIHandlerImpl::PromptForFile(const char *prompt, const char *fileRegEx, PRBool shouldFileExist, char **outFile)
{
    NS_ENSURE_ARG_POINTER(outFile);
    nsIFileSpecWithUI* file = NS_CreateFileSpecWithUI();
    
    if (file == nsnull)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = file->ChooseInputFile(prompt, 
                                        nsIFileSpecWithUI::eAllFiles | nsIFileSpecWithUI::eExtraFilter, 
                                        fileRegEx,   // FIX name?
                                        fileRegEx);

    if (NS_FAILED(rv)) 
        return rv;

    rv = file->GetNativePath(outFile);
    
    NS_RELEASE(file);

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

PRStatus InitPSMUICallbacks(PCMT_CONTROL control)
{
    if (!control)
        return PR_FAILURE;  
    
    CMT_SetPromptCallback(control, (promptCallback_fn)PromptUserCallback, nsnull);
    CMT_SetAppFreeCallback(control, (applicationFreeCallback_fn) ApplicationFreeCallback);
    CMT_SetFilePathPromptCallback(control, (filePathPromptCallback_fn) FilePathPromptCallback, nsnull);

    if (CMT_SetUIHandlerCallback(control, (uiHandlerCallback_fn) CartmanUIHandler, NULL) != CMTSuccess) 
            return PR_FAILURE;

    PR_CreateThread(PR_USER_THREAD,
                    CARTMAN_UIEventLoop,
                    control, 
                    PR_PRIORITY_NORMAL, 
                    PR_GLOBAL_THREAD, 
                    PR_UNJOINABLE_THREAD,
                    0);  

    return PR_SUCCESS;
}

PRStatus DisplayPSMUIDialog(PCMT_CONTROL control, void *arg)
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
    pwin = CartmanUIHandler(advRID, arg, width, height, (char*)urlItem.data, NULL);
    return PR_SUCCESS;
}



void* CartmanUIHandler(uint32 resourceID, void* clientContext, uint32 width, uint32 height, char* urlStr, void *data)
{
    nsresult rv = NS_OK;
    
    NS_WITH_PROXIED_SERVICE(nsIPSMUIHandler, handler, nsPSMUIHandlerImpl::GetCID(), NS_UI_THREAD_EVENTQ, &rv);
    
    if(NS_SUCCEEDED(rv))
	    handler->DisplayURI(width, height, urlStr);

    return nsnull;
}

    
    
char * PromptUserCallback(void *arg, char *prompt, int isPasswd)
{

    nsresult rv = NS_OK;
    PRUnichar *password;
    PRInt32  value;

    NS_WITH_PROXIED_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, NS_UI_THREAD_EVENTQ, &rv);
    
    if (NS_SUCCEEDED(rv)) {
	    rv = dialog->PromptPassword(nsString(prompt).GetUnicode(), nsnull /* window title */, &password, &value);

        if (NS_SUCCEEDED(rv)) {
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
    nsAllocator::Free(userInput);
}

char * FilePathPromptCallback(void *arg, char *prompt, char *fileRegEx, CMUint32 shouldFileExist)
{
    nsresult rv = NS_OK;
    
    char* filePath = nsnull;
    
    NS_WITH_PROXIED_SERVICE(nsIPSMUIHandler, handler, nsPSMUIHandlerImpl::GetCID(), NS_UI_THREAD_EVENTQ, &rv);
    
    if(NS_SUCCEEDED(rv))
	    handler->PromptForFile(prompt, fileRegEx, (PRBool)shouldFileExist, &filePath);

    return filePath;
}
