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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIComponentManager.h"
#include "nsEditorController.h"
#include "nsIEditor.h"
#include "nsIEditorMailSupport.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMSelection.h"
#include "nsIHTMLEditor.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"

#include "nsISelectionController.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"

#include "nsEditorCommands.h"


#define kMaxStackCommandNameLength 128


NS_IMPL_ADDREF(nsEditorController)
NS_IMPL_RELEASE(nsEditorController)

NS_INTERFACE_MAP_BEGIN(nsEditorController)
	NS_INTERFACE_MAP_ENTRY(nsIController)
	NS_INTERFACE_MAP_ENTRY(nsIEditorController)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditorController)
NS_INTERFACE_MAP_END

nsEditorController::nsEditorController()
: mContent(NULL)
, mEditor(NULL)
{
  NS_INIT_REFCNT();  
}

nsEditorController::~nsEditorController()
{
  // if the singleton command manger has a refcount of 1 at this point, we need to
  // set it to null
}

NS_IMETHODIMP nsEditorController::Init()
{
  nsresult  rv;
 
  // get our ref to the singleton command manager
  rv = GetCommandManager(getter_AddRefs(mCommandManager));
  if (NS_FAILED(rv)) return rv;  
  
  return rv;
}


#define NS_REGISTER_ONE_COMMAND(_cmdClass, _cmdName)                                \
  {                                                                                 \
  _cmdClass* theCmd = new _cmdClass;                                                \
  nsAutoString cmdString(_cmdName);                                                 \
  rv = RegisterOneCommand(cmdString.GetUnicode(), inCommandManager, theCmd);        \
  }

#define NS_REGISTER_FIRST_COMMAND(_cmdClass, _cmdName)                              \
  {                                                                                 \
  _cmdClass* theCmd = new _cmdClass;                                                \
  nsAutoString cmdString(_cmdName);                                                 \
  rv = RegisterOneCommand(cmdString.GetUnicode(), inCommandManager, theCmd);        \

#define NS_REGISTER_NEXT_COMMAND(_cmdClass, _cmdName)                               \
  cmdString = _cmdName;                                                             \
  rv = RegisterOneCommand(cmdString.GetUnicode(), inCommandManager, theCmd);

#define NS_REGISTER_LAST_COMMAND(_cmdClass, _cmdName)                               \
  cmdString = _cmdName;                                                             \
  rv = RegisterOneCommand(cmdString.GetUnicode(), inCommandManager, theCmd);        \
  }


// static
nsresult nsEditorController::RegisterEditorCommands(nsIControllerCommandManager *inCommandManager)
{
  nsresult rv;
 
  // now register all our commands
  NS_REGISTER_ONE_COMMAND(nsUndoCommand, "cmd_undo");
  NS_REGISTER_ONE_COMMAND(nsRedoCommand, "cmd_redo");

  NS_REGISTER_ONE_COMMAND(nsCutCommand, "cmd_cut");
  NS_REGISTER_ONE_COMMAND(nsCopyCommand, "cmd_copy");
  NS_REGISTER_ONE_COMMAND(nsSelectAllCommand, "cmd_selectAll");
  
  NS_REGISTER_FIRST_COMMAND(nsPasteCommand, "cmd_paste");
  NS_REGISTER_LAST_COMMAND(nsPasteCommand, "cmd_pasteQuote");  
  
  NS_REGISTER_FIRST_COMMAND(nsDeleteCommand, "cmd_delete");
  NS_REGISTER_NEXT_COMMAND(nsDeleteCommand, "cmd_deleteCharBackward");
  NS_REGISTER_NEXT_COMMAND(nsDeleteCommand, "cmd_deleteCharForward");
  NS_REGISTER_NEXT_COMMAND(nsDeleteCommand, "cmd_deleteWordBackward");
  NS_REGISTER_NEXT_COMMAND(nsDeleteCommand, "cmd_deleteWordForward");
  NS_REGISTER_NEXT_COMMAND(nsDeleteCommand, "cmd_deleteToBeginningOfLine");
  NS_REGISTER_LAST_COMMAND(nsDeleteCommand, "cmd_deleteToEndOfLine");

  NS_REGISTER_FIRST_COMMAND(nsSelectionMoveCommands, "cmd_scrollTop");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_scrollBottom");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_moveTop");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_moveBottom");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectTop");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_lineNext");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_linePrevious");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectLineNext");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectLinePrevious");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_charPrevious");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_charNext");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectCharPrevious");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectCharNext");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_beginLine");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_endLine");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectBeginLine");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectEndLine");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_wordPrevious");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_wordNext");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectWordPrevious");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectWordNext");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_scrollPageUp");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_scrollPageDown");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_scrollLineUp");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_scrollLineDown");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_movePageUp");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_movePageDown");
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectPageUp");
  NS_REGISTER_LAST_COMMAND(nsSelectionMoveCommands, "cmd_selectPageDown");
    
  return NS_OK;
}

