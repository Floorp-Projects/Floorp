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
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kathleen Brade <brade@netscape.com>
 *   Simon Fraser   <sfraser@netscape.com>
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


#include "nsGlobalWindowCommands.h"

#include "nsIComponentManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCRT.h"
#include "nsString.h"
#include "mozilla/Preferences.h"

#include "nsIControllerCommandTable.h"
#include "nsICommandParams.h"

#include "nsPIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsISelectionController.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewer.h"
#include "nsFocusManager.h"
#include "nsCopySupport.h"
#include "nsGUIEvent.h"

#include "nsIClipboardDragDropHooks.h"
#include "nsIClipboardDragDropHookList.h"

using namespace mozilla;

const char * const sSelectAllString = "cmd_selectAll";
const char * const sSelectNoneString = "cmd_selectNone";
const char * const sCopyImageLocationString = "cmd_copyImageLocation";
const char * const sCopyImageContentsString = "cmd_copyImageContents";
const char * const sCopyImageString = "cmd_copyImage";

const char * const sScrollTopString = "cmd_scrollTop";
const char * const sScrollBottomString = "cmd_scrollBottom";
const char * const sScrollPageUpString = "cmd_scrollPageUp";
const char * const sScrollPageDownString = "cmd_scrollPageDown";
const char * const sMovePageUpString = "cmd_movePageUp";
const char * const sMovePageDownString = "cmd_movePageDown";
const char * const sScrollLineUpString = "cmd_scrollLineUp";
const char * const sScrollLineDownString = "cmd_scrollLineDown";
const char * const sScrollLeftString = "cmd_scrollLeft";
const char * const sScrollRightString = "cmd_scrollRight";

// These are so the browser can use editor navigation key bindings
// helps with accessibility (boolean pref accessibility.browsewithcaret)

const char * const sSelectCharPreviousString = "cmd_selectCharPrevious";
const char * const sSelectCharNextString = "cmd_selectCharNext";

const char * const sWordPreviousString = "cmd_wordPrevious";
const char * const sWordNextString = "cmd_wordNext";
const char * const sSelectWordPreviousString = "cmd_selectWordPrevious";
const char * const sSelectWordNextString = "cmd_selectWordNext";

const char * const sBeginLineString = "cmd_beginLine";
const char * const sEndLineString = "cmd_endLine";
const char * const sSelectBeginLineString = "cmd_selectBeginLine";
const char * const sSelectEndLineString = "cmd_selectEndLine";

const char * const sSelectLinePreviousString = "cmd_selectLinePrevious";
const char * const sSelectLineNextString = "cmd_selectLineNext";

const char * const sSelectPagePreviousString = "cmd_selectPagePrevious";
const char * const sSelectPageNextString = "cmd_selectPageNext";

const char * const sSelectTopString = "cmd_selectTop";
const char * const sSelectBottomString = "cmd_selectBottom";


#if 0
#pragma mark -
#endif

// a base class for selection-related commands, for code sharing
class nsSelectionCommandsBase : public nsIControllerCommand
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

protected:

  // subclasses override DoSelectCommand
  virtual nsresult DoSelectCommand(const char *aCommandName, nsIDOMWindow *aWindow) = 0;

  static nsresult  GetPresShellFromWindow(nsIDOMWindow *aWindow, nsIPresShell **aPresShell);
  static nsresult  GetSelectionControllerFromWindow(nsIDOMWindow *aWindow, nsISelectionController **aSelCon);

  // no member variables, please, we're stateless!
};

// this class implements commands whose behavior depends on the 'browse with caret' setting
class nsSelectMoveScrollCommand : public nsSelectionCommandsBase
{
protected:

  virtual nsresult DoSelectCommand(const char *aCommandName, nsIDOMWindow *aWindow);
  
  nsresult    DoCommandBrowseWithCaretOn(const char *aCommandName,
                                         nsIDOMWindow *aWindow,
                                         nsISelectionController *aSelectionController);
  nsresult    DoCommandBrowseWithCaretOff(const char *aCommandName, nsISelectionController *aSelectionController);

  // no member variables, please, we're stateless!
};

