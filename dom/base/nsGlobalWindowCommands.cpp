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
#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"

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


#if 0
#pragma mark -
#endif

// a base class for selection-related commands, for code sharing
class nsSelectionCommandsBase : public nsIControllerCommand
{
public:
  virtual ~nsSelectionCommandsBase() {}

  NS_DECL_ISUPPORTS
  NS_IMETHOD IsCommandEnabled(const char * aCommandName, nsISupports *aCommandContext, bool *_retval);
  NS_IMETHOD GetCommandStateParams(const char * aCommandName, nsICommandParams *aParams, nsISupports *aCommandContext);
  NS_IMETHOD DoCommandParams(const char * aCommandName, nsICommandParams *aParams, nsISupports *aCommandContext);

protected:

  static nsresult  GetPresShellFromWindow(nsPIDOMWindow *aWindow, nsIPresShell **aPresShell);
  static nsresult  GetSelectionControllerFromWindow(nsPIDOMWindow *aWindow, nsISelectionController **aSelCon);

  // no member variables, please, we're stateless!
};

// this class implements commands whose behavior depends on the 'browse with caret' setting
class nsSelectMoveScrollCommand : public nsSelectionCommandsBase
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
  *outCmdEnabled = true;
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

/* void doCommandParams (in string aCommandName, in nsICommandParams aParams, in nsISupports aCommandContext); */
NS_IMETHODIMP
nsSelectionCommandsBase::DoCommandParams(const char *aCommandName,
                                       nsICommandParams *aParams, nsISupports *aCommandContext)
{
  return DoCommand(aCommandName, aCommandContext);
}

// protected methods

nsresult
nsSelectionCommandsBase::GetPresShellFromWindow(nsPIDOMWindow *aWindow, nsIPresShell **aPresShell)
{
  *aPresShell = nullptr;
  NS_ENSURE_TRUE(aWindow, NS_ERROR_FAILURE);

  nsIDocShell *docShell = aWindow->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aPresShell = docShell->GetPresShell());
  return NS_OK;
}

nsresult
nsSelectionCommandsBase::GetSelectionControllerFromWindow(nsPIDOMWindow *aWindow, nsISelectionController **aSelCon)
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
  nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);       

  // We allow the caret to be moved with arrow keys on any window for which
  // the caret is enabled. In particular, this includes caret-browsing mode
  // in non-chrome documents.
  bool caretOn = false;
  selCont->GetCaretEnabled(&caretOn);
  if (!caretOn) {
    caretOn = Preferences::GetBool("accessibility.browsewithcaret");
    if (caretOn) {
      nsCOMPtr<nsIDocShell> docShell = piWindow->GetDocShell();
      if (docShell && docShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
        caretOn = false;
      }
    }
  }

  for (size_t i = 0; i < ArrayLength(browseCommands); i++) {
    bool forward = !strcmp(aCommandName, browseCommands[i].forward);
    if (forward || !strcmp(aCommandName, browseCommands[i].reverse)) {
      if (caretOn && browseCommands[i].move &&
          NS_SUCCEEDED((selCont->*(browseCommands[i].move))(forward, false))) {
        // adjust the focus to the new caret position
        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        if (fm) {
          nsCOMPtr<nsIDOMElement> result;
          fm->MoveFocus(piWindow, nullptr, nsIFocusManager::MOVEFOCUS_CARET,
                        nsIFocusManager::FLAG_NOSCROLL,
                        getter_AddRefs(result));
        }
        return NS_OK;
      }
      return (selCont->*(browseCommands[i].scroll))(forward);
    }
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}


#if 0
#pragma mark -
#endif

nsresult
nsSelectCommand::DoCommand(const char *aCommandName, nsISupports *aCommandContext)
{
  nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(aCommandContext));
  nsCOMPtr<nsISelectionController> selCont;
  GetSelectionControllerFromWindow(piWindow, getter_AddRefs(selCont));
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);       

  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

  // These commands are so the browser can use caret navigation key bindings -
  // Helps with accessibility - aaronl@netscape.com
  if (!nsCRT::strcmp(aCommandName, sSelectCharPreviousString))
    rv = selCont->CharacterMove(false, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectCharNextString))
    rv = selCont->CharacterMove(true, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectWordPreviousString))
    rv = selCont->WordMove(false, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectWordNextString))
    rv = selCont->WordMove(true, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectBeginLineString))
    rv = selCont->IntraLineMove(false, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectEndLineString))
    rv = selCont->IntraLineMove(true, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectLinePreviousString))
    rv = selCont->LineMove(false, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectLineNextString))
    rv = selCont->LineMove(true, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectPageUpString))
    rv = selCont->PageMove(false, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectPageDownString))
    rv = selCont->PageMove(true, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectTopString))
    rv = selCont->CompleteMove(false, true);
  else if (!nsCRT::strcmp(aCommandName, sSelectBottomString))
    rv = selCont->CompleteMove(true, true);

  return rv;
}

#if 0
#pragma mark -
#endif

class nsClipboardCommand MOZ_FINAL : public nsIControllerCommand
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
  *outCmdEnabled = false;

  if (strcmp(aCommandName, "cmd_copy"))
    return NS_OK;

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
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

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsCopySupport::FireClipboardEvent(NS_COPY, nsIClipboard::kGlobalClipboard, presShell, nullptr);
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
  virtual ~nsSelectionCommand() {}

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

NS_IMPL_ISUPPORTS1(nsWebNavigationBaseCommand, nsIControllerCommand)

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

class nsClipboardDragDropHookCommand MOZ_FINAL : public nsIControllerCommand
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
  return aParams->SetBooleanValue("state_enabled", true);
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
