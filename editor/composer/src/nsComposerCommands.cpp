/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ryan Cassin <rcassin@supernova.org>
 *   Daniel Glazman <glazman@netscape.com>
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


#include "nsIEditor.h"
#include "nsIHTMLEditor.h"

#include "nsIEditorShell.h"

#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"

#include "nsIClipboard.h"

#include "nsCOMPtr.h"

#include "nsComposerCommands.h"
#include "nsIEditorMailSupport.h"
#include "nsReadableUtils.h"
#include "nsICommandParams.h"
#include "nsComponentManagerUtils.h"
#include "nsCRT.h"
//prototype
nsresult GetListState(nsIEditor *aEditor, PRBool *aMixed, PRUnichar **tagStr);
nsresult PasteAsQuotation(nsIEditor *aEditor, PRInt32 aSelectionType);
nsresult RemoveOneProperty(nsIHTMLEditor *aEditor,const nsString& aProp, const nsString &aAttr);
nsresult RemoveTextProperty(nsIEditor *aEditor, const PRUnichar *prop, const PRUnichar *attr);
nsresult SetTextProperty(nsIEditor *aEditor, const PRUnichar *prop, const PRUnichar *attr, const PRUnichar *value);
nsresult GetListItemState(nsIEditor *aEditor, PRBool *aMixed, PRUnichar **_retval);


//defines
#define COMMAND_NAME    NS_LITERAL_STRING("cmd_name")
#define STATE_ENABLED   NS_LITERAL_STRING("state_enabled")
#define STATE_ALL       NS_LITERAL_STRING("state_all")
#define STATE_ATTRIBUTE NS_LITERAL_STRING("state_attribute")
#define STATE_BEGIN     NS_LITERAL_STRING("state_begin")
#define STATE_END       NS_LITERAL_STRING("state_end")
#define STATE_MIXED     NS_LITERAL_STRING("state_mixed")



nsBaseComposerCommand::nsBaseComposerCommand()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS1(nsBaseComposerCommand, nsIControllerCommand)


//--------------------------------------------------------------------------------------------------------------------
nsresult
nsBaseComposerCommand::GetInterfaceNode(const nsAString & nodeID, nsIEditorShell* editorShell, nsIDOMElement **outNode)
//--------------------------------------------------------------------------------------------------------------------
{
  *outNode = nsnull;
  NS_ASSERTION(editorShell, "Should have an editorShell here");

  nsCOMPtr<nsIDOMWindowInternal> webshellWindow;
  editorShell->GetWebShellWindow(getter_AddRefs(webshellWindow));  
  if (!webshellWindow) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMDocument> chromeDoc;
  webshellWindow->GetDocument(getter_AddRefs(chromeDoc));
  if (!chromeDoc) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMElement> elem;
  return chromeDoc->GetElementById(nsAutoString(nodeID), outNode);
}

//--------------------------------------------------------------------------------------------------------------------
nsresult
nsBaseComposerCommand::GetCommandNodeState(const nsAString & aCommandName, nsIEditorShell* editorShell, nsString& outNodeState)
//--------------------------------------------------------------------------------------------------------------------
{
  nsCOMPtr<nsIDOMElement> uiNode;
  GetInterfaceNode(aCommandName, editorShell, getter_AddRefs(uiNode));
  if (!uiNode) return NS_ERROR_FAILURE;

  return uiNode->GetAttribute(NS_LITERAL_STRING("state"), outNodeState);
}


//--------------------------------------------------------------------------------------------------------------------
nsresult
nsBaseComposerCommand::SetCommandNodeState(const nsAString & aCommandName, nsIEditorShell* editorShell, const nsString& inNodeState)
//--------------------------------------------------------------------------------------------------------------------
{
  nsCOMPtr<nsIDOMElement> uiNode;
  GetInterfaceNode(aCommandName, editorShell, getter_AddRefs(uiNode));
  if (!uiNode) return NS_ERROR_FAILURE;
  
  return uiNode->SetAttribute(NS_LITERAL_STRING("state"), inNodeState);
}

//--------------------------------------------------------------------------------------------------------------------
PRBool
nsBaseComposerCommand::EditingHTML(nsIEditorShell* inEditorShell)
//--------------------------------------------------------------------------------------------------------------------
{
  nsXPIDLCString  mimeType;
  inEditorShell->GetContentsMIMEType(getter_Copies(mimeType));
  if (nsCRT::strcmp(mimeType, "text/html") != 0)
    return PR_FALSE;

  PRBool sourceMode = PR_FALSE;
  inEditorShell->IsHTMLSourceMode(&sourceMode);
  if (sourceMode)
    return PR_FALSE;
    
  return PR_TRUE;
}

PRBool
nsBaseComposerCommand::EditingHTML(nsIEditor* inEditor)
//--------------------------------------------------------------------------------------------------------------------
{
  //look at mime type
  return PR_TRUE;
}


#ifdef XP_MAC
#pragma mark -
#endif


nsBaseStateUpdatingCommand::nsBaseStateUpdatingCommand(const char* aTagName)
: nsBaseComposerCommand()
, mTagName(aTagName)
, mGotState(PR_FALSE)
, mState(PR_FALSE)
{
}

nsBaseStateUpdatingCommand::~nsBaseStateUpdatingCommand()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsBaseStateUpdatingCommand, nsBaseComposerCommand, nsIStateUpdatingControllerCommand);

NS_IMETHODIMP
nsBaseStateUpdatingCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;  
 
  if (editorShell && EditingHTML(editorShell))
  {
    *outCmdEnabled = PR_TRUE;
    // also udpate the command state. 
    UpdateCommandState(aCommandName, refCon);
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsBaseStateUpdatingCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  if (!editorShell) return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = ToggleState(editorShell, mTagName);
  if (NS_FAILED(rv)) return rv;
  
  return UpdateCommandState(aCommandName, refCon);
}