// this class implements other selection commands
class nsSelectCommand : public nsSelectionCommandsBase
{
protected:

  virtual nsresult DoSelectCommand(const char *aCommandName, nsIDOMWindow *aWindow);

  // no member variables, please, we're stateless!
};

#if 0
#pragma mark -
#endif


NS_IMPL_ISUPPORTS1(nsSelectionCommandsBase, nsIControllerCommand)

/* boolean isCommandEnabled (in string aCommandName, in nsISupports aCommandContext); */
NS_IMETHODIMP
nsSelectionCommandsBase::IsCommandEnabled(const char * aCommandName,
                                      nsISupports *aCommandContext,
                                      bool *outCmdEnabled)
{
  // XXX this needs fixing. e.g. you can't scroll up if you're already at the top of
  // the document.
  *outCmdEnabled = PR_TRUE;
  return NS_OK;
}

/* void getCommandStateParams (in string aCommandName, in nsICommandParams aParams, in nsISupports aCommandContext); */
NS_IMETHODIMP
nsSelectionCommandsBase::GetCommandStateParams(const char *aCommandName,
                                            nsICommandParams *aParams, nsISupports *aCommandContext)
{
  // XXX we should probably return the enabled state
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSelectionCommandsBase::DoCommand(const char *aCommandName, nsISupports *aCommandContext)
{
  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(aCommandContext);
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);       

  return DoSelectCommand(aCommandName, window);
}

/* void doCommandParams (in string aCommandName, in nsICommandParams aParams, in nsISupports aCommandContext); */
NS_IMETHODIMP
nsSelectionCommandsBase::DoCommandParams(const char *aCommandName,
                                       nsICommandParams *aParams, nsISupports *aCommandContext)
{
  return DoCommand(aCommandName, aCommandContext);
}

// protected methods

nsresult
nsSelectionCommandsBase::GetPresShellFromWindow(nsIDOMWindow *aWindow, nsIPresShell **aPresShell)
{
  *aPresShell = nsnull;

  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(win, NS_ERROR_FAILURE);

  nsIDocShell *docShell = win->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  return docShell->GetPresShell(aPresShell);
}

nsresult
nsSelectionCommandsBase::GetSelectionControllerFromWindow(nsIDOMWindow *aWindow, nsISelectionController **aSelCon)
{
  *aSelCon = nsnull;

  nsCOMPtr<nsIPresShell> presShell;
  GetPresShellFromWindow(aWindow, getter_AddRefs(presShell));
  if (presShell)
    return CallQueryInterface(presShell, aSelCon);

  return NS_ERROR_FAILURE;
}

#if 0
#pragma mark -
#endif

nsresult
nsSelectMoveScrollCommand::DoSelectCommand(const char *aCommandName, nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(aWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);       

  // We allow the caret to be moved with arrow keys on any window for which
  // the caret is enabled. In particular, this includes caret-browsing mode
  // in non-chrome documents.
  bool caretOn = false;
  selCont->GetCaretEnabled(&caretOn);
  if (!caretOn) {
    caretOn = Preferences::GetBool("accessibility.browsewithcaret");
    if (caretOn) {
      nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(aWindow);
      if (piWindow) {
        nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(piWindow->GetDocShell());
        if (dsti) {
          PRInt32 itemType;
          dsti->GetItemType(&itemType);
          if (itemType == nsIDocShellTreeItem::typeChrome) {
            caretOn = PR_FALSE;
          }
        }
      }
    }
  }

  if (caretOn) {
    return DoCommandBrowseWithCaretOn(aCommandName, aWindow, selCont);
  }

  return DoCommandBrowseWithCaretOff(aCommandName, selCont);
}

