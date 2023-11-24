/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindowCommands.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCommandParams.h"
#include "nsCRT.h"
#include "nsString.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"

#include "nsControllerCommandTable.h"

#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsISelectionController.h"
#include "nsIWebNavigation.h"
#include "nsIDocumentViewerEdit.h"
#include "nsIDocumentViewer.h"
#include "nsFocusManager.h"
#include "nsCopySupport.h"
#include "nsIClipboard.h"
#include "ContentEventHandler.h"
#include "nsContentUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/TextEditor.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/intl/WordBreaker.h"
#include "mozilla/layers/KeyboardMap.h"

using namespace mozilla;
using namespace mozilla::layers;

constexpr const char* sSelectAllString = "cmd_selectAll";
constexpr const char* sSelectNoneString = "cmd_selectNone";
constexpr const char* sCopyImageLocationString = "cmd_copyImageLocation";
constexpr const char* sCopyImageContentsString = "cmd_copyImageContents";
constexpr const char* sCopyImageString = "cmd_copyImage";

constexpr const char* sScrollTopString = "cmd_scrollTop";
constexpr const char* sScrollBottomString = "cmd_scrollBottom";
constexpr const char* sScrollPageUpString = "cmd_scrollPageUp";
constexpr const char* sScrollPageDownString = "cmd_scrollPageDown";
constexpr const char* sScrollLineUpString = "cmd_scrollLineUp";
constexpr const char* sScrollLineDownString = "cmd_scrollLineDown";
constexpr const char* sScrollLeftString = "cmd_scrollLeft";
constexpr const char* sScrollRightString = "cmd_scrollRight";
constexpr const char* sMoveTopString = "cmd_moveTop";
constexpr const char* sMoveBottomString = "cmd_moveBottom";
constexpr const char* sMovePageUpString = "cmd_movePageUp";
constexpr const char* sMovePageDownString = "cmd_movePageDown";
constexpr const char* sLinePreviousString = "cmd_linePrevious";
constexpr const char* sLineNextString = "cmd_lineNext";
constexpr const char* sCharPreviousString = "cmd_charPrevious";
constexpr const char* sCharNextString = "cmd_charNext";

// These are so the browser can use editor navigation key bindings
// helps with accessibility (boolean pref accessibility.browsewithcaret)

constexpr const char* sSelectCharPreviousString = "cmd_selectCharPrevious";
constexpr const char* sSelectCharNextString = "cmd_selectCharNext";

constexpr const char* sWordPreviousString = "cmd_wordPrevious";
constexpr const char* sWordNextString = "cmd_wordNext";
constexpr const char* sSelectWordPreviousString = "cmd_selectWordPrevious";
constexpr const char* sSelectWordNextString = "cmd_selectWordNext";

constexpr const char* sBeginLineString = "cmd_beginLine";
constexpr const char* sEndLineString = "cmd_endLine";
constexpr const char* sSelectBeginLineString = "cmd_selectBeginLine";
constexpr const char* sSelectEndLineString = "cmd_selectEndLine";

constexpr const char* sSelectLinePreviousString = "cmd_selectLinePrevious";
constexpr const char* sSelectLineNextString = "cmd_selectLineNext";

constexpr const char* sSelectPageUpString = "cmd_selectPageUp";
constexpr const char* sSelectPageDownString = "cmd_selectPageDown";

constexpr const char* sSelectTopString = "cmd_selectTop";
constexpr const char* sSelectBottomString = "cmd_selectBottom";

// Physical-direction movement and selection commands
constexpr const char* sMoveLeftString = "cmd_moveLeft";
constexpr const char* sMoveRightString = "cmd_moveRight";
constexpr const char* sMoveUpString = "cmd_moveUp";
constexpr const char* sMoveDownString = "cmd_moveDown";
constexpr const char* sMoveLeft2String = "cmd_moveLeft2";
constexpr const char* sMoveRight2String = "cmd_moveRight2";
constexpr const char* sMoveUp2String = "cmd_moveUp2";
constexpr const char* sMoveDown2String = "cmd_moveDown2";

constexpr const char* sSelectLeftString = "cmd_selectLeft";
constexpr const char* sSelectRightString = "cmd_selectRight";
constexpr const char* sSelectUpString = "cmd_selectUp";
constexpr const char* sSelectDownString = "cmd_selectDown";
constexpr const char* sSelectLeft2String = "cmd_selectLeft2";
constexpr const char* sSelectRight2String = "cmd_selectRight2";
constexpr const char* sSelectUp2String = "cmd_selectUp2";
constexpr const char* sSelectDown2String = "cmd_selectDown2";

#if 0
#  pragma mark -
#endif

