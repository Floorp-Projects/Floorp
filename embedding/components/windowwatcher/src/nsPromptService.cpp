/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsPromptService.h"

#include "nsDialogParamBlock.h"
#include "nsXPComFactory.h"
#include "nsIComponentManager.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h"

const char *kPromptURL="chrome://global/content/commonDialog.xul";
const char *kSelectPromptURL="chrome://global/content/selectDialog.xul";
const char *kQuestionIconURL ="chrome://global/skin/question-icon.gif";
const char *kAlertIconURL ="chrome://global/skin/alert-icon.gif";
const char *kWarningIconURL ="chrome://global/skin/message-icon.gif";

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
  NS_INIT_REFCNT();
}

nsPromptService::~nsPromptService() {
}

nsresult
nsPromptService::Init()
{
  nsresult rv;
  mWatcher = do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
  return rv;
}

NS_IMETHODIMP
nsPromptService::Alert(nsIDOMWindow *parent,
                   const PRUnichar *dialogTitle, const PRUnichar *text)
{
  nsresult rv;
  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt(eNumberButtons, 1);
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  nsString url;
  url.AssignWithConversion(kAlertIconURL);
  block->SetString(eIconURL, url.GetUnicode());

  rv = DoDialog(parent, block, kPromptURL);

  return rv;
}



NS_IMETHODIMP
nsPromptService::AlertCheck(nsIDOMWindow *parent,
                            const PRUnichar *dialogTitle, const PRUnichar *text,
                            const PRUnichar *checkMsg, PRBool *checkValue)

{
  nsresult rv;
  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt( eNumberButtons,1 );
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  nsString url;
  url.AssignWithConversion(kAlertIconURL);

  block->SetString(eIconURL, url.GetUnicode());
  block->SetString(eCheckboxMsg, checkMsg);
  block->SetInt(eCheckboxState, *checkValue);

  rv = DoDialog(parent, block, kPromptURL);

  block->GetInt(eCheckboxState, checkValue);

  return rv;
}

NS_IMETHODIMP
nsPromptService::Confirm(nsIDOMWindow *parent,
                   const PRUnichar *dialogTitle, const PRUnichar *text,
                   PRBool *_retval)
{
  nsresult rv;
  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  // Stuff in Parameters
  block->SetInt(eNumberButtons, 2);
  block->SetString(eMsg, text);
  
  block->SetString(eDialogTitle, dialogTitle);

  nsString url;
  url.AssignWithConversion(kQuestionIconURL);
  block->SetString(eIconURL, url.GetUnicode());
  
  rv = DoDialog(parent, block, kPromptURL);
  
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
  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt( eNumberButtons,2 );
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  nsString url; url.AssignWithConversion( kQuestionIconURL );
  block->SetString(eIconURL, url.GetUnicode());
  block->SetString(eCheckboxMsg, checkMsg);
  block->SetInt(eCheckboxState, *checkValue);

  rv = DoDialog(parent, block, kPromptURL);
  PRInt32 tempInt = 0;
  block->GetInt(eButtonPressed, &tempInt);
  *_retval = tempInt ? PR_FALSE : PR_TRUE;

  block->GetInt(eCheckboxState, & tempInt);
  *checkValue = PRBool( tempInt );

  return rv;
}

NS_IMETHODIMP
nsPromptService::UniversalDialog(nsIDOMWindow *parent,
                   const PRUnichar *titleMessage, const PRUnichar *dialogTitle,
                   const PRUnichar *text, const PRUnichar *checkboxMsg,
                   const PRUnichar *button0Text, const PRUnichar *button1Text,
                   const PRUnichar *button2Text, const PRUnichar *button3Text,
                   const PRUnichar *editfield1Msg, const PRUnichar *editfield2Msg,
                   PRUnichar **editfield1Value, PRUnichar **editfield2Value,
                   const PRUnichar *iconURL, PRBool *checkboxState,
                   PRInt32 numberButtons, PRInt32 numberEditfields,
                   PRInt32 editfield1Password, PRInt32 *buttonPressed)
{
  nsresult rv;

  /* check for at least one button */
  if (numberButtons < 1)
    rv = NS_ERROR_FAILURE;

  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  /* load up input parameters */

  block->SetString(eTitleMessage, titleMessage);
  block->SetString(eDialogTitle, dialogTitle);
  block->SetString(eMsg, text);
  block->SetString(eCheckboxMsg, checkboxMsg);
  if (numberButtons >= 4)
    block->SetString(eButton3Text, button3Text);

  if (numberButtons >= 3)
    block->SetString(eButton2Text, button2Text);

  if (numberButtons >= 2)
    block->SetString(eButton1Text, button1Text);

  if (numberButtons >= 1)
    block->SetString(eButton0Text, button0Text);

  if (numberEditfields >= 2) {
    block->SetString(eEditfield2Msg, editfield2Msg);
    block->SetString(eEditfield2Value, *editfield2Value);
  }
  if (numberEditfields >= 1) {
    block->SetString(eEditfield1Msg, editfield1Msg);
    block->SetString(eEditfield1Value, *editfield1Value);
    block->SetInt(eEditField1Password, editfield1Password);
  }
  if (iconURL)
    block->SetString(eIconURL, iconURL);
  else
    block->SetString(eIconURL, NS_ConvertASCIItoUCS2(kQuestionIconURL).GetUnicode());

  if (checkboxMsg)
    block->SetInt(eCheckboxState, *checkboxState);

  block->SetInt(eNumberButtons, numberButtons);
  block->SetInt(eNumberEditfields, numberEditfields);

  /* perform the dialog */

  rv = DoDialog(parent, block, kPromptURL);

  /* get back output parameters */

  if (buttonPressed)
    block->GetInt(eButtonPressed, buttonPressed);

  if (checkboxMsg && checkboxState)
    block->GetInt(eCheckboxState, checkboxState);

  if (numberEditfields >= 2 && editfield2Value)
    block->GetString(eEditfield2Value, editfield2Value);

  if (numberEditfields >= 1 && editfield1Value)
    block->GetString(eEditfield1Value, editfield1Value);

  return rv;
}