nsresult
nsSelectMoveScrollCommand::DoCommandBrowseWithCaretOn(const char *aCommandName,
                  nsIDOMWindow *aWindow, nsISelectionController *aSelectionController)
{
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

  if (!nsCRT::strcmp(aCommandName, sScrollTopString))
    rv = aSelectionController->CompleteMove(PR_FALSE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName,sScrollBottomString))
    rv = aSelectionController->CompleteMove(PR_TRUE, PR_FALSE);
  // cmd_MovePageUp/Down are used on Window/Unix. They move the caret
  // in caret browsing mode.
  else if (!nsCRT::strcmp(aCommandName, sMovePageUpString))
    rv = aSelectionController->PageMove(PR_FALSE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sMovePageDownString))
    rv = aSelectionController->PageMove(PR_TRUE, PR_FALSE);
  // cmd_ScrollPageUp/Down are used on Mac, and for the spacebar on all platforms.
  // They do not move the caret in caret browsing mode.
  else if (!nsCRT::strcmp(aCommandName, sScrollPageUpString))
    rv = aSelectionController->ScrollPage(PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sScrollPageDownString))
    rv = aSelectionController->ScrollPage(PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sScrollLineUpString))
    rv = aSelectionController->LineMove(PR_FALSE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sScrollLineDownString))
    rv = aSelectionController->LineMove(PR_TRUE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sWordPreviousString))
    rv = aSelectionController->WordMove(PR_FALSE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sWordNextString))
    rv = aSelectionController->WordMove(PR_TRUE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sScrollLeftString))
    rv = aSelectionController->CharacterMove(PR_FALSE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sScrollRightString))
    rv = aSelectionController->CharacterMove(PR_TRUE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sBeginLineString))
    rv = aSelectionController->IntraLineMove(PR_FALSE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sEndLineString))
    rv = aSelectionController->IntraLineMove(PR_TRUE, PR_FALSE);

  if (NS_SUCCEEDED(rv))
  {
    // adjust the focus to the new caret position
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsCOMPtr<nsIDOMElement> result;
      fm->MoveFocus(aWindow, nsnull, nsIFocusManager::MOVEFOCUS_CARET,
                    nsIFocusManager::FLAG_NOSCROLL,
                    getter_AddRefs(result));
    }
  }

  return rv;
}

nsresult
nsSelectMoveScrollCommand::DoCommandBrowseWithCaretOff(const char *aCommandName, nsISelectionController *aSelectionController)
{
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

  if (!nsCRT::strcmp(aCommandName, sScrollTopString))   
    rv = aSelectionController->CompleteScroll(PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName,sScrollBottomString))
    rv = aSelectionController->CompleteScroll(PR_TRUE);

  // cmd_MovePageUp/Down are used on Window/Unix. They move the caret
  // in caret browsing mode.
  else if (!nsCRT::strcmp(aCommandName, sMovePageUpString))
    rv = aSelectionController->ScrollPage(PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sMovePageDownString))
    rv = aSelectionController->ScrollPage(PR_TRUE);
  // cmd_ScrollPageUp/Down are used on Mac. They do not move the
  // caret in caret browsing mode.
  else if (!nsCRT::strcmp(aCommandName, sScrollPageUpString))
    rv = aSelectionController->ScrollPage(PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sScrollPageDownString))
    rv = aSelectionController->ScrollPage(PR_TRUE);

  else if (!nsCRT::strcmp(aCommandName, sScrollLineUpString))
    rv = aSelectionController->ScrollLine(PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sScrollLineDownString))
    rv = aSelectionController->ScrollLine(PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sScrollLeftString))
    rv = aSelectionController->ScrollHorizontal(PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sScrollRightString))
    rv = aSelectionController->ScrollHorizontal(PR_FALSE);
  // cmd_beginLine/endLine with caret browsing off
  // will act as cmd_scrollTop/Bottom
  else if (!nsCRT::strcmp(aCommandName, sBeginLineString))
    rv = aSelectionController->CompleteScroll(PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName, sEndLineString))
    rv = aSelectionController->CompleteScroll(PR_TRUE);

  return rv;
}


#if 0
#pragma mark -
#endif