// a base class for selection-related commands, for code sharing
class nsSelectionCommandsBase : public nsIControllerCommand {
 public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD IsCommandEnabled(const char* aCommandName,
                              nsISupports* aCommandContext,
                              bool* _retval) override;
  NS_IMETHOD GetCommandStateParams(const char* aCommandName,
                                   nsICommandParams* aParams,
                                   nsISupports* aCommandContext) override;
  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD DoCommandParams(const char* aCommandName,
                             nsICommandParams* aParams,
                             nsISupports* aCommandContext) override;

 protected:
  virtual ~nsSelectionCommandsBase() = default;

  static nsresult GetPresShellFromWindow(nsPIDOMWindowOuter* aWindow,
                                         PresShell** aPresShell);
  static nsresult GetSelectionControllerFromWindow(
      nsPIDOMWindowOuter* aWindow, nsISelectionController** aSelCon);

  // no member variables, please, we're stateless!
};

// this class implements commands whose behavior depends on the 'browse with
// caret' setting
class nsSelectMoveScrollCommand : public nsSelectionCommandsBase {
 public:
  NS_IMETHOD DoCommand(const char* aCommandName,
                       nsISupports* aCommandContext) override;

  // no member variables, please, we're stateless!
};

// this class implements physical-movement versions of the above
class nsPhysicalSelectMoveScrollCommand : public nsSelectionCommandsBase {
 public:
  NS_IMETHOD DoCommand(const char* aCommandName,
                       nsISupports* aCommandContext) override;

  // no member variables, please, we're stateless!
};

// this class implements other selection commands
class nsSelectCommand : public nsSelectionCommandsBase {
 public:
  NS_IMETHOD DoCommand(const char* aCommandName,
                       nsISupports* aCommandContext) override;

  // no member variables, please, we're stateless!
};

// this class implements physical-movement versions of selection commands
class nsPhysicalSelectCommand : public nsSelectionCommandsBase {
 public:
  NS_IMETHOD DoCommand(const char* aCommandName,
                       nsISupports* aCommandContext) override;

  // no member variables, please, we're stateless!
};

#if 0
#  pragma mark -
#endif

NS_IMPL_ISUPPORTS(nsSelectionCommandsBase, nsIControllerCommand)

NS_IMETHODIMP
nsSelectionCommandsBase::IsCommandEnabled(const char* aCommandName,
                                          nsISupports* aCommandContext,
                                          bool* outCmdEnabled) {
  // XXX this needs fixing. e.g. you can't scroll up if you're already at the
  // top of the document.
  *outCmdEnabled = true;
  return NS_OK;
}

NS_IMETHODIMP
nsSelectionCommandsBase::GetCommandStateParams(const char* aCommandName,
                                               nsICommandParams* aParams,
                                               nsISupports* aCommandContext) {
  // XXX we should probably return the enabled state
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSelectionCommandsBase::DoCommandParams(const char* aCommandName,
                                         nsICommandParams* aParams,
                                         nsISupports* aCommandContext) {
  return DoCommand(aCommandName, aCommandContext);
}

// protected methods

nsresult nsSelectionCommandsBase::GetPresShellFromWindow(
    nsPIDOMWindowOuter* aWindow, PresShell** aPresShell) {
  *aPresShell = nullptr;
  NS_ENSURE_TRUE(aWindow, NS_ERROR_FAILURE);

  nsIDocShell* docShell = aWindow->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aPresShell = docShell->GetPresShell());
  return NS_OK;
}

nsresult nsSelectionCommandsBase::GetSelectionControllerFromWindow(
    nsPIDOMWindowOuter* aWindow, nsISelectionController** aSelCon) {
  RefPtr<PresShell> presShell;
  GetPresShellFromWindow(aWindow, getter_AddRefs(presShell));
  if (!presShell) {
    *aSelCon = nullptr;
    return NS_ERROR_FAILURE;
  }
  *aSelCon = presShell.forget().take();
  return NS_OK;
}

#if 0
#  pragma mark -
#endif

// Helpers for nsSelectMoveScrollCommand and nsPhysicalSelectMoveScrollCommand
static void AdjustFocusAfterCaretMove(nsPIDOMWindowOuter* aWindow) {
  // adjust the focus to the new caret position
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    RefPtr<dom::Element> result;
    fm->MoveFocus(aWindow, nullptr, nsIFocusManager::MOVEFOCUS_CARET,
                  nsIFocusManager::FLAG_NOSCROLL, getter_AddRefs(result));
  }
}