NS_IMETHODIMP
nsBaseStateUpdatingCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) 
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv = ToggleState(editor, mTagName);
  if (NS_FAILED(rv)) return rv;
  
  return NS_OK;
}

NS_IMETHODIMP
nsBaseStateUpdatingCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  nsresult rv = NS_OK;
  if (editor)
  {
    rv = GetCurrentState(editor, mTagName, aParams);
  }
  return rv;
}

NS_IMETHODIMP
nsBaseStateUpdatingCommand::UpdateCommandState(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  nsresult rv = NS_OK;
  if (editorShell)
  {
    PRBool stateIsSet;
    rv = GetCurrentState(editorShell, mTagName, stateIsSet);
    if (NS_FAILED(rv)) return rv;

    if (!mGotState || (stateIsSet != mState))
    {
      // poke the UI
      SetCommandNodeState(aCommandName, editorShell, NS_ConvertASCIItoUCS2(stateIsSet ? "true" : "false"));

      mGotState = PR_TRUE;
      mState = stateIsSet;
    }
  }
  return rv;
}


/*
#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsCloseCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    *outCmdEnabled = PR_TRUE;   // check if loading etc?
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsCloseCommand::DoCommand(const nsAString & aCommandName, const nsAString & aCommandParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  
  if (editorShell)
  {
    PRBool wasConfirmed;
    rv = editorShell->CloseWindow(&wasConfirmed);
  }
  
  return rv;  
}
*/

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsPasteQuotationCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
      editor->CanPaste(nsIClipboard::kGlobalClipboard, outCmdEnabled);
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsPasteQuotationCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    rv = editorShell->PasteAsQuotation(nsIClipboard::kGlobalClipboard);
  }
  
  return rv;  
}

NS_IMETHODIMP
nsPasteQuotationCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    rv = PasteAsQuotation(editor,nsIClipboard::kGlobalClipboard);
  }
  
  return rv;  
}

NS_IMETHODIMP
nsPasteQuotationCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  PRBool enabled = PR_FALSE;
  if (editor)
  {
    editor->CanPaste(nsIClipboard::kGlobalClipboard, &enabled);
    aParams->SetBooleanValue(STATE_ENABLED,enabled);
  }
 
  return NS_OK;
}

#ifdef XP_MAC
#pragma mark -
#endif


nsStyleUpdatingCommand::nsStyleUpdatingCommand(const char* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsStyleUpdatingCommand::GetCurrentState(nsIEditorShell *aEditorShell, const char* aTagName, PRBool& outStyleSet)
{
  NS_ASSERTION(aEditorShell, "Need editor shell here");
  nsresult rv = NS_OK;

  PRBool firstOfSelectionHasProp = PR_FALSE;
  PRBool anyOfSelectionHasProp = PR_FALSE;
  PRBool allOfSelectionHasProp = PR_FALSE;

  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom(aTagName));
  rv = htmlEditor->GetInlineProperty(styleAtom, NS_LITERAL_STRING(""), NS_LITERAL_STRING(""), &firstOfSelectionHasProp, &anyOfSelectionHasProp, &allOfSelectionHasProp);
  outStyleSet = allOfSelectionHasProp;			// change this to alter the behaviour

  return rv;
}
 
nsresult
nsStyleUpdatingCommand::GetCurrentState(nsIEditor *aEditor, const char* aTagName, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need editor shell here");
  nsresult rv = NS_OK;

  PRBool firstOfSelectionHasProp = PR_FALSE;
  PRBool anyOfSelectionHasProp = PR_FALSE;
  PRBool allOfSelectionHasProp = PR_FALSE;

  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_NOT_INITIALIZED;
  
  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom(aTagName));
  rv = htmlEditor->GetInlineProperty(styleAtom, NS_LITERAL_STRING(""), NS_LITERAL_STRING(""), &firstOfSelectionHasProp, &anyOfSelectionHasProp, &allOfSelectionHasProp);\
  aParams->SetBooleanValue(STATE_ALL,allOfSelectionHasProp);
  aParams->SetBooleanValue(STATE_BEGIN,firstOfSelectionHasProp);
  aParams->SetBooleanValue(STATE_END,allOfSelectionHasProp);//not completely accurate
  aParams->SetBooleanValue(STATE_MIXED,anyOfSelectionHasProp && !allOfSelectionHasProp);
  return NS_OK;
}

nsresult
nsStyleUpdatingCommand::ToggleState(nsIEditorShell *aEditorShell, const char* aTagName)
{
  PRBool styleSet;
  nsresult rv = GetCurrentState(aEditorShell, aTagName, styleSet);
  if (NS_FAILED(rv)) return rv;
  
  nsAutoString tagName; tagName.AssignWithConversion(aTagName);
  if (styleSet)
    rv = aEditorShell->RemoveTextProperty(tagName.get(), nsnull);
  else
  {
    // Superscript and Subscript styles are mutually exclusive
    nsAutoString removeName; 
    aEditorShell->BeginBatchChanges();

    if (tagName.Equals(NS_LITERAL_STRING("sub")))
    {
      removeName.AssignWithConversion("sup");
      rv = aEditorShell->RemoveTextProperty(tagName.get(), nsnull);
    } 
    else if (tagName.Equals(NS_LITERAL_STRING("sup")))
    {
      removeName.AssignWithConversion("sub");
      rv = aEditorShell->RemoveTextProperty(tagName.get(), nsnull);
    }
    if (NS_SUCCEEDED(rv))
      rv = aEditorShell->SetTextProperty(tagName.get(), nsnull, nsnull);

    aEditorShell->EndBatchChanges();
  }

  return rv;
}

