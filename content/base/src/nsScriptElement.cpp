/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    return nsContentUtils::DispatchTrustedEvent(cont->OwnerDoc(),
                                                cont,
                                                NS_LITERAL_STRING("error"),
                                                false /* bubbles */,
                                                false /* cancelable */);
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

    nsEventDispatcher::Dispatch(cont, presContext, &event, nullptr, &status);
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
