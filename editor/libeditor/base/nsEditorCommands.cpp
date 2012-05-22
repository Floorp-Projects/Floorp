/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsCRT.h"
#include "nsString.h"

#include "nsIEditor.h"
#include "nsIPlaintextEditor.h"
#include "nsIEditorMailSupport.h"
#include "nsISelectionController.h"
#include "nsIClipboard.h"

#include "nsEditorCommands.h"
#include "nsIDocument.h"


#define STATE_ENABLED  "state_enabled"
#define STATE_DATA "state_data"


nsBaseEditorCommand::nsBaseEditorCommand()
{
}

NS_IMPL_ISUPPORTS1(nsBaseEditorCommand, nsIControllerCommand)


NS_IMETHODIMP
nsUndoCommand::IsCommandEnabled(const char * aCommandName, 
                                nsISupports *aCommandRefCon, 
                                bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
  {
    bool isEnabled, isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanUndo(&isEnabled, outCmdEnabled);
  }

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsUndoCommand::DoCommand(const char *aCommandName, nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->Undo(1);
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsUndoCommand::DoCommandParams(const char *aCommandName,
                               nsICommandParams *aParams,
                               nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsUndoCommand::GetCommandStateParams(const char *aCommandName,
                                     nsICommandParams *aParams,
                                     nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}

NS_IMETHODIMP
nsRedoCommand::IsCommandEnabled(const char * aCommandName,
                                nsISupports *aCommandRefCon,
                                bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
  {
    bool isEnabled, isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanRedo(&isEnabled, outCmdEnabled);
  }

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsRedoCommand::DoCommand(const char *aCommandName, nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->Redo(1);
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsRedoCommand::DoCommandParams(const char *aCommandName,
                               nsICommandParams *aParams,
                               nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsRedoCommand::GetCommandStateParams(const char *aCommandName,
                                     nsICommandParams *aParams,
                                     nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}

NS_IMETHODIMP
nsClearUndoCommand::IsCommandEnabled(const char * aCommandName,
                                     nsISupports *refCon, bool *outCmdEnabled)
{ 
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}
  

NS_IMETHODIMP
nsClearUndoCommand::DoCommand(const char *aCommandName, nsISupports *refCon)
{ 
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_NOT_IMPLEMENTED);
  
  editor->EnableUndo(false);  // Turning off undo clears undo/redo stacks.
  editor->EnableUndo(true);   // This re-enables undo/redo.
  
  return NS_OK;
}
                                  
NS_IMETHODIMP                       
nsClearUndoCommand::DoCommandParams(const char *aCommandName,
                                    nsICommandParams *aParams,
                                    nsISupports *refCon)
{
  return DoCommand(aCommandName, refCon);
}
                                                  
NS_IMETHODIMP                                     
nsClearUndoCommand::GetCommandStateParams(const char *aCommandName,
                                          nsICommandParams *aParams,
                                          nsISupports *refCon)
{ 
  NS_ENSURE_ARG_POINTER(aParams);
  
  bool enabled;
  nsresult rv = IsCommandEnabled(aCommandName, refCon, &enabled);
  NS_ENSURE_SUCCESS(rv, rv);
   
  return aParams->SetBooleanValue(STATE_ENABLED, enabled);
}

NS_IMETHODIMP
nsCutCommand::IsCommandEnabled(const char * aCommandName,
                               nsISupports *aCommandRefCon,
                               bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
  {
    bool isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanCut(outCmdEnabled);
  }

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsCutCommand::DoCommand(const char *aCommandName, nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->Cut();
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsCutCommand::DoCommandParams(const char *aCommandName,
                              nsICommandParams *aParams,
                              nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsCutCommand::GetCommandStateParams(const char *aCommandName,
                                    nsICommandParams *aParams,
                                    nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}


NS_IMETHODIMP
nsCutOrDeleteCommand::IsCommandEnabled(const char * aCommandName,
                                       nsISupports *aCommandRefCon,
                                       bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsCutOrDeleteCommand::DoCommand(const char *aCommandName,
                                nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
  {
    nsCOMPtr<nsISelection> selection;
    nsresult rv = editor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(rv) && selection && selection->Collapsed()) {
      return editor->DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
    }
    return editor->Cut();
  }
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsCutOrDeleteCommand::DoCommandParams(const char *aCommandName,
                                      nsICommandParams *aParams,
                                      nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsCutOrDeleteCommand::GetCommandStateParams(const char *aCommandName,
                                            nsICommandParams *aParams,
                                            nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}

NS_IMETHODIMP
nsCopyCommand::IsCommandEnabled(const char * aCommandName,
                                nsISupports *aCommandRefCon,
                                bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->CanCopy(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsCopyCommand::DoCommand(const char *aCommandName, nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->Copy();
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsCopyCommand::DoCommandParams(const char *aCommandName,
                               nsICommandParams *aParams,
                               nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsCopyCommand::GetCommandStateParams(const char *aCommandName,
                                     nsICommandParams *aParams,
                                     nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}

NS_IMETHODIMP
nsCopyOrDeleteCommand::IsCommandEnabled(const char * aCommandName,
                                        nsISupports *aCommandRefCon,
                                        bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsCopyOrDeleteCommand::DoCommand(const char *aCommandName,
                                 nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
  {
    nsCOMPtr<nsISelection> selection;
    nsresult rv = editor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(rv) && selection && selection->Collapsed()) {
      return editor->DeleteSelection(nsIEditor::eNextWord, nsIEditor::eStrip);
    }
    return editor->Copy();
  }
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsCopyOrDeleteCommand::DoCommandParams(const char *aCommandName,
                                       nsICommandParams *aParams,
                                       nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsCopyOrDeleteCommand::GetCommandStateParams(const char *aCommandName,
                                             nsICommandParams *aParams,
                                             nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}

NS_IMETHODIMP
nsPasteCommand::IsCommandEnabled(const char *aCommandName,
                                 nsISupports *aCommandRefCon,
                                 bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
  {
    bool isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanPaste(nsIClipboard::kGlobalClipboard, outCmdEnabled);
  }

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsPasteCommand::DoCommand(const char *aCommandName, nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);
  
  return editor->Paste(nsIClipboard::kGlobalClipboard);
}

NS_IMETHODIMP 
nsPasteCommand::DoCommandParams(const char *aCommandName,
                                nsICommandParams *aParams,
                                nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsPasteCommand::GetCommandStateParams(const char *aCommandName,
                                      nsICommandParams *aParams,
                                      nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}

NS_IMETHODIMP
nsPasteTransferableCommand::IsCommandEnabled(const char *aCommandName,
                                             nsISupports *aCommandRefCon,
                                             bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
  {
    bool isEditable = false;
    nsresult rv = editor->GetIsSelectionEditable(&isEditable);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEditable)
      return editor->CanPasteTransferable(nsnull, outCmdEnabled);
  }

  *outCmdEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
nsPasteTransferableCommand::DoCommand(const char *aCommandName, nsISupports *aCommandRefCon)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsPasteTransferableCommand::DoCommandParams(const char *aCommandName,
                                            nsICommandParams *aParams,
                                            nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsISupports> supports;
  aParams->GetISupportsValue("transferable", getter_AddRefs(supports));
  NS_ENSURE_TRUE(supports, NS_ERROR_FAILURE);

  nsCOMPtr<nsITransferable> trans = do_QueryInterface(supports);
  NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);

  return editor->PasteTransferable(trans);
}

NS_IMETHODIMP 
nsPasteTransferableCommand::GetCommandStateParams(const char *aCommandName,
                                                  nsICommandParams *aParams,
                                                  nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  nsCOMPtr<nsITransferable> trans;

  nsCOMPtr<nsISupports> supports;
  aParams->GetISupportsValue("transferable", getter_AddRefs(supports));
  if (supports) {
    trans = do_QueryInterface(supports);
    NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);
  }

  bool canPaste;
  nsresult rv = editor->CanPasteTransferable(trans, &canPaste);
  NS_ENSURE_SUCCESS(rv, rv);

  return aParams->SetBooleanValue(STATE_ENABLED, canPaste);
}

NS_IMETHODIMP
nsSwitchTextDirectionCommand::IsCommandEnabled(const char *aCommandName,
                                 nsISupports *aCommandRefCon,
                                 bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
nsSwitchTextDirectionCommand::DoCommand(const char *aCommandName, nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  return editor->SwitchTextDirection();
}

NS_IMETHODIMP 
nsSwitchTextDirectionCommand::DoCommandParams(const char *aCommandName,
                                nsICommandParams *aParams,
                                nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsSwitchTextDirectionCommand::GetCommandStateParams(const char *aCommandName,
                                      nsICommandParams *aParams,
                                      nsISupports *aCommandRefCon)
{
  bool canSwitchTextDirection = true;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canSwitchTextDirection);
  return aParams->SetBooleanValue(STATE_ENABLED, canSwitchTextDirection);
}

NS_IMETHODIMP
nsDeleteCommand::IsCommandEnabled(const char * aCommandName,
                                  nsISupports *aCommandRefCon,
                                  bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  *outCmdEnabled = false;

  // we can delete when we can cut
  NS_ENSURE_TRUE(editor, NS_OK);
    
  bool isEditable = false;
  nsresult rv = editor->GetIsSelectionEditable(&isEditable);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isEditable)
    return NS_OK;
  else if (!nsCRT::strcmp(aCommandName,"cmd_delete"))
    return editor->CanCut(outCmdEnabled);
  else if (!nsCRT::strcmp(aCommandName,"cmd_forwardDelete"))
    return editor->CanCut(outCmdEnabled);
  else if (!nsCRT::strcmp(aCommandName,"cmd_deleteCharBackward"))
    *outCmdEnabled = true;
  else if (!nsCRT::strcmp(aCommandName,"cmd_deleteCharForward"))
    *outCmdEnabled = true;
  else if (!nsCRT::strcmp(aCommandName,"cmd_deleteWordBackward"))
    *outCmdEnabled = true;
  else if (!nsCRT::strcmp(aCommandName,"cmd_deleteWordForward"))
    *outCmdEnabled = true;
  else if (!nsCRT::strcmp(aCommandName,"cmd_deleteToBeginningOfLine"))
    *outCmdEnabled = true;
  else if (!nsCRT::strcmp(aCommandName,"cmd_deleteToEndOfLine"))
    *outCmdEnabled = true;  

  return NS_OK;
}


NS_IMETHODIMP
nsDeleteCommand::DoCommand(const char *aCommandName, nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);
    
  nsIEditor::EDirection deleteDir = nsIEditor::eNone;
  
  if (!nsCRT::strcmp("cmd_delete",aCommandName))
    deleteDir = nsIEditor::ePrevious;
  else if (!nsCRT::strcmp("cmd_forwardDelete", aCommandName))
    deleteDir = nsIEditor::eNext;
  else if (!nsCRT::strcmp("cmd_deleteCharBackward",aCommandName))
    deleteDir = nsIEditor::ePrevious;
  else if (!nsCRT::strcmp("cmd_deleteCharForward",aCommandName))
    deleteDir = nsIEditor::eNext;
  else if (!nsCRT::strcmp("cmd_deleteWordBackward",aCommandName))
    deleteDir = nsIEditor::ePreviousWord;
  else if (!nsCRT::strcmp("cmd_deleteWordForward",aCommandName))
    deleteDir = nsIEditor::eNextWord;
  else if (!nsCRT::strcmp("cmd_deleteToBeginningOfLine",aCommandName))
    deleteDir = nsIEditor::eToBeginningOfLine;
  else if (!nsCRT::strcmp("cmd_deleteToEndOfLine",aCommandName))
    deleteDir = nsIEditor::eToEndOfLine;

  return editor->DeleteSelection(deleteDir, nsIEditor::eStrip);
}

NS_IMETHODIMP 
nsDeleteCommand::DoCommandParams(const char *aCommandName,
                                 nsICommandParams *aParams,
                                 nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsDeleteCommand::GetCommandStateParams(const char *aCommandName,
                                       nsICommandParams *aParams,
                                       nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}

NS_IMETHODIMP
nsSelectAllCommand::IsCommandEnabled(const char * aCommandName,
                                     nsISupports *aCommandRefCon,
                                     bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);

  nsresult rv = NS_OK;
  // You can always select all, unless the selection is editable,
  // and the editable region is empty!
  *outCmdEnabled = true;
  bool docIsEmpty;

  // you can select all if there is an editor which is non-empty
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor) {
    rv = editor->GetDocumentIsEmpty(&docIsEmpty);
    NS_ENSURE_SUCCESS(rv, rv);
    *outCmdEnabled = !docIsEmpty;
  }

  return rv;
}


NS_IMETHODIMP
nsSelectAllCommand::DoCommand(const char *aCommandName,
                              nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->SelectAll();
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsSelectAllCommand::DoCommandParams(const char *aCommandName,
                                    nsICommandParams *aParams,
                                    nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsSelectAllCommand::GetCommandStateParams(const char *aCommandName,
                                          nsICommandParams *aParams,
                                          nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}


NS_IMETHODIMP
nsSelectionMoveCommands::IsCommandEnabled(const char * aCommandName,
                                          nsISupports *aCommandRefCon,
                                          bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
nsSelectionMoveCommands::DoCommand(const char *aCommandName,
                                   nsISupports *aCommandRefCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(aCommandRefCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMDocument> domDoc;
  editor->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (doc) {
    // Most of the commands below (possibly all of them) need layout to
    // be up to date.
    doc->FlushPendingNotifications(Flush_Layout);
  }

  nsCOMPtr<nsISelectionController> selCont;
  nsresult rv = editor->GetSelectionController(getter_AddRefs(selCont)); 
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(selCont, NS_ERROR_FAILURE);

  // complete scroll commands
  if (!nsCRT::strcmp(aCommandName,"cmd_scrollTop"))
    return selCont->CompleteScroll(false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_scrollBottom"))
    return selCont->CompleteScroll(true);

  // complete move commands
  else if (!nsCRT::strcmp(aCommandName,"cmd_moveTop"))
    return selCont->CompleteMove(false, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_moveBottom"))
    return selCont->CompleteMove(true, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectTop"))
    return selCont->CompleteMove(false, true);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectBottom"))
    return selCont->CompleteMove(true, true);

  // line move commands
  else if (!nsCRT::strcmp(aCommandName,"cmd_lineNext"))
    return selCont->LineMove(true, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_linePrevious"))
    return selCont->LineMove(false, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectLineNext"))
    return selCont->LineMove(true, true);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectLinePrevious"))
    return selCont->LineMove(false, true);

  // character move commands
  else if (!nsCRT::strcmp(aCommandName,"cmd_charPrevious"))
    return selCont->CharacterMove(false, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_charNext"))
    return selCont->CharacterMove(true, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectCharPrevious"))
    return selCont->CharacterMove(false, true);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectCharNext"))
    return selCont->CharacterMove(true, true);

  // intra line move commands
  else if (!nsCRT::strcmp(aCommandName,"cmd_beginLine"))
    return selCont->IntraLineMove(false, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_endLine"))
    return selCont->IntraLineMove(true, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectBeginLine"))
    return selCont->IntraLineMove(false, true);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectEndLine"))
    return selCont->IntraLineMove(true, true);
  
  // word move commands
  else if (!nsCRT::strcmp(aCommandName,"cmd_wordPrevious"))
    return selCont->WordMove(false, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_wordNext"))
    return selCont->WordMove(true, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectWordPrevious"))
    return selCont->WordMove(false, true);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectWordNext"))
    return selCont->WordMove(true, true);
  
  // scroll page commands
  else if (!nsCRT::strcmp(aCommandName,"cmd_scrollPageUp"))
    return selCont->ScrollPage(false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_scrollPageDown"))
    return selCont->ScrollPage(true);
  
  // scroll line commands
  else if (!nsCRT::strcmp(aCommandName,"cmd_scrollLineUp"))
    return selCont->ScrollLine(false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_scrollLineDown"))
    return selCont->ScrollLine(true);
  
  // page move commands
  else if (!nsCRT::strcmp(aCommandName,"cmd_movePageUp"))
    return selCont->PageMove(false, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_movePageDown"))
    return selCont->PageMove(true, false);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectPageUp"))
    return selCont->PageMove(false, true);
  else if (!nsCRT::strcmp(aCommandName,"cmd_selectPageDown"))
    return selCont->PageMove(true, true);
    
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsSelectionMoveCommands::DoCommandParams(const char *aCommandName,
                                         nsICommandParams *aParams,
                                         nsISupports *aCommandRefCon)
{
  return DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP 
nsSelectionMoveCommands::GetCommandStateParams(const char *aCommandName,
                                               nsICommandParams *aParams,
                                               nsISupports *aCommandRefCon)
{
  bool canUndo;
  IsCommandEnabled(aCommandName, aCommandRefCon, &canUndo);
  return aParams->SetBooleanValue(STATE_ENABLED,canUndo);
}


NS_IMETHODIMP
nsInsertPlaintextCommand::IsCommandEnabled(const char * aCommandName,
                                           nsISupports *refCon, 
                                           bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = false;
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsInsertPlaintextCommand::DoCommand(const char *aCommandName,
                                    nsISupports *refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInsertPlaintextCommand::DoCommandParams(const char *aCommandName,
                                          nsICommandParams *aParams,
                                          nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  nsCOMPtr<nsIPlaintextEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_NOT_IMPLEMENTED);

  // Get text to insert from command params
  nsAutoString text;
  nsresult rv = aParams->GetStringValue(STATE_DATA, text);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!text.IsEmpty())
    return editor->InsertText(text);

  return NS_OK;
}

NS_IMETHODIMP
nsInsertPlaintextCommand::GetCommandStateParams(const char *aCommandName,
                                                nsICommandParams *aParams,
                                                nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  return aParams->SetBooleanValue(STATE_ENABLED, outCmdEnabled);
}


NS_IMETHODIMP
nsPasteQuotationCommand::IsCommandEnabled(const char * aCommandName,
                                          nsISupports *refCon,
                                          bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(refCon);
  if (editor && mailEditor) {
    PRUint32 flags;
    editor->GetFlags(&flags);
    if (!(flags & nsIPlaintextEditor::eEditorSingleLineMask))
      return editor->CanPaste(nsIClipboard::kGlobalClipboard, outCmdEnabled);
  }

  *outCmdEnabled = false;
  return NS_OK;
}


NS_IMETHODIMP
nsPasteQuotationCommand::DoCommand(const char *aCommandName,
                                   nsISupports *refCon)
{
  nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(refCon);
  if (mailEditor)
    return mailEditor->PasteAsQuotation(nsIClipboard::kGlobalClipboard);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPasteQuotationCommand::DoCommandParams(const char *aCommandName,
                                         nsICommandParams *aParams,
                                         nsISupports *refCon)
{
  nsCOMPtr<nsIEditorMailSupport>  mailEditor = do_QueryInterface(refCon);
  if (mailEditor)
    return mailEditor->PasteAsQuotation(nsIClipboard::kGlobalClipboard);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPasteQuotationCommand::GetCommandStateParams(const char *aCommandName,
                                               nsICommandParams *aParams,
                                               nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (editor)
  {
    bool enabled = false;
    editor->CanPaste(nsIClipboard::kGlobalClipboard, &enabled);
    aParams->SetBooleanValue(STATE_ENABLED, enabled);
  }
 
  return NS_OK;
}