// static
nsresult nsEditorController::RegisterOneCommand(const PRUnichar* aCommandName,
              nsIControllerCommandManager *inCommandManager,
              nsBaseCommand* aCommand)
{
  nsCOMPtr<nsIControllerCommand> editorCommand;

  NS_ADDREF(aCommand);
  nsresult rv = aCommand->QueryInterface(NS_GET_IID(nsIControllerCommand), getter_AddRefs(editorCommand));
  NS_RELEASE(aCommand);
  if (NS_FAILED(rv)) return rv;
  
  return inCommandManager->RegisterCommand(aCommandName, editorCommand);   // this is the owning ref
}


NS_IMETHODIMP nsEditorController::SetContent(nsIHTMLContent *aContent)
{
  // indiscriminately sets mContent, no ref counting here
  mContent = aContent;
  return NS_OK;
}

NS_IMETHODIMP nsEditorController::SetEditor(nsIEditor *aEditor)
{
  // null editors are allowed
  mEditor = aEditor;
  return NS_OK;
}


/* =======================================================================
 * nsIController
 * ======================================================================= */

NS_IMETHODIMP nsEditorController::IsCommandEnabled(const PRUnichar *aCommand, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = PR_FALSE;
  
  nsCOMPtr<nsIEditor> editor;
  NS_ENSURE_SUCCESS(GetEditor(getter_AddRefs(editor)), NS_ERROR_FAILURE);
  // a null editor is a legal state
  if (!editor)
    return NS_OK;     // just say we don't handle the command
  
  if (!mCommandManager)
  {
    NS_ASSERTION(0, "No command handler!");
    return NS_ERROR_UNEXPECTED;
  }
    
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  mCommandManager->FindCommandHandler(aCommand, getter_AddRefs(commandHandler));
    
  if (!commandHandler)
  {
#if DEBUG
    nsCAutoString msg("EditorController asked about a command that it does not handle -- ");
    msg.Append(aCommand);
    NS_WARNING(msg);
#endif
    return NS_OK;    // we don't handle this command
  }
  
  return commandHandler->IsCommandEnabled(aCommand, editor, aResult);
}

NS_IMETHODIMP nsEditorController::SupportsCommand(const PRUnichar *aCommand, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_ARG_POINTER(aResult);

  // XXX: need to check the readonly and disabled states

  *aResult = PR_FALSE;
  
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  mCommandManager->FindCommandHandler(aCommand, getter_AddRefs(commandHandler));

  *aResult = (commandHandler.get() != NULL);
  return NS_OK;
}

NS_IMETHODIMP nsEditorController::DoCommand(const PRUnichar *aCommand)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  nsCOMPtr<nsIEditor> editor;
  nsCOMPtr<nsISelectionController> selCont;
  NS_ENSURE_SUCCESS(GetEditor(getter_AddRefs(editor)), NS_ERROR_FAILURE);
  if (!editor)
  { // Q: What does it mean if there is no editor?  
    // A: It means we've never had focus, so we can't do anything
    return NS_OK;
  }

  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  mCommandManager->FindCommandHandler(aCommand, getter_AddRefs(commandHandler));
  
  if (!commandHandler)
  {
#if DEBUG
    nsCAutoString msg("EditorController asked to do a command that it does not handle -- ");
    msg.Append(aCommand);
    NS_WARNING(msg);
#endif
    return NS_OK;    // we don't handle this command
  }
  
  return commandHandler->DoCommand(aCommand, editor);
}

