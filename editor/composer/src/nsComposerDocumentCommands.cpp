/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ryan Cassin <rcassin@supernova.org>
 *   Daniel Glazman <glazman@netscape.com>
 *   Charles Manske <cmanske@netscape.com>
 *   Kathleen Brade <brade@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#include "nsIEditor.h"
#include "nsIEditingSession.h"
#include "nsIPlaintextEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIHTMLObjectResizer.h"
#include "nsIHTMLInlineTableEditor.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocShell.h"
#include "nsIURI.h"

#include "nsCOMPtr.h"

#include "nsComposerCommands.h"
#include "nsICommandParams.h"
#include "nsCRT.h"

//defines
#define STATE_ENABLED  "state_enabled"
#define STATE_ATTRIBUTE "state_attribute"
#define STATE_DATA "state_data"

static
nsresult
GetPresContextFromEditor(nsIEditor *aEditor, nsPresContext **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  NS_ENSURE_ARG_POINTER(aEditor);

  nsCOMPtr<nsISelectionController> selCon;
  nsresult rv = aEditor->GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell = do_QueryInterface(selCon);
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aResult = presShell->GetPresContext());
  return NS_OK;
}

NS_IMETHODIMP
nsSetDocumentOptionsCommand::IsCommandEnabled(const char * aCommandName,
                                              nsISupports *refCon,
                                              PRBool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSetDocumentOptionsCommand::DoCommand(const char *aCommandName,
                                       nsISupports *refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSetDocumentOptionsCommand::DoCommandParams(const char *aCommandName,
                                             nsICommandParams *aParams,
                                             nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);

  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_INVALID_ARG);

  nsRefPtr<nsPresContext> presContext;
  nsresult rv = GetPresContextFromEditor(editor, getter_AddRefs(presContext));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  PRInt32 animationMode; 
  rv = aParams->GetLongValue("imageAnimation", &animationMode);
  if (NS_SUCCEEDED(rv))
  {
    // for possible values of animation mode, see:
    // http://lxr.mozilla.org/seamonkey/source/modules/libpr0n/public/imgIContainer.idl
    presContext->SetImageAnimationMode(animationMode);
  }

  PRBool allowPlugins; 
  rv = aParams->GetBooleanValue("plugins", &allowPlugins);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupports> container = presContext->GetContainer();
    NS_ENSURE_TRUE(container, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

    rv = docShell->SetAllowPlugins(allowPlugins);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSetDocumentOptionsCommand::GetCommandStateParams(const char *aCommandName,
                                                   nsICommandParams *aParams,
                                                   nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  // The base editor owns most state info
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_INVALID_ARG);

  // Always get the enabled state
  PRBool outCmdEnabled = PR_FALSE;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  nsresult rv = aParams->SetBooleanValue(STATE_ENABLED, outCmdEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  // get pres context
  nsRefPtr<nsPresContext> presContext;
  rv = GetPresContextFromEditor(editor, getter_AddRefs(presContext));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  PRInt32 animationMode;
  rv = aParams->GetLongValue("imageAnimation", &animationMode);
  if (NS_SUCCEEDED(rv))
  {
    // for possible values of animation mode, see
    // http://lxr.mozilla.org/seamonkey/source/modules/libpr0n/public/imgIContainer.idl
    rv = aParams->SetLongValue("imageAnimation",
                               presContext->ImageAnimationMode());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRBool allowPlugins; 
  rv = aParams->GetBooleanValue("plugins", &allowPlugins);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupports> container = presContext->GetContainer();
    NS_ENSURE_TRUE(container, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

    rv = docShell->GetAllowPlugins(&allowPlugins);
    NS_ENSURE_SUCCESS(rv, rv);

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
nsSetDocumentStateCommand::IsCommandEnabled(const char * aCommandName,
                                            nsISupports *refCon,
                                            PRBool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  if (editor)
    return editor->GetIsSelectionEditable(outCmdEnabled);

  *outCmdEnabled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSetDocumentStateCommand::DoCommand(const char *aCommandName,
                                     nsISupports *refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSetDocumentStateCommand::DoCommandParams(const char *aCommandName,
                                           nsICommandParams *aParams,
                                           nsISupports *refCon)
{
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_INVALID_ARG);

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentModified"))
  {
    NS_ENSURE_ARG_POINTER(aParams);

    PRBool modified; 
    nsresult rv = aParams->GetBooleanValue(STATE_ATTRIBUTE, &modified);

    // Should we fail if this param wasn't set?
    // I'm not sure we should be that strict
    NS_ENSURE_SUCCESS(rv, rv);

    if (modified)
      return editor->IncrementModificationCount(1);

    return editor->ResetModificationCount();
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentReadOnly"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    PRBool isReadOnly; 
    nsresult rvRO = aParams->GetBooleanValue(STATE_ATTRIBUTE, &isReadOnly);
    NS_ENSURE_SUCCESS(rvRO, rvRO);

    PRUint32 flags;
    editor->GetFlags(&flags);
    if (isReadOnly)
      flags |= nsIPlaintextEditor::eEditorReadonlyMask;
    else
      flags &= ~(nsIPlaintextEditor::eEditorReadonlyMask);

    return editor->SetFlags(flags);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentUseCSS"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    nsCOMPtr<nsIHTMLEditor> htmleditor = do_QueryInterface(refCon);
    NS_ENSURE_TRUE(htmleditor, NS_ERROR_INVALID_ARG);

    PRBool desireCSS;
    nsresult rvCSS = aParams->GetBooleanValue(STATE_ATTRIBUTE, &desireCSS);
    NS_ENSURE_SUCCESS(rvCSS, rvCSS);

    return htmleditor->SetIsCSSEnabled(desireCSS);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_insertBrOnReturn"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    nsCOMPtr<nsIHTMLEditor> htmleditor = do_QueryInterface(refCon);
    NS_ENSURE_TRUE(htmleditor, NS_ERROR_INVALID_ARG);

    PRBool insertBrOnReturn;
    nsresult rvBR = aParams->GetBooleanValue(STATE_ATTRIBUTE,
                                              &insertBrOnReturn);
    NS_ENSURE_SUCCESS(rvBR, rvBR);

    return htmleditor->SetReturnInParagraphCreatesNewParagraph(!insertBrOnReturn);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableObjectResizing"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    nsCOMPtr<nsIHTMLObjectResizer> resizer = do_QueryInterface(refCon);
    NS_ENSURE_TRUE(resizer, NS_ERROR_INVALID_ARG);

    PRBool enabled;
    nsresult rvOR = aParams->GetBooleanValue(STATE_ATTRIBUTE, &enabled);
    NS_ENSURE_SUCCESS(rvOR, rvOR);

    return resizer->SetObjectResizingEnabled(enabled);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableInlineTableEditing"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    nsCOMPtr<nsIHTMLInlineTableEditor> editor = do_QueryInterface(refCon);
    NS_ENSURE_TRUE(editor, NS_ERROR_INVALID_ARG);

    PRBool enabled;
    nsresult rvOR = aParams->GetBooleanValue(STATE_ATTRIBUTE, &enabled);
    NS_ENSURE_SUCCESS(rvOR, rvOR);

    return editor->SetInlineTableEditingEnabled(enabled);
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSetDocumentStateCommand::GetCommandStateParams(const char *aCommandName,
                                                 nsICommandParams *aParams,
                                                 nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(refCon);

  // The base editor owns most state info
  nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
  NS_ENSURE_TRUE(editor, NS_ERROR_INVALID_ARG);

  // Always get the enabled state
  PRBool outCmdEnabled = PR_FALSE;
  IsCommandEnabled(aCommandName, refCon, &outCmdEnabled);
  nsresult rv = aParams->SetBooleanValue(STATE_ENABLED, outCmdEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentModified"))
  {
    PRBool modified;
    rv = editor->GetDocumentModified(&modified);
    NS_ENSURE_SUCCESS(rv, rv);

    return aParams->SetBooleanValue(STATE_ATTRIBUTE, modified);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentReadOnly"))
  {
    NS_ENSURE_ARG_POINTER(aParams);

    PRUint32 flags;
    editor->GetFlags(&flags);
    PRBool isReadOnly = flags & nsIPlaintextEditor::eEditorReadonlyMask;
    return aParams->SetBooleanValue(STATE_ATTRIBUTE, isReadOnly);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_setDocumentUseCSS"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    nsCOMPtr<nsIHTMLEditor> htmleditor = do_QueryInterface(refCon);
    NS_ENSURE_TRUE(htmleditor, NS_ERROR_INVALID_ARG);

    PRBool isCSS;
    htmleditor->GetIsCSSEnabled(&isCSS);
    return aParams->SetBooleanValue(STATE_ATTRIBUTE, isCSS);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_insertBrOnReturn"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    nsCOMPtr<nsIHTMLEditor> htmleditor = do_QueryInterface(refCon);
    NS_ENSURE_TRUE(htmleditor, NS_ERROR_INVALID_ARG);

    PRBool createPOnReturn;
    htmleditor->GetReturnInParagraphCreatesNewParagraph(&createPOnReturn);
    return aParams->SetBooleanValue(STATE_ATTRIBUTE, !createPOnReturn);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableObjectResizing"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    nsCOMPtr<nsIHTMLObjectResizer> resizer = do_QueryInterface(refCon);
    NS_ENSURE_TRUE(resizer, NS_ERROR_INVALID_ARG);

    PRBool enabled;
    resizer->GetObjectResizingEnabled(&enabled);
    return aParams->SetBooleanValue(STATE_ATTRIBUTE, enabled);
  }

  if (!nsCRT::strcmp(aCommandName, "cmd_enableInlineTableEditing"))
  {
    NS_ENSURE_ARG_POINTER(aParams);
    nsCOMPtr<nsIHTMLInlineTableEditor> editor = do_QueryInterface(refCon);
    NS_ENSURE_TRUE(editor, NS_ERROR_INVALID_ARG);

    PRBool enabled;
    editor->GetInlineTableEditingEnabled(&enabled);
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
 *  nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(mDocShell);
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
nsDocumentStateCommand::IsCommandEnabled(const char* aCommandName,
                                         nsISupports *refCon,
                                         PRBool *outCmdEnabled)
{
  NS_ENSURE_ARG_POINTER(outCmdEnabled);
  // Always return false to discourage callers from using DoCommand()
  *outCmdEnabled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentStateCommand::DoCommand(const char *aCommandName,
                                  nsISupports *refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocumentStateCommand::DoCommandParams(const char *aCommandName,
                                        nsICommandParams *aParams,
                                        nsISupports *refCon)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocumentStateCommand::GetCommandStateParams(const char *aCommandName,
                                              nsICommandParams *aParams,
                                              nsISupports *refCon)
{
  NS_ENSURE_ARG_POINTER(aParams);
  NS_ENSURE_ARG_POINTER(aCommandName);
  nsresult rv;

  if (!nsCRT::strcmp(aCommandName, "obs_documentCreated"))
  {
    PRUint32 editorStatus = nsIEditingSession::eEditorErrorUnknown;

    nsCOMPtr<nsIEditingSession> editingSession = do_QueryInterface(refCon);
    if (editingSession)
    {
      // refCon is initially set to nsIEditingSession until editor
      //  is successfully created and source doc is loaded
      // Embedder gets error status if this fails
      // If called before startup is finished, 
      //    status = eEditorCreationInProgress
      rv = editingSession->GetEditorStatus(&editorStatus);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
      // If refCon is an editor, then everything started up OK!
      nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
      if (editor)
        editorStatus = nsIEditingSession::eEditorOK;
    }

    // Note that if refCon is not-null, but is neither
    // an nsIEditingSession or nsIEditor, we return "eEditorErrorUnknown"
    aParams->SetLongValue(STATE_DATA, editorStatus);
    return NS_OK;
  }  
  else if (!nsCRT::strcmp(aCommandName, "obs_documentLocationChanged"))
  {
    nsCOMPtr<nsIEditor> editor = do_QueryInterface(refCon);
    if (editor)
    {
      nsCOMPtr<nsIDOMDocument> domDoc;
      editor->GetDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

      nsIURI *uri = doc->GetDocumentURI();
      NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

      return aParams->SetISupportsValue(STATE_DATA, (nsISupports*)uri);
    }
    return NS_OK;
  }  

  return NS_ERROR_NOT_IMPLEMENTED;
}