static bool IsCaretOnInWindow(nsPIDOMWindowOuter* aWindow,
                              nsISelectionController* aSelCont) {
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

static constexpr struct BrowseCommand {
  Command reverse, forward;
  KeyboardScrollAction::KeyboardScrollActionType scrollAction;
  nsresult (NS_STDCALL nsISelectionController::*scroll)(bool);
  nsresult (NS_STDCALL nsISelectionController::*move)(bool, bool);
} browseCommands[] = {
    {Command::ScrollTop, Command::ScrollBottom,
     KeyboardScrollAction::eScrollComplete,
     &nsISelectionController::CompleteScroll},
    {Command::ScrollPageUp, Command::ScrollPageDown,
     KeyboardScrollAction::eScrollPage, &nsISelectionController::ScrollPage},
    {Command::ScrollLineUp, Command::ScrollLineDown,
     KeyboardScrollAction::eScrollLine, &nsISelectionController::ScrollLine},
    {Command::ScrollLeft, Command::ScrollRight,
     KeyboardScrollAction::eScrollCharacter,
     &nsISelectionController::ScrollCharacter},
    {Command::MoveTop, Command::MoveBottom,
     KeyboardScrollAction::eScrollComplete,
     &nsISelectionController::CompleteScroll,
     &nsISelectionController::CompleteMove},
    {Command::MovePageUp, Command::MovePageDown,
     KeyboardScrollAction::eScrollPage, &nsISelectionController::ScrollPage,
     &nsISelectionController::PageMove},
    {Command::LinePrevious, Command::LineNext,
     KeyboardScrollAction::eScrollLine, &nsISelectionController::ScrollLine,
     &nsISelectionController::LineMove},
    {Command::WordPrevious, Command::WordNext,
     KeyboardScrollAction::eScrollCharacter,
     &nsISelectionController::ScrollCharacter,
     &nsISelectionController::WordMove},
    {Command::CharPrevious, Command::CharNext,
     KeyboardScrollAction::eScrollCharacter,
     &nsISelectionController::ScrollCharacter,
     &nsISelectionController::CharacterMove},
    {Command::BeginLine, Command::EndLine,
     KeyboardScrollAction::eScrollComplete,
     &nsISelectionController::CompleteScroll,
     &nsISelectionController::IntraLineMove}};

nsresult nsSelectMoveScrollCommand::DoCommand(const char* aCommandName,
                                              nsISupports* aCommandContext) {
  nsCOMPtr<nsPIDOMWindowOuter> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);

  const bool caretOn = IsCaretOnInWindow(piWindow, selCont);
  const Command command = GetInternalCommand(aCommandName);
  for (const BrowseCommand& browseCommand : browseCommands) {
    const bool forward = command == browseCommand.forward;
    if (!forward && command != browseCommand.reverse) {
      continue;
    }
    RefPtr<HTMLEditor> htmlEditor =
        HTMLEditor::GetFrom(nsContentUtils::GetActiveEditor(piWindow));
    if (htmlEditor) {
      htmlEditor->PreHandleSelectionChangeCommand(command);
    }
    nsresult rv = NS_OK;
    if (caretOn && browseCommand.move &&
        NS_SUCCEEDED((selCont->*(browseCommand.move))(forward, false))) {
      AdjustFocusAfterCaretMove(piWindow);
    } else {
      rv = (selCont->*(browseCommand.scroll))(forward);
    }
    if (htmlEditor) {
      htmlEditor->PostHandleSelectionChangeCommand(command);
    }
    return rv;
  }

  MOZ_ASSERT(false, "Forgot to handle new command?");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// XXX It's not clear to me yet how we should handle the "scroll" option
// for these commands; for now, I'm mapping them back to ScrollCharacter,
// ScrollLine, etc., as if for horizontal-mode content, but this may need
// to be reconsidered once we have more experience with vertical content.
static const struct PhysicalBrowseCommand {
  Command command;
  int16_t direction, amount;
  KeyboardScrollAction::KeyboardScrollActionType scrollAction;
  nsresult (NS_STDCALL nsISelectionController::*scroll)(bool);
} physicalBrowseCommands[] = {
    {Command::MoveLeft, nsISelectionController::MOVE_LEFT, 0,
     KeyboardScrollAction::eScrollCharacter,
     &nsISelectionController::ScrollCharacter},
    {Command::MoveRight, nsISelectionController::MOVE_RIGHT, 0,
     KeyboardScrollAction::eScrollCharacter,
     &nsISelectionController::ScrollCharacter},
    {Command::MoveUp, nsISelectionController::MOVE_UP, 0,
     KeyboardScrollAction::eScrollLine, &nsISelectionController::ScrollLine},
    {Command::MoveDown, nsISelectionController::MOVE_DOWN, 0,
     KeyboardScrollAction::eScrollLine, &nsISelectionController::ScrollLine},
    {Command::MoveLeft2, nsISelectionController::MOVE_LEFT, 1,
     KeyboardScrollAction::eScrollCharacter,
     &nsISelectionController::ScrollCharacter},
    {Command::MoveRight2, nsISelectionController::MOVE_RIGHT, 1,
     KeyboardScrollAction::eScrollCharacter,
     &nsISelectionController::ScrollCharacter},
    {Command::MoveUp2, nsISelectionController::MOVE_UP, 1,
     KeyboardScrollAction::eScrollComplete,
     &nsISelectionController::CompleteScroll},
    {Command::MoveDown2, nsISelectionController::MOVE_DOWN, 1,
     KeyboardScrollAction::eScrollComplete,
     &nsISelectionController::CompleteScroll},
};

nsresult nsPhysicalSelectMoveScrollCommand::DoCommand(
    const char* aCommandName, nsISupports* aCommandContext) {
  nsCOMPtr<nsPIDOMWindowOuter> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);

  const bool caretOn = IsCaretOnInWindow(piWindow, selCont);
  Command command = GetInternalCommand(aCommandName);
  for (const PhysicalBrowseCommand& browseCommand : physicalBrowseCommands) {
    if (command != browseCommand.command) {
      continue;
    }
    RefPtr<HTMLEditor> htmlEditor =
        HTMLEditor::GetFrom(nsContentUtils::GetActiveEditor(piWindow));
    if (htmlEditor) {
      htmlEditor->PreHandleSelectionChangeCommand(command);
    }
    nsresult rv = NS_OK;
    if (caretOn && NS_SUCCEEDED(selCont->PhysicalMove(
                       browseCommand.direction, browseCommand.amount, false))) {
      AdjustFocusAfterCaretMove(piWindow);
    } else {
      const bool forward =
          (browseCommand.direction == nsISelectionController::MOVE_RIGHT ||
           browseCommand.direction == nsISelectionController::MOVE_DOWN);
      rv = (selCont->*(browseCommand.scroll))(forward);
    }
    if (htmlEditor) {
      htmlEditor->PostHandleSelectionChangeCommand(command);
    }
    return rv;
  }

  MOZ_ASSERT(false, "Forgot to handle new command?");
  return NS_ERROR_NOT_IMPLEMENTED;
}

