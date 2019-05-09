/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorCommands.h"

#include "mozilla/HTMLEditor.h"          // for HTMLEditor
#include "mozilla/TextEditor.h"          // for TextEditor
#include "mozilla/dom/Document.h"        // for Document
#include "nsCommandParams.h"             // for nsCommandParams
#include "nsIDocShell.h"                 // for nsIDocShell
#include "nsIEditingSession.h"           // for nsIEditingSession, etc
#include "nsISelectionController.h"      // for nsISelectionController
#include "nsISupportsImpl.h"             // for nsPresContext::Release
#include "nsISupportsUtils.h"            // for NS_IF_ADDREF
#include "nsIURI.h"                      // for nsIURI
#include "nsPresContext.h"               // for nsPresContext

// defines
#define STATE_ENABLED "state_enabled"
#define STATE_ALL "state_all"
#define STATE_ATTRIBUTE "state_attribute"
#define STATE_DATA "state_data"

namespace mozilla {

/*****************************************************************************
 * mozilla::SetDocumentStateCommand
 *
 *  Commands for document state that may be changed via doCommandParams
 *  As of 11/11/02, this is just "cmd_setDocumentModified"
 *  Note that you can use the same command class, SetDocumentStateCommand,
 *    for more than one of this type of command
 *    We check the input command param for different behavior
 *****************************************************************************/

StaticRefPtr<SetDocumentStateCommand> SetDocumentStateCommand::sInstance;

bool SetDocumentStateCommand::IsCommandEnabled(Command aCommand,
                                               TextEditor* aTextEditor) const {
  // These commands are always enabled if given editor is an HTMLEditor.
  return aTextEditor && aTextEditor->AsHTMLEditor();
}

nsresult SetDocumentStateCommand::DoCommand(Command aCommand,
                                            TextEditor& aTextEditor) const {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult SetDocumentStateCommand::DoCommandParams(
    Command aCommand, nsCommandParams* aParams, TextEditor& aTextEditor) const {
  if (NS_WARN_IF(!aParams)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aTextEditor.AsHTMLEditor())) {
    return NS_ERROR_FAILURE;
  }

  switch (aCommand) {
    case Command::SetDocumentModified: {
      ErrorResult error;
      bool modified = aParams->GetBool(STATE_ATTRIBUTE, error);
      // Should we fail if this param wasn't set?
      // I'm not sure we should be that strict
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      if (modified) {
        nsresult rv = aTextEditor.IncrementModificationCount(1);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
      nsresult rv = aTextEditor.ResetModificationCount();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::SetDocumentReadOnly: {
      ErrorResult error;
      bool isReadOnly = aParams->GetBool(STATE_ATTRIBUTE, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      if (isReadOnly) {
        nsresult rv =
            aTextEditor.AddFlags(nsIPlaintextEditor::eEditorReadonlyMask);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
      nsresult rv =
          aTextEditor.RemoveFlags(nsIPlaintextEditor::eEditorReadonlyMask);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::SetDocumentUseCSS: {
      HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      ErrorResult error;
      bool desireCSS = aParams->GetBool(STATE_ATTRIBUTE, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      nsresult rv = htmlEditor->SetIsCSSEnabled(desireCSS);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::SetDocumentInsertBROnEnterKeyPress: {
      HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      ErrorResult error;
      bool insertBrOnReturn = aParams->GetBool(STATE_ATTRIBUTE, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      nsresult rv = htmlEditor->SetReturnInParagraphCreatesNewParagraph(
          !insertBrOnReturn);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::SetDocumentDefaultParagraphSeparator: {
      HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }

      nsAutoCString newValue;
      nsresult rv = aParams->GetCString(STATE_ATTRIBUTE, newValue);
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
    case Command::ToggleObjectResizers: {
      HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      ErrorResult error;
      bool enabled = aParams->GetBool(STATE_ATTRIBUTE, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      MOZ_KnownLive(htmlEditor)->EnableObjectResizer(enabled);
      return NS_OK;
    }
    case Command::ToggleInlineTableEditor: {
      HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      ErrorResult error;
      bool enabled = aParams->GetBool(STATE_ATTRIBUTE, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      MOZ_KnownLive(htmlEditor)->EnableInlineTableEditor(enabled);
      return NS_OK;
    }
    case Command::ToggleAbsolutePositionEditor: {
      HTMLEditor* htmlEditor = aTextEditor.AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      ErrorResult error;
      bool enabled = aParams->GetBool(STATE_ATTRIBUTE, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      MOZ_KnownLive(htmlEditor)->EnableAbsolutePositionEditor(enabled);
      return NS_OK;
    }
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

nsresult SetDocumentStateCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  // If the result is set to STATE_ALL as bool value, queryCommandState()
  // returns the bool value.
  // If the result is set to STATE_ATTRIBUTE as CString value,
  // queryCommandValue() returns the string value.
  // Otherwise, ignored.

  // The base editor owns most state info
  if (NS_WARN_IF(!aTextEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aTextEditor->AsHTMLEditor())) {
    return NS_ERROR_FAILURE;
  }

  // Always get the enabled state
  nsresult rv =
      aParams.SetBool(STATE_ENABLED, IsCommandEnabled(aCommand, aTextEditor));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  switch (aCommand) {
    case Command::SetDocumentModified: {
      bool modified;
      rv = aTextEditor->GetDocumentModified(&modified);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // XXX Nobody refers this result due to wrong type.
      rv = aParams.SetBool(STATE_ATTRIBUTE, modified);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::SetDocumentReadOnly: {
      // XXX Nobody refers this result due to wrong type.
      rv = aParams.SetBool(STATE_ATTRIBUTE, aTextEditor->IsReadonly());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::SetDocumentUseCSS: {
      HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      rv = aParams.SetBool(STATE_ALL, htmlEditor->IsCSSEnabled());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::SetDocumentInsertBROnEnterKeyPress: {
      HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      bool createPOnReturn;
      htmlEditor->GetReturnInParagraphCreatesNewParagraph(&createPOnReturn);
      // XXX Nobody refers this result due to wrong type.
      rv = aParams.SetBool(STATE_ATTRIBUTE, !createPOnReturn);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::SetDocumentDefaultParagraphSeparator: {
      HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }

      switch (htmlEditor->GetDefaultParagraphSeparator()) {
        case ParagraphSeparator::div: {
          DebugOnly<nsresult> rv =
              aParams.SetCString(STATE_ATTRIBUTE, NS_LITERAL_CSTRING("div"));
          NS_WARNING_ASSERTION(
              NS_SUCCEEDED(rv),
              "Failed to set command params to return \"div\"");
          return NS_OK;
        }
        case ParagraphSeparator::p: {
          DebugOnly<nsresult> rv =
              aParams.SetCString(STATE_ATTRIBUTE, NS_LITERAL_CSTRING("p"));
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "Failed to set command params to return \"p\"");
          return NS_OK;
        }
        case ParagraphSeparator::br: {
          DebugOnly<nsresult> rv =
              aParams.SetCString(STATE_ATTRIBUTE, NS_LITERAL_CSTRING("br"));
          NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                               "Failed to set command params to return \"br\"");
          return NS_OK;
        }
        default:
          MOZ_ASSERT_UNREACHABLE("Invalid paragraph separator value");
          return NS_ERROR_UNEXPECTED;
      }
    }
    case Command::ToggleObjectResizers: {
      HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      // We returned the result as STATE_ATTRIBUTE with bool value 60 or
      // earlier. So, the result was ignored by both
      // nsHTMLDocument::QueryCommandValue() and
      // nsHTMLDocument::QueryCommandState().
      rv = aParams.SetBool(STATE_ALL, htmlEditor->IsObjectResizerEnabled());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::ToggleInlineTableEditor: {
      HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      // We returned the result as STATE_ATTRIBUTE with bool value 60 or
      // earlier. So, the result was ignored by both
      // nsHTMLDocument::QueryCommandValue() and
      // nsHTMLDocument::QueryCommandState().
      rv = aParams.SetBool(STATE_ALL, htmlEditor->IsInlineTableEditorEnabled());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    case Command::ToggleAbsolutePositionEditor: {
      HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      return aParams.SetBool(STATE_ALL,
                             htmlEditor->IsAbsolutePositionEditorEnabled());
    }
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

/*****************************************************************************
 * mozilla::DocumentStateCommand
 *
 * Commands just for state notification
 *  As of 11/21/02, possible commands are:
 *    "obs_documentCreated"
 *    "obs_documentWillBeDestroyed"
 *    "obs_documentLocationChanged"
 *  Note that you can use the same command class, DocumentStateCommand
 *    for these or future observer commands.
 *    We check the input command param for different behavior
 *
 *  How to use:
 *  1. Get the nsCommandManager for the current editor
 *  2. Implement an nsIObserve object, e.g:
 *
 *    void Observe(
 *        in nsISupports aSubject, // The nsCommandManager calling this
 *                                 // Observer
 *        in string      aTopic,   // command name, e.g.:"obs_documentCreated"
 *                                 //    or "obs_documentWillBeDestroyed"
          in wstring     aData );  // ignored (set to "command_status_changed")
 *
 *  3. Add the observer by:
 *       commandManager.addObserver(observeobject, obs_documentCreated);
 *  4. In the appropriate location in editorSession, editor, or commands code,
 *     trigger the notification of this observer by something like:
 *
 *  RefPtr<nsCommandManager> commandManager = mDocShell->GetCommandManager();
 *  commandManager->CommandStatusChanged(obs_documentCreated);
 *
 *  5. Use GetCommandStateParams() to obtain state information
 *     e.g., any creation state codes when creating an editor are
 *     supplied for "obs_documentCreated" command in the
 *     "state_data" param's value
 *****************************************************************************/

StaticRefPtr<DocumentStateCommand> DocumentStateCommand::sInstance;

bool DocumentStateCommand::IsCommandEnabled(Command aCommand,
                                            TextEditor* aTextEditor) const {
  // Always return false to discourage callers from using DoCommand()
  return false;
}

nsresult DocumentStateCommand::DoCommand(Command aCommand,
                                         TextEditor& aTextEditor) const {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult DocumentStateCommand::DoCommandParams(Command aCommand,
                                               nsCommandParams* aParams,
                                               TextEditor& aTextEditor) const {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult DocumentStateCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
  switch (aCommand) {
    case Command::EditorObserverDocumentCreated: {
      uint32_t editorStatus = nsIEditingSession::eEditorErrorUnknown;
      if (aEditingSession) {
        // Current context is initially set to nsIEditingSession until editor is
        // successfully created and source doc is loaded.  Embedder gets error
        // status if this fails.  If called before startup is finished,
        // status will be eEditorCreationInProgress.
        nsresult rv = aEditingSession->GetEditorStatus(&editorStatus);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else if (aTextEditor) {
        // If current context is an editor, then everything started up OK!
        editorStatus = nsIEditingSession::eEditorOK;
      }

      // Note that if refCon is not-null, but is neither
      // an nsIEditingSession or nsIEditor, we return "eEditorErrorUnknown"
      DebugOnly<nsresult> rv = aParams.SetInt(STATE_DATA, editorStatus);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to set editor status");
      return NS_OK;
    }
    case Command::EditorObserverDocumentLocationChanged: {
      if (!aTextEditor) {
        return NS_OK;
      }
      Document* document = aTextEditor->GetDocument();
      if (NS_WARN_IF(!document)) {
        return NS_ERROR_FAILURE;
      }
      nsIURI* uri = document->GetDocumentURI();
      if (NS_WARN_IF(!uri)) {
        return NS_ERROR_FAILURE;
      }
      nsresult rv = aParams.SetISupports(STATE_DATA, uri);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

}  // namespace mozilla