nsresult
nsSelectCommand::DoSelectCommand(const char *aCommandName, nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(aWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);       

  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

  // These commands are so the browser can use caret navigation key bindings -
  // Helps with accessibility - aaronl@netscape.com
  if (!nsCRT::strcmp(aCommandName, sSelectCharPreviousString))
    rv = selCont->CharacterMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectCharNextString))
    rv = selCont->CharacterMove(PR_TRUE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectWordPreviousString))
    rv = selCont->WordMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectWordNextString))
    rv = selCont->WordMove(PR_TRUE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectBeginLineString))
    rv = selCont->IntraLineMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectEndLineString))
    rv = selCont->IntraLineMove(PR_TRUE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectLinePreviousString))
    rv = selCont->LineMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectLineNextString))
    rv = selCont->LineMove(PR_TRUE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectTopString))
    rv = selCont->CompleteMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName, sSelectBottomString))
    rv = selCont->CompleteMove(PR_TRUE, PR_TRUE);

  return rv;
}

#if 0
#pragma mark -
#endif

class nsClipboardCommand : public nsIControllerCommand
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND
};

NS_IMPL_ISUPPORTS1(nsClipboardCommand, nsIControllerCommand)

nsresult
nsClipboardCommand::IsCommandEnabled(const char* aCommandName, nsISupports *aContext, bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = PR_FALSE;

  if (strcmp(aCommandName, "cmd_copy"))
    return NS_OK;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(window->GetExtantDocument());
  *outCmdEnabled = nsCopySupport::CanCopy(doc);
  return NS_OK;
}

