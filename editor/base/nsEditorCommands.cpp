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


#include "nsCRT.h"

#include "nsIEditor.h"
#include "nsIEditorMailSupport.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsIClipboard.h"

#include "nsEditorCommands.h"


nsBaseCommand::nsBaseCommand()
{
  NS_INIT_REFCNT();
}


nsBaseCommand::~nsBaseCommand()
{
}

NS_IMPL_ISUPPORTS(nsBaseCommand, NS_GET_IID(nsIControllerCommand));


NS_IMETHODIMP
nsUndoCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (aEditor)
  {
    PRBool isEnabled;
    return aEditor->CanUndo(isEnabled, *outCmdEnabled);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsUndoCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  if (aEditor)
    return aEditor->Undo(1);
    
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsRedoCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (aEditor)
  {
    PRBool isEnabled;
    return aEditor->CanRedo(isEnabled, *outCmdEnabled);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsRedoCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  if (aEditor)
    return aEditor->Redo(1);
    
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsCutCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (aEditor)
    return aEditor->CanCut(*outCmdEnabled);

  return NS_OK;
}


NS_IMETHODIMP
nsCutCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  if (aEditor)
    return aEditor->Cut();
    
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsCopyCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (aEditor)
    return aEditor->CanCopy(*outCmdEnabled);

  return NS_OK;
}


NS_IMETHODIMP
nsCopyCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  if (aEditor)
    return aEditor->Copy();
    
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsPasteCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (aEditor)
    return aEditor->CanPaste(nsIClipboard::kGlobalClipboard, *outCmdEnabled);

  return NS_OK;
}


NS_IMETHODIMP
nsPasteCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  if (!aEditor)
    return NS_ERROR_FAILURE;
  
  nsresult rv = NS_OK;
  nsAutoString cmdString(aCommand);
  if (cmdString.EqualsWithConversion("cmd_paste"))
    rv = aEditor->Paste(nsIClipboard::kGlobalClipboard);
  else if (cmdString.EqualsWithConversion("cmd_pasteQuote"))
  {
    nsCOMPtr<nsIEditorMailSupport> mailEditor = do_QueryInterface(aEditor, &rv);
    if (mailEditor)
      rv = mailEditor->PasteAsQuotation(nsIClipboard::kGlobalClipboard);
  }
    
  return rv;
}

NS_IMETHODIMP
nsDeleteCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  // we can delete when we can cut
  if (!aEditor)
    return NS_OK;
    
  nsresult rv = NS_OK;
  
  nsAutoString cmdString(aCommand);

  if (cmdString.EqualsWithConversion("cmd_delete"))
    rv = aEditor->CanCut(*outCmdEnabled);
  else if (cmdString.EqualsWithConversion("cmd_deleteCharBackward"))
    *outCmdEnabled = PR_TRUE;
  else if (cmdString.EqualsWithConversion("cmd_deleteCharForward"))
    *outCmdEnabled = PR_TRUE;
  else if (cmdString.EqualsWithConversion("cmd_deleteWordBackward"))
    *outCmdEnabled = PR_TRUE;
  else if (cmdString.EqualsWithConversion("cmd_deleteWordForward"))
    *outCmdEnabled = PR_TRUE;
  else if (cmdString.EqualsWithConversion("cmd_deleteToBeginningOfLine"))
    *outCmdEnabled = PR_TRUE;
  else if (cmdString.EqualsWithConversion("cmd_deleteToEndOfLine"))
    *outCmdEnabled = PR_TRUE;  

  return rv;
}


NS_IMETHODIMP
nsDeleteCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  if (!aEditor)
    return NS_ERROR_FAILURE;
    
  nsAutoString cmdString(aCommand);

  nsIEditor::EDirection deleteDir = nsIEditor::eNone;
  
  if (cmdString.EqualsWithConversion("cmd_delete"))
    deleteDir = nsIEditor::ePrevious;
  else if (cmdString.EqualsWithConversion("cmd_deleteCharBackward"))
    deleteDir = nsIEditor::ePrevious;
  else if (cmdString.EqualsWithConversion("cmd_deleteCharForward"))
    deleteDir = nsIEditor::eNext;
  else if (cmdString.EqualsWithConversion("cmd_deleteWordBackward"))
    deleteDir = nsIEditor::ePreviousWord;
  else if (cmdString.EqualsWithConversion("cmd_deleteWordForward"))
    deleteDir = nsIEditor::eNextWord;
  else if (cmdString.EqualsWithConversion("cmd_deleteToBeginningOfLine"))
    deleteDir = nsIEditor::eToBeginningOfLine;
  else if (cmdString.EqualsWithConversion("cmd_deleteToEndOfLine"))
    deleteDir = nsIEditor::eToEndOfLine;

  return aEditor->DeleteSelection(deleteDir);
}