nsresult
nsStyleUpdatingCommand::ToggleState(nsIEditor *aEditor, const char* aTagName)
{
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor)
    return NS_ERROR_NO_INTERFACE;
  PRBool styleSet;
  //create some params now...
  nsresult rv;
  nsCOMPtr<nsICommandParams> params = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
       if (NS_FAILED(rv) || !params)
    return rv;

  rv = GetCurrentState(aEditor, aTagName, params);
  if (NS_FAILED(rv)) 
    return rv;
  rv = params->GetBooleanValue(STATE_ALL,&styleSet);
  if (NS_FAILED(rv)) 
    return rv;
  nsAutoString tagName; tagName.AssignWithConversion(aTagName);
  if (styleSet)
    rv = RemoveTextProperty(aEditor, tagName.get(), nsnull);
  else
  {
    // Superscript and Subscript styles are mutually exclusive
    nsAutoString removeName; 
    aEditor->BeginTransaction();

    if (tagName.Equals(NS_LITERAL_STRING("sub")))
    {
      removeName.AssignWithConversion("sup");
      rv = RemoveTextProperty(aEditor,tagName.get(), nsnull);
    } 
    else if (tagName.Equals(NS_LITERAL_STRING("sup")))
    {
      removeName.AssignWithConversion("sub");
      rv = RemoveTextProperty(aEditor, tagName.get(), nsnull);
    }
    if (NS_SUCCEEDED(rv))
      rv = SetTextProperty(aEditor,tagName.get(), nsnull, nsnull);

    aEditor->EndTransaction();
  }

  return rv;
}
#ifdef XP_MAC
#pragma mark -
#endif

nsListCommand::nsListCommand(const char* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsListCommand::GetCurrentState(nsIEditorShell *aEditorShell, const char* aTagName, PRBool& outInList)
{
  NS_ASSERTION(aEditorShell, "Need editorShell here");

  PRBool bMixed;
  PRUnichar *tagStr;
  nsresult rv = aEditorShell->GetListState(&bMixed, &tagStr);
  if (NS_FAILED(rv)) return rv;

  // Need to use mTagName????
  outInList = (0 == nsCRT::strcmp(tagStr, NS_ConvertASCIItoUCS2(mTagName).get()));

  if (tagStr) nsCRT::free(tagStr);

  return NS_OK;
}

nsresult
nsListCommand::GetCurrentState(nsIEditor *aEditor, const char* aTagName, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need editor here");

  PRBool bMixed;
  PRUnichar *tagStr;
  nsresult rv = GetListState(aEditor,&bMixed, &tagStr);
  if (NS_FAILED(rv)) return rv;

  // Need to use mTagName????
  PRBool inList = (0 == nsCRT::strcmp(tagStr, NS_ConvertASCIItoUCS2(mTagName).get()));
  aParams->SetBooleanValue(STATE_ALL, !bMixed && inList);
  aParams->SetBooleanValue(STATE_MIXED, bMixed);
  if (tagStr) nsCRT::free(tagStr);
  return NS_OK;
}

nsresult
nsListCommand::ToggleState(nsIEditorShell *aEditorShell, const char* aTagName)
{
  PRBool inList;
  // Need to use mTagName????
  nsresult rv = GetCurrentState(aEditorShell, mTagName, inList);
  if (NS_FAILED(rv)) return rv;
  nsAutoString listType; listType.AssignWithConversion(mTagName);

  if (inList)
    rv = aEditorShell->RemoveList(listType.get());    
  else
    rv = aEditorShell->MakeOrChangeList(listType.get(), PR_FALSE, nsnull);
    
  return rv;
}

nsresult
nsListCommand::ToggleState(nsIEditor *aEditor, const char* aTagName)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(aEditor);
  if (!editor)
    return NS_NOINTERFACE;
  PRBool inList;
  // Need to use mTagName????
  nsresult rv;
  nsCOMPtr<nsICommandParams> params = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
       if (NS_FAILED(rv) || !params)
    return rv;
  rv = GetCurrentState(aEditor, mTagName, params);
  rv = params->GetBooleanValue(STATE_ALL,&inList);
  if (NS_FAILED(rv)) 
    return rv;

  nsAutoString listType; listType.AssignWithConversion(mTagName);
  nsString empty;
  if (inList)
    rv = editor->RemoveList(listType);    
  else
    rv = editor->MakeOrChangeList(listType, PR_FALSE, empty);
    
  return rv;
}

#ifdef XP_MAC
#pragma mark -
#endif

nsListItemCommand::nsListItemCommand(const char* aTagName)
: nsBaseStateUpdatingCommand(aTagName)
{
}

nsresult
nsListItemCommand::GetCurrentState(nsIEditorShell *aEditorShell, const char* aTagName, PRBool& outInList)
{
  NS_ASSERTION(aEditorShell, "Need editorShell here");

  PRBool bMixed;
  PRUnichar *tagStr;
  nsresult rv = aEditorShell->GetListItemState(&bMixed, &tagStr);
  if (NS_FAILED(rv)) return rv;

  outInList = (0 == nsCRT::strcmp(tagStr, NS_ConvertASCIItoUCS2(mTagName).get()));

  if (tagStr) nsCRT::free(tagStr);

  return NS_OK;
}

nsresult
nsListItemCommand::ToggleState(nsIEditorShell *aEditorShell, const char* aTagName)
{
  NS_ASSERTION(aEditorShell, "Need editorShell here");
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  if (!editor) return NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_NOT_INITIALIZED;

  PRBool inList;
  // Need to use mTagName????
  nsresult rv = GetCurrentState(aEditorShell, mTagName, inList);
  if (NS_FAILED(rv)) return rv;
  
  if (inList)
  {
    // To remove a list, first get what kind of list we're in
    PRBool bMixed;
    PRUnichar *tagStr;
    rv = aEditorShell->GetListState(&bMixed, &tagStr);
    if (NS_FAILED(rv)) return rv; 
    if (tagStr)
    {
      if (!bMixed)
      {
        rv = htmlEditor->RemoveList(nsDependentString(tagStr));    
      }
      nsCRT::free(tagStr);
    }
  }
  else
  {
    nsAutoString itemType; itemType.AssignWithConversion(mTagName);
    // Set to the requested paragraph type
    //XXX Note: This actually doesn't work for "LI",
    //    but we currently don't use this for non DL lists anyway.
    // Problem: won't this replace any current block paragraph style?
    rv = htmlEditor->SetParagraphFormat(itemType);
  }
    
  return rv;
}

