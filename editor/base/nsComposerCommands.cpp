/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   
 */


#include "nsIEditor.h"
#include "nsIHTMLEditor.h"

#include "nsIEditorShell.h"

#include "nsIDOMSelection.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"

#include "nsIClipboard.h"

#include "nsXPIDLString.h"

#include "nsComposerCommands.h"



// utility methods

//---------------------------------------------------------------------------
static PRBool XULNodeExists(nsIEditorShell* aEditorShell, const char* nodeID)
//---------------------------------------------------------------------------
{
  nsCOMPtr<nsIDOMWindow> webshellWindow;
  aEditorShell->GetWebShellWindow(getter_AddRefs(webshellWindow));  

  nsCOMPtr<nsIDOMDocument> chromeDoc;
  webshellWindow->GetDocument(getter_AddRefs(chromeDoc));
  if (!chromeDoc) return PR_FALSE;
  
  nsCOMPtr<nsIDOMElement> elem;
  nsresult rv = chromeDoc->GetElementById( NS_ConvertASCIItoUCS2(nodeID), getter_AddRefs(elem) );

  return NS_SUCCEEDED(rv) && (elem.get() != nsnull);
}

//---------------------------------------------------------------------------
static nsresult SetNodeAttribute(nsIEditorShell* aEditorShell, const PRUnichar* nodeID,
              const PRUnichar* attributeName, const nsString& newValue)
//---------------------------------------------------------------------------
{
  nsCOMPtr<nsIDOMWindow> webshellWindow;
  aEditorShell->GetWebShellWindow(getter_AddRefs(webshellWindow));  

  nsCOMPtr<nsIDOMDocument> chromeDoc;
  webshellWindow->GetDocument(getter_AddRefs(chromeDoc));
  if (!chromeDoc) return PR_FALSE;

  nsCOMPtr<nsIDOMElement> elem;
  nsresult rv = chromeDoc->GetElementById( nsAutoString(nodeID), getter_AddRefs(elem) );
  if (NS_FAILED(rv) || !elem) return rv;
  
  return elem->SetAttribute(nsAutoString(attributeName), newValue);
}


//---------------------------------------------------------------------------
static nsresult GetNodeAttribute(nsIEditorShell* aEditorShell, const PRUnichar* nodeID,
              const PRUnichar* attributeName, nsString& outValue)
//---------------------------------------------------------------------------
{
  nsCOMPtr<nsIDOMWindow> webshellWindow;
  aEditorShell->GetWebShellWindow(getter_AddRefs(webshellWindow));  

  nsCOMPtr<nsIDOMDocument> chromeDoc;
  webshellWindow->GetDocument(getter_AddRefs(chromeDoc));
  if (!chromeDoc) return PR_FALSE;

  nsCOMPtr<nsIDOMElement> elem;
  nsresult rv = chromeDoc->GetElementById( nsAutoString(nodeID), getter_AddRefs(elem) );
  if (NS_FAILED(rv) || !elem) return rv;
  
  return elem->GetAttribute(nsAutoString(attributeName), outValue);
}


#ifdef XP_MAC
#pragma mark -
#endif


nsBaseStateUpdatingCommand::nsBaseStateUpdatingCommand(const char* aTagName)
: nsBaseCommand()
, mTagName(aTagName)
{
}

nsBaseStateUpdatingCommand::~nsBaseStateUpdatingCommand()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsBaseStateUpdatingCommand, nsBaseCommand, nsIStateUpdatingControllerCommand);

NS_IMETHODIMP
nsBaseStateUpdatingCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;  
  if (!editorShell) return NS_OK;
  // check for plain text?
  nsCOMPtr<nsIEditor> editor;
  editorShell->GetEditor(getter_AddRefs(editor));
  if (!editor) return NS_OK;
 
  *outCmdEnabled = PR_TRUE;
    
  // also udpate the command state  
  return UpdateCommandState(aCommand, refCon);
}


NS_IMETHODIMP
nsBaseStateUpdatingCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  if (!editorShell) return NS_ERROR_NOT_INITIALIZED;

  return ToggleState(editorShell, mTagName);
}

NS_IMETHODIMP
nsBaseStateUpdatingCommand::UpdateCommandState(const PRUnichar *aCommandName, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  nsresult rv = NS_OK;
  if (editorShell)
  {
    PRBool stateIsSet;
    rv = GetCurrentState(editorShell, mTagName, stateIsSet);
    if (NS_FAILED(rv)) return rv;

    PRBool oldState  = !stateIsSet;
    
    // what was it last set to?
    void* stateData;
    if (NS_SUCCEEDED(editorShell->GetCommandStateData(aCommandName, &stateData)))
      oldState = (PRBool)stateData;
    
    if (stateIsSet != oldState)
    {
      // poke the UI
      rv = SetNodeAttribute(editorShell, aCommandName, NS_ConvertASCIItoUCS2("state").GetUnicode(),
                  NS_ConvertASCIItoUCS2(stateIsSet ? "true" : "false"));
    
      editorShell->SetCommandStateData(aCommandName, (void*)stateIsSet);
    }
  }
  return rv;
}



