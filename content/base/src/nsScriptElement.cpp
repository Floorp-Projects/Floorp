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
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
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
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsPresContext.h"
#include "nsScriptLoader.h"
#include "nsIParser.h"
#include "nsAutoPtr.h"
#include "nsGkAtoms.h"
#include "nsContentSink.h"

using namespace mozilla::dom;

NS_IMETHODIMP
nsScriptElement::ScriptAvailable(nsresult aResult,
                                 nsIScriptElement *aElement,
                                 bool aIsInline,
                                 nsIURI *aURI,
                                 PRInt32 aLineNo)
{
  if (!aIsInline && NS_FAILED(aResult)) {
    nsCOMPtr<nsIContent> cont =
      do_QueryInterface((nsIScriptElement*) this);

    nsRefPtr<nsPresContext> presContext =
      nsContentUtils::GetContextForContent(cont);

    nsEventStatus status = nsEventStatus_eIgnore;
    nsScriptErrorEvent event(true, NS_LOAD_ERROR);

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
                                 bool aIsInline)
{
  nsresult rv = NS_OK;
  if (!aIsInline) {
    nsCOMPtr<nsIContent> cont =
      do_QueryInterface((nsIScriptElement*) this);

    nsRefPtr<nsPresContext> presContext =
      nsContentUtils::GetContextForContent(cont);

    nsEventStatus status = nsEventStatus_eIgnore;
    PRUint32 type = NS_SUCCEEDED(aResult) ? NS_LOAD : NS_LOAD_ERROR;
    nsEvent event(true, type);
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
                                  Element* aElement,
                                  PRInt32 aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  PRInt32 aModType)
{
  MaybeProcessScript();
}

void
nsScriptElement::ContentAppended(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aFirstNewContent,
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

bool
nsScriptElement::MaybeProcessScript()
{
  nsCOMPtr<nsIContent> cont =
    do_QueryInterface((nsIScriptElement*) this);

  NS_ASSERTION(cont->DebugGetSlots()->mMutationObservers.Contains(this),
               "You forgot to add self as observer");

  if (mAlreadyStarted || !mDoneAddingChildren || !cont->IsInDoc() ||
      mMalformed || !HasScriptContent()) {
    return false;
  }

  FreezeUriAsyncDefer();

  mAlreadyStarted = true;

  nsIDocument* ownerDoc = cont->OwnerDoc();
  nsCOMPtr<nsIParser> parser = ((nsIScriptElement*) this)->GetCreatorParser();
  if (parser) {
    nsCOMPtr<nsIContentSink> sink = parser->GetContentSink();
    if (sink) {
      nsCOMPtr<nsIDocument> parserDoc = do_QueryInterface(sink->GetTarget());
      if (ownerDoc != parserDoc) {
        // Willful violation of HTML5 as of 2010-12-01
        return false;
      }
    }
  }

  nsRefPtr<nsScriptLoader> loader = ownerDoc->ScriptLoader();
  return loader->ProcessScriptElement(this);
}