NS_IMETHODIMP nsEditorController::OnEvent(const PRUnichar *aEventName)
{
  NS_ENSURE_ARG_POINTER(aEventName);

  return NS_OK;
}


NS_IMETHODIMP nsEditorController::GetEditor(nsIEditor ** aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  if (mEditor)
  {
    *aEditor = mEditor;
    NS_ADDREF(*aEditor);
    return NS_OK; 
  }
  return NS_OK;
}

NS_IMETHODIMP nsEditorController::GetSelectionController(nsISelectionController ** aSelCon)
{
  nsCOMPtr<nsIEditor>editor;
  nsresult result = GetEditor(getter_AddRefs(editor));
  if (NS_FAILED(result) || !editor)
    return result ? result : NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  result = editor->GetPresShell(getter_AddRefs(presShell)); 
  if (NS_FAILED(result) || !presShell)
    return result ? result : NS_ERROR_FAILURE;
  
  nsCOMPtr<nsISelectionController> selController = do_QueryInterface(presShell); 
  if (selController)
  {
    *aSelCon = selController;
    NS_ADDREF(*aSelCon);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
/*
  NS_ENSURE_ARG_POINTER(aSelCon);
  nsCOMPtr<nsIDocument> doc; 
  mContent->GetDocument(*getter_AddRefs(doc)); 

  *aSelCon = nsnull;
  if (doc)
  {
    PRInt32 i = doc->GetNumberOfShells(); 
    if (i == 0) 
      return NS_ERROR_FAILURE; 

    nsCOMPtr<nsIPresShell> presShell = getter_AddRefs(doc->GetShellAt(0)); 
    nsCOMPtr<nsISelectionController> selController = do_QueryInterface(presShell); 
    if (selController)
    {
      *aSelCon = selController;
      (*aSelCon)->AddRef();
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
  */
}

NS_IMETHODIMP nsEditorController::GetFrame(nsIGfxTextControlFrame **aFrame)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  //DEPRICATED. WILL REMOVE LATER
/*
  *aFrame = nsnull;
  NS_ENSURE_STATE(mContent);

  nsIFormControlFrame *frame = nsnull;
  NS_ENSURE_SUCCESS( 
    nsGenericHTMLElement::GetPrimaryFrame(mContent, frame), 
    NS_ERROR_FAILURE
  );
  if (!frame) { return NS_ERROR_FAILURE; }

  NS_ENSURE_SUCCESS(
    frame->QueryInterface(NS_GET_IID(nsIGfxTextControlFrame), (void**)aFrame), 
    NS_ERROR_FAILURE
  );
  */
  return NS_OK;
}

PRBool nsEditorController::IsEnabled()
{
  return PR_TRUE; // XXX: need to implement this
  /*
      PRUint32 flags=0;
      NS_ENSURE_SUCCESS(mEditor->GetFlags(&flags), NS_ERROR_FAILURE);
      check eEditorReadonlyBit and eEditorDisabledBit
   */
}

#ifdef XP_MAC
#pragma mark -
#endif

nsWeakPtr nsEditorController::sCommandManager = NULL;

// static
nsresult nsEditorController::GetCommandManager(nsIControllerCommandManager* *outCommandManager)
{
  NS_ENSURE_ARG_POINTER(outCommandManager);

  nsCOMPtr<nsIControllerCommandManager> cmdManager = do_QueryReferent(sCommandManager);
  if (!cmdManager)
  {
    nsresult rv;
    cmdManager = do_CreateInstance("component://netscape/rdf/controller-command-manager", &rv);
    if (NS_FAILED(rv)) return rv;

    // register the commands. This just happens once per instance
    rv = nsEditorController::RegisterEditorCommands(cmdManager);
    if (NS_FAILED(rv)) return rv;

    // save the singleton in our static weak reference
    sCommandManager = getter_AddRefs(NS_GetWeakReference(cmdManager, &rv));
    if (NS_FAILED(rv))  return rv;
  }

  NS_ADDREF(*outCommandManager = cmdManager);
  return NS_OK;
}


