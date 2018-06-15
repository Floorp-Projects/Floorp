/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/HTMLEditor.h"         // for HTMLEditor
#include "mozilla/HTMLEditorCommands.h" // for SetDocumentOptionsCommand, etc
#include "mozilla/TextEditor.h"         // for TextEditor
#include "nsCOMPtr.h"                   // for nsCOMPtr, do_QueryInterface, etc
#include "nsCRT.h"                      // for nsCRT
#include "nsDebug.h"                    // for NS_ENSURE_ARG_POINTER, etc
#include "nsError.h"                    // for NS_ERROR_INVALID_ARG, etc
#include "nsICommandParams.h"           // for nsICommandParams
#include "nsIDocShell.h"                // for nsIDocShell
#include "nsIDocument.h"                // for nsIDocument
#include "nsIEditingSession.h"          // for nsIEditingSession, etc
#include "nsIEditor.h"                  // for nsIEditor
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc
#include "nsIPresShell.h"               // for nsIPresShell
#include "nsISelectionController.h"     // for nsISelectionController
#include "nsISupportsImpl.h"            // for nsPresContext::Release
#include "nsISupportsUtils.h"           // for NS_IF_ADDREF
#include "nsIURI.h"                     // for nsIURI
#include "nsPresContext.h"              // for nsPresContext
#include "nscore.h"                     // for NS_IMETHODIMP, nsresult, etc

class nsISupports;

//defines
#define STATE_ENABLED  "state_enabled"
#define STATE_ALL "state_all"
#define STATE_ATTRIBUTE "state_attribute"
#define STATE_DATA "state_data"

