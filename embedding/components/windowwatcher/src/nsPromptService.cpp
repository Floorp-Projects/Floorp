/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPromptService.h"

#include "nsDialogParamBlock.h"
#include "nsIComponentManager.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsXPIDLString.h"

static const char kPromptURL[] = "chrome://global/content/commonDialog.xul";
static const char kSelectPromptURL[] = "chrome://global/content/selectDialog.xul";
static const char kQuestionIconClass[] = "question-icon";
static const char kAlertIconClass[] = "alert-icon";
static const char kWarningIconClass[] = "message-icon";

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define kCommonDialogsProperties "chrome://global/locale/commonDialogs.properties"


/****************************************************************
 ************************* ParamBlock ***************************
 ****************************************************************/

class ParamBlock {

public:
  ParamBlock() {
    mBlock = 0;
  }
  ~ParamBlock() {
    NS_IF_RELEASE(mBlock);
  }
  nsresult Init() {
    return nsComponentManager::CreateInstance(
                                 NS_DIALOGPARAMBLOCK_CONTRACTID,
                                 0, NS_GET_IID(nsIDialogParamBlock),
                                 (void**) &mBlock);
  }
  nsIDialogParamBlock * operator->() const { return mBlock; }
  operator nsIDialogParamBlock * const () { return mBlock; }

private:
  nsIDialogParamBlock *mBlock;
};

/****************************************************************
 ************************ nsPromptService ***********************
 ****************************************************************/

NS_IMPL_ISUPPORTS2(nsPromptService, nsIPromptService, nsPIPromptService)

nsPromptService::nsPromptService() {
}

nsPromptService::~nsPromptService() {
}

nsresult
nsPromptService::Init()
{
  nsresult rv;
  mWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  return rv;
}