#if 0
#  pragma mark -
#endif

static const struct SelectCommand {
  Command reverse, forward;
  nsresult (NS_STDCALL nsISelectionController::*select)(bool, bool);
} selectCommands[] = {{Command::SelectCharPrevious, Command::SelectCharNext,
                       &nsISelectionController::CharacterMove},
                      {Command::SelectWordPrevious, Command::SelectWordNext,
                       &nsISelectionController::WordMove},
                      {Command::SelectBeginLine, Command::SelectEndLine,
                       &nsISelectionController::IntraLineMove},
                      {Command::SelectLinePrevious, Command::SelectLineNext,
                       &nsISelectionController::LineMove},
                      {Command::SelectPageUp, Command::SelectPageDown,
                       &nsISelectionController::PageMove},
                      {Command::SelectTop, Command::SelectBottom,
                       &nsISelectionController::CompleteMove}};

nsresult nsSelectCommand::DoCommand(const char* aCommandName,
                                    nsISupports* aCommandContext) {
  nsCOMPtr<nsPIDOMWindowOuter> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);

  // These commands are so the browser can use caret navigation key bindings -
  // Helps with accessibility - aaronl@netscape.com
  const Command command = GetInternalCommand(aCommandName);
  for (const SelectCommand& selectCommand : selectCommands) {
    const bool forward = command == selectCommand.forward;
    if (!forward && command != selectCommand.reverse) {
      continue;
    }
    RefPtr<HTMLEditor> htmlEditor =
        HTMLEditor::GetFrom(nsContentUtils::GetActiveEditor(piWindow));
    if (htmlEditor) {
      htmlEditor->PreHandleSelectionChangeCommand(command);
    }
    nsresult rv = (selCont->*(selectCommand.select))(forward, true);
    if (htmlEditor) {
      htmlEditor->PostHandleSelectionChangeCommand(command);
    }
    return rv;
  }

  MOZ_ASSERT(false, "Forgot to handle new command?");
  return NS_ERROR_NOT_IMPLEMENTED;
}

#if 0
#  pragma mark -
#endif

static const struct PhysicalSelectCommand {
  Command command;
  int16_t direction, amount;
} physicalSelectCommands[] = {
    {Command::SelectLeft, nsISelectionController::MOVE_LEFT, 0},
    {Command::SelectRight, nsISelectionController::MOVE_RIGHT, 0},
    {Command::SelectUp, nsISelectionController::MOVE_UP, 0},
    {Command::SelectDown, nsISelectionController::MOVE_DOWN, 0},
    {Command::SelectLeft2, nsISelectionController::MOVE_LEFT, 1},
    {Command::SelectRight2, nsISelectionController::MOVE_RIGHT, 1},
    {Command::SelectUp2, nsISelectionController::MOVE_UP, 1},
    {Command::SelectDown2, nsISelectionController::MOVE_DOWN, 1}};

