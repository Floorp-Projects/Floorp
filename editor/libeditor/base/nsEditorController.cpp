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
 *   Ryan Cassin <rcassin@supernova.org>
 */

#include "nsIComponentManager.h"
#include "nsEditorController.h"
#include "nsIEditor.h"
#include "nsIEditorShell.h"
#include "nsIEditorMailSupport.h"
#include "nsIFormControlFrame.h"
#include "nsISelection.h"
#include "nsIHTMLEditor.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"

#include "nsISelectionController.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"

#include "nsEditorCommands.h"
#include "nsComposerCommands.h"


NS_IMPL_ADDREF(nsEditorController)
NS_IMPL_RELEASE(nsEditorController)

NS_INTERFACE_MAP_BEGIN(nsEditorController)
	NS_INTERFACE_MAP_ENTRY(nsIController)
	NS_INTERFACE_MAP_ENTRY(nsIEditorController)
	NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditorController)
NS_INTERFACE_MAP_END

nsEditorController::nsEditorController()
: mCommandRefCon(nsnull)
{
  NS_INIT_REFCNT();  
}

nsEditorController::~nsEditorController()
{
}

NS_IMETHODIMP nsEditorController::Init(nsISupports *aCommandRefCon)
{
  nsresult  rv;
 
  // get our ref to the singleton command manager
  rv = GetEditorCommandManager(getter_AddRefs(mCommandManager));  
  if (NS_FAILED(rv)) return rv;  

  mCommandRefCon = aCommandRefCon;     // no addref  
  return NS_OK;
}

NS_IMETHODIMP nsEditorController::SetCommandRefCon(nsISupports *aCommandRefCon)
{
  mCommandRefCon = aCommandRefCon;     // no addref  
  return NS_OK;
}

NS_IMETHODIMP nsEditorController::GetInterface(const nsIID & aIID, void * *result)
{
  NS_ENSURE_ARG_POINTER(result);

  if (NS_SUCCEEDED(QueryInterface(aIID, result)))
    return NS_OK;
  
  if (mCommandManager && aIID.Equals(NS_GET_IID(nsIControllerCommandManager)))
    return mCommandManager->QueryInterface(aIID, result);
    
  return NS_NOINTERFACE;
}


#define NS_REGISTER_ONE_COMMAND(_cmdClass, _cmdName)                                      \
  {                                                                                       \
    _cmdClass* theCmd;                                                                    \
    NS_NEWXPCOM(theCmd, _cmdClass);                                                       \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                                           \
    rv = inCommandManager->RegisterCommand(NS_ConvertASCIItoUCS2(_cmdName).GetUnicode(),  \
                                   NS_STATIC_CAST(nsIControllerCommand *, theCmd));       \
  }

#define NS_REGISTER_FIRST_COMMAND(_cmdClass, _cmdName)                                    \
  {                                                                                       \
    _cmdClass* theCmd;                                                                    \
    NS_NEWXPCOM(theCmd, _cmdClass);                                                       \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                                           \
    rv = inCommandManager->RegisterCommand(NS_ConvertASCIItoUCS2(_cmdName).GetUnicode(),  \
                                   NS_STATIC_CAST(nsIControllerCommand *, theCmd));

#define NS_REGISTER_NEXT_COMMAND(_cmdClass, _cmdName)                                     \
    rv = inCommandManager->RegisterCommand(NS_ConvertASCIItoUCS2(_cmdName).GetUnicode(),  \
                                   NS_STATIC_CAST(nsIControllerCommand *, theCmd));

#define NS_REGISTER_LAST_COMMAND(_cmdClass, _cmdName)                                     \
    rv = inCommandManager->RegisterCommand(NS_ConvertASCIItoUCS2(_cmdName).GetUnicode(),  \
                                   NS_STATIC_CAST(nsIControllerCommand *, theCmd));       \
  }


