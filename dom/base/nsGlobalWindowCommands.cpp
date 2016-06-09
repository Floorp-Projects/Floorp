/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsGlobalWindowCommands.h"

#include "nsIComponentManager.h"
#include "nsIDOMElement.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCRT.h"
#include "nsString.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"

#include "nsIControllerCommandTable.h"
#include "nsICommandParams.h"

#include "nsPIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsIDocShell.h"
#include "nsISelectionController.h"
#include "nsIWebNavigation.h"
#include "nsIContentViewerEdit.h"
#include "nsIContentViewer.h"
#include "nsFocusManager.h"
#include "nsCopySupport.h"
#include "nsIClipboard.h"
#include "ContentEventHandler.h"
#include "nsContentUtils.h"
#include "nsIWordBreaker.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Selection.h"

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
const char * const sScrollLineUpString = "cmd_scrollLineUp";
const char * const sScrollLineDownString = "cmd_scrollLineDown";
const char * const sScrollLeftString = "cmd_scrollLeft";
const char * const sScrollRightString = "cmd_scrollRight";
const char * const sMoveTopString = "cmd_moveTop";
const char * const sMoveBottomString = "cmd_moveBottom";
const char * const sMovePageUpString = "cmd_movePageUp";
const char * const sMovePageDownString = "cmd_movePageDown";
const char * const sLinePreviousString = "cmd_linePrevious";
const char * const sLineNextString = "cmd_lineNext";
const char * const sCharPreviousString = "cmd_charPrevious";
const char * const sCharNextString = "cmd_charNext";

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

const char * const sSelectPageUpString = "cmd_selectPageUp";
const char * const sSelectPageDownString = "cmd_selectPageDown";

const char * const sSelectTopString = "cmd_selectTop";
const char * const sSelectBottomString = "cmd_selectBottom";

// Physical-direction movement and selection commands
const char * const sMoveLeftString = "cmd_moveLeft";
const char * const sMoveRightString = "cmd_moveRight";
const char * const sMoveUpString = "cmd_moveUp";
const char * const sMoveDownString = "cmd_moveDown";
const char * const sMoveLeft2String = "cmd_moveLeft2";
const char * const sMoveRight2String = "cmd_moveRight2";
const char * const sMoveUp2String = "cmd_moveUp2";
const char * const sMoveDown2String = "cmd_moveDown2";

const char * const sSelectLeftString = "cmd_selectLeft";
const char * const sSelectRightString = "cmd_selectRight";
const char * const sSelectUpString = "cmd_selectUp";
const char * const sSelectDownString = "cmd_selectDown";
const char * const sSelectLeft2String = "cmd_selectLeft2";
const char * const sSelectRight2String = "cmd_selectRight2";
const char * const sSelectUp2String = "cmd_selectUp2";
const char * const sSelectDown2String = "cmd_selectDown2";

#if 0
#pragma mark -
#endif

// a base class for selection-related commands, for code sharing
class nsSelectionCommandsBase : public nsIControllerCommand
{
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD IsCommandEnabled(const char* aCommandName, nsISupports* aCommandContext, bool* _retval) override;
  NS_IMETHOD GetCommandStateParams(const char* aCommandName, nsICommandParams* aParams, nsISupports* aCommandContext) override;
  NS_IMETHOD DoCommandParams(const char* aCommandName, nsICommandParams* aParams, nsISupports* aCommandContext) override;

protected:
  virtual ~nsSelectionCommandsBase() {}

  static nsresult  GetPresShellFromWindow(nsPIDOMWindowOuter *aWindow, nsIPresShell **aPresShell);
  static nsresult  GetSelectionControllerFromWindow(nsPIDOMWindowOuter *aWindow, nsISelectionController **aSelCon);

  // no member variables, please, we're stateless!
};

// this class implements commands whose behavior depends on the 'browse with caret' setting
class nsSelectMoveScrollCommand : public nsSelectionCommandsBase
{
public:

  NS_IMETHOD DoCommand(const char * aCommandName, nsISupports *aCommandContext);

  // no member variables, please, we're stateless!
};

// this class implements physical-movement versions of the above
class nsPhysicalSelectMoveScrollCommand : public nsSelectionCommandsBase
{
public:

