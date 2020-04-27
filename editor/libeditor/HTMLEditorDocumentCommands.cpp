/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorCommands.h"

#include "mozilla/HTMLEditor.h"    // for HTMLEditor
#include "mozilla/TextEditor.h"    // for TextEditor
#include "mozilla/dom/Document.h"  // for Document
#include "nsCommandParams.h"       // for nsCommandParams
#include "nsIEditingSession.h"     // for nsIEditingSession, etc
#include "nsIPrincipal.h"          // for nsIPrincipal
#include "nsISupportsImpl.h"       // for nsPresContext::Release
#include "nsISupportsUtils.h"      // for NS_IF_ADDREF
#include "nsIURI.h"                // for nsIURI
#include "nsPresContext.h"         // for nsPresContext

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
                                            TextEditor& aTextEditor,
                                            nsIPrincipal* aPrincipal) const {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult SetDocumentStateCommand::DoCommandParam(
    Command aCommand, const Maybe<bool>& aBoolParam, TextEditor& aTextEditor,
    nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(aBoolParam.isNothing())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aTextEditor.AsHTMLEditor())) {
    return NS_ERROR_FAILURE;
  }

  switch (aCommand) {
    case Command::SetDocumentModified: {
      if (aBoolParam.value()) {
        nsresult rv = aTextEditor.IncrementModificationCount(1);
        NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                             "EditorBase::IncrementModificationCount() failed");
        return rv;
      }
      nsresult rv = aTextEditor.ResetModificationCount();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "EditorBase::ResetModificationCount() failed");
      return rv;
    }
    case Command::SetDocumentReadOnly: {
      ErrorResult error;
      if (aBoolParam.value()) {
        nsresult rv = aTextEditor.AddFlags(nsIEditor::eEditorReadonlyMask);
        NS_WARNING_ASSERTION(
            NS_SUCCEEDED(rv),
            "EditorBase::AddFlags(nsIEditor::eEditorReadonlyMask) failed");
        return rv;
      }
      nsresult rv = aTextEditor.RemoveFlags(nsIEditor::eEditorReadonlyMask);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "EditorBase::RemoveFlags(nsIEditor::eEditorReadonlyMask) failed");
      return rv;
    }
    case Command::SetDocumentUseCSS: {
      nsresult rv =
          aTextEditor.AsHTMLEditor()->SetIsCSSEnabled(aBoolParam.value());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "HTMLEditor::SetIsCSSEnabled() failed");
      return rv;
    }
    case Command::SetDocumentInsertBROnEnterKeyPress: {
      nsresult rv =
          aTextEditor.AsHTMLEditor()->SetReturnInParagraphCreatesNewParagraph(
              !aBoolParam.value());
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::SetReturnInParagraphCreatesNewParagraph() failed");
      return rv;
    }
    case Command::ToggleObjectResizers: {
      MOZ_KnownLive(aTextEditor.AsHTMLEditor())
          ->EnableObjectResizer(aBoolParam.value());
      return NS_OK;
    }
    case Command::ToggleInlineTableEditor: {
      MOZ_KnownLive(aTextEditor.AsHTMLEditor())
          ->EnableInlineTableEditor(aBoolParam.value());
      return NS_OK;
    }
    case Command::ToggleAbsolutePositionEditor: {
      MOZ_KnownLive(aTextEditor.AsHTMLEditor())
          ->EnableAbsolutePositionEditor(aBoolParam.value());
      return NS_OK;
    }
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