nsresult
nsListItemCommand::GetCurrentState(nsIEditor *aEditor, const char* aTagName, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need editor here");

  PRBool bMixed;
  PRUnichar *tagStr;
  nsresult rv = GetListItemState(aEditor,&bMixed, &tagStr);
  if (NS_FAILED(rv)) return rv;

  PRBool inList = (0 == nsCRT::strcmp(tagStr, NS_ConvertASCIItoUCS2(mTagName).get()));
  aParams->SetBooleanValue(STATE_ALL, !bMixed && inList);
  aParams->SetBooleanValue(STATE_MIXED, bMixed);

  if (tagStr) nsCRT::free(tagStr);

  return NS_OK;
}

nsresult
nsListItemCommand::ToggleState(nsIEditor *aEditor, const char* aTagName)
{
  NS_ASSERTION(aEditor, "Need editor here");
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_NOT_INITIALIZED;

  PRBool inList;
  // Need to use mTagName????
  nsresult rv;
  nsCOMPtr<nsICommandParams> params = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
       if (NS_FAILED(rv) || !params)
    return rv;
  rv = GetCurrentState(aEditor, mTagName, params);
  rv = params->GetBooleanValue(STATE_ALL,&inList);
  if (NS_FAILED(rv)) 
    return rv;
  if (NS_FAILED(rv)) return rv;
  
  if (inList)
  {
    // To remove a list, first get what kind of list we're in
    PRBool bMixed;
    PRUnichar *tagStr;
    rv = GetListState(aEditor,&bMixed, &tagStr);
    if (NS_FAILED(rv)) return rv; 
    if (tagStr)
    {
      if (!bMixed)
      {
        rv = htmlEditor->RemoveList(nsDependentString(tagStr));    
      }
      nsCRT::free(tagStr);
    }
  }
  else
  {
    nsAutoString itemType; itemType.AssignWithConversion(mTagName);
    // Set to the requested paragraph type
    //XXX Note: This actually doesn't work for "LI",
    //    but we currently don't use this for non DL lists anyway.
    // Problem: won't this replace any current block paragraph style?
    rv = htmlEditor->SetParagraphFormat(itemType);
  }
    
  return rv;
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsRemoveListCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell && EditingHTML(editorShell))
  {
    // It is enabled if we are in any list type
    PRBool bMixed;
    PRUnichar *tagStr;
    nsresult rv = editorShell->GetListState(&bMixed, &tagStr);
    if (NS_FAILED(rv)) return rv;

    if (bMixed)
      *outCmdEnabled = PR_TRUE;
    else
      *outCmdEnabled = (tagStr && *tagStr);
    
    if (tagStr) nsCRT::free(tagStr);
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsRemoveListCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
    // This removes any list type
    rv = editorShell->RemoveList(nsnull);

  return rv;  
}

NS_IMETHODIMP
nsRemoveListCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    // This removes any list type
    nsString aListType(nsnull);
    rv = editor->RemoveList(aListType);
  }

  return rv;  
}

NS_IMETHODIMP
nsRemoveListCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsString tString;
  PRBool outCmdEnabled = PR_FALSE;
  aParams->GetStringValue(COMMAND_NAME,tString);
  IsCommandEnabled(tString, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsIndentCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell && EditingHTML(editorShell))
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
      *outCmdEnabled = PR_TRUE;     // can always indent (I guess)
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsIndentCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    NS_NAMED_LITERAL_STRING(indentStr, "indent");
    rv = editorShell->Indent(indentStr.get());
  }
  
  return rv;  
}

NS_IMETHODIMP
nsIndentCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    NS_NAMED_LITERAL_STRING(indentStr, "indent");
    nsAutoString aIndent(indentStr);
    nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
    if (htmlEditor)
      rv = htmlEditor->Indent(aIndent);
  }
  
  return rv;  
}

NS_IMETHODIMP
nsIndentCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsString tString;
  PRBool outCmdEnabled = PR_FALSE;
  aParams->GetStringValue(COMMAND_NAME,tString);
  IsCommandEnabled(tString, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}


//OUTDENT

NS_IMETHODIMP
nsOutdentCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell && EditingHTML(editorShell))
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
    if (htmlEditor)
    {
      PRBool canIndent, canOutdent;
      htmlEditor->GetIndentState(&canIndent, &canOutdent);
      
      *outCmdEnabled = canOutdent;
    }
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsOutdentCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell && EditingHTML(editorShell))
  {
    NS_NAMED_LITERAL_STRING(indentStr, "outdent");
    rv = editorShell->Indent(indentStr.get());
  }
  
  return rv;  
}

NS_IMETHODIMP
nsOutdentCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor && EditingHTML(editor))
  {
    NS_NAMED_LITERAL_STRING(indentStr, "outdent");
    nsAutoString aIndent(indentStr);
    nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
    if (htmlEditor)
      rv = htmlEditor->Indent(aIndent);
  }
  
  return rv;  
}

NS_IMETHODIMP
nsOutdentCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsString tString;
  PRBool outCmdEnabled = PR_FALSE;
  aParams->GetStringValue(COMMAND_NAME,tString);
  IsCommandEnabled(tString, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

#ifdef XP_MAC
#pragma mark -
#endif


nsMultiStateCommand::nsMultiStateCommand()
: nsBaseComposerCommand()
, mGotState(PR_FALSE)
{
}

nsMultiStateCommand::~nsMultiStateCommand()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsMultiStateCommand, nsBaseComposerCommand, nsIStateUpdatingControllerCommand);

