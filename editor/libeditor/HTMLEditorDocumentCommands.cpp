/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/HTMLEditor.h"         // for HTMLEditor
#include "mozilla/HTMLEditorCommands.h" // for SetDocumentOptionsCommand, etc
#include "mozilla/TextEditor.h"         // for TextEditor
#include "nsCommandParams.h"            // for nsCommandParams
#include "nsCOMPtr.h"                   // for nsCOMPtr, do_QueryInterface, etc
#include "nsCRT.h"                      // for nsCRT
#include "nsDebug.h"                    // for NS_ENSURE_ARG_POINTER, etc
#include "nsError.h"                    // for NS_ERROR_INVALID_ARG, etc
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
  if (NS_WARN_IF(!outCmdEnabled)) {
    return NS_ERROR_INVALID_ARG;
  }

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
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

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

  nsCommandParams* params = aParams->AsCommandParams();

  IgnoredErrorResult error;
  int32_t animationMode = params->GetInt("imageAnimation", error);
  if (!error.Failed()) {
    // for possible values of animation mode, see:
    // http://lxr.mozilla.org/seamonkey/source/image/public/imgIContainer.idl
    presContext->SetImageAnimationMode(animationMode);
  } else {
    error.SuppressException();
  }

  bool allowPlugins = params->GetBool("plugins", error);
  if (error.Failed()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell(presContext->GetDocShell());
  if (NS_WARN_IF(!docShell)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = docShell->SetAllowPlugins(allowPlugins);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
SetDocumentOptionsCommand::GetCommandStateParams(const char* aCommandName,
                                                 nsICommandParams* aParams,
                                                 nsISupports* refCon)
{
  if (NS_WARN_IF(!aParams) || NS_WARN_IF(!refCon)) {
    return NS_ERROR_INVALID_ARG;
  }

  // The base editor owns most state info
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  TextEditor* textEditor = editor->AsTextEditor();
  MOZ_ASSERT(textEditor);

  nsCommandParams* params = aParams->AsCommandParams();

  // Always get the enabled state
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  nsresult rv = params->SetBool(STATE_ENABLED, outCmdEnabled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // get pres context
  RefPtr<nsPresContext> presContext = textEditor->GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return NS_ERROR_FAILURE;
  }

  IgnoredErrorResult error;
  Unused << params->GetInt("imageAnimation", error);
  if (!error.Failed()) {
    // for possible values of animation mode, see
    // http://lxr.mozilla.org/seamonkey/source/image/public/imgIContainer.idl
    rv = params->SetInt("imageAnimation", presContext->ImageAnimationMode());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    error.SuppressException();
  }

  bool allowPlugins = params->GetBool("plugins", error);
  if (error.Failed()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell(presContext->GetDocShell());
  if (NS_WARN_IF(!docShell)) {
    return NS_ERROR_FAILURE;
  }

  allowPlugins = docShell->PluginsAllowedInCurrentDoc();
  rv = params->SetBool("plugins", allowPlugins);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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
  if (NS_WARN_IF(!outCmdEnabled)) {
    return NS_ERROR_INVALID_ARG;
  }

  // These commands are always enabled
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
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  TextEditor* textEditor = editor->AsTextEditor();
  MOZ_ASSERT(textEditor);

  nsCommandParams* params = aParams->AsCommandParams();

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentModified")) {
    ErrorResult error;
    bool modified = params->GetBool(STATE_ATTRIBUTE, error);
    // Should we fail if this param wasn't set?
    // I'm not sure we should be that strict
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    if (modified) {
      nsresult rv = textEditor->IncrementModificationCount(1);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    nsresult rv = textEditor->ResetModificationCount();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentReadOnly")) {
    ErrorResult error;
    bool isReadOnly = params->GetBool(STATE_ATTRIBUTE, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    if (isReadOnly) {
      nsresult rv =
        textEditor->AddFlags(nsIPlaintextEditor::eEditorReadonlyMask);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    nsresult rv =
      textEditor->RemoveFlags(nsIPlaintextEditor::eEditorReadonlyMask);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentUseCSS")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }
    ErrorResult error;
    bool desireCSS = params->GetBool(STATE_ATTRIBUTE, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    nsresult rv = htmlEditor->SetIsCSSEnabled(desireCSS);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_insertBrOnReturn")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }
    ErrorResult error;
    bool insertBrOnReturn = params->GetBool(STATE_ATTRIBUTE, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    nsresult rv =
      htmlEditor->SetReturnInParagraphCreatesNewParagraph(!insertBrOnReturn);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_defaultParagraphSeparator")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    nsAutoCString newValue;
    nsresult rv = params->GetCString(STATE_ATTRIBUTE, newValue);
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
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }
    ErrorResult error;
    bool enabled = params->GetBool(STATE_ATTRIBUTE, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    nsresult rv = htmlEditor->SetObjectResizingEnabled(enabled);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableInlineTableEditing")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }
    ErrorResult error;
    bool enabled = params->GetBool(STATE_ATTRIBUTE, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    nsresult rv = htmlEditor->SetInlineTableEditingEnabled(enabled);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SetDocumentStateCommand::GetCommandStateParams(const char* aCommandName,
                                               nsICommandParams* aParams,
                                               nsISupports* refCon)
{
  if (NS_WARN_IF(!aParams) || NS_WARN_IF(!refCon)) {
    return NS_ERROR_INVALID_ARG;
  }

  // The base editor owns most state info
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (NS_WARN_IF(!editor)) {
    return NS_ERROR_INVALID_ARG;
  }
  TextEditor* textEditor = editor->AsTextEditor();
  MOZ_ASSERT(textEditor);

  nsCommandParams* params = aParams->AsCommandParams();

  // Always get the enabled state
  bool outCmdEnabled = false;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  nsresult rv = params->SetBool(STATE_ENABLED, outCmdEnabled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentModified")) {
    bool modified;
    rv = textEditor->GetDocumentModified(&modified);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = params->SetBool(STATE_ATTRIBUTE, modified);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentReadOnly")) {
    rv = params->SetBool(STATE_ATTRIBUTE, textEditor->IsReadonly());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentUseCSS")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }
    bool isCSS;
    htmlEditor->GetIsCSSEnabled(&isCSS);
    rv = params->SetBool(STATE_ALL, isCSS);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_insertBrOnReturn")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }
    bool createPOnReturn;
    htmlEditor->GetReturnInParagraphCreatesNewParagraph(&createPOnReturn);
    rv = params->SetBool(STATE_ATTRIBUTE, !createPOnReturn);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_defaultParagraphSeparator")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }

    switch (htmlEditor->GetDefaultParagraphSeparator()) {
      case ParagraphSeparator::div: {
        DebugOnly<nsresult> rv =
          params->SetCString(STATE_ATTRIBUTE, NS_LITERAL_CSTRING("div"));
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
          "Failed to set command params to return \"div\"");
        return NS_OK;
      }
      case ParagraphSeparator::p: {
        DebugOnly<nsresult> rv =
          params->SetCString(STATE_ATTRIBUTE, NS_LITERAL_CSTRING("p"));
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
          "Failed to set command params to return \"p\"");
        return NS_OK;
      }
      case ParagraphSeparator::br: {
        DebugOnly<nsresult> rv =
          params->SetCString(STATE_ATTRIBUTE, NS_LITERAL_CSTRING("br"));
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
          "Failed to set command params to return \"br\"");
        return NS_OK;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid paragraph separator value");
        return NS_ERROR_UNEXPECTED;
    }
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableObjectResizing")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }
    bool enabled;
    htmlEditor->GetObjectResizingEnabled(&enabled);
    rv = params->SetBool(STATE_ATTRIBUTE, enabled);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableInlineTableEditing")) {
    HTMLEditor* htmlEditor = textEditor->AsHTMLEditor();
    if (NS_WARN_IF(!htmlEditor)) {
      return NS_ERROR_INVALID_ARG;
    }
    bool enabled;
    htmlEditor->GetInlineTableEditingEnabled(&enabled);
    rv = params->SetBool(STATE_ATTRIBUTE, enabled);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
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
  if (NS_WARN_IF(!outCmdEnabled)) {
    return NS_ERROR_INVALID_ARG;
  }

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
  if (NS_WARN_IF(!aParams) || NS_WARN_IF(!aCommandName)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCommandParams* params = aParams->AsCommandParams();

  if (!nsCRT::strcmp(aCommandName, "obs_documentCreated")) {
    uint32_t editorStatus = nsIEditingSession::eEditorErrorUnknown;

    nsCOMPtr<nsIEditingSession> editingSession = do_QueryInterface(refCon);
    if (editingSession) {
      // refCon is initially set to nsIEditingSession until editor
      //  is successfully created and source doc is loaded
      // Embedder gets error status if this fails
      // If called before startup is finished,
      //    status = eEditorCreationInProgress
      nsresult rv = editingSession->GetEditorStatus(&editorStatus);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // If refCon is an editor, then everything started up OK!
      nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
      if (editor) {
        editorStatus = nsIEditingSession::eEditorOK;
      }
    }

    // Note that if refCon is not-null, but is neither
    // an nsIEditingSession or nsIEditor, we return "eEditorErrorUnknown"
    DebugOnly<nsresult> rv = params->SetInt(STATE_DATA, editorStatus);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to set editor status");
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
    if (NS_WARN_IF(!doc)) {
      return NS_ERROR_FAILURE;
    }
    nsIURI* uri = doc->GetDocumentURI();
    if (NS_WARN_IF(!uri)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = params->SetISupports(STATE_DATA, uri);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace mozilla