nsresult
nsClipboardCommand::DoCommand(const char *aCommandName, nsISupports *aContext)
{
  if (strcmp(aCommandName, "cmd_copy"))
    return NS_OK;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsCopySupport::FireClipboardEvent(NS_COPY, presShell, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardCommand::GetCommandStateParams(const char *aCommandName,
                                              nsICommandParams *aParams, nsISupports *aCommandContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsClipboardCommand::DoCommandParams(const char *aCommandName, nsICommandParams* aParams, nsISupports *aContext)
{
  return DoCommand(aCommandName, aContext);
}

#if 0
#pragma mark -
#endif

class nsSelectionCommand : public nsIControllerCommand
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

protected:

  virtual nsresult    IsClipboardCommandEnabled(const char * aCommandName, nsIContentViewerEdit* aEdit, bool *outCmdEnabled) = 0;
  virtual nsresult    DoClipboardCommand(const char *aCommandName, nsIContentViewerEdit* aEdit, nsICommandParams* aParams) = 0;
  
  static nsresult     GetContentViewerEditFromContext(nsISupports *aContext, nsIContentViewerEdit **aEditInterface);
  
  // no member variables, please, we're stateless!
};


NS_IMPL_ISUPPORTS1(nsSelectionCommand, nsIControllerCommand)


/*---------------------------------------------------------------------------

  nsSelectionCommand

----------------------------------------------------------------------------*/

NS_IMETHODIMP
nsSelectionCommand::IsCommandEnabled(const char * aCommandName,
                                     nsISupports *aCommandContext,
                                     bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = PR_FALSE;

  nsCOMPtr<nsIContentViewerEdit> contentEdit;
  GetContentViewerEditFromContext(aCommandContext,  getter_AddRefs(contentEdit));
  NS_ENSURE_TRUE(contentEdit, NS_ERROR_NOT_INITIALIZED);

  return IsClipboardCommandEnabled(aCommandName, contentEdit, outCmdEnabled);
}

NS_IMETHODIMP
nsSelectionCommand::DoCommand(const char *aCommandName,
                              nsISupports *aCommandContext)
{
  nsCOMPtr<nsIContentViewerEdit> contentEdit;
  GetContentViewerEditFromContext(aCommandContext,  getter_AddRefs(contentEdit));
  NS_ENSURE_TRUE(contentEdit, NS_ERROR_NOT_INITIALIZED);

  return DoClipboardCommand(aCommandName, contentEdit, nsnull);
}

NS_IMETHODIMP
nsSelectionCommand::GetCommandStateParams(const char *aCommandName,
                                          nsICommandParams *aParams,
                                          nsISupports *aCommandContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSelectionCommand::DoCommandParams(const char *aCommandName,
                                    nsICommandParams *aParams,
                                    nsISupports *aCommandContext)
{
  nsCOMPtr<nsIContentViewerEdit> contentEdit;
  GetContentViewerEditFromContext(aCommandContext,  getter_AddRefs(contentEdit));
  NS_ENSURE_TRUE(contentEdit, NS_ERROR_NOT_INITIALIZED);

  return DoClipboardCommand(aCommandName, contentEdit, aParams);
}

nsresult
nsSelectionCommand::GetContentViewerEditFromContext(nsISupports *aContext,
                                                    nsIContentViewerEdit **aEditInterface)
{
  NS_ENSURE_ARG(aEditInterface);
  *aEditInterface = nsnull;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContentViewer> viewer;
  docShell->GetContentViewer(getter_AddRefs(viewer));
  nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
  NS_ENSURE_TRUE(edit, NS_ERROR_FAILURE);

  *aEditInterface = edit;
  NS_ADDREF(*aEditInterface);
  return NS_OK;
}

#if 0
#pragma mark -
#endif

#define NS_DECL_CLIPBOARD_COMMAND(_cmd)                                                     \
class _cmd : public nsSelectionCommand                                                  \
{                                                                                           \
protected:                                                                                  \
                                                                                            \
  virtual nsresult    IsClipboardCommandEnabled(const char* aCommandName,                   \
                                  nsIContentViewerEdit* aEdit, bool *outCmdEnabled);      \
  virtual nsresult    DoClipboardCommand(const char* aCommandName,                          \
                                  nsIContentViewerEdit* aEdit, nsICommandParams* aParams);  \
  /* no member variables, please, we're stateless! */                                       \
};

NS_DECL_CLIPBOARD_COMMAND(nsClipboardCopyLinkCommand)
NS_DECL_CLIPBOARD_COMMAND(nsClipboardImageCommands)
NS_DECL_CLIPBOARD_COMMAND(nsClipboardSelectAllNoneCommands)
NS_DECL_CLIPBOARD_COMMAND(nsClipboardGetContentsCommand)

nsresult
nsClipboardCopyLinkCommand::IsClipboardCommandEnabled(const char* aCommandName, nsIContentViewerEdit* aEdit, bool *outCmdEnabled)
{
  return aEdit->GetInLink(outCmdEnabled);
}

nsresult
nsClipboardCopyLinkCommand::DoClipboardCommand(const char *aCommandName, nsIContentViewerEdit* aEdit, nsICommandParams* aParams)
{
  return aEdit->CopyLinkLocation();
}

#if 0
#pragma mark -
#endif

nsresult
nsClipboardImageCommands::IsClipboardCommandEnabled(const char* aCommandName, nsIContentViewerEdit* aEdit, bool *outCmdEnabled)
{
  return aEdit->GetInImage(outCmdEnabled);
}

nsresult
nsClipboardImageCommands::DoClipboardCommand(const char *aCommandName, nsIContentViewerEdit* aEdit, nsICommandParams* aParams)
{
  if (!nsCRT::strcmp(sCopyImageLocationString, aCommandName))
    return aEdit->CopyImage(nsIContentViewerEdit::COPY_IMAGE_TEXT);
  if (!nsCRT::strcmp(sCopyImageContentsString, aCommandName))
    return aEdit->CopyImage(nsIContentViewerEdit::COPY_IMAGE_DATA);

  PRInt32 copyFlags = nsIContentViewerEdit::COPY_IMAGE_ALL;
  if (aParams)
    aParams->GetLongValue("imageCopy", &copyFlags);
  return aEdit->CopyImage(copyFlags);
}

#if 0
#pragma mark -
#endif

nsresult
nsClipboardSelectAllNoneCommands::IsClipboardCommandEnabled(const char* aCommandName, nsIContentViewerEdit* aEdit, bool *outCmdEnabled)
{
  *outCmdEnabled = PR_TRUE;
  return NS_OK;
}

nsresult
nsClipboardSelectAllNoneCommands::DoClipboardCommand(const char *aCommandName, nsIContentViewerEdit* aEdit, nsICommandParams* aParams)
{
  if (!nsCRT::strcmp(sSelectAllString, aCommandName))
    return aEdit->SelectAll();

  return aEdit->ClearSelection();
}


#if 0
#pragma mark -
#endif

nsresult
nsClipboardGetContentsCommand::IsClipboardCommandEnabled(const char* aCommandName, nsIContentViewerEdit* aEdit, bool *outCmdEnabled)
{
  return aEdit->GetCanGetContents(outCmdEnabled);
}

nsresult
nsClipboardGetContentsCommand::DoClipboardCommand(const char *aCommandName, nsIContentViewerEdit* aEdit, nsICommandParams* aParams)
{
  NS_ENSURE_ARG(aParams);

  nsCAutoString mimeType("text/plain");

  nsXPIDLCString format;    // nsICommandParams needs to use nsACString
  if (NS_SUCCEEDED(aParams->GetCStringValue("format", getter_Copies(format))))
    mimeType.Assign(format);
  
  bool selectionOnly = false;
  aParams->GetBooleanValue("selection_only", &selectionOnly);
  
  nsAutoString contents;
  nsresult rv = aEdit->GetContents(mimeType.get(), selectionOnly, contents);
  if (NS_FAILED(rv))
    return rv;
    
  return aParams->SetStringValue("result", contents);
}


#if 0
#pragma mark -
#endif

class nsWebNavigationBaseCommand : public nsIControllerCommand
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

protected:

  virtual nsresult    IsWebNavCommandEnabled(const char * aCommandName, nsIWebNavigation* aWebNavigation, bool *outCmdEnabled) = 0;
  virtual nsresult    DoWebNavCommand(const char *aCommandName, nsIWebNavigation* aWebNavigation) = 0;
  
  static nsresult     GetWebNavigationFromContext(nsISupports *aContext, nsIWebNavigation **aWebNavigation);
  
  // no member variables, please, we're stateless!
};

#if 0   // Remove unless needed again, bug 204777
class nsGoForwardCommand : public nsWebNavigationBaseCommand
{
protected:

  virtual nsresult    IsWebNavCommandEnabled(const char * aCommandName, nsIWebNavigation* aWebNavigation, bool *outCmdEnabled);
  virtual nsresult    DoWebNavCommand(const char *aCommandName, nsIWebNavigation* aWebNavigation);
  // no member variables, please, we're stateless!
};

class nsGoBackCommand : public nsWebNavigationBaseCommand
{
protected:

  virtual nsresult    IsWebNavCommandEnabled(const char * aCommandName, nsIWebNavigation* aWebNavigation, bool *outCmdEnabled);
  virtual nsresult    DoWebNavCommand(const char *aCommandName, nsIWebNavigation* aWebNavigation);
  // no member variables, please, we're stateless!
};
#endif

/*---------------------------------------------------------------------------

  nsWebNavigationCommands
     no params
----------------------------------------------------------------------------*/

NS_IMPL_ISUPPORTS1(nsWebNavigationBaseCommand, nsIControllerCommand)

NS_IMETHODIMP
nsWebNavigationBaseCommand::IsCommandEnabled(const char * aCommandName,
                                          nsISupports *aCommandContext,
                                          bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = PR_FALSE;

  nsCOMPtr<nsIWebNavigation> webNav;
  GetWebNavigationFromContext(aCommandContext, getter_AddRefs(webNav));
  NS_ENSURE_TRUE(webNav, NS_ERROR_INVALID_ARG);
  
  return IsCommandEnabled(aCommandName, webNav, outCmdEnabled);
}

NS_IMETHODIMP
nsWebNavigationBaseCommand::GetCommandStateParams(const char *aCommandName,
                                            nsICommandParams *aParams, nsISupports *aCommandContext)
{
  // XXX we should probably return the enabled state
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebNavigationBaseCommand::DoCommand(const char *aCommandName,
                                   nsISupports *aCommandContext)
{
  nsCOMPtr<nsIWebNavigation> webNav;
  GetWebNavigationFromContext(aCommandContext, getter_AddRefs(webNav));
  NS_ENSURE_TRUE(webNav, NS_ERROR_INVALID_ARG);
  
  return DoWebNavCommand(aCommandName, webNav);
}

/* void doCommandParams (in string aCommandName, in nsICommandParams aParams, in nsISupports aCommandContext); */
NS_IMETHODIMP
nsWebNavigationBaseCommand::DoCommandParams(const char *aCommandName,
                                       nsICommandParams *aParams, nsISupports *aCommandContext)
{
  return DoCommand(aCommandName, aCommandContext);
}

nsresult
nsWebNavigationBaseCommand::GetWebNavigationFromContext(nsISupports *aContext, nsIWebNavigation **aWebNavigation)
{
  nsCOMPtr<nsIInterfaceRequestor> windowReq = do_QueryInterface(aContext);
  CallGetInterface(windowReq.get(), aWebNavigation);
  return (*aWebNavigation) ? NS_OK : NS_ERROR_FAILURE;
}

#if 0
#pragma mark -
#endif

#if 0   // Remove unless needed again, bug 204777
nsresult
nsGoForwardCommand::IsWebNavCommandEnabled(const char * aCommandName, nsIWebNavigation* aWebNavigation, bool *outCmdEnabled)
{
  return aWebNavigation->GetCanGoForward(outCmdEnabled);
}

nsresult
nsGoForwardCommand::DoWebNavCommand(const char *aCommandName, nsIWebNavigation* aWebNavigation)
{
  return aWebNavigation->GoForward();
}

nsresult
nsGoBackCommand::IsWebNavCommandEnabled(const char * aCommandName, nsIWebNavigation* aWebNavigation, bool *outCmdEnabled)
{
  return aWebNavigation->GetCanGoBack(outCmdEnabled);
}

nsresult
nsGoBackCommand::DoWebNavCommand(const char *aCommandName, nsIWebNavigation* aWebNavigation)
{
  return aWebNavigation->GoBack();
}
#endif

/*---------------------------------------------------------------------------

  nsClipboardDragDropHookCommand
      params        value type   possible values
      "addhook"     isupports    nsIClipboardDragDropHooks as nsISupports
      "removehook"  isupports    nsIClipboardDragDropHooks as nsISupports

----------------------------------------------------------------------------*/

class nsClipboardDragDropHookCommand : public nsIControllerCommand
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

protected:                                                                                   
  // no member variables, please, we're stateless!
};


NS_IMPL_ISUPPORTS1(nsClipboardDragDropHookCommand, nsIControllerCommand)

NS_IMETHODIMP
nsClipboardDragDropHookCommand::IsCommandEnabled(const char * aCommandName,
                                                 nsISupports *aCommandContext,
                                                 bool *outCmdEnabled)
{
  *outCmdEnabled = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsClipboardDragDropHookCommand::DoCommand(const char *aCommandName,
                                          nsISupports *aCommandContext)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsClipboardDragDropHookCommand::DoCommandParams(const char *aCommandName,
                                                nsICommandParams *aParams,
                                                nsISupports *aCommandContext)
{
  NS_ENSURE_ARG(aParams);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aCommandContext);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsIDocShell *docShell = window->GetDocShell();

  nsCOMPtr<nsIClipboardDragDropHookList> obj = do_GetInterface(docShell);
  if (!obj) return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsISupports> isuppHook;

  nsresult returnValue = NS_OK;
  nsresult rv = aParams->GetISupportsValue("addhook", getter_AddRefs(isuppHook));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIClipboardDragDropHooks> hook = do_QueryInterface(isuppHook);
    if (hook)
      returnValue = obj->AddClipboardDragDropHooks(hook);
    else
      returnValue = NS_ERROR_INVALID_ARG;
  }

  rv = aParams->GetISupportsValue("removehook", getter_AddRefs(isuppHook));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIClipboardDragDropHooks> hook = do_QueryInterface(isuppHook);
    if (hook)
    {
      rv = obj->RemoveClipboardDragDropHooks(hook);
      if (NS_FAILED(rv) && NS_SUCCEEDED(returnValue))
        returnValue = rv;
    }
    else
      returnValue = NS_ERROR_INVALID_ARG;
  }

  return returnValue;
}