NS_IMETHODIMP
nsMultiStateCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell && EditingHTML(editorShell))
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
    {
      // should be disabled sometimes, like if the current selection is an image
      *outCmdEnabled = PR_TRUE;
    }
  }
    
  nsresult rv = UpdateCommandState(aCommandName, refCon);
  if (NS_FAILED(rv)) {
    *outCmdEnabled = PR_FALSE;
    return NS_OK;
  }
   
  return NS_OK; 
}


NS_IMETHODIMP
nsMultiStateCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
      // we have to grab the state attribute on our command node to find out
      // what format to set the paragraph to
      nsAutoString stateAttribute;    
      rv = GetCommandNodeState(aCommandName, editorShell, stateAttribute);
      if (NS_FAILED(rv)) return rv;

      rv = SetState(editorShell, stateAttribute);
  }
  
  return rv;  
}

NS_IMETHODIMP
nsMultiStateCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
      // we have to grab the state attribute on our command node to find out
      // what format to set the paragraph to
      nsAutoString tValue;
      nsresult rv;
      aParams->GetStringValue(STATE_ATTRIBUTE,tValue);
      rv = SetState(editor, tValue);
  }
  
  return rv;  
}

NS_IMETHODIMP
nsMultiStateCommand::UpdateCommandState(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  nsresult rv = NS_OK;
  if (editorShell)
  {
      nsString  curFormat;
      PRBool    isMixed;
     
      rv = GetCurrentState(editorShell, curFormat, isMixed);
      if (NS_FAILED(rv)) return rv;
      
      if (isMixed)
        curFormat.Assign(NS_LITERAL_STRING("mixed"));
        
      if (!mGotState || (curFormat != mStateString))
      {
        // poke the UI
        SetCommandNodeState(aCommandName, editorShell, curFormat);

        mGotState = PR_TRUE;
        mStateString = curFormat;
      }
  }
  return rv;
}

NS_IMETHODIMP
nsMultiStateCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  nsresult rv = NS_OK;
  if (editor)
  {
      rv = GetCurrentState(editor, aParams);
  }
  return rv;
}

#ifdef XP_MAC
#pragma mark -
#endif

nsParagraphStateCommand::nsParagraphStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsParagraphStateCommand::GetCurrentState(nsIEditorShell *aEditorShell, nsString& outStateString, PRBool& outMixed)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
  return htmlEditor->GetParagraphState(&outMixed, outStateString);
}

nsresult
nsParagraphStateCommand::GetCurrentState(nsIEditor *aEditor, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
  PRBool outMixed;
  nsString outStateString;
  nsresult rv = htmlEditor->GetParagraphState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv))
  {
    aParams->SetBooleanValue(STATE_MIXED,outMixed);
    aParams->SetStringValue(STATE_ATTRIBUTE, outStateString);
  }
  return rv;
}


nsresult
nsParagraphStateCommand::SetState(nsIEditorShell *aEditorShell, nsString& newState)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->SetParagraphFormat(newState);
}

nsresult
nsParagraphStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->SetParagraphFormat(newState);
}

#ifdef XP_MAC
#pragma mark -
#endif

nsFontFaceStateCommand::nsFontFaceStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsFontFaceStateCommand::GetCurrentState(nsIEditorShell *aEditorShell, nsString& outStateString, PRBool& outMixed)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->GetFontFaceState(&outMixed, outStateString);
}

nsresult
nsFontFaceStateCommand::GetCurrentState(nsIEditor *aEditor, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
  nsString outStateString;
  PRBool outMixed;
  nsresult rv = htmlEditor->GetFontFaceState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv))
  {
    aParams->SetBooleanValue(STATE_MIXED,outMixed);
    aParams->SetStringValue(STATE_ATTRIBUTE,outStateString);
  }
  return rv;
}


nsresult
nsFontFaceStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
  
  nsresult rv;
  
  
  nsCOMPtr<nsIAtom> ttAtom = getter_AddRefs(NS_NewAtom("tt"));
  nsCOMPtr<nsIAtom> fontAtom = getter_AddRefs(NS_NewAtom("font"));

  if (newState.Equals(NS_LITERAL_STRING("tt")))
  {
    // The old "teletype" attribute  
    rv = htmlEditor->SetInlineProperty(ttAtom, NS_LITERAL_STRING(""), NS_LITERAL_STRING(""));  
    // Clear existing font face
    rv = htmlEditor->RemoveInlineProperty(fontAtom, NS_LITERAL_STRING("face"));
  }
  else
  {
    // Remove any existing TT nodes
    rv = htmlEditor->RemoveInlineProperty(ttAtom, NS_LITERAL_STRING(""));  

    if (newState.IsEmpty() || newState.Equals(NS_LITERAL_STRING("normal"))) {
      rv = htmlEditor->RemoveInlineProperty(fontAtom, NS_LITERAL_STRING("face"));
    } else {
      rv = htmlEditor->SetInlineProperty(fontAtom, NS_LITERAL_STRING("face"), newState);
    }
  }
  
  return rv;
}
nsresult
nsFontFaceStateCommand::SetState(nsIEditorShell *aEditorShell, nsString& newState)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
  
  nsresult rv;
  
  
  nsCOMPtr<nsIAtom> ttAtom = getter_AddRefs(NS_NewAtom("tt"));
  nsCOMPtr<nsIAtom> fontAtom = getter_AddRefs(NS_NewAtom("font"));

  if (newState.Equals(NS_LITERAL_STRING("tt")))
  {
    // The old "teletype" attribute  
    rv = htmlEditor->SetInlineProperty(ttAtom, NS_LITERAL_STRING(""), NS_LITERAL_STRING(""));  
    // Clear existing font face
    rv = htmlEditor->RemoveInlineProperty(fontAtom, NS_LITERAL_STRING("face"));
  }
  else
  {
    // Remove any existing TT nodes
    rv = htmlEditor->RemoveInlineProperty(ttAtom, NS_LITERAL_STRING(""));  

    if (newState.IsEmpty() || newState.Equals(NS_LITERAL_STRING("normal"))) {
      rv = htmlEditor->RemoveInlineProperty(fontAtom, NS_LITERAL_STRING("face"));
    } else {
      rv = htmlEditor->SetInlineProperty(fontAtom, NS_LITERAL_STRING("face"), newState);
    }
  }
  
  return rv;
}