#ifdef XP_MAC
#pragma mark -
#endif


NS_IMETHODIMP
nsAlwaysEnabledCommands::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = (editorShell.get() != nsnull);   // enabled if we have an editorShell  
  return NS_OK;
}


NS_IMETHODIMP
nsAlwaysEnabledCommands::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  if (!editorShell) return NS_ERROR_NULL_POINTER;
  
  nsresult rv = NS_OK;
  
  nsAutoString cmdString(aCommand);
  
  /*
  if (cmdString.EqualsWithConversion("cmd_scrollTop"))
    return selCont->CompleteScroll(PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_scrollBottom"))
    return selCont->CompleteScroll(PR_TRUE);
  */

  return rv;  
}

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
nsCloseCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
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

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsPrintingCommands::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (!editorShell) return NS_OK;

  nsAutoString cmdString(aCommand);
  if (cmdString.EqualsWithConversion("cmd_print"))
    *outCmdEnabled = PR_TRUE;
  else if (cmdString.EqualsWithConversion("cmd_printSetup"))
    *outCmdEnabled = PR_FALSE;        // not implemented yet
  else if (cmdString.EqualsWithConversion("cmd_printPreview"))
    *outCmdEnabled = PR_FALSE;        // not implemented yet
  
  return NS_OK;
}


NS_IMETHODIMP
nsPrintingCommands::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  if (!editorShell) return NS_ERROR_NULL_POINTER;
  nsresult rv = NS_OK;
  
  
  nsAutoString cmdString(aCommand);
  if (cmdString.EqualsWithConversion("cmd_print"))
    rv = editorShell->Print();
  else if (cmdString.EqualsWithConversion("cmd_printSetup"))
    rv = NS_ERROR_NOT_IMPLEMENTED;
  else if (cmdString.EqualsWithConversion("cmd_printPreview"))
    rv = NS_ERROR_NOT_IMPLEMENTED;
  
  return rv;  
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsSaveCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    PRBool docModified;
    if (NS_SUCCEEDED(editorShell->GetDocumentModified(&docModified)))
      *outCmdEnabled = docModified;
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsSaveCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  
  if (editorShell)
  {
    PRBool wasSaved;
    rv = editorShell->SaveDocument(PR_FALSE, PR_FALSE, &wasSaved);
  }
  
  return rv;  
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsSaveAsCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = (editorShell.get() != nsnull);  
  return NS_OK;
}


NS_IMETHODIMP
nsSaveAsCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    PRBool wasSaved;
    rv = editorShell->SaveDocument(PR_TRUE, PR_FALSE, &wasSaved);
  }
  
  return rv;  
}


#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsPasteQuotationCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
      editor->CanPaste(nsIClipboard::kGlobalClipboard, *outCmdEnabled);
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsPasteQuotationCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    rv = editorShell->PasteAsQuotation(nsIClipboard::kGlobalClipboard);
  }
  
  return rv;  
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
  rv = htmlEditor->GetInlineProperty(styleAtom, nsnull, nsnull, firstOfSelectionHasProp, anyOfSelectionHasProp, allOfSelectionHasProp);
  outStyleSet = allOfSelectionHasProp;			// change this to alter the behaviour

  return rv;
}
 