namespace mozilla {

NS_IMETHODIMP
SetDocumentOptionsCommand::IsCommandEnabled(const char* aCommandName,
                                            nsISupports* refCon,
                                            bool* outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (!editor) {
    *outCmdEnabled = false;
    return NS_OK;
  }
  TextEditor* textEditor = editor->AsTextEditor();
  MOZ_ASSERT(textEditor);
  *outCmdEnabled = textEditor->IsSelectionEditable();
  return NS_OK;
}

NS_IMETHODIMP
SetDocumentOptionsCommand::DoCommand(const char* aCommandName,
                                     nsISupports* refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SetDocumentOptionsCommand::DoCommandParams(const char* aCommandName,
                                           nsICommandParams* aParams,
                                           nsISupports* refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  TextEditor* textEditor = editor->AsTextEditor();
  MOZ_ASSERT(textEditor);

  RefPtr<nsPresContext> presContext = textEditor->GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return NS_ERROR_FAILURE;
  }

  int32_t animationMode;
  nsresult rv = aParams->GetLongValue("imageAnimation", &animationMode);
  if (NS_SUCCEEDED(rv)) {
    // for possible values of animation mode, see:
    // http://lxr.mozilla.org/seamonkey/source/image/public/imgIContainer.idl
    presContext->SetImageAnimationMode(animationMode);
  }

  bool allowPlugins;
  rv = aParams->GetBooleanValue("plugins", &allowPlugins);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocShell> docShell(presContext->GetDocShell());
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

    rv = docShell->SetAllowPlugins(allowPlugins);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
SetDocumentOptionsCommand::GetCommandStateParams(const char* aCommandName,
                                                 nsICommandParams* aParams,
                                                 nsISupports* refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  // The base editor owns most state info
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  TextEditor* textEditor = editor->AsTextEditor();
  MOZ_ASSERT(textEditor);

  // Always get the enabled state
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  nsresult rv = aParams->SetBooleanValue(STATE_ENABLED, outCmdEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  // get pres context
  RefPtr<nsPresContext> presContext = textEditor->GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return NS_ERROR_FAILURE;
  }

  int32_t animationMode;
  rv = aParams->GetLongValue("imageAnimation", &animationMode);
  if (NS_SUCCEEDED(rv)) {
    // for possible values of animation mode, see
    // http://lxr.mozilla.org/seamonkey/source/image/public/imgIContainer.idl
    rv = aParams->SetLongValue("imageAnimation",
                               presContext->ImageAnimationMode());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool allowPlugins = false;
  rv = aParams->GetBooleanValue("plugins", &allowPlugins);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocShell> docShell(presContext->GetDocShell());
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

    allowPlugins = docShell->PluginsAllowedInCurrentDoc();

    rv = aParams->SetBooleanValue("plugins", allowPlugins);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


/**
 *  Commands for document state that may be changed via doCommandParams
 *  As of 11/11/02, this is just "cmd_setDocumentModified"
 *  Note that you can use the same command class, nsSetDocumentStateCommand,
 *    for more than one of this type of command
 *    We check the input command param for different behavior
 */

NS_IMETHODIMP
SetDocumentStateCommand::IsCommandEnabled(const char* aCommandName,
                                          nsISupports* refCon,
                                          bool* outCmdEnabled)
{
  // These commands are always enabled
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  *outCmdEnabled = true;
  return NS_OK;
}

NS_IMETHODIMP
SetDocumentStateCommand::DoCommand(const char* aCommandName,
                                   nsISupports* refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SetDocumentStateCommand::DoCommandParams(const char* aCommandName,
                                         nsICommandParams* aParams,
                                         nsISupports* refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  TextEditor* textEditor = editor->AsTextEditor();
  MOZ_ASSERT(textEditor);

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentModified")) {
    NS_ENSURE_ARG_POINTER(aParams);

    bool modified;
    nsresult rv = aParams->GetBooleanValue(STATE_ATTRIBUTE, &modified);

    // Should we fail if this param wasn't set?
    // I'm not sure we should be that strict
    NS_ENSURE_SUCCESS(rv, rv);

    if (modified) {
      return textEditor->IncrementModificationCount(1);
    }

    return textEditor->ResetModificationCount();
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentReadOnly")) {
    NS_ENSURE_ARG_POINTER(aParams);
    bool isReadOnly;
    nsresult rvRO = aParams->GetBooleanValue(STATE_ATTRIBUTE, &isReadOnly);
    NS_ENSURE_SUCCESS(rvRO, rvRO);
    return isReadOnly ?
      textEditor->AddFlags(nsIPlaintextEditor::eEditorReadonlyMask) :
      textEditor->RemoveFlags(nsIPlaintextEditor::eEditorReadonlyMask);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentUseCSS")) {
    NS_ENSURE_ARG_POINTER(aParams);
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    bool desireCSS;
    nsresult rvCSS = aParams->GetBooleanValue(STATE_ATTRIBUTE, &desireCSS);
    NS_ENSURE_SUCCESS(rvCSS, rvCSS);

    return htmlEditor->SetIsCSSEnabled(desireCSS);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_insertBrOnReturn")) {
    NS_ENSURE_ARG_POINTER(aParams);
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    bool insertBrOnReturn;
    nsresult rvBR = aParams->GetBooleanValue(STATE_ATTRIBUTE,
                                              &insertBrOnReturn);
    NS_ENSURE_SUCCESS(rvBR, rvBR);

    return htmlEditor->SetReturnInParagraphCreatesNewParagraph(!insertBrOnReturn);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_defaultParagraphSeparator")) {
    if (NS_WARN_IF(!aParams)) {
      return NS_ERROR_NULL_POINTER;
    }
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    nsCString newValue;
    nsresult rv = aParams->GetCStringValue(STATE_ATTRIBUTE,
                                           getter_Copies(newValue));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (newValue.LowerCaseEqualsLiteral("div")) {
      htmlEditor->SetDefaultParagraphSeparator(ParagraphSeparator::div);
      return NS_OK;
    }
    if (newValue.LowerCaseEqualsLiteral("p")) {
      htmlEditor->SetDefaultParagraphSeparator(ParagraphSeparator::p);
      return NS_OK;
    }
    if (newValue.LowerCaseEqualsLiteral("br")) {
      // Mozilla extension for backwards compatibility
      htmlEditor->SetDefaultParagraphSeparator(ParagraphSeparator::br);
      return NS_OK;
    }

    // This should not be reachable from nsHTMLDocument::ExecCommand
    NS_WARNING("Invalid default paragraph separator");
    return NS_ERROR_UNEXPECTED;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableObjectResizing")) {
    NS_ENSURE_ARG_POINTER(aParams);
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    bool enabled;
    nsresult rvOR = aParams->GetBooleanValue(STATE_ATTRIBUTE, &enabled);
    NS_ENSURE_SUCCESS(rvOR, rvOR);

    return htmlEditor->SetObjectResizingEnabled(enabled);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableInlineTableEditing")) {
    NS_ENSURE_ARG_POINTER(aParams);
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    bool enabled;
    nsresult rvOR = aParams->GetBooleanValue(STATE_ATTRIBUTE, &enabled);
    NS_ENSURE_SUCCESS(rvOR, rvOR);

    return htmlEditor->SetInlineTableEditingEnabled(enabled);
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SetDocumentStateCommand::GetCommandStateParams(const char* aCommandName,
                                               nsICommandParams* aParams,
                                               nsISupports* refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  // The base editor owns most state info
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  TextEditor* textEditor = editor->AsTextEditor();
  MOZ_ASSERT(textEditor);

  // Always get the enabled state
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  nsresult rv = aParams->SetBooleanValue(STATE_ENABLED, outCmdEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentModified")) {
    bool modified;
    rv = textEditor->GetDocumentModified(&modified);
    NS_ENSURE_SUCCESS(rv, rv);

    return aParams->SetBooleanValue(STATE_ATTRIBUTE, modified);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentReadOnly")) {
    NS_ENSURE_ARG_POINTER(aParams);
    return aParams->SetBooleanValue(STATE_ATTRIBUTE, textEditor->IsReadonly());
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentUseCSS")) {
    NS_ENSURE_ARG_POINTER(aParams);
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    bool isCSS;
    htmlEditor->GetIsCSSEnabled(&isCSS);
    return aParams->SetBooleanValue(STATE_ALL, isCSS);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_insertBrOnReturn")) {
    NS_ENSURE_ARG_POINTER(aParams);
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    bool createPOnReturn;
    htmlEditor->GetReturnInParagraphCreatesNewParagraph(&createPOnReturn);
    return aParams->SetBooleanValue(STATE_ATTRIBUTE, !createPOnReturn);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_defaultParagraphSeparator")) {
    if (NS_WARN_IF(!aParams)) {
      return NS_ERROR_NULL_POINTER;
    }
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    switch (htmlEditor->GetDefaultParagraphSeparator()) {
      case ParagraphSeparator::div:
        aParams->SetCStringValue(STATE_ATTRIBUTE, "div");
        return NS_OK;

      case ParagraphSeparator::p:
        aParams->SetCStringValue(STATE_ATTRIBUTE, "p");
        return NS_OK;

      case ParagraphSeparator::br:
        aParams->SetCStringValue(STATE_ATTRIBUTE, "br");
        return NS_OK;

      default:
        MOZ_ASSERT_UNREACHABLE("Invalid paragraph separator value");
        return NS_ERROR_UNEXPECTED;
    }
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableObjectResizing")) {
    NS_ENSURE_ARG_POINTER(aParams);
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    bool enabled;
    htmlEditor->GetObjectResizingEnabled(&enabled);
    return aParams->SetBooleanValue(STATE_ATTRIBUTE, enabled);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableInlineTableEditing")) {
    NS_ENSURE_ARG_POINTER(aParams);
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    bool enabled;
    htmlEditor->GetInlineTableEditingEnabled(&enabled);
    return aParams->SetBooleanValue(STATE_ATTRIBUTE, enabled);
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

/**
 * Commands just for state notification
 *  As of 11/21/02, possible commands are:
 *    "obs_documentCreated"
 *    "obs_documentWillBeDestroyed"
 *    "obs_documentLocationChanged"
 *  Note that you can use the same command class, nsDocumentStateCommand
 *    for these or future observer commands.
 *    We check the input command param for different behavior
 *
 *  How to use:
 *  1. Get the nsICommandManager for the current editor
 *  2. Implement an nsIObserve object, e.g:
 *
 *    void Observe(
 *        in nsISupports aSubject, // The nsICommandManager calling this Observer
 *        in string      aTopic,   // command name, e.g.:"obs_documentCreated"
 *                                 //    or "obs_documentWillBeDestroyed"
          in wstring     aData );  // ignored (set to "command_status_changed")
 *
 *  3. Add the observer by:
 *       commandManager.addObserver(observeobject, obs_documentCreated);
 *  4. In the appropriate location in editorSession, editor, or commands code,
 *     trigger the notification of this observer by something like:
 *
 *  nsCOMPtr<nsICommandManager> commandManager = mDocShell->GetCommandManager();
 *  nsCOMPtr<nsPICommandUpdater> commandUpdater = do_QueryInterface(commandManager);
 *  NS_ENSURE_TRUE(commandUpdater, NS_ERROR_FAILURE);
 *    commandUpdater->CommandStatusChanged(obs_documentCreated);
 *
 *  5. Use GetCommandStateParams() to obtain state information
 *     e.g., any creation state codes when creating an editor are
 *     supplied for "obs_documentCreated" command in the
 *     "state_data" param's value
 *
 */

NS_IMETHODIMP
DocumentStateCommand::IsCommandEnabled(const char* aCommandName,
                                       nsISupports* refCon,
                                       bool* outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  // Always return false to discourage callers from using DoCommand()
  *outCmdEnabled = false;
  return NS_OK;
}

NS_IMETHODIMP
DocumentStateCommand::DoCommand(const char* aCommandName,
                                nsISupports* refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DocumentStateCommand::DoCommandParams(const char* aCommandName,
                                      nsICommandParams* aParams,
                                      nsISupports* refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DocumentStateCommand::GetCommandStateParams(const char* aCommandName,
                                            nsICommandParams* aParams,
                                            nsISupports* refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(aCommandName);
  nsresult rv;

  if (!nsCRT::strcmp(aCommandName, "obs_documentCreated")) {
    uint32_t editorStatus = nsIEditingSession::eEditorErrorUnknown;

    nsCOMPtr<nsIEditingSession> editingSession = do_QueryInterface(refCon);
    if (editingSession) {
      // refCon is initially set to nsIEditingSession until editor
      //  is successfully created and source doc is loaded
      // Embedder gets error status if this fails
      // If called before startup is finished,
      //    status = eEditorCreationInProgress
      rv = editingSession->GetEditorStatus(&editorStatus);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // If refCon is an editor, then everything started up OK!
      nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
      if (editor) {
        editorStatus = nsIEditingSession::eEditorOK;
      }
    }

    // Note that if refCon is not-null, but is neither
    // an nsIEditingSession or nsIEditor, we return "eEditorErrorUnknown"
    aParams->SetLongValue(STATE_DATA, editorStatus);
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "obs_documentLocationChanged")) {
    nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
    if (!editor) {
      return NS_OK;
    }
    TextEditor* textEditor = editor->AsTextEditor();
    MOZ_ASSERT(textEditor);

    nsCOMPtr<nsIDocument> doc = textEditor->GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    nsIURI *uri = doc->GetDocumentURI();
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

    return aParams->SetISupportsValue(STATE_DATA, (nsISupports*)uri);
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace mozilla