#ifdef XP_MAC
#pragma mark -
#endif

nsFontColorStateCommand::nsFontColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsFontColorStateCommand::GetCurrentState(nsIEditorShell *aEditorShell, nsString& outStateString, PRBool& outMixed)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->GetFontColorState(&outMixed, outStateString);
}

nsresult
nsFontColorStateCommand::GetCurrentState(nsIEditor *aEditor, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  PRBool outMixed;
  nsString outStateString;
  nsresult rv = htmlEditor->GetFontColorState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv))
  {
    aParams->SetBooleanValue(STATE_MIXED,outMixed);
    aParams->SetStringValue(STATE_ATTRIBUTE,outStateString);
  }
  return rv;
}

nsresult
nsFontColorStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
  
  nsresult rv;
  
  
  nsCOMPtr<nsIAtom> fontAtom = getter_AddRefs(NS_NewAtom("font"));

  if (newState.IsEmpty() || newState.Equals(NS_LITERAL_STRING("normal"))) {
    rv = htmlEditor->RemoveInlineProperty(fontAtom, NS_LITERAL_STRING("color"));
  } else {
    rv = htmlEditor->SetInlineProperty(fontAtom, NS_LITERAL_STRING("color"), newState);
  }
  
  return rv;
}

nsresult
nsFontColorStateCommand::SetState(nsIEditorShell *aEditorShell, nsString& newState)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
  
  nsresult rv;
  
  
  nsCOMPtr<nsIAtom> fontAtom = getter_AddRefs(NS_NewAtom("font"));

  if (newState.IsEmpty() || newState.Equals(NS_LITERAL_STRING("normal"))) {
    rv = htmlEditor->RemoveInlineProperty(fontAtom, NS_LITERAL_STRING("color"));
  } else {
    rv = htmlEditor->SetInlineProperty(fontAtom, NS_LITERAL_STRING("color"), newState);
  }
  
  return rv;
}

#ifdef XP_MAC
#pragma mark -
#endif

nsHighlightColorStateCommand::nsHighlightColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsHighlightColorStateCommand::GetCurrentState(nsIEditorShell *aEditorShell, nsString& outStateString, PRBool& outMixed)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->GetHighlightColorState(&outMixed, outStateString);
}

nsresult
nsHighlightColorStateCommand::GetCurrentState(nsIEditor *aEditor, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  PRBool outMixed;
  nsString outStateString;
  nsresult rv = htmlEditor->GetHighlightColorState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv))
  {
    aParams->SetBooleanValue(STATE_MIXED,outMixed);
    aParams->SetStringValue(STATE_ATTRIBUTE,outStateString);
  }
  return rv;
}

nsresult
nsHighlightColorStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");

  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  nsresult rv;

  nsCOMPtr<nsIAtom> fontAtom = getter_AddRefs(NS_NewAtom("font"));

  if (!newState.Length() || newState.Equals(NS_LITERAL_STRING("normal"))) {
    rv = htmlEditor->RemoveInlineProperty(fontAtom, NS_LITERAL_STRING("bgcolor"));
  } else {
    rv = htmlEditor->SetCSSInlineProperty(fontAtom, NS_LITERAL_STRING("bgcolor"), newState);
  }

  return rv;
}

nsresult
nsHighlightColorStateCommand::SetState(nsIEditorShell *aEditorShell, nsString& newState)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");

  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  nsresult rv;

  nsCOMPtr<nsIAtom> fontAtom = getter_AddRefs(NS_NewAtom("font"));

  if (!newState.Length() || newState.Equals(NS_LITERAL_STRING("normal"))) {
    rv = htmlEditor->RemoveInlineProperty(fontAtom, NS_LITERAL_STRING("bgcolor"));
  } else {
    rv = htmlEditor->SetCSSInlineProperty(fontAtom, NS_LITERAL_STRING("bgcolor"), newState);
  }

  return rv;
}

NS_IMETHODIMP
nsHighlightColorStateCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell && EditingHTML(editorShell))
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
    if (!htmlEditor) return NS_ERROR_FAILURE;
    PRBool useCSS;
    htmlEditor->GetIsCSSEnabled(&useCSS);
      
    *outCmdEnabled = useCSS;
  }

  nsresult rv = UpdateCommandState(aCommandName, refCon);
  if (NS_FAILED(rv)) {
    *outCmdEnabled = PR_FALSE;
    return NS_OK;
  }

  return NS_OK;
}


#ifdef XP_MAC
#pragma mark -
#endif

nsBackgroundColorStateCommand::nsBackgroundColorStateCommand()
: nsMultiStateCommand()
{
}

nsresult
nsBackgroundColorStateCommand::GetCurrentState(nsIEditorShell *aEditorShell, nsString& outStateString, PRBool& outMixed)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;


  return htmlEditor->GetBackgroundColorState(&outMixed, outStateString);
}