NS_IMETHODIMP
nsPromptService::Alert(nsIDOMWindow *parent,
                   const PRUnichar *dialogTitle, const PRUnichar *text)
{
  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("Alert", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt(eNumberButtons, 1);
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  nsString url;
  NS_ConvertASCIItoUCS2 styleClass(kAlertIconClass);
  block->SetString(eIconClass, styleClass.get());

  rv = DoDialog(parent, block, kPromptURL);

  return rv;
}



NS_IMETHODIMP
nsPromptService::AlertCheck(nsIDOMWindow *parent,
                            const PRUnichar *dialogTitle, const PRUnichar *text,
                            const PRUnichar *checkMsg, PRBool *checkValue)

{
  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("Alert", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt( eNumberButtons,1 );
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  NS_ConvertASCIItoUCS2 styleClass(kAlertIconClass);
  block->SetString(eIconClass, styleClass.get());
  block->SetString(eCheckboxMsg, checkMsg);
  block->SetInt(eCheckboxState, *checkValue);

  rv = DoDialog(parent, block, kPromptURL);
  if (NS_FAILED(rv))
    return rv;

  block->GetInt(eCheckboxState, checkValue);

  return rv;
}

NS_IMETHODIMP
nsPromptService::Confirm(nsIDOMWindow *parent,
                   const PRUnichar *dialogTitle, const PRUnichar *text,
                   PRBool *_retval)
{
  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("Confirm", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  // Stuff in Parameters
  block->SetInt(eNumberButtons, 2);
  block->SetString(eMsg, text);
  
  block->SetString(eDialogTitle, dialogTitle);

  NS_ConvertASCIItoUCS2 styleClass(kQuestionIconClass);
  block->SetString(eIconClass, styleClass.get());
  
  rv = DoDialog(parent, block, kPromptURL);
  if (NS_FAILED(rv))
    return rv;
  
  PRInt32 buttonPressed = 0;
  block->GetInt(eButtonPressed, &buttonPressed);
  *_retval = buttonPressed ? PR_FALSE : PR_TRUE;

  return rv;
}

NS_IMETHODIMP
nsPromptService::ConfirmCheck(nsIDOMWindow *parent,
                   const PRUnichar *dialogTitle, const PRUnichar *text,
                   const PRUnichar *checkMsg, PRBool *checkValue,
                   PRBool *_retval)
{
  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("ConfirmCheck", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt( eNumberButtons,2 );
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  NS_ConvertASCIItoUCS2 styleClass(kQuestionIconClass);
  block->SetString(eIconClass, styleClass.get());
  block->SetString(eCheckboxMsg, checkMsg);
  block->SetInt(eCheckboxState, *checkValue);

  rv = DoDialog(parent, block, kPromptURL);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 tempInt = 0;
  block->GetInt(eButtonPressed, &tempInt);
  *_retval = tempInt ? PR_FALSE : PR_TRUE;

  block->GetInt(eCheckboxState, & tempInt);
  *checkValue = PRBool( tempInt );
  
  return rv;
}

NS_IMETHODIMP
nsPromptService::ConfirmEx(nsIDOMWindow *parent,
                    const PRUnichar *dialogTitle, const PRUnichar *text,
                    PRUint32 buttonFlags, const PRUnichar *button0Title,
                    const PRUnichar *button1Title, const PRUnichar *button2Title,
                    const PRUnichar *checkMsg, PRBool *checkValue,
                    PRInt32 *buttonPressed)
{
  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("Confirm", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetString(eDialogTitle, dialogTitle);
  block->SetString(eMsg, text);
  
  int buttonIDs[] = { eButton0Text, eButton1Text, eButton2Text };
  const PRUnichar* buttonStrings[] = { button0Title, button1Title, button2Title };

#define BUTTON_DEFAULT_MASK 0x03000000

  block->SetInt(eDefaultButton, (buttonFlags & BUTTON_DEFAULT_MASK) >> 24);
  PRInt32 numberButtons = 0;
  for (int i = 0; i < 3; i++) { 
    
    nsXPIDLString buttonTextStr;
    const PRUnichar* buttonText = 0;
    switch (buttonFlags & 0xff) {
      case BUTTON_TITLE_OK:
        GetLocaleString("OK", getter_Copies(buttonTextStr));
        break;
      case BUTTON_TITLE_CANCEL:
        GetLocaleString("Cancel", getter_Copies(buttonTextStr));
        break;
      case BUTTON_TITLE_YES:
        GetLocaleString("Yes", getter_Copies(buttonTextStr));
        break;
      case BUTTON_TITLE_NO:
        GetLocaleString("No", getter_Copies(buttonTextStr));
        break;
      case BUTTON_TITLE_SAVE:
        GetLocaleString("Save", getter_Copies(buttonTextStr));
        break;
      case BUTTON_TITLE_DONT_SAVE:
        GetLocaleString("DontSave", getter_Copies(buttonTextStr));
        break;
      case BUTTON_TITLE_REVERT:
        GetLocaleString("Revert", getter_Copies(buttonTextStr));
        break;
      case BUTTON_TITLE_IS_STRING:
        buttonText = buttonStrings[i];
        break;
    }
    if (!buttonText)
      buttonText = buttonTextStr.get();

    if (buttonText) {
      block->SetString(buttonIDs[i], buttonText);
      ++numberButtons;
    }
    buttonFlags >>= 8;
  }
  block->SetInt(eNumberButtons, numberButtons);
  
  block->SetString(eIconClass, NS_ConvertASCIItoUCS2(kQuestionIconClass).get());

  if (checkMsg && checkValue) {
    block->SetString(eCheckboxMsg, checkMsg);
    // since we're setting a PRInt32, we have to sanitize the PRBool first.
    // (myBool != PR_FALSE) is guaranteed to return either 1 or 0.
    block->SetInt(eCheckboxState, *checkValue != PR_FALSE);
  }
  
  /* perform the dialog */

  rv = DoDialog(parent, block, kPromptURL);
  if (NS_FAILED(rv))
    return rv;

  /* get back output parameters */

  if (buttonPressed)
    block->GetInt(eButtonPressed, buttonPressed);

  if (checkMsg && checkValue) {
    // GetInt returns a PRInt32; we need to sanitize it into PRBool
    PRInt32 tempValue;
    block->GetInt(eCheckboxState, &tempValue);
    *checkValue = (tempValue == 1);
  }

  return rv;
}

NS_IMETHODIMP
nsPromptService::Prompt(nsIDOMWindow *parent,
                        const PRUnichar *dialogTitle, const PRUnichar *text,
                        PRUnichar **value,
                        const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
  NS_ENSURE_ARG(value);
  NS_ENSURE_ARG(_retval);

  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("Prompt", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt(eNumberButtons, 2);
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  NS_ConvertASCIItoUCS2 styleClass(kQuestionIconClass);
  block->SetString(eIconClass, styleClass.get());
  block->SetInt(eNumberEditfields, 1);
  if (*value)
    block->SetString(eEditfield1Value, *value);
  if (checkMsg && checkValue) {
    block->SetString(eCheckboxMsg, checkMsg);
    block->SetInt(eCheckboxState, *checkValue);
  }

  rv = DoDialog(parent, block, kPromptURL);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 tempInt = 0;
  block->GetInt(eButtonPressed, &tempInt);
  *_retval = tempInt ? PR_FALSE : PR_TRUE;

  if (*_retval) {
    PRUnichar *tempStr;
    
    rv = block->GetString(eEditfield1Value, &tempStr);
    if (NS_FAILED(rv))
      return rv;
    if (*value)
      nsMemory::Free(*value);
    *value = tempStr;

    if (checkValue)
      block->GetInt(eCheckboxState, checkValue);  
  }
  return rv;
}

NS_IMETHODIMP
nsPromptService::PromptUsernameAndPassword(nsIDOMWindow *parent,
                    const PRUnichar *dialogTitle, const PRUnichar *text,
                    PRUnichar **username, PRUnichar **password,
                    const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)

{
  NS_ENSURE_ARG(username);
  NS_ENSURE_ARG(password);
  NS_ENSURE_ARG(_retval);
  
  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("PromptUsernameAndPassword", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt(eNumberButtons, 2);
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  NS_ConvertASCIItoUCS2 styleClass(kQuestionIconClass);
  block->SetString(eIconClass, styleClass.get());
  block->SetInt( eNumberEditfields, 2 );
  if (*username)
    block->SetString(eEditfield1Value, *username);
  if (*password)
    block->SetString(eEditfield2Value, *password);
  if (checkMsg && checkValue) {
    block->SetString(eCheckboxMsg, checkMsg);
    block->SetInt(eCheckboxState, *checkValue);
  }
  
  rv = DoDialog(parent, block, kPromptURL);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 tempInt = 0;
  block->GetInt(eButtonPressed, &tempInt);
  *_retval = tempInt ? PR_FALSE : PR_TRUE;
  
  if (*_retval) {
    PRUnichar *tempStr;
    
    rv = block->GetString(eEditfield1Value, &tempStr);
    if (NS_FAILED(rv))
      return rv;
    if (*username)
      nsMemory::Free(*username);
    *username = tempStr;

    rv = block->GetString(eEditfield2Value, &tempStr);
    if (NS_FAILED(rv))
      return rv;
    if (*password)
      nsMemory::Free(*password);
    *password = tempStr;

    if (checkValue)
      block->GetInt(eCheckboxState, checkValue);
  }
  return rv;
}

NS_IMETHODIMP nsPromptService::PromptPassword(nsIDOMWindow *parent,
                                const PRUnichar *dialogTitle, const PRUnichar *text,
                                PRUnichar **password,
                                const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
  NS_ENSURE_ARG(password);
  NS_ENSURE_ARG(_retval);
	
  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("PromptPassword", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt(eNumberButtons, 2);
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  nsString url;
  NS_ConvertASCIItoUCS2 styleClass(kQuestionIconClass);
  block->SetString(eIconClass, styleClass.get());
  block->SetInt(eNumberEditfields, 1);
  block->SetInt(eEditField1Password, 1);
  if (*password)
    block->SetString(eEditfield1Value, *password);
  if (checkMsg && checkValue) {
    block->SetString(eCheckboxMsg, checkMsg);
    block->SetInt(eCheckboxState, *checkValue);
  }

  rv = DoDialog(parent, block, kPromptURL);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 tempInt = 0;
  block->GetInt(eButtonPressed, &tempInt);
  *_retval = tempInt ? PR_FALSE : PR_TRUE;
  if (*_retval) {
    PRUnichar *tempStr;
    
    rv = block->GetString(eEditfield1Value, &tempStr);
    if (NS_FAILED(rv))
      return rv;
    if (*password)
      nsMemory::Free(*password);
    *password = tempStr;

    if (checkValue)
      block->GetInt(eCheckboxState, checkValue);  
  }
  return rv;
}

NS_IMETHODIMP
nsPromptService::Select(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                   const PRUnichar* text, PRUint32 count,
                   const PRUnichar **selectList, PRInt32 *outSelection,
                   PRBool *_retval)
{	
  nsresult rv;
  nsXPIDLString stringOwner;
 
  if (!dialogTitle) {
    rv = GetLocaleString("Select", getter_Copies(stringOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    dialogTitle = stringOwner.get();
  }

  const PRInt32 eSelection = 2;
  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetNumberStrings(count + 2);
  if (dialogTitle)
    block->SetString(0, dialogTitle);
  
  block->SetString(1, text);
  block->SetInt(eSelection, count);
  for (PRUint32 i = 2; i <= count+1; i++) {
    nsAutoString temp(selectList[i-2]);
    const PRUnichar* text = temp.get();
    block->SetString(i, text);
  }

  *outSelection = -1;
  rv = DoDialog(parent, block, kSelectPromptURL);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 buttonPressed = 0;
  block->GetInt(eButtonPressed, &buttonPressed);
  block->GetInt(eSelection, outSelection);
  *_retval = buttonPressed ? PR_FALSE : PR_TRUE;

  return rv;
}

nsresult
nsPromptService::DoDialog(nsIDOMWindow *aParent,
                   nsIDialogParamBlock *aParamBlock, const char *aChromeURL)
{
  NS_ENSURE_ARG(aParamBlock);
  NS_ENSURE_ARG(aChromeURL);
  if (!mWatcher)
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;

  // get a parent, if at all possible
  // (though we'd rather this didn't fail, it's OK if it does. so there's
  // no failure or null check.)
  nsCOMPtr<nsIDOMWindow> activeParent; // retain ownership for method lifetime
  if (!aParent) {
    mWatcher->GetActiveWindow(getter_AddRefs(activeParent));
    aParent = activeParent;
  }

  nsCOMPtr<nsISupports> arguments(do_QueryInterface(aParamBlock));
  nsCOMPtr<nsIDOMWindow> dialog;
  rv = mWatcher->OpenWindow(aParent, aChromeURL, "_blank",
                            "centerscreen,chrome,modal,titlebar", arguments,
                            getter_AddRefs(dialog));

  return rv;
}

nsresult
nsPromptService::GetLocaleString(const char *aKey, PRUnichar **aResult)
{
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
  nsCOMPtr<nsIStringBundle> stringBundle;
 
  rv = stringService->CreateBundle(kCommonDialogsProperties, getter_AddRefs(stringBundle));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  rv = stringBundle->GetStringFromName(NS_ConvertASCIItoUCS2(aKey).get(), aResult);

  return rv;
}
 