nsresult nsPhysicalSelectCommand::DoCommand(const char* aCommandName,
                                            nsISupports* aCommandContext) {
  nsCOMPtr<nsPIDOMWindowOuter> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);

  const Command command = GetInternalCommand(aCommandName);
  for (const PhysicalSelectCommand& selectCommand : physicalSelectCommands) {
    if (command != selectCommand.command) {
      continue;
    }
    RefPtr<HTMLEditor> htmlEditor =
        HTMLEditor::GetFrom(nsContentUtils::GetActiveEditor(piWindow));
    if (htmlEditor) {
      htmlEditor->PreHandleSelectionChangeCommand(command);
    }
    nsresult rv = selCont->PhysicalMove(selectCommand.direction,
                                        selectCommand.amount, true);
    if (htmlEditor) {
      htmlEditor->PostHandleSelectionChangeCommand(command);
    }
    return rv;
  }

  MOZ_ASSERT(false, "Forgot to handle new command?");
  return NS_ERROR_NOT_IMPLEMENTED;
}

#if 0
#  pragma mark -
#endif

class nsClipboardCommand final : public nsIControllerCommand {
  ~nsClipboardCommand() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND
};

NS_IMPL_ISUPPORTS(nsClipboardCommand, nsIControllerCommand)

nsresult nsClipboardCommand::IsCommandEnabled(const char* aCommandName,
                                              nsISupports* aContext,
                                              bool* outCmdEnabled) {
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = false;

  if (strcmp(aCommandName, "cmd_copy") && strcmp(aCommandName, "cmd_cut") &&
      strcmp(aCommandName, "cmd_paste")) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  RefPtr<dom::Document> doc = window->GetExtantDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  if (doc->AreClipboardCommandsUnconditionallyEnabled()) {
    // In HTML and XHTML documents, we always want the cut, copy and paste
    // commands to be enabled, but if the document is chrome, let it control it.
    *outCmdEnabled = true;
  } else {
    // Cut isn't enabled in xul documents which use nsClipboardCommand
    if (strcmp(aCommandName, "cmd_copy") == 0) {
      *outCmdEnabled = nsCopySupport::CanCopy(doc);
    }
  }
  return NS_OK;
}

nsresult nsClipboardCommand::DoCommand(const char* aCommandName,
                                       nsISupports* aContext) {
  if (strcmp(aCommandName, "cmd_cut") && strcmp(aCommandName, "cmd_copy") &&
      strcmp(aCommandName, "cmd_paste"))
    return NS_OK;

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  RefPtr<PresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  EventMessage eventMessage = eCopy;
  if (strcmp(aCommandName, "cmd_cut") == 0) {
    eventMessage = eCut;
  } else if (strcmp(aCommandName, "cmd_paste") == 0) {
    eventMessage = ePaste;
  }

  bool actionTaken = false;
  nsCopySupport::FireClipboardEvent(eventMessage,
                                    nsIClipboard::kGlobalClipboard, presShell,
                                    nullptr, &actionTaken);

  return actionTaken ? NS_OK : NS_SUCCESS_DOM_NO_OPERATION;
}

NS_IMETHODIMP
nsClipboardCommand::GetCommandStateParams(const char* aCommandName,
                                          nsICommandParams* aParams,
                                          nsISupports* aCommandContext) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsClipboardCommand::DoCommandParams(const char* aCommandName,
                                             nsICommandParams* aParams,
                                             nsISupports* aContext) {
  return DoCommand(aCommandName, aContext);
}

#if 0
#  pragma mark -
#endif

class nsSelectionCommand : public nsIControllerCommand {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

 protected:
  virtual ~nsSelectionCommand() = default;

  virtual nsresult IsClipboardCommandEnabled(const char* aCommandName,
                                             nsIDocumentViewerEdit* aEdit,
                                             bool* outCmdEnabled) = 0;
  virtual nsresult DoClipboardCommand(const char* aCommandName,
                                      nsIDocumentViewerEdit* aEdit,
                                      nsICommandParams* aParams) = 0;

  static nsresult GetDocumentViewerEditFromContext(
      nsISupports* aContext, nsIDocumentViewerEdit** aEditInterface);

  // no member variables, please, we're stateless!
};

NS_IMPL_ISUPPORTS(nsSelectionCommand, nsIControllerCommand)

/*---------------------------------------------------------------------------

  nsSelectionCommand

----------------------------------------------------------------------------*/

NS_IMETHODIMP
nsSelectionCommand::IsCommandEnabled(const char* aCommandName,
                                     nsISupports* aCommandContext,
                                     bool* outCmdEnabled) {
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = false;

  nsCOMPtr<nsIDocumentViewerEdit> documentEdit;
  GetDocumentViewerEditFromContext(aCommandContext,
                                   getter_AddRefs(documentEdit));
  NS_ENSURE_TRUE(documentEdit, NS_ERROR_NOT_INITIALIZED);

  return IsClipboardCommandEnabled(aCommandName, documentEdit, outCmdEnabled);
}