nsresult
nsBackgroundColorStateCommand::GetCurrentState(nsIEditor *aEditor, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  PRBool outMixed;
  nsString outStateString;
  nsresult rv =  htmlEditor->GetBackgroundColorState(&outMixed, outStateString);
  if (NS_SUCCEEDED(rv))
  {
    aParams->SetBooleanValue(STATE_MIXED, outMixed);
    aParams->SetStringValue(STATE_ATTRIBUTE, outStateString);
  }
  return rv;
}

nsresult
nsBackgroundColorStateCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->SetBackgroundColor(newState);
}

nsresult
nsBackgroundColorStateCommand::SetState(nsIEditorShell *aEditorShell, nsString& newState)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->SetBackgroundColor(newState);
}

#ifdef XP_MAC
#pragma mark -
#endif

nsAlignCommand::nsAlignCommand()
: nsMultiStateCommand()
{
}

nsresult
nsAlignCommand::GetCurrentState(nsIEditorShell *aEditorShell, nsString& outStateString, PRBool& outMixed)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
 
  nsIHTMLEditor::EAlignment firstAlign;
  nsresult rv = htmlEditor->GetAlignment(&outMixed, &firstAlign);
  if (NS_FAILED(rv)) return rv;
  switch (firstAlign)
  {
    default:
    case nsIHTMLEditor::eLeft:
      outStateString.Assign(NS_LITERAL_STRING("left"));
      break;
      
    case nsIHTMLEditor::eCenter:
      outStateString.Assign(NS_LITERAL_STRING("center"));
      break;
      
    case nsIHTMLEditor::eRight:
      outStateString.Assign(NS_LITERAL_STRING("right"));
      break;

    case nsIHTMLEditor::eJustify:
      outStateString.Assign(NS_LITERAL_STRING("justify"));
      break;
  }
  
  return NS_OK;
}

nsresult
nsAlignCommand::GetCurrentState(nsIEditor *aEditor, nsICommandParams *aParams)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;
 
  nsIHTMLEditor::EAlignment firstAlign;
  PRBool outMixed;
  nsString outStateString;
  nsresult rv = htmlEditor->GetAlignment(&outMixed, &firstAlign);
  
  if (NS_FAILED(rv)) 
    return rv;
  
  switch (firstAlign)
  {
    default:
    case nsIHTMLEditor::eLeft:
      outStateString.Assign(NS_LITERAL_STRING("left"));
      break;
      
    case nsIHTMLEditor::eCenter:
      outStateString.Assign(NS_LITERAL_STRING("center"));
      break;
      
    case nsIHTMLEditor::eRight:
      outStateString.Assign(NS_LITERAL_STRING("right"));
      break;

    case nsIHTMLEditor::eJustify:
      outStateString.Assign(NS_LITERAL_STRING("justify"));
      break;
  }
  aParams->SetBooleanValue(STATE_MIXED, outMixed);
  aParams->SetStringValue(STATE_ATTRIBUTE, outStateString);
  return NS_OK;
}

nsresult
nsAlignCommand::SetState(nsIEditor *aEditor, nsString& newState)
{
  NS_ASSERTION(aEditor, "Need an editor shell here");
  
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->Align(newState);
}

nsresult
nsAlignCommand::SetState(nsIEditorShell *aEditorShell, nsString& newState)
{
  NS_ASSERTION(aEditorShell, "Need an editor shell here");
  
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) return NS_ERROR_FAILURE;

  return htmlEditor->Align(newState);
}


#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsRemoveStylesCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  nsCOMPtr<nsIEditor>editor;
  if (editorShell && EditingHTML(editorShell))
  {
    editorShell->GetEditor(getter_AddRefs(editor));
  }
  else
    editor = do_QueryInterface(refCon);

  *outCmdEnabled = PR_FALSE;
  if (editor)
  {
    // test if we have any styles?
    *outCmdEnabled = PR_TRUE;
  }
  
  return NS_OK;
}



NS_IMETHODIMP
nsRemoveStylesCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    rv = editorShell->RemoveTextProperty(NS_LITERAL_STRING("all").get(), NS_LITERAL_STRING("").get());
    if (NS_FAILED(rv)) return rv;
    
    // now get all the style buttons to update
    nsCOMPtr<nsIDOMWindowInternal> contentWindow;
    editorShell->GetContentWindow(getter_AddRefs(contentWindow));
    if (contentWindow)
      contentWindow->UpdateCommands(NS_LITERAL_STRING("style"));
  }
  
  return rv;  
}

NS_IMETHODIMP
nsRemoveStylesCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsString tString;
  aParams->GetStringValue(COMMAND_NAME,tString);
  return DoCommand(tString, refCon);
}

NS_IMETHODIMP
nsRemoveStylesCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsString tString;
  PRBool outCmdEnabled = PR_FALSE;
  aParams->GetStringValue(COMMAND_NAME,tString);
  IsCommandEnabled(tString, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsIncreaseFontSizeCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  nsCOMPtr<nsIEditor>editor;
  if (editorShell && EditingHTML(editorShell))
  {
    editorShell->GetEditor(getter_AddRefs(editor));
  }
  else
    editor = do_QueryInterface(refCon);

  *outCmdEnabled = PR_FALSE;
  if (editor)
  {
    // test if we have any styles?
    *outCmdEnabled = PR_TRUE;
  }
  
  return NS_OK;}


NS_IMETHODIMP
nsIncreaseFontSizeCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    rv = editorShell->IncreaseFontSize();
  }
  
  return rv;  
}

NS_IMETHODIMP
nsIncreaseFontSizeCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    rv = editor->IncreaseFontSize();
  }
  
  return rv;  
}