// XXX use passwordRealm and savePassword, here and in
// PromptUsernameAndPassword and PromptPassword
NS_IMETHODIMP
nsPromptService::Prompt(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                   const PRUnichar *text, const PRUnichar *passwordRealm,
                   PRUint32 savePassword, const PRUnichar *defaultText,
                   PRUnichar **result, PRBool *_retval)
{
  nsresult rv;
  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt(eNumberButtons, 2);
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  nsString url; url.AssignWithConversion(kQuestionIconURL);
  block->SetString(eIconURL, url.GetUnicode());
  block->SetInt(eNumberEditfields, 1);
  block->SetString(eEditfield1Value, defaultText);

  rv = DoDialog(parent, block, kPromptURL);

  block->GetString(eEditfield1Value, result);
  PRInt32 tempInt = 0;
  block->GetInt(eButtonPressed, &tempInt);
  *_retval = tempInt ? PR_FALSE : PR_TRUE;

  return rv;
}

NS_IMETHODIMP
nsPromptService::PromptUsernameAndPassword(nsIDOMWindow *parent,
                   const PRUnichar *dialogTitle, const PRUnichar *text,
                   const PRUnichar *passwordRealm, PRUint32 savePassword,
                   PRUnichar **user, PRUnichar **pwd, PRBool *_retval)

{
  nsresult rv;
  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt(eNumberButtons, 2);
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  nsString url;
  url.AssignWithConversion(kQuestionIconURL);
  block->SetString(eIconURL, url.GetUnicode());
  block->SetInt( eNumberEditfields, 2 );
  block->SetString(eEditfield1Value, *user);
  block->SetString(eEditfield2Value, *pwd);

  rv = DoDialog(parent, block, kPromptURL);

  block->GetString(eEditfield1Value, user);
  block->GetString(eEditfield2Value, pwd);
  PRInt32 tempInt = 0;
  block->GetInt(eButtonPressed, &tempInt);
  *_retval = tempInt ? PR_FALSE : PR_TRUE;

  return rv;
}

NS_IMETHODIMP nsPromptService::PromptPassword(nsIDOMWindow *parent,
                const PRUnichar *dialogTitle, const PRUnichar *text,
                const PRUnichar *passwordRealm, PRUint32 savePassword,
                PRUnichar **pwd, PRBool *_retval)
{	
  nsresult rv;
  ParamBlock block;
  rv = block.Init();
  if (NS_FAILED(rv))
    return rv;

  block->SetInt(eNumberButtons, 2);
  block->SetString(eMsg, text);

  block->SetString(eDialogTitle, dialogTitle);

  nsString url;
  url.AssignWithConversion(kQuestionIconURL);
  block->SetString(eIconURL, url.GetUnicode());
  block->SetInt(eNumberEditfields, 1);
  block->SetInt(eEditField1Password, 1);

  rv = DoDialog(parent, block, kPromptURL);

  block->GetString(eEditfield1Value, pwd);
  PRInt32 tempInt = 0;
  block->GetInt(eButtonPressed, &tempInt);
  *_retval = tempInt ? PR_FALSE : PR_TRUE;

  return rv;
}

NS_IMETHODIMP
nsPromptService::Select(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                   const PRUnichar* text, PRUint32 count,
                   const PRUnichar **selectList, PRInt32 *outSelection,
                   PRBool *_retval)
{	
  nsresult rv;
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
    const PRUnichar* text = temp.GetUnicode();
    block->SetString(i, text);
  }

  *outSelection = -1;
  rv = DoDialog(parent, block, kSelectPromptURL);

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
  mWatcher->OpenWindow(aParent, aChromeURL, "_blank",
              "centerscreen,chrome,modal,titlebar", arguments,
              getter_AddRefs(dialog));

  return rv;
}
 
