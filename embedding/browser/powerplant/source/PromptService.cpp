/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "PromptService.h"
#include "nsCOMPtr.h"
#include "nsIPromptService.h"
#include "nsIFactory.h"
#include "nsString.h"
#include "nsIWindowWatcher.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"

// Local Includes
#include "ApplIDs.h"
#include "UMacUnicode.h"

// PowerPlant
#include <LStaticText.h>
#include <LCheckBox.h>
#include <LEditText.h>
#include <UModalDialogs.h>
#include <LPushButton.h>

//*****************************************************************************
// CPromptService
//*****************************************************************************   

class CPromptService: public nsIPromptService
{
public:
			CPromptService();
  virtual	~CPromptService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROMPTSERVICE
  
protected:
  LCommander* GetParentCommander(nsIDOMWindow* aDOMWindow);
};

//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CPromptService, nsIPromptService)

CPromptService::CPromptService()
{
	NS_INIT_REFCNT();
}

CPromptService::~CPromptService()
{
}

LCommander* CPromptService::GetParentCommander(nsIDOMWindow* aDOMWindow)
{
  nsresult rv;
  LCommander *resultCommander = LCommander::GetDefaultCommander(); 
  
	nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
	if (!wwatch) return nsnull;
	
	nsCOMPtr<nsIWebBrowserChrome> windowChrome;
	rv = wwatch->GetChromeForWindow(aDOMWindow, getter_AddRefs(windowChrome));
	if (NS_FAILED(rv)) return nsnull;
	
	nsCOMPtr<nsIEmbeddingSiteWindow> window(do_QueryInterface(windowChrome, &rv));
	if (NS_FAILED(rv)) return nsnull;
	
	WindowPtr macWindow;
	rv = window->GetSiteWindow(&macWindow);
	if (NS_FAILED(rv)) return nsnull;
	
	resultCommander = LWindow::FetchWindowObject(macWindow);
	NS_ASSERTION(resultCommander, "Couldn't get PowerPlant window");
	return resultCommander;
}

NS_IMETHODIMP CPromptService::Alert(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                    const PRUnichar *text)
{    
    StDialogHandler	 theHandler(dlog_Alert, GetParentCommander(parent));
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(text), cStr);
    cStr.ReplaceChar('\n', '\r');   			
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());

    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
   		break;
	}

    return NS_OK;
}

NS_IMETHODIMP CPromptService::AlertCheck(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                         const PRUnichar *text,
                                         const PRUnichar *checkMsg, PRBool *checkValue)
{
    NS_ENSURE_ARG_POINTER(checkValue);

    StDialogHandler	theHandler(dlog_ConfirmCheck, GetParentCommander(parent));
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(text), cStr);
    cStr.ReplaceChar('\n', '\r');   			
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
    LCheckBox *checkBox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(checkMsg), pStr);
    checkBox->SetDescriptor(pStr);
    checkBox->SetValue(*checkValue ? 1 : 0);

    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    *checkValue = checkBox->GetValue();    
   		    break;
   		}
	}

    return NS_OK;
}

NS_IMETHODIMP CPromptService::Confirm(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                      const PRUnichar *text,
                                      PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    StDialogHandler	theHandler(dlog_Confirm, GetParentCommander(parent));
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;
    
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);
   			
    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(text), cStr);
    cStr.ReplaceChar('\n', '\r');   			
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());

    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    *_retval = PR_TRUE;    
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return NS_OK;
}

NS_IMETHODIMP CPromptService::ConfirmCheck(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                           const PRUnichar *text,
                                           const PRUnichar *checkMsg, PRBool *checkValue,
                                           PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(checkValue);
    NS_ENSURE_ARG_POINTER(_retval);

    StDialogHandler	theHandler(dlog_ConfirmCheck, GetParentCommander(parent));
    LWindow			 *theDialog = theHandler.GetDialog();
    nsCAutoString    cStr;
    Str255           pStr;

    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(dialogTitle), pStr);
    theDialog->SetDescriptor(pStr);

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(text), cStr);
    cStr.ReplaceChar('\n', '\r');   			
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
    LCheckBox *checkBox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(checkMsg), pStr);
    checkBox->SetDescriptor(pStr);
    checkBox->SetValue(*checkValue ? 1 : 0);

    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    *_retval = PR_TRUE;
		    *checkValue = checkBox->GetValue();    
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return NS_OK;
}