  NS_IMETHOD DoCommand(const char * aCommandName, nsISupports *aCommandContext);

  // no member variables, please, we're stateless!
};

// this class implements other selection commands
class nsSelectCommand : public nsSelectionCommandsBase
{
public:

  NS_IMETHOD DoCommand(const char * aCommandName, nsISupports *aCommandContext);

  // no member variables, please, we're stateless!
};

// this class implements physical-movement versions of selection commands
class nsPhysicalSelectCommand : public nsSelectionCommandsBase
{
public:

  NS_IMETHOD DoCommand(const char * aCommandName, nsISupports *aCommandContext);

  // no member variables, please, we're stateless!
};

#if 0
#pragma mark -
#endif


NS_IMPL_ISUPPORTS(nsSelectionCommandsBase, nsIControllerCommand)

NS_IMETHODIMP
nsSelectionCommandsBase::IsCommandEnabled(const char * aCommandName,
                                      nsISupports *aCommandContext,
                                      bool *outCmdEnabled)
{
  // XXX this needs fixing. e.g. you can't scroll up if you're already at the top of
  // the document.
  *outCmdEnabled = true;
  return NS_OK;
}

NS_IMETHODIMP
nsSelectionCommandsBase::GetCommandStateParams(const char *aCommandName,
                                            nsICommandParams *aParams, nsISupports *aCommandContext)
{
  // XXX we should probably return the enabled state
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSelectionCommandsBase::DoCommandParams(const char *aCommandName,
                                       nsICommandParams *aParams, nsISupports *aCommandContext)
{
  return DoCommand(aCommandName, aCommandContext);
}

// protected methods

nsresult
nsSelectionCommandsBase::GetPresShellFromWindow(nsPIDOMWindowOuter *aWindow, nsIPresShell **aPresShell)
{
  *aPresShell = nullptr;
  NS_ENSURE_TRUE(aWindow, NS_ERROR_FAILURE);

  nsIDocShell *docShell = aWindow->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aPresShell = docShell->GetPresShell());
  return NS_OK;
}

nsresult
nsSelectionCommandsBase::GetSelectionControllerFromWindow(nsPIDOMWindowOuter *aWindow, nsISelectionController **aSelCon)
{
  *aSelCon = nullptr;

  nsCOMPtr<nsIPresShell> presShell;
  GetPresShellFromWindow(aWindow, getter_AddRefs(presShell));
  if (presShell)
    return CallQueryInterface(presShell, aSelCon);

  return NS_ERROR_FAILURE;
}

#if 0
#pragma mark -
#endif

// Helpers for nsSelectMoveScrollCommand and nsPhysicalSelectMoveScrollCommand
static void
AdjustFocusAfterCaretMove(nsPIDOMWindowOuter* aWindow)
{
  // adjust the focus to the new caret position
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsCOMPtr<nsIDOMElement> result;
    fm->MoveFocus(aWindow, nullptr, nsIFocusManager::MOVEFOCUS_CARET,
                  nsIFocusManager::FLAG_NOSCROLL, getter_AddRefs(result));
  }
}

static bool
IsCaretOnInWindow(nsPIDOMWindowOuter* aWindow, nsISelectionController* aSelCont)
{
  // We allow the caret to be moved with arrow keys on any window for which
  // the caret is enabled. In particular, this includes caret-browsing mode
  // in non-chrome documents.
  bool caretOn = false;
  aSelCont->GetCaretEnabled(&caretOn);
  if (!caretOn) {
    caretOn = Preferences::GetBool("accessibility.browsewithcaret");
    if (caretOn) {
      nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
      if (docShell && docShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
        caretOn = false;
      }
    }
  }
  return caretOn;
}