nsresult SetDocumentStateCommand::DoCommandParam(
    Command aCommand, const nsACString& aCStringParam, TextEditor& aTextEditor,
    nsIPrincipal* aPrincipal) const {
  if (NS_WARN_IF(aCStringParam.IsVoid())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aTextEditor.AsHTMLEditor())) {
    return NS_ERROR_FAILURE;
  }

  switch (aCommand) {
    case Command::SetDocumentDefaultParagraphSeparator: {
      if (aCStringParam.LowerCaseEqualsLiteral("div")) {
        aTextEditor.AsHTMLEditor()->SetDefaultParagraphSeparator(
            ParagraphSeparator::div);
        return NS_OK;
      }
      if (aCStringParam.LowerCaseEqualsLiteral("p")) {
        aTextEditor.AsHTMLEditor()->SetDefaultParagraphSeparator(
            ParagraphSeparator::p);
        return NS_OK;
      }
      if (aCStringParam.LowerCaseEqualsLiteral("br")) {
        // Mozilla extension for backwards compatibility
        aTextEditor.AsHTMLEditor()->SetDefaultParagraphSeparator(
            ParagraphSeparator::br);
        return NS_OK;
      }

      // This should not be reachable from nsHTMLDocument::ExecCommand
      // XXX Shouldn't return error in this case because Chrome does not throw
      //     exception in this case.
      NS_WARNING("Invalid default paragraph separator");
      return NS_ERROR_UNEXPECTED;
    }
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

nsresult SetDocumentStateCommand::GetCommandStateParams(
    Command aCommand, nsCommandParams& aParams, TextEditor* aTextEditor,
    nsIEditingSession* aEditingSession) const {
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
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::GetDocumentModified() failed");
        return rv;
      }
      // XXX Nobody refers this result due to wrong type.
      rv = aParams.SetBool(STATE_ATTRIBUTE, modified);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCommandParams::SetBool(STATE_ATTRIBUTE) failed");
      return rv;
    }
    case Command::SetDocumentReadOnly: {
      // XXX Nobody refers this result due to wrong type.
      rv = aParams.SetBool(STATE_ATTRIBUTE, aTextEditor->IsReadonly());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCommandParams::SetBool(STATE_ATTRIBUTE) failed");
      return rv;
    }
    case Command::SetDocumentUseCSS: {
      HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      rv = aParams.SetBool(STATE_ALL, htmlEditor->IsCSSEnabled());
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCommandParams::SetBool(STATE_ALL) failed");
      return rv;
    }
    case Command::SetDocumentInsertBROnEnterKeyPress: {
      HTMLEditor* htmlEditor = aTextEditor->AsHTMLEditor();
      if (NS_WARN_IF(!htmlEditor)) {
        return NS_ERROR_INVALID_ARG;
      }
      bool createPOnReturn;
      DebugOnly<nsresult> rvIgnored =
          htmlEditor->GetReturnInParagraphCreatesNewParagraph(&createPOnReturn);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rvIgnored),
          "HTMLEditor::GetReturnInParagraphCreatesNewParagraph() failed");
      // XXX Nobody refers this result due to wrong type.
      rv = aParams.SetBool(STATE_ATTRIBUTE, !createPOnReturn);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCommandParams::SetBool(STATE_ATTRIBUTE) failed");
      return rv;
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
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCommandParams::SetBool(STATE_ALL) failed");
      return rv;
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
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCommandParams::SetBool(STATE_ALL) failed");
      return rv;
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
                                         TextEditor& aTextEditor,
                                         nsIPrincipal* aPrincipal) const {
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
        if (NS_FAILED(rv)) {
          NS_WARNING("nsIEditingSession::GetEditorStatus() failed");
          return rv;
        }
      } else if (aTextEditor) {
        // If current context is an editor, then everything started up OK!
        editorStatus = nsIEditingSession::eEditorOK;
      }

      // Note that if refCon is not-null, but is neither
      // an nsIEditingSession or nsIEditor, we return "eEditorErrorUnknown"
      DebugOnly<nsresult> rvIgnored = aParams.SetInt(STATE_DATA, editorStatus);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "Failed to set editor status");
      return NS_OK;
    }
    case Command::EditorObserverDocumentLocationChanged: {
      if (!aTextEditor) {
        return NS_OK;
      }
      dom::Document* document = aTextEditor->GetDocument();
      if (NS_WARN_IF(!document)) {
        return NS_ERROR_FAILURE;
      }
      nsIURI* uri = document->GetDocumentURI();
      if (NS_WARN_IF(!uri)) {
        return NS_ERROR_FAILURE;
      }
      nsresult rv = aParams.SetISupports(STATE_DATA, uri);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "nsCOmmandParms::SetISupports(STATE_DATA) failed");
      return rv;
    }
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

}  // namespace mozilla