NS_IMETHODIMP
nsClipboardDragDropHookCommand::GetCommandStateParams(const char *aCommandName,
                                                      nsICommandParams *aParams,
                                                      nsISupports *aCommandContext)
{
  NS_ENSURE_ARG_POINTER(aParams);
  return aParams->SetBooleanValue("state_enabled", PR_TRUE);
}

/*---------------------------------------------------------------------------

  RegisterWindowCommands

----------------------------------------------------------------------------*/

#define NS_REGISTER_ONE_COMMAND(_cmdClass, _cmdName)                \
  {                                                                 \
    _cmdClass* theCmd = new _cmdClass();                            \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                     \
    rv = inCommandTable->RegisterCommand(_cmdName,                  \
                   static_cast<nsIControllerCommand *>(theCmd));    \
  }

#define NS_REGISTER_FIRST_COMMAND(_cmdClass, _cmdName)              \
  {                                                                 \
    _cmdClass* theCmd = new _cmdClass();                            \
    if (!theCmd) return NS_ERROR_OUT_OF_MEMORY;                     \
    rv = inCommandTable->RegisterCommand(_cmdName,                  \
                   static_cast<nsIControllerCommand *>(theCmd));

#define NS_REGISTER_NEXT_COMMAND(_cmdClass, _cmdName)               \
    rv = inCommandTable->RegisterCommand(_cmdName,                  \
                   static_cast<nsIControllerCommand *>(theCmd));

#define NS_REGISTER_LAST_COMMAND(_cmdClass, _cmdName)               \
    rv = inCommandTable->RegisterCommand(_cmdName,                  \
                   static_cast<nsIControllerCommand *>(theCmd));    \
  }