NS_IMETHODIMP
nsSelectAllCommand::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (aEditor)
    *outCmdEnabled = PR_TRUE;     // you can always select all

  return NS_OK;
}


NS_IMETHODIMP
nsSelectAllCommand::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  if (aEditor)
    return aEditor->SelectAll();
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSelectionMoveCommands::IsCommandEnabled(const PRUnichar *aCommand, nsISupports * refCon, PRBool *outCmdEnabled)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  *outCmdEnabled = PR_FALSE;
  if (!aEditor)
    return NS_ERROR_FAILURE;

  *outCmdEnabled = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP
nsSelectionMoveCommands::DoCommand(const PRUnichar *aCommand, nsISupports * refCon)
{
  nsCOMPtr<nsIEditor> aEditor = do_QueryInterface(refCon);
  if (!aEditor)
    return NS_ERROR_FAILURE;
 
  nsresult rv;
    
  nsCOMPtr<nsISelectionController> selCont;
  rv = aEditor->GetSelectionController(getter_AddRefs(selCont)); 
  if (NS_FAILED(rv) || !selCont)
    return rv?rv:NS_ERROR_FAILURE;
  
  nsAutoString cmdString(aCommand);
  
  // complete scroll commands
  if (cmdString.EqualsWithConversion("cmd_scrollTop"))
    return selCont->CompleteScroll(PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_scrollBottom"))
    return selCont->CompleteScroll(PR_TRUE);

  // complete move commands
  else if (cmdString.EqualsWithConversion("cmd_moveTop"))
    return selCont->CompleteMove(PR_FALSE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_moveBottom"))
    return selCont->CompleteMove(PR_TRUE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_selectTop"))
    return selCont->CompleteMove(PR_FALSE, PR_TRUE);
  else if (cmdString.EqualsWithConversion("cmd_selectBottom"))
    return selCont->CompleteMove(PR_TRUE, PR_TRUE);

  // line move commands
  else if (cmdString.EqualsWithConversion("cmd_lineNext"))
    return selCont->LineMove(PR_TRUE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_linePrevious"))
    return selCont->LineMove(PR_FALSE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_selectLineNext"))
    return selCont->LineMove(PR_TRUE, PR_TRUE);
  else if (cmdString.EqualsWithConversion("cmd_selectLinePrevious"))
    return selCont->LineMove(PR_FALSE, PR_TRUE);

  // character move commands
  else if (cmdString.EqualsWithConversion("cmd_charPrevious"))
    return selCont->CharacterMove(PR_FALSE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_charNext"))
    return selCont->CharacterMove(PR_TRUE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_selectCharPrevious"))
    return selCont->CharacterMove(PR_FALSE, PR_TRUE);
  else if (cmdString.EqualsWithConversion("cmd_selectCharNext"))
    return selCont->CharacterMove(PR_TRUE, PR_TRUE);

  // intra line move commands
  else if (cmdString.EqualsWithConversion("cmd_beginLine"))
    return selCont->IntraLineMove(PR_FALSE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_endLine"))
    return selCont->IntraLineMove(PR_TRUE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_selectBeginLine"))
    return selCont->IntraLineMove(PR_FALSE, PR_TRUE);
  else if (cmdString.EqualsWithConversion("cmd_selectEndLine"))
    return selCont->IntraLineMove(PR_TRUE, PR_TRUE);
  
  // word move commands
  else if (cmdString.EqualsWithConversion("cmd_wordPrevious"))
    return selCont->WordMove(PR_FALSE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_wordNext"))
    return selCont->WordMove(PR_TRUE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_selectWordPrevious"))
    return selCont->WordMove(PR_FALSE, PR_TRUE);
  else if (cmdString.EqualsWithConversion("cmd_selectWordNext"))
    return selCont->WordMove(PR_TRUE, PR_TRUE);
  
  // scroll page commands
  else if (cmdString.EqualsWithConversion("cmd_scrollPageUp"))
    return selCont->ScrollPage(PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_scrollPageDown"))
    return selCont->ScrollPage(PR_TRUE);
  
  // scroll line commands
  else if (cmdString.EqualsWithConversion("cmd_scrollLineUp"))
    return selCont->ScrollLine(PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_scrollLineDown"))
    return selCont->ScrollLine(PR_TRUE);
  
  // page move commands
  else if (cmdString.EqualsWithConversion("cmd_scrollPageUp"))
    return selCont->PageMove(PR_FALSE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_scrollPageDown"))
    return selCont->PageMove(PR_TRUE, PR_FALSE);
  else if (cmdString.EqualsWithConversion("cmd_selectPageUp"))
    return selCont->PageMove(PR_FALSE, PR_TRUE);
  else if (cmdString.EqualsWithConversion("cmd_selectPageDown"))
    return selCont->PageMove(PR_TRUE, PR_TRUE);
    
  return NS_ERROR_FAILURE;
}