NS_IMETHODIMP CPromptService::ConfirmEx(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                        const PRUnichar *text,
                                        PRUint32 buttonFlags,
                                        const PRUnichar *button0Title,
                                        const PRUnichar *button1Title,
                                        const PRUnichar *button2Title,
                                        const PRUnichar *checkMsg, PRBool *checkValue,
                                        PRInt32 *buttonPressed)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPromptService::Prompt(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                     const PRUnichar *text, PRUnichar **value,
                                     const PRUnichar *checkMsg, PRBool *checkValue,
                                     PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult resultErr = NS_OK;

    StDialogHandler	theHandler(dlog_Prompt, GetParentCommander(parent));
    LWindow			 *theDialog = theHandler.GetDialog();
    LCheckBox        *checkbox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    nsCAutoString   cStr;
    Str255          pStr;

    if (dialogTitle) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(dialogTitle), pStr);
        theDialog->SetDescriptor(pStr);
    }

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
    LEditText *responseText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Rslt'));
    if (value && *value) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(*value), pStr);
        responseText->SetDescriptor(pStr);
    }
    
    if (checkValue) {
        checkbox->SetValue(*checkValue);
        if (checkMsg) {
            CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(checkMsg), pStr);
            checkbox->SetDescriptor(pStr);
        }
    }
    else
        checkbox->Hide();

    theDialog->SetLatentSub(responseText);    
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString ucStr;

		    *_retval = PR_TRUE;
		    if (value && *value) {
		        nsMemory::Free(*value);
		        *value = nsnull;
		    }
		    responseText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *value = ucStr.ToNewUnicode();    
   		    if (*value == nsnull)
   		        resultErr = NS_ERROR_OUT_OF_MEMORY;
   		        
   		    if (checkValue)
   		        *checkValue = checkbox->GetValue();
   		        
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return resultErr;
}