static const struct BrowseCommand {
  const char *reverse, *forward;
  nsresult (NS_STDCALL nsISelectionController::*scroll)(bool);
  nsresult (NS_STDCALL nsISelectionController::*move)(bool, bool);
} browseCommands[] = {
 { sScrollTopString, sScrollBottomString,
   &nsISelectionController::CompleteScroll },
 { sScrollPageUpString, sScrollPageDownString,
   &nsISelectionController::ScrollPage },
 { sScrollLineUpString, sScrollLineDownString,
   &nsISelectionController::ScrollLine },
 { sScrollLeftString, sScrollRightString,
   &nsISelectionController::ScrollCharacter },
 { sMoveTopString, sMoveBottomString,
   &nsISelectionController::CompleteScroll,
   &nsISelectionController::CompleteMove },
 { sMovePageUpString, sMovePageDownString,
   &nsISelectionController::ScrollPage,
   &nsISelectionController::PageMove },
 { sLinePreviousString, sLineNextString,
   &nsISelectionController::ScrollLine,
   &nsISelectionController::LineMove },
 { sWordPreviousString, sWordNextString,
   &nsISelectionController::ScrollCharacter,
   &nsISelectionController::WordMove },
 { sCharPreviousString, sCharNextString,
   &nsISelectionController::ScrollCharacter,
   &nsISelectionController::CharacterMove },
 { sBeginLineString, sEndLineString,
   &nsISelectionController::CompleteScroll,
   &nsISelectionController::IntraLineMove }
};