NS_IMETHODIMP
nsSelectionCommand::DoCommand(const char* aCommandName,
                              nsISupports* aCommandContext) {
  nsCOMPtr<nsIDocumentViewerEdit> documentEdit;
  GetDocumentViewerEditFromContext(aCommandContext,
                                   getter_AddRefs(documentEdit));
  NS_ENSURE_TRUE(documentEdit, NS_ERROR_NOT_INITIALIZED);

  return DoClipboardCommand(aCommandName, documentEdit, nullptr);
}

NS_IMETHODIMP
nsSelectionCommand::GetCommandStateParams(const char* aCommandName,
                                          nsICommandParams* aParams,
                                          nsISupports* aCommandContext) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSelectionCommand::DoCommandParams(const char* aCommandName,
                                    nsICommandParams* aParams,
                                    nsISupports* aCommandContext) {
  nsCOMPtr<nsIDocumentViewerEdit> documentEdit;
  GetDocumentViewerEditFromContext(aCommandContext,
                                   getter_AddRefs(documentEdit));
  NS_ENSURE_TRUE(documentEdit, NS_ERROR_NOT_INITIALIZED);

  return DoClipboardCommand(aCommandName, documentEdit, aParams);
}

nsresult nsSelectionCommand::GetDocumentViewerEditFromContext(
    nsISupports* aContext, nsIDocumentViewerEdit** aEditInterface) {
  NS_ENSURE_ARG(aEditInterface);
  *aEditInterface = nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocumentViewer> viewer;
  docShell->GetDocViewer(getter_AddRefs(viewer));
  nsCOMPtr<nsIDocumentViewerEdit> edit(do_QueryInterface(viewer));
  NS_ENSURE_TRUE(edit, NS_ERROR_FAILURE);

  edit.forget(aEditInterface);
  return NS_OK;
}

#if 0
#  pragma mark -
#endif

#define NS_DECL_CLIPBOARD_COMMAND(_cmd)                                       \
  class _cmd : public nsSelectionCommand {                                    \
   protected:                                                                 \
    virtual nsresult IsClipboardCommandEnabled(const char* aCommandName,      \
                                               nsIDocumentViewerEdit* aEdit,  \
                                               bool* outCmdEnabled) override; \
    virtual nsresult DoClipboardCommand(const char* aCommandName,             \
                                        nsIDocumentViewerEdit* aEdit,         \
                                        nsICommandParams* aParams) override;  \
    /* no member variables, please, we're stateless! */                       \
  };

NS_DECL_CLIPBOARD_COMMAND(nsClipboardCopyLinkCommand)
NS_DECL_CLIPBOARD_COMMAND(nsClipboardImageCommands)
NS_DECL_CLIPBOARD_COMMAND(nsClipboardSelectAllNoneCommands)

nsresult nsClipboardCopyLinkCommand::IsClipboardCommandEnabled(
    const char* aCommandName, nsIDocumentViewerEdit* aEdit,
    bool* outCmdEnabled) {
  return aEdit->GetInLink(outCmdEnabled);
}

nsresult nsClipboardCopyLinkCommand::DoClipboardCommand(
    const char* aCommandName, nsIDocumentViewerEdit* aEdit,
    nsICommandParams* aParams) {
  return aEdit->CopyLinkLocation();
}

#if 0
#  pragma mark -
#endif

nsresult nsClipboardImageCommands::IsClipboardCommandEnabled(
    const char* aCommandName, nsIDocumentViewerEdit* aEdit,
    bool* outCmdEnabled) {
  return aEdit->GetInImage(outCmdEnabled);
}

nsresult nsClipboardImageCommands::DoClipboardCommand(
    const char* aCommandName, nsIDocumentViewerEdit* aEdit,
    nsICommandParams* aParams) {
  if (!nsCRT::strcmp(sCopyImageLocationString, aCommandName))
    return aEdit->CopyImage(nsIDocumentViewerEdit::COPY_IMAGE_TEXT);
  if (!nsCRT::strcmp(sCopyImageContentsString, aCommandName))
    return aEdit->CopyImage(nsIDocumentViewerEdit::COPY_IMAGE_DATA);
  int32_t copyFlags = nsIDocumentViewerEdit::COPY_IMAGE_DATA |
                      nsIDocumentViewerEdit::COPY_IMAGE_HTML;
  if (aParams) {
    copyFlags = aParams->AsCommandParams()->GetInt("imageCopy");
  }
  return aEdit->CopyImage(copyFlags);
}

#if 0
#  pragma mark -
#endif