// static
nsresult
nsWindowCommandRegistration::RegisterWindowCommands(
                               nsIControllerCommandTable *inCommandTable)
{
  nsresult rv;

  // XXX rework the macros to use a loop is possible, reducing code size
  
  // this set of commands is affected by the 'browse with caret' setting
  NS_REGISTER_FIRST_COMMAND(nsSelectMoveScrollCommand, sScrollTopString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollBottomString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sWordPreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sWordNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sBeginLineString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sEndLineString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sMovePageUpString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sMovePageDownString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollPageUpString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollPageDownString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollLineUpString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollLineDownString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollLeftString);
  NS_REGISTER_LAST_COMMAND(nsSelectMoveScrollCommand, sScrollRightString);

  NS_REGISTER_FIRST_COMMAND(nsSelectCommand, sSelectCharPreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectCharNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectWordPreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectWordNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectBeginLineString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectEndLineString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectLinePreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectLineNextString);
  // XXX these commands were never implemented. fix me.
  // NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectPagePreviousString);
  // NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectPageNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectTopString);
  NS_REGISTER_LAST_COMMAND(nsSelectCommand, sSelectBottomString);

  NS_REGISTER_ONE_COMMAND(nsClipboardCommand, "cmd_cut");
  NS_REGISTER_ONE_COMMAND(nsClipboardCommand, "cmd_copy");
  NS_REGISTER_ONE_COMMAND(nsClipboardCommand, "cmd_paste");
  NS_REGISTER_ONE_COMMAND(nsClipboardCopyLinkCommand, "cmd_copyLink");
  NS_REGISTER_FIRST_COMMAND(nsClipboardImageCommands, sCopyImageLocationString);
  NS_REGISTER_NEXT_COMMAND(nsClipboardImageCommands, sCopyImageContentsString);
  NS_REGISTER_LAST_COMMAND(nsClipboardImageCommands, sCopyImageString);
  NS_REGISTER_FIRST_COMMAND(nsClipboardSelectAllNoneCommands, sSelectAllString);
  NS_REGISTER_LAST_COMMAND(nsClipboardSelectAllNoneCommands, sSelectNoneString);

  NS_REGISTER_ONE_COMMAND(nsClipboardGetContentsCommand, "cmd_getContents");

#if 0   // Remove unless needed again, bug 204777
  NS_REGISTER_ONE_COMMAND(nsGoBackCommand, "cmd_browserBack");
  NS_REGISTER_ONE_COMMAND(nsGoForwardCommand, "cmd_browserForward");
#endif

  NS_REGISTER_ONE_COMMAND(nsClipboardDragDropHookCommand, "cmd_clipboardDragDropHook");

  return rv;
}