nsresult
nsSelectMoveScrollCommand::DoCommand(const char *aCommandName, nsISupports *aCommandContext)
{
  nsCOMPtr<nsPIDOMWindowOuter> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);       

  bool caretOn = IsCaretOnInWindow(piWindow, selCont);

  for (size_t i = 0; i < ArrayLength(browseCommands); i++) {
    bool forward = !strcmp(aCommandName, browseCommands[i].forward);
    if (forward || !strcmp(aCommandName, browseCommands[i].reverse)) {
      if (caretOn && browseCommands[i].move &&
          NS_SUCCEEDED((selCont->*(browseCommands[i].move))(forward, false))) {
        AdjustFocusAfterCaretMove(piWindow);
        return NS_OK;
      }
      return (selCont->*(browseCommands[i].scroll))(forward);
    }
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

// XXX It's not clear to me yet how we should handle the "scroll" option
// for these commands; for now, I'm mapping them back to ScrollCharacter,
// ScrollLine, etc., as if for horizontal-mode content, but this may need
// to be reconsidered once we have more experience with vertical content.
static const struct PhysicalBrowseCommand {
  const char *command;
  int16_t direction, amount;
  nsresult (NS_STDCALL nsISelectionController::*scroll)(bool);
} physicalBrowseCommands[] = {
 { sMoveLeftString, nsISelectionController::MOVE_LEFT, 0,
   &nsISelectionController::ScrollCharacter },
 { sMoveRightString, nsISelectionController::MOVE_RIGHT, 0,
   &nsISelectionController::ScrollCharacter },
 { sMoveUpString, nsISelectionController::MOVE_UP, 0,
   &nsISelectionController::ScrollLine },
 { sMoveDownString, nsISelectionController::MOVE_DOWN, 0,
   &nsISelectionController::ScrollLine },
 { sMoveLeft2String, nsISelectionController::MOVE_LEFT, 1,
   &nsISelectionController::ScrollCharacter },
 { sMoveRight2String, nsISelectionController::MOVE_RIGHT, 1,
   &nsISelectionController::ScrollCharacter },
 { sMoveUp2String, nsISelectionController::MOVE_UP, 1,
   &nsISelectionController::CompleteScroll },
 { sMoveDown2String, nsISelectionController::MOVE_DOWN, 1,
   &nsISelectionController::CompleteScroll },
};

nsresult
nsPhysicalSelectMoveScrollCommand::DoCommand(const char *aCommandName,
                                             nsISupports *aCommandContext)
{
  nsCOMPtr<nsPIDOMWindowOuter> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);       

  bool caretOn = IsCaretOnInWindow(piWindow, selCont);

  for (size_t i = 0; i < ArrayLength(physicalBrowseCommands); i++) {
    const PhysicalBrowseCommand& cmd = physicalBrowseCommands[i];
    if (!strcmp(aCommandName, cmd.command)) {
      int16_t dir = cmd.direction;
      if (caretOn &&
          NS_SUCCEEDED(selCont->PhysicalMove(dir, cmd.amount, false))) {
        AdjustFocusAfterCaretMove(piWindow);
        return NS_OK;
      }

      bool forward = (dir == nsISelectionController::MOVE_RIGHT ||
                      dir == nsISelectionController::MOVE_DOWN);
      return (selCont->*(cmd.scroll))(forward);
    }
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

#if 0
#pragma mark -
#endif

static const struct SelectCommand {
  const char *reverse, *forward;
  nsresult (NS_STDCALL nsISelectionController::*select)(bool, bool);
} selectCommands[] = {
 { sSelectCharPreviousString, sSelectCharNextString,
   &nsISelectionController::CharacterMove },
 { sSelectWordPreviousString, sSelectWordNextString,
   &nsISelectionController::WordMove },
 { sSelectBeginLineString, sSelectEndLineString,
   &nsISelectionController::IntraLineMove },
 { sSelectLinePreviousString, sSelectLineNextString,
   &nsISelectionController::LineMove },
 { sSelectPageUpString, sSelectPageDownString,
   &nsISelectionController::PageMove },
 { sSelectTopString, sSelectBottomString,
   &nsISelectionController::CompleteMove }
};

nsresult
nsSelectCommand::DoCommand(const char *aCommandName,
                           nsISupports *aCommandContext)
{
  nsCOMPtr<nsPIDOMWindowOuter> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);       

  // These commands are so the browser can use caret navigation key bindings -
  // Helps with accessibility - aaronl@netscape.com
  for (size_t i = 0; i < ArrayLength(selectCommands); i++) {
    bool forward = !strcmp(aCommandName, selectCommands[i].forward);
    if (forward || !strcmp(aCommandName, selectCommands[i].reverse)) {
      return (selCont->*(selectCommands[i].select))(forward, true);
    }
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

#if 0
#pragma mark -
#endif

static const struct PhysicalSelectCommand {
  const char *command;
  int16_t direction, amount;
} physicalSelectCommands[] = {
 { sSelectLeftString, nsISelectionController::MOVE_LEFT, 0 },
 { sSelectRightString, nsISelectionController::MOVE_RIGHT, 0 },
 { sSelectUpString, nsISelectionController::MOVE_UP, 0 },
 { sSelectDownString, nsISelectionController::MOVE_DOWN, 0 },
 { sSelectLeft2String, nsISelectionController::MOVE_LEFT, 1 },
 { sSelectRight2String, nsISelectionController::MOVE_RIGHT, 1 },
 { sSelectUp2String, nsISelectionController::MOVE_UP, 1 },
 { sSelectDown2String, nsISelectionController::MOVE_DOWN, 1 }
};

nsresult
nsPhysicalSelectCommand::DoCommand(const char *aCommandName,
                                   nsISupports *aCommandContext)
{
  nsCOMPtr<nsPIDOMWindowOuter> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);       

  for (size_t i = 0; i < ArrayLength(physicalSelectCommands); i++) {
    if (!strcmp(aCommandName, physicalSelectCommands[i].command)) {
      return selCont->PhysicalMove(physicalSelectCommands[i].direction,
                                   physicalSelectCommands[i].amount,
                                   true);
    }
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

#if 0
#pragma mark -
#endif

class nsClipboardCommand final : public nsIControllerCommand
{
  ~nsClipboardCommand() {}

public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND
};

NS_IMPL_ISUPPORTS(nsClipboardCommand, nsIControllerCommand)

nsresult
nsClipboardCommand::IsCommandEnabled(const char* aCommandName, nsISupports *aContext, bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = false;

  if (strcmp(aCommandName, "cmd_copy") &&
      strcmp(aCommandName, "cmd_copyAndCollapseToEnd") &&
      strcmp(aCommandName, "cmd_cut") &&
      strcmp(aCommandName, "cmd_paste"))
    return NS_OK;

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (doc->IsHTMLOrXHTML()) {
    // In HTML and XHTML documents, we always want the cut, copy and paste
    // commands to be enabled.
    *outCmdEnabled = true;
  } else {
    // Cut isn't enabled in xul documents which use nsClipboardCommand
    if (strcmp(aCommandName, "cmd_copy") == 0 ||
        strcmp(aCommandName, "cmd_copyAndCollapseToEnd") == 0) {
      *outCmdEnabled = nsCopySupport::CanCopy(doc);
    }
  }
  return NS_OK;
}

nsresult
nsClipboardCommand::DoCommand(const char *aCommandName, nsISupports *aContext)
{
  if (strcmp(aCommandName, "cmd_cut") &&
      strcmp(aCommandName, "cmd_copy") &&
      strcmp(aCommandName, "cmd_copyAndCollapseToEnd") &&
      strcmp(aCommandName, "cmd_paste"))
    return NS_OK;

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  EventMessage eventMessage = eCopy;
  if (strcmp(aCommandName, "cmd_cut") == 0) {
    eventMessage = eCut;
  } else if (strcmp(aCommandName, "cmd_paste") == 0) {
    eventMessage = ePaste;
  }

  bool actionTaken = false;
  bool notCancelled =
    nsCopySupport::FireClipboardEvent(eventMessage,
                                      nsIClipboard::kGlobalClipboard,
                                      presShell, nullptr, &actionTaken);

  if (notCancelled && !strcmp(aCommandName, "cmd_copyAndCollapseToEnd")) {
    dom::Selection *sel =
      presShell->GetCurrentSelection(SelectionType::eNormal);
    NS_ENSURE_TRUE(sel, NS_ERROR_FAILURE);
    sel->CollapseToEnd();
  }

  if (actionTaken) {
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
  virtual ~nsSelectionCommand() {}

  virtual nsresult    IsClipboardCommandEnabled(const char * aCommandName, nsIContentViewerEdit* aEdit, bool *outCmdEnabled) = 0;
  virtual nsresult    DoClipboardCommand(const char *aCommandName, nsIContentViewerEdit* aEdit, nsICommandParams* aParams) = 0;
  
  static nsresult     GetContentViewerEditFromContext(nsISupports *aContext, nsIContentViewerEdit **aEditInterface);
  
  // no member variables, please, we're stateless!
};


NS_IMPL_ISUPPORTS(nsSelectionCommand, nsIControllerCommand)


/*---------------------------------------------------------------------------

  nsSelectionCommand

----------------------------------------------------------------------------*/

NS_IMETHODIMP
nsSelectionCommand::IsCommandEnabled(const char * aCommandName,
                                     nsISupports *aCommandContext,
                                     bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = false;

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

  return DoClipboardCommand(aCommandName, contentEdit, nullptr);
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
  *aEditInterface = nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContentViewer> viewer;
  docShell->GetContentViewer(getter_AddRefs(viewer));
  nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
  NS_ENSURE_TRUE(edit, NS_ERROR_FAILURE);

  edit.forget(aEditInterface);
  return NS_OK;
}

#if 0
#pragma mark -
#endif

#define NS_DECL_CLIPBOARD_COMMAND(_cmd)                                                     \
class _cmd : public nsSelectionCommand                                                      \
{                                                                                           \
protected:                                                                                  \
                                                                                            \
  virtual nsresult    IsClipboardCommandEnabled(const char* aCommandName,                   \
                                  nsIContentViewerEdit* aEdit, bool *outCmdEnabled);        \
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
  int32_t copyFlags = nsIContentViewerEdit::COPY_IMAGE_DATA | 
                      nsIContentViewerEdit::COPY_IMAGE_HTML;
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
  *outCmdEnabled = true;
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

  nsAutoCString mimeType("text/plain");

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

#if 0   // Remove unless needed again, bug 204777
class nsWebNavigationBaseCommand : public nsIControllerCommand
{
public:
  virtual ~nsWebNavigationBaseCommand() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

protected:

  virtual nsresult    IsWebNavCommandEnabled(const char * aCommandName, nsIWebNavigation* aWebNavigation, bool *outCmdEnabled) = 0;
  virtual nsresult    DoWebNavCommand(const char *aCommandName, nsIWebNavigation* aWebNavigation) = 0;
  
  static nsresult     GetWebNavigationFromContext(nsISupports *aContext, nsIWebNavigation **aWebNavigation);
  
  // no member variables, please, we're stateless!
};

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

/*---------------------------------------------------------------------------

  nsWebNavigationCommands
     no params
----------------------------------------------------------------------------*/

NS_IMPL_ISUPPORTS(nsWebNavigationBaseCommand, nsIControllerCommand)

NS_IMETHODIMP
nsWebNavigationBaseCommand::IsCommandEnabled(const char * aCommandName,
                                          nsISupports *aCommandContext,
                                          bool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = false;

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

class nsClipboardDragDropHookCommand final : public nsIControllerCommand
{
  ~nsClipboardDragDropHookCommand() {}

public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

protected:                                                                                   
  // no member variables, please, we're stateless!
};


NS_IMPL_ISUPPORTS(nsClipboardDragDropHookCommand, nsIControllerCommand)

NS_IMETHODIMP
nsClipboardDragDropHookCommand::IsCommandEnabled(const char * aCommandName,
                                                 nsISupports *aCommandContext,
                                                 bool *outCmdEnabled)
{
  *outCmdEnabled = true;
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

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aCommandContext);
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
  return aParams->SetBooleanValue("state_enabled", true);
}

class nsLookUpDictionaryCommand final : public nsIControllerCommand
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

private:
  virtual ~nsLookUpDictionaryCommand()
  {
  }
};

NS_IMPL_ISUPPORTS(nsLookUpDictionaryCommand, nsIControllerCommand)

NS_IMETHODIMP
nsLookUpDictionaryCommand::IsCommandEnabled(
                             const char* aCommandName,
                             nsISupports* aCommandContext,
                             bool* aRetval)
{
  *aRetval = true;
  return NS_OK;
}

NS_IMETHODIMP
nsLookUpDictionaryCommand::GetCommandStateParams(const char* aCommandName,
                                                 nsICommandParams* aParams,
                                                 nsISupports* aCommandContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLookUpDictionaryCommand::DoCommand(const char* aCommandName,
                                     nsISupports *aCommandContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLookUpDictionaryCommand::DoCommandParams(const char* aCommandName,
                                           nsICommandParams* aParams,
                                           nsISupports* aCommandContext)
{
  if (NS_WARN_IF(!nsContentUtils::IsSafeToRunScript())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  int32_t x;
  int32_t y;

  nsresult rv = aParams->GetLongValue("x", &x);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aParams->GetLongValue("y", &y);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LayoutDeviceIntPoint point(x, y);

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aCommandContext);
  if (NS_WARN_IF(!window)) {
    return NS_ERROR_FAILURE;
  }

  nsIDocShell* docShell = window->GetDocShell();
  if (NS_WARN_IF(!docShell)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return NS_ERROR_FAILURE;
  }

  nsPresContext* presContext = presShell->GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWidget> widget = presContext->GetRootWidget();
  if (NS_WARN_IF(!widget)) {
    return NS_ERROR_FAILURE;
  }

  WidgetQueryContentEvent charAt(true, eQueryCharacterAtPoint, widget);
  charAt.mRefPoint.x = x;
  charAt.mRefPoint.y = y;
  ContentEventHandler handler(presContext);
  handler.OnQueryCharacterAtPoint(&charAt);

  if (NS_WARN_IF(!charAt.mSucceeded) ||
      charAt.mReply.mOffset == WidgetQueryContentEvent::NOT_FOUND) {
    return NS_ERROR_FAILURE;
  }

  WidgetQueryContentEvent textContent(true, eQueryTextContent, widget);
  // OSX 10.7 queries 50 characters before/after current point.  So we fetch
  // same length.
  uint32_t offset = charAt.mReply.mOffset;
  if (offset > 50) {
    offset -= 50;
  } else {
    offset = 0;
  }
  textContent.InitForQueryTextContent(offset, 100);
  handler.OnQueryTextContent(&textContent);
  if (NS_WARN_IF(!textContent.mSucceeded ||
                 textContent.mReply.mString.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  // XXX nsIWordBreaker doesn't use contextual breaker.
  // If OS provides it, widget should use it if contextual breaker is needed.
  nsCOMPtr<nsIWordBreaker> wordBreaker = nsContentUtils::WordBreaker();
  if (NS_WARN_IF(!wordBreaker)) {
    return NS_ERROR_FAILURE;
  }

  nsWordRange range =
    wordBreaker->FindWord(textContent.mReply.mString.get(),
                          textContent.mReply.mString.Length(),
                          charAt.mReply.mOffset - offset);
  if (range.mEnd == range.mBegin) {
    return NS_ERROR_FAILURE;
  }
  range.mBegin += offset;
  range.mEnd += offset;

  WidgetQueryContentEvent lookUpContent(true, eQueryTextContent, widget);
  lookUpContent.InitForQueryTextContent(range.mBegin,
                                        range.mEnd - range.mBegin);
  lookUpContent.RequestFontRanges();
  handler.OnQueryTextContent(&lookUpContent);
  if (NS_WARN_IF(!lookUpContent.mSucceeded ||
                 lookUpContent.mReply.mString.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  WidgetQueryContentEvent charRect(true, eQueryTextRect, widget);
  charRect.InitForQueryTextRect(range.mBegin, range.mEnd - range.mBegin);
  handler.OnQueryTextRect(&charRect);
  if (NS_WARN_IF(!charRect.mSucceeded)) {
    return NS_ERROR_FAILURE;
  }

  widget->LookUpDictionary(lookUpContent.mReply.mString,
                           lookUpContent.mReply.mFontRanges,
                           charRect.mReply.mWritingMode.IsVertical(),
                           charRect.mReply.mRect.TopLeft());

  return NS_OK;
}

/*---------------------------------------------------------------------------

  RegisterWindowCommands

----------------------------------------------------------------------------*/

#define NS_REGISTER_ONE_COMMAND(_cmdClass, _cmdName)                \
  {                                                                 \
    _cmdClass* theCmd = new _cmdClass();                            \
    rv = inCommandTable->RegisterCommand(_cmdName,                  \
                   static_cast<nsIControllerCommand *>(theCmd));    \
  }

#define NS_REGISTER_FIRST_COMMAND(_cmdClass, _cmdName)              \
  {                                                                 \
    _cmdClass* theCmd = new _cmdClass();                            \
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
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollPageUpString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollPageDownString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollLineUpString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollLineDownString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollLeftString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sScrollRightString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sMoveTopString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sMoveBottomString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sWordPreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sWordNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sBeginLineString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sEndLineString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sMovePageUpString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sMovePageDownString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sLinePreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sLineNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectMoveScrollCommand, sCharPreviousString);
  NS_REGISTER_LAST_COMMAND(nsSelectMoveScrollCommand, sCharNextString);

  NS_REGISTER_FIRST_COMMAND(nsPhysicalSelectMoveScrollCommand, sMoveLeftString);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectMoveScrollCommand, sMoveRightString);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectMoveScrollCommand, sMoveUpString);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectMoveScrollCommand, sMoveDownString);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectMoveScrollCommand, sMoveLeft2String);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectMoveScrollCommand, sMoveRight2String);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectMoveScrollCommand, sMoveUp2String);
  NS_REGISTER_LAST_COMMAND(nsPhysicalSelectMoveScrollCommand, sMoveDown2String);

  NS_REGISTER_FIRST_COMMAND(nsSelectCommand, sSelectCharPreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectCharNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectWordPreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectWordNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectBeginLineString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectEndLineString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectLinePreviousString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectLineNextString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectPageUpString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectPageDownString);
  NS_REGISTER_NEXT_COMMAND(nsSelectCommand, sSelectTopString);
  NS_REGISTER_LAST_COMMAND(nsSelectCommand, sSelectBottomString);

  NS_REGISTER_FIRST_COMMAND(nsPhysicalSelectCommand, sSelectLeftString);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectCommand, sSelectRightString);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectCommand, sSelectUpString);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectCommand, sSelectDownString);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectCommand, sSelectLeft2String);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectCommand, sSelectRight2String);
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectCommand, sSelectUp2String);
  NS_REGISTER_LAST_COMMAND(nsPhysicalSelectCommand, sSelectDown2String);

  NS_REGISTER_ONE_COMMAND(nsClipboardCommand, "cmd_cut");
  NS_REGISTER_ONE_COMMAND(nsClipboardCommand, "cmd_copy");
  NS_REGISTER_ONE_COMMAND(nsClipboardCommand, "cmd_copyAndCollapseToEnd");
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

  NS_REGISTER_ONE_COMMAND(nsLookUpDictionaryCommand, "cmd_lookUpDictionary");

  return rv;
}