nsresult nsClipboardSelectAllNoneCommands::IsClipboardCommandEnabled(
    const char* aCommandName, nsIDocumentViewerEdit* aEdit,
    bool* outCmdEnabled) {
  *outCmdEnabled = true;
  return NS_OK;
}

nsresult nsClipboardSelectAllNoneCommands::DoClipboardCommand(
    const char* aCommandName, nsIDocumentViewerEdit* aEdit,
    nsICommandParams* aParams) {
  if (!nsCRT::strcmp(sSelectAllString, aCommandName)) return aEdit->SelectAll();

  return aEdit->ClearSelection();
}

#if 0
#  pragma mark -
#endif

#if 0  // Remove unless needed again, bug 204777
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

class nsLookUpDictionaryCommand final : public nsIControllerCommand {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLERCOMMAND

 private:
  virtual ~nsLookUpDictionaryCommand() = default;
};

NS_IMPL_ISUPPORTS(nsLookUpDictionaryCommand, nsIControllerCommand)

NS_IMETHODIMP
nsLookUpDictionaryCommand::IsCommandEnabled(const char* aCommandName,
                                            nsISupports* aCommandContext,
                                            bool* aRetval) {
  *aRetval = true;
  return NS_OK;
}

NS_IMETHODIMP
nsLookUpDictionaryCommand::GetCommandStateParams(const char* aCommandName,
                                                 nsICommandParams* aParams,
                                                 nsISupports* aCommandContext) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLookUpDictionaryCommand::DoCommand(const char* aCommandName,
                                     nsISupports* aCommandContext) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLookUpDictionaryCommand::DoCommandParams(const char* aCommandName,
                                           nsICommandParams* aParams,
                                           nsISupports* aCommandContext) {
  if (NS_WARN_IF(!nsContentUtils::IsSafeToRunScript())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCommandParams* params = aParams->AsCommandParams();

  ErrorResult error;
  int32_t x = params->GetInt("x", error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  int32_t y = params->GetInt("y", error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
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

  PresShell* presShell = docShell->GetPresShell();
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

  WidgetQueryContentEvent queryCharAtPointEvent(true, eQueryCharacterAtPoint,
                                                widget);
  queryCharAtPointEvent.mRefPoint.x = x;
  queryCharAtPointEvent.mRefPoint.y = y;
  ContentEventHandler handler(presContext);
  handler.OnQueryCharacterAtPoint(&queryCharAtPointEvent);

  if (NS_WARN_IF(queryCharAtPointEvent.Failed()) ||
      queryCharAtPointEvent.DidNotFindChar()) {
    return NS_ERROR_FAILURE;
  }

  WidgetQueryContentEvent querySelectedTextEvent(true, eQuerySelectedText,
                                                 widget);
  handler.OnQuerySelectedText(&querySelectedTextEvent);
  if (NS_WARN_IF(querySelectedTextEvent.DidNotFindSelection())) {
    return NS_ERROR_FAILURE;
  }

  uint32_t offset = queryCharAtPointEvent.mReply->StartOffset();
  uint32_t begin, length;

  // macOS prioritizes user selected text if the current point falls within the
  // selection range. So we check the selection first.
  if (querySelectedTextEvent.FoundSelection() &&
      querySelectedTextEvent.mReply->IsOffsetInRange(offset)) {
    begin = querySelectedTextEvent.mReply->StartOffset();
    length = querySelectedTextEvent.mReply->DataLength();
  } else {
    WidgetQueryContentEvent queryTextContentEvent(true, eQueryTextContent,
                                                  widget);
    // OSX 10.7 queries 50 characters before/after current point.  So we fetch
    // same length.
    if (offset > 50) {
      offset -= 50;
    } else {
      offset = 0;
    }
    queryTextContentEvent.InitForQueryTextContent(offset, 100);
    handler.OnQueryTextContent(&queryTextContentEvent);
    if (NS_WARN_IF(queryTextContentEvent.Failed()) ||
        NS_WARN_IF(queryTextContentEvent.mReply->IsDataEmpty())) {
      return NS_ERROR_FAILURE;
    }

    intl::WordRange range = intl::WordBreaker::FindWord(
        queryTextContentEvent.mReply->DataRef(),
        queryCharAtPointEvent.mReply->StartOffset() - offset);
    if (range.mEnd == range.mBegin) {
      return NS_ERROR_FAILURE;
    }
    begin = range.mBegin + offset;
    length = range.mEnd - range.mBegin;
  }

  WidgetQueryContentEvent queryLookUpContentEvent(true, eQueryTextContent,
                                                  widget);
  queryLookUpContentEvent.InitForQueryTextContent(begin, length);
  queryLookUpContentEvent.RequestFontRanges();
  handler.OnQueryTextContent(&queryLookUpContentEvent);
  if (NS_WARN_IF(queryLookUpContentEvent.Failed()) ||
      NS_WARN_IF(queryLookUpContentEvent.mReply->IsDataEmpty())) {
    return NS_ERROR_FAILURE;
  }

  WidgetQueryContentEvent queryTextRectEvent(true, eQueryTextRect, widget);
  queryTextRectEvent.InitForQueryTextRect(begin, length);
  handler.OnQueryTextRect(&queryTextRectEvent);
  if (NS_WARN_IF(queryTextRectEvent.Failed())) {
    return NS_ERROR_FAILURE;
  }

  widget->LookUpDictionary(queryLookUpContentEvent.mReply->DataRef(),
                           queryLookUpContentEvent.mReply->mFontRanges,
                           queryTextRectEvent.mReply->mWritingMode.IsVertical(),
                           queryTextRectEvent.mReply->mRect.TopLeft());

  return NS_OK;
}

/*---------------------------------------------------------------------------

  RegisterWindowCommands

----------------------------------------------------------------------------*/

#define NS_REGISTER_ONE_COMMAND(_cmdClass, _cmdName)           \
  {                                                            \
    _cmdClass* theCmd = new _cmdClass();                       \
    rv = aCommandTable->RegisterCommand(                       \
        _cmdName, static_cast<nsIControllerCommand*>(theCmd)); \
  }

#define NS_REGISTER_FIRST_COMMAND(_cmdClass, _cmdName) \
  {                                                    \
    _cmdClass* theCmd = new _cmdClass();               \
    rv = aCommandTable->RegisterCommand(               \
        _cmdName, static_cast<nsIControllerCommand*>(theCmd));

#define NS_REGISTER_NEXT_COMMAND(_cmdClass, _cmdName) \
  rv = aCommandTable->RegisterCommand(                \
      _cmdName, static_cast<nsIControllerCommand*>(theCmd));

#define NS_REGISTER_LAST_COMMAND(_cmdClass, _cmdName)        \
  rv = aCommandTable->RegisterCommand(                       \
      _cmdName, static_cast<nsIControllerCommand*>(theCmd)); \
  }

// static
nsresult nsWindowCommandRegistration::RegisterWindowCommands(
    nsControllerCommandTable* aCommandTable) {
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
  NS_REGISTER_NEXT_COMMAND(nsPhysicalSelectMoveScrollCommand,
                           sMoveRight2String);
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
  NS_REGISTER_ONE_COMMAND(nsClipboardCommand, "cmd_paste");
  NS_REGISTER_ONE_COMMAND(nsClipboardCopyLinkCommand, "cmd_copyLink");
  NS_REGISTER_FIRST_COMMAND(nsClipboardImageCommands, sCopyImageLocationString);
  NS_REGISTER_NEXT_COMMAND(nsClipboardImageCommands, sCopyImageContentsString);
  NS_REGISTER_LAST_COMMAND(nsClipboardImageCommands, sCopyImageString);
  NS_REGISTER_FIRST_COMMAND(nsClipboardSelectAllNoneCommands, sSelectAllString);
  NS_REGISTER_LAST_COMMAND(nsClipboardSelectAllNoneCommands, sSelectNoneString);

#if 0  // Remove unless needed again, bug 204777
  NS_REGISTER_ONE_COMMAND(nsGoBackCommand, "cmd_browserBack");
  NS_REGISTER_ONE_COMMAND(nsGoForwardCommand, "cmd_browserForward");
#endif

  NS_REGISTER_ONE_COMMAND(nsLookUpDictionaryCommand, "cmd_lookUpDictionary");

  return rv;
}

/* static */
bool nsGlobalWindowCommands::FindScrollCommand(
    const char* aCommandName, KeyboardScrollAction* aOutAction) {
  // Search for a keyboard scroll action to do for this command in
  // browseCommands and physicalBrowseCommands. Each command exists in only one
  // of them, so the order we examine browseCommands and physicalBrowseCommands
  // doesn't matter.

  const Command command = GetInternalCommand(aCommandName);
  if (command == Command::DoNothing) {
    return false;
  }
  for (const BrowseCommand& browseCommand : browseCommands) {
    const bool forward = command == browseCommand.forward;
    const bool reverse = command == browseCommand.reverse;
    if (forward || reverse) {
      *aOutAction = KeyboardScrollAction(browseCommand.scrollAction, forward);
      return true;
    }
  }

  for (const PhysicalBrowseCommand& browseCommand : physicalBrowseCommands) {
    if (command != browseCommand.command) {
      continue;
    }
    const bool forward =
        (browseCommand.direction == nsISelectionController::MOVE_RIGHT ||
         browseCommand.direction == nsISelectionController::MOVE_DOWN);

    *aOutAction = KeyboardScrollAction(browseCommand.scrollAction, forward);
    return true;
  }

  return false;
}