NS_IMETHODIMP CPromptService::PromptUsernameAndPassword(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                                        const PRUnichar *text,
                                                        PRUnichar **username, PRUnichar **password,
                                                        const PRUnichar *checkMsg, PRBool *checkValue,
                                                        PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult resultErr = NS_OK;

    StDialogHandler	theHandler(dlog_PromptNameAndPass, GetParentCommander(parent));
    LWindow			 *theDialog = theHandler.GetDialog();
    LCheckBox        *checkbox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    nsCAutoString   cStr;
    Str255          pStr;

    if (dialogTitle) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(dialogTitle), pStr);
        theDialog->SetDescriptor(pStr);
    }

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
    LEditText *userText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Name'));
    if (username && *username) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(*username), pStr);
        userText->SetDescriptor(pStr);
    }
    LEditText *pwdText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Pass'));
    if (password && *password) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(*password), pStr);
        pwdText->SetDescriptor(pStr);
    }

    if (checkValue) {
        checkbox->SetValue(*checkValue);
        if (checkMsg) {
            CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(checkMsg), pStr);
            checkbox->SetDescriptor(pStr);
        }
    }
    else
        checkbox->Hide();
 
    theDialog->SetLatentSub(userText);   
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString    ucStr;
		    
		    if (username && *username) {
		        nsMemory::Free(*username);
		        *username = nsnull;
		    }
		    userText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *username = ucStr.ToNewUnicode();
		    if (*username == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;
		    
		    if (password && *password) {
		        nsMemory::Free(*password);
		        *password = nsnull;
		    }
		    pwdText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *password = ucStr.ToNewUnicode();
		    if (*password == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;

   		    if (checkValue)
   		        *checkValue = checkbox->GetValue();
		    
		    *_retval = PR_TRUE;        
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return resultErr;
}

NS_IMETHODIMP CPromptService::PromptPassword(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                             const PRUnichar *text,
                                             PRUnichar **password,
                                             const PRUnichar *checkMsg, PRBool *checkValue,
                                             PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    nsresult resultErr = NS_OK;

    StDialogHandler	 theHandler(dlog_PromptPassword, GetParentCommander(parent));
    LWindow			 *theDialog = theHandler.GetDialog();
    LCheckBox        *checkbox = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID('Chck'));
    nsCAutoString    cStr;
    Str255           pStr;

    if (dialogTitle) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(dialogTitle), pStr);
        theDialog->SetDescriptor(pStr);
    }

    LStaticText	*msgText = dynamic_cast<LStaticText*>(theDialog->FindPaneByID('Msg '));
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(text), cStr);
    cStr.ReplaceChar('\n', '\r');
    msgText->SetText(const_cast<char *>(cStr.get()), cStr.Length());
    
    LEditText *pwdText = dynamic_cast<LEditText*>(theDialog->FindPaneByID('Pass'));
    if (password && *password) {
        CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(*password), pStr);
        pwdText->SetDescriptor(pStr);
    }
    
    if (checkValue) {
        checkbox->SetValue(*checkValue);
        if (checkMsg) {
            CPlatformUCSConversion::GetInstance()->UCSToPlatform(nsDependentString(checkMsg), pStr);
            checkbox->SetDescriptor(pStr);
        }
    }
    else
        checkbox->Hide();
 
    theDialog->SetLatentSub(pwdText);   
    theDialog->Show();
    theDialog->Select();
	
	while (true)  // This is our modal dialog event loop
	{				
		MessageT	hitMessage = theHandler.DoDialog();
		
		if (hitMessage == msg_OK)
		{
		    nsAutoString    ucStr;
		    		    
		    if (password && *password) {
		        nsMemory::Free(*password);
		        *password = nsnull;
		    }
		    pwdText->GetDescriptor(pStr);
		    CPlatformUCSConversion::GetInstance()->PlatformToUCS(pStr, ucStr);
		    *password = ucStr.ToNewUnicode();
		    if (*password == nsnull)
		        resultErr = NS_ERROR_OUT_OF_MEMORY;

   		    if (checkValue)
   		        *checkValue = checkbox->GetValue();

		    *_retval = PR_TRUE;        
   		    break;
   		}
   		else if (hitMessage == msg_Cancel)
   		{
   		    *_retval = PR_FALSE;
   		    break;
   		}
	}

    return resultErr;
}

NS_IMETHODIMP CPromptService::Select(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                     const PRUnichar *text, PRUint32 count,
                                     const PRUnichar **selectList, PRInt32 *outSelection,
                                     PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
 
//*****************************************************************************
// CPromptServiceFactory
//*****************************************************************************   

class CPromptServiceFactory : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

  CPromptServiceFactory();
  virtual ~CPromptServiceFactory();
};

//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CPromptServiceFactory, nsIFactory)

CPromptServiceFactory::CPromptServiceFactory()
{
  NS_INIT_ISUPPORTS();
}

CPromptServiceFactory::~CPromptServiceFactory()
{
}

NS_IMETHODIMP CPromptServiceFactory::CreateInstance(nsISupports *aOuter, const nsIID & aIID, void **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = NULL;  
  CPromptService *inst = new CPromptService;    
  if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;
    
  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (rv != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  
    
  return rv;
}

NS_IMETHODIMP CPromptServiceFactory::LockFactory(PRBool lock)
{
    return NS_OK;
}

//*****************************************************************************   

nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory)
{
  NS_ENSURE_ARG_POINTER(aFactory);
  *aFactory = nsnull;
  
  CPromptServiceFactory *result = new CPromptServiceFactory;
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;
    
  NS_ADDREF(result);
  *aFactory = result;
  
  return NS_OK;
}


