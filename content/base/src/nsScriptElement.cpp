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
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc> (original developer)
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

#include "nsScriptElement.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsPresContext.h"
#include "nsScriptLoader.h"
#include "nsGkAtoms.h"
#include "nsIParser.h"
#include "nsAutoPtr.h"
#include "nsGkAtoms.h"

static nsPresContext*
GetContextForContent(nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetCurrentDoc();
  if (doc) {
    nsIPresShell *presShell = doc->GetShellAt(0);
    if (presShell) {
      return presShell->GetPresContext();
    }
  }
  
  return nsnull;
}

NS_IMETHODIMP
nsScriptElement::ScriptAvailable(nsresult aResult,
                                 nsIScriptElement *aElement,
                                 PRBool aIsInline,
                                 nsIURI *aURI,
                                 PRInt32 aLineNo)
{
  if (!aIsInline && NS_FAILED(aResult)) {
    nsCOMPtr<nsIContent> cont =
      do_QueryInterface((nsIScriptElement*) this);

    nsCOMPtr<nsPresContext> presContext = GetContextForContent(cont);

    nsEventStatus status = nsEventStatus_eIgnore;
    nsScriptErrorEvent event(PR_TRUE, NS_LOAD_ERROR);

    event.lineNr = aLineNo;

    NS_NAMED_LITERAL_STRING(errorString, "Error loading script");
    event.errorMsg = errorString.get();

    nsCAutoString spec;
    aURI->GetSpec(spec);

    NS_ConvertUTF8toUTF16 fileName(spec);
    event.fileName = fileName.get();

    nsEventDispatcher::Dispatch(cont, presContext, &event, nsnull, &status);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsScriptElement::ScriptEvaluated(nsresult aResult,
                                 nsIScriptElement *aElement,
                                 PRBool aIsInline)
{
  nsresult rv = NS_OK;
  if (!aIsInline) {
    nsCOMPtr<nsIContent> cont =
      do_QueryInterface((nsIScriptElement*) this);

    nsCOMPtr<nsPresContext> presContext = GetContextForContent(cont);

    nsEventStatus status = nsEventStatus_eIgnore;
    PRUint32 type = NS_SUCCEEDED(aResult) ? NS_LOAD : NS_LOAD_ERROR;
    nsEvent event(PR_TRUE, type);
    if (type == NS_LOAD) {
      // Load event doesn't bubble.
      event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;
    }

    nsEventDispatcher::Dispatch(cont, presContext, &event, nsnull, &status);
  }

  return rv;
}

void
nsScriptElement::CharacterDataChanged(nsIDocument *aDocument,
                                      nsIContent* aContent,
                                      CharacterDataChangeInfo* aInfo)
{
  MaybeProcessScript();
}

void
nsScriptElement::AttributeChanged(nsIDocument* aDocument,
                                  nsIContent* aContent,
                                  PRInt32 aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  PRInt32 aModType)
{
  MaybeProcessScript();
}

void
nsScriptElement::ContentAppended(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 PRInt32 aNewIndexInContainer)
{
  MaybeProcessScript();
}

void
nsScriptElement::ContentInserted(nsIDocument *aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aChild,
                                 PRInt32 aIndexInContainer)
{
  MaybeProcessScript();
}

static PRBool
InNonScriptingContainer(nsINode* aNode)
{
  aNode = aNode->GetNodeParent();
  while (aNode) {
    // XXX noframes and noembed are currently unconditionally not
    // displayed and processed. This might change if we support either
    // prefs or per-document container settings for not allowing
    // frames or plugins.
    if (aNode->IsNodeOfType(nsINode::eHTML)) {
      nsIAtom *localName = NS_STATIC_CAST(nsIContent*, aNode)->Tag();
      if (localName == nsGkAtoms::iframe ||
          localName == nsGkAtoms::noframes ||
          localName == nsGkAtoms::noembed) {
        return PR_TRUE;
      }
    }
    aNode = aNode->GetNodeParent();
  }

  return PR_FALSE;
}

nsresult
nsScriptElement::MaybeProcessScript()
{
  nsCOMPtr<nsIContent> cont =
    do_QueryInterface((nsIScriptElement*) this);

  NS_ASSERTION(cont->DebugGetSlots()->mMutationObservers.Contains(this),
               "You forgot to add self as observer");

  if (mIsEvaluated || !mDoneAddingChildren || !cont->IsInDoc() ||
      mMalformed || InNonScriptingContainer(cont) ||
      !HasScriptContent()) {
    return NS_OK;
  }

  nsresult scriptresult = NS_OK;
  nsRefPtr<nsScriptLoader> loader = cont->GetOwnerDoc()->GetScriptLoader();
  if (loader) {
    mIsEvaluated = PR_TRUE;
    scriptresult = loader->ProcessScriptElement(this);

    // The only error we don't ignore is NS_ERROR_HTMLPARSER_BLOCK
    // However we don't want to override other success values
    // (such as NS_CONTENT_SCRIPT_IS_EVENTHANDLER)
    if (NS_FAILED(scriptresult) &&
        scriptresult != NS_ERROR_HTMLPARSER_BLOCK) {
      scriptresult = NS_OK;
    }
  }

  return scriptresult;
}