NS_IMETHODIMP
nsIncreaseFontSizeCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsString tString;
  PRBool outCmdEnabled = PR_FALSE;
  aParams->GetStringValue(COMMAND_NAME,tString);
  IsCommandEnabled(tString, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsDecreaseFontSizeCommand::IsCommandEnabled(const nsAString & aCommandName, nsISupports *refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  nsCOMPtr<nsIEditor>editor;
  if (editorShell && EditingHTML(editorShell))
  {
    editorShell->GetEditor(getter_AddRefs(editor));
  }
  else
    editor = do_QueryInterface(refCon);

  *outCmdEnabled = PR_FALSE;
  if (editor)
  {
    // test if we are at min size?
    *outCmdEnabled = PR_TRUE;
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsDecreaseFontSizeCommand::DoCommand(const nsAString & aCommandName, nsISupports *refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    rv = editorShell->DecreaseFontSize();
  }
  
  return rv;  
}

NS_IMETHODIMP
nsDecreaseFontSizeCommand::DoCommandParams(nsICommandParams *aParams, nsISupports *refCon)
{
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editor)
  {
    rv = editor->DecreaseFontSize();
  }
  
  return rv;  
}

NS_IMETHODIMP
nsDecreaseFontSizeCommand::GetCommandState(nsICommandParams *aParams, nsISupports *refCon)
{
  nsString tString;
  PRBool outCmdEnabled = PR_FALSE;
  aParams->GetStringValue(COMMAND_NAME,tString);
  IsCommandEnabled(tString, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED,outCmdEnabled);
}

/****************************/
//HELPER METHODS
/****************************/

nsresult 
PasteAsQuotation(nsIEditor *aEditor, PRInt32 aSelectionType)
{  
  nsresult  err = NS_ERROR_NOT_IMPLEMENTED;
  
  nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(aEditor);
  if (mailEditor)
    err = mailEditor->PasteAsQuotation(aSelectionType);

  return err;
}

nsresult
GetListState(nsIEditor *aEditor, PRBool *aMixed, PRUnichar **_retval)
{
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(aEditor);
  nsresult err = NS_ERROR_NO_INTERFACE;
  if (htmlEditor)
  {
    PRBool bOL, bUL, bDL;
    err = htmlEditor->GetListState(aMixed, &bOL, &bUL, &bDL);
    if (NS_SUCCEEDED(err))
    {
      if (!*aMixed)
      {
        nsAutoString tagStr;
        if (bOL) 
          tagStr.AssignWithConversion("ol");
        else if (bUL) 
          tagStr.AssignWithConversion("ul");
        else if (bDL) 
          tagStr.AssignWithConversion("dl");
        *_retval = ToNewUnicode(tagStr);
      }
    }  
  }
  return err;
}

nsresult
RemoveOneProperty(nsIHTMLEditor *aEditor,const nsString& aProp, const nsString &aAttr)
{
  nsresult  err = NS_NOINTERFACE;

  if (!aEditor) 
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom(aProp));      /// XXX Hack alert! Look in nsIEditProperty.h for this
  if (! styleAtom) 
    return NS_ERROR_OUT_OF_MEMORY;

  err = aEditor->RemoveInlineProperty(styleAtom, aAttr);

  return err;
}


// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
nsresult
RemoveTextProperty(nsIEditor *aEditor, const PRUnichar *prop, const PRUnichar *attr)
{
  if (!aEditor) 
    return NS_ERROR_NOT_INITIALIZED;
  nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(aEditor);
  if (!editor) 
    return NS_ERROR_NOT_INITIALIZED;
  // OK, I'm really hacking now. This is just so that we can accept 'all' as input.  
  nsAutoString  allStr(prop);
  nsAutoString  aAttr(attr);
  
  ToLowerCase(allStr);
  PRBool    doingAll = (allStr.Equals(NS_LITERAL_STRING("all")));
  nsresult  err = NS_OK;

  if (doingAll)
  {
    err = editor->RemoveAllInlineProperties();
  }
  else
  {
    nsAutoString  aProp(prop);
    err = RemoveOneProperty(editor,aProp, aAttr);
  }
  
  return err;
}

// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
nsresult
SetTextProperty(nsIEditor *aEditor, const PRUnichar *prop, const PRUnichar *attr, const PRUnichar *value)
{
  //static initialization 
  static const PRUnichar sEmptyStr = PRUnichar('\0');
  
  nsresult  err = NS_NOINTERFACE;

  if (!aEditor) 
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIAtom> styleAtom = getter_AddRefs(NS_NewAtom(prop));      /// XXX Hack alert! Look in nsIEditProperty.h for this
  if (! styleAtom) 
    return NS_ERROR_OUT_OF_MEMORY;
 

  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(aEditor,&err);
  if (htmlEditor)
    err = htmlEditor->SetInlineProperty(styleAtom,
                                nsDependentString(attr?attr:&sEmptyStr),
                                nsDependentString(value?value:&sEmptyStr));

  return err;
}

nsresult
GetListItemState(nsIEditor *aEditor, PRBool *aMixed, PRUnichar **_retval)
{
  if (!aMixed || !_retval || !aEditor) 
    return NS_ERROR_NULL_POINTER;
  *_retval = nsnull;
  *aMixed = PR_FALSE;

  nsresult  err = NS_NOINTERFACE;
  nsCOMPtr<nsIHTMLEditor>  htmlEditor = do_QueryInterface(aEditor);
  if (htmlEditor)
  {
    PRBool bLI,bDT,bDD;
    err = htmlEditor->GetListItemState(aMixed, &bLI, &bDT, &bDD);
    if (NS_SUCCEEDED(err))
    {
      if (!*aMixed)
      {
        nsAutoString tagStr;
        if (bLI) tagStr.Assign(NS_LITERAL_STRING("li"));
        else if (bDT) tagStr.Assign(NS_LITERAL_STRING("dt"));
        else if (bDD) tagStr.Assign(NS_LITERAL_STRING("dd"));
        *_retval = ToNewUnicode(tagStr);
      }
    }  
  }
  return err;
}