// static
nsresult nsEditorController::RegisterEditorCommands(nsIControllerCommandManager *inCommandManager)
{
  nsresult rv;
 
  // now register all our commands
  // These are commands that will be used in text widgets, and in composer
  
  NS_REGISTER_ONE_COMMAND(nsUndoCommand, "cmd_undo");
  NS_REGISTER_ONE_COMMAND(nsRedoCommand, "cmd_redo");

  NS_REGISTER_ONE_COMMAND(nsCutCommand, "cmd_cut");
  NS_REGISTER_ONE_COMMAND(nsCopyCommand, "cmd_copy");
  NS_REGISTER_ONE_COMMAND(nsSelectAllCommand, "cmd_selectAll");
  
  NS_REGISTER_ONE_COMMAND(nsPasteCommand, "cmd_paste");
  
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
  NS_REGISTER_NEXT_COMMAND(nsSelectionMoveCommands, "cmd_selectBottom");
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

/* =======================================================================
 * nsIController
 * ======================================================================= */

NS_IMETHODIMP nsEditorController::IsCommandEnabled(const PRUnichar *aCommand, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  return mCommandManager->IsCommandEnabled(aCommand, mCommandRefCon, aResult);
}

NS_IMETHODIMP nsEditorController::SupportsCommand(const PRUnichar *aCommand, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  return mCommandManager->SupportsCommand(aCommand, mCommandRefCon, aResult);
}

NS_IMETHODIMP nsEditorController::DoCommand(const PRUnichar *aCommand)
{
  return mCommandManager->DoCommand(aCommand, mCommandRefCon);
}

NS_IMETHODIMP nsEditorController::OnEvent(const PRUnichar *aEventName)
{
  NS_ENSURE_ARG_POINTER(aEventName);

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


nsWeakPtr nsEditorController::sEditorCommandManager = NULL;

// static
nsresult nsEditorController::GetEditorCommandManager(nsIControllerCommandManager* *outCommandManager)
{
  NS_ENSURE_ARG_POINTER(outCommandManager);

  nsCOMPtr<nsIControllerCommandManager> cmdManager = do_QueryReferent(sEditorCommandManager);
  if (!cmdManager)
  {
    nsresult rv;
    cmdManager = do_CreateInstance("@mozilla.org/rdf/controller-command-manager;1", &rv);
    if (NS_FAILED(rv)) return rv;

    // register the commands. This just happens once per instance
    rv = nsEditorController::RegisterEditorCommands(cmdManager);
    if (NS_FAILED(rv)) return rv;

    // save the singleton in our static weak reference
    sEditorCommandManager = getter_AddRefs(NS_GetWeakReference(cmdManager, &rv));
    if (NS_FAILED(rv))  return rv;
  }

  NS_ADDREF(*outCommandManager = cmdManager);
  return NS_OK;
}



#ifdef XP_MAC
#pragma mark -
#endif

nsComposerController::nsComposerController()
{
}

nsComposerController::~nsComposerController()
{
}

NS_IMETHODIMP nsComposerController::Init(nsISupports *aCommandRefCon)
{
  nsresult  rv;
 
  rv = nsEditorController::Init(aCommandRefCon);
  if (NS_FAILED(rv)) return rv;  
  
  mCommandManager = do_CreateInstance("@mozilla.org/rdf/controller-command-manager;1", &rv);
  if (NS_FAILED(rv)) return rv;

  // register the commands.
  rv = nsComposerController::RegisterComposerCommands(mCommandManager);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

#define NS_REGISTER_STYLE_COMMAND(_cmdClass, _cmdName, _styleTag)                         \
  {                                                                                       \
    _cmdClass* theCmd = new _cmdClass(_styleTag);                                         \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                                           \
    rv = inCommandManager->RegisterCommand(NS_ConvertASCIItoUCS2(_cmdName).GetUnicode(),  \
                                   NS_STATIC_CAST(nsIControllerCommand *, theCmd));       \
  }
  

// static
nsresult nsComposerController::RegisterComposerCommands(nsIControllerCommandManager *inCommandManager)
{
  nsresult rv;
  
  // File menu
  NS_REGISTER_FIRST_COMMAND(nsPrintingCommands, "cmd_print");
  NS_REGISTER_NEXT_COMMAND(nsPrintingCommands, "cmd_printSetup");
  NS_REGISTER_NEXT_COMMAND(nsPrintingCommands,"cmd_print_button");
  NS_REGISTER_NEXT_COMMAND(nsPrintingCommands,"cmd_printSetup");
  NS_REGISTER_LAST_COMMAND(nsPrintingCommands, "cmd_printPreview");
  
  // Edit menu
  NS_REGISTER_ONE_COMMAND(nsPasteQuotationCommand, "cmd_pasteQuote");

  // indent/outdent
  NS_REGISTER_ONE_COMMAND(nsIndentCommand, "cmd_indent");
  NS_REGISTER_ONE_COMMAND(nsOutdentCommand, "cmd_outdent");

  // Styles
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_bold", "b");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_italic", "i");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_underline", "u");

  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_strikethrough", "strike");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_superscript", "sup");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_subscript", "sub");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_nobreak", "nobr");

  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_em", "em");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_strong", "strong");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_cite", "cite");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_abbr", "abbr");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_acronym", "acronym");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_code", "code");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_samp", "samp");
  NS_REGISTER_STYLE_COMMAND(nsStyleUpdatingCommand, "cmd_var", "var");
  
  // lists
  NS_REGISTER_STYLE_COMMAND(nsListCommand,     "cmd_ol", "ol");
  NS_REGISTER_STYLE_COMMAND(nsListCommand,     "cmd_ul", "ul");
  NS_REGISTER_STYLE_COMMAND(nsListItemCommand, "cmd_dt", "dt");
  NS_REGISTER_STYLE_COMMAND(nsListItemCommand, "cmd_dd", "dd");
  NS_REGISTER_ONE_COMMAND(nsRemoveListCommand, "cmd_removeList");

  // format stuff
  NS_REGISTER_ONE_COMMAND(nsParagraphStateCommand,       "cmd_paragraphState");
  NS_REGISTER_ONE_COMMAND(nsFontFaceStateCommand,        "cmd_fontFace");
  NS_REGISTER_ONE_COMMAND(nsFontColorStateCommand,       "cmd_fontColor");
  NS_REGISTER_ONE_COMMAND(nsBackgroundColorStateCommand, "cmd_backgroundColor");

  NS_REGISTER_ONE_COMMAND(nsAlignCommand, "cmd_align");
  NS_REGISTER_ONE_COMMAND(nsRemoveStylesCommand, "cmd_removeStyles");

  NS_REGISTER_ONE_COMMAND(nsIncreaseFontSizeCommand, "cmd_increaseFont");
  NS_REGISTER_ONE_COMMAND(nsDecreaseFontSizeCommand, "cmd_decreaseFont");

  return NS_OK;
}