nsresult
nsStyleUpdatingCommand::ToggleState(nsIEditorShell *aEditorShell, const char* aTagName)
{
  PRBool styleSet;
  nsresult rv = GetCurrentState(aEditorShell, aTagName, styleSet);
  if (NS_FAILED(rv)) return rv;
  
  nsAutoString tagName; tagName.AssignWithConversion(aTagName);
  if (styleSet)
    rv = aEditorShell->RemoveTextProperty(tagName.GetUnicode(), nsnull);
  else
    rv = aEditorShell->SetTextProperty(tagName.GetUnicode(), nsnull, nsnull);

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
  nsCOMPtr<nsIEditor> editor;
  aEditorShell->GetEditor(getter_AddRefs(editor));
  if (!editor) return NS_ERROR_UNEXPECTED;
  
  nsCOMPtr<nsIDOMSelection>  domSelection;
  editor->GetSelection(getter_AddRefs(domSelection));
  if (!domSelection) return NS_ERROR_UNEXPECTED;
  
  nsCOMPtr<nsIDOMNode> domNode;
  domSelection->GetAnchorNode(getter_AddRefs(domNode));
  if (!domNode) return NS_ERROR_UNEXPECTED;
  
  // tagStr will hold the list state when we're done.
  nsAutoString  tagStr; tagStr.AssignWithConversion("ol");
  nsCOMPtr<nsIDOMElement> parentElement;
  nsresult rv = aEditorShell->GetElementOrParentByTagName(tagStr.GetUnicode(), domNode, getter_AddRefs(parentElement));
  if (NS_FAILED(rv)) return rv;  

  if (!parentElement)
  {
    tagStr.AssignWithConversion("ul");
    rv = aEditorShell->GetElementOrParentByTagName(tagStr.GetUnicode(), domNode, getter_AddRefs(parentElement));
    if (NS_FAILED(rv)) return rv;
    
    if (!parentElement)
      tagStr.Truncate();
  }

  outInList = tagStr.EqualsWithConversion(mTagName);
  return NS_OK;
}


nsresult
nsListCommand::ToggleState(nsIEditorShell *aEditorShell, const char* aTagName)
{
  PRBool inList;
  nsresult rv = GetCurrentState(aEditorShell, aTagName, inList);
  if (NS_FAILED(rv)) return rv;
  
  nsAutoString listType; listType.AssignWithConversion(aTagName);
  if (inList)
    rv = aEditorShell->RemoveList(listType.GetUnicode());    
  else
    rv = aEditorShell->MakeOrChangeList(listType.GetUnicode());
    
  return rv;
}


#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsIndentCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
      *outCmdEnabled = PR_TRUE;     // can always indent (I guess)
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsIndentCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    nsAutoString indentStr; indentStr.AssignWithConversion("indent");
    rv = editorShell->Indent(indentStr.GetUnicode());
  }
  
  return rv;  
}

NS_IMETHODIMP
nsOutdentCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
    {
      // XXX fix me. You can't outdent if you're already at the top level.
      //PRBool canOutdent;
      //editor->CanIndent("outdent", &canOutdent);
      
      *outCmdEnabled = PR_TRUE;
    }
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsOutdentCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    nsAutoString indentStr; indentStr.AssignWithConversion("outdent");
    rv = editorShell->Indent(indentStr.GetUnicode());
  }
  
  return rv;  
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsParagraphStateCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
    {
      *outCmdEnabled = PR_TRUE;
    }
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsParagraphStateCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    // we have to grab the state attribute on our command node to find out
    // what format to set the paragraph to
    nsAutoString stateAttribute;
    rv = GetNodeAttribute(editorShell, aCommand, NS_ConvertASCIItoUCS2("state").GetUnicode(), stateAttribute);
    if (NS_FAILED(rv)) return rv;
    
    rv = editorShell->SetParagraphFormat(stateAttribute.GetUnicode());
  }
  
  return rv;  
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsAlignCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
    {
      *outCmdEnabled = PR_TRUE;
    }
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsAlignCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    // we have to grab the state attribute on our command node to find out
    // what format to set the paragraph to
    nsAutoString stateAttribute;
    rv = GetNodeAttribute(editorShell, aCommand, NS_ConvertASCIItoUCS2("state").GetUnicode(), stateAttribute);
    if (NS_FAILED(rv)) return rv;
    
    rv = editorShell->Align(stateAttribute.GetUnicode());
  }
  
  return rv;  
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsRemoveStylesCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
    {
      // test if we have any styles?
      *outCmdEnabled = PR_TRUE;
    }
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsRemoveStylesCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    rv = editorShell->RemoveTextProperty(NS_ConvertASCIItoUCS2("all").GetUnicode(), NS_ConvertASCIItoUCS2("").GetUnicode());
  }
  
  return rv;  
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsIncreaseFontSizeCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
    {
      // test if we are at the max size?
      *outCmdEnabled = PR_TRUE;
    }
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsIncreaseFontSizeCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    rv = editorShell->IncreaseFontSize();
  }
  
  return rv;  
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsDecreaseFontSizeCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (editorShell)
  {
    nsCOMPtr<nsIEditor> editor;
    editorShell->GetEditor(getter_AddRefs(editor));
    if (editor)
    {
      // test if we are at the min size?
      *outCmdEnabled = PR_TRUE;
    }
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsDecreaseFontSizeCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditorShell> editorShell = do_QueryInterface(refCon);

  nsresult rv = NS_OK;
  if (editorShell)
  {
    rv = editorShell->DecreaseFontSize();
  }
  
  return rv;  
}
