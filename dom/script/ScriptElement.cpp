/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptElement.h"
#include "ScriptLoader.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsPresContext.h"
#include "nsIParser.h"
#include "nsGkAtoms.h"
#include "nsContentSink.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMETHODIMP
ScriptElement::ScriptAvailable(nsresult aResult, nsIScriptElement* aElement,
                               bool aIsInlineClassicScript, nsIURI* aURI,
                               uint32_t aLineNo) {
  if (!aIsInlineClassicScript && NS_FAILED(aResult)) {
    nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
    if (parser) {
      nsCOMPtr<nsIContentSink> sink = parser->GetContentSink();
      if (sink) {
        nsCOMPtr<Document> parserDoc = do_QueryInterface(sink->GetTarget());
        if (GetAsContent()->OwnerDoc() != parserDoc) {
          // Suppress errors when we've moved between docs.
          // /html/semantics/scripting-1/the-script-element/moving-between-documents/move-back-iframe-fetch-error-external-module.html
          // See also https://bugzilla.mozilla.org/show_bug.cgi?id=1849107
          return NS_OK;
        }
      }
    }

    if (parser) {
      parser->IncrementScriptNestingLevel();
    }
    nsresult rv = FireErrorEvent();
    if (parser) {
      parser->DecrementScriptNestingLevel();
    }
    return rv;
  }
  return NS_OK;
}

/* virtual */
nsresult ScriptElement::FireErrorEvent() {
  nsIContent* cont = GetAsContent();

  return nsContentUtils::DispatchTrustedEvent(
      cont->OwnerDoc(), cont, u"error"_ns, CanBubble::eNo, Cancelable::eNo);
}

NS_IMETHODIMP
ScriptElement::ScriptEvaluated(nsresult aResult, nsIScriptElement* aElement,
                               bool aIsInline) {
  nsresult rv = NS_OK;
  if (!aIsInline) {
    nsCOMPtr<nsIContent> cont = GetAsContent();

    RefPtr<nsPresContext> presContext =
        nsContentUtils::GetContextForContent(cont);

    nsEventStatus status = nsEventStatus_eIgnore;
    EventMessage message = NS_SUCCEEDED(aResult) ? eLoad : eLoadError;
    WidgetEvent event(true, message);
    // Load event doesn't bubble.
    event.mFlags.mBubbles = (message != eLoad);

    EventDispatcher::Dispatch(cont, presContext, &event, nullptr, &status);
  }

  return rv;
}

void ScriptElement::CharacterDataChanged(nsIContent* aContent,
                                         const CharacterDataChangeInfo&) {
  MaybeProcessScript();
}

void ScriptElement::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                     nsAtom* aAttribute, int32_t aModType,
                                     const nsAttrValue* aOldValue) {
  // https://html.spec.whatwg.org/#script-processing-model
  // When a script element el that is not parser-inserted experiences one of the
  // events listed in the following list, the user agent must immediately
  // prepare the script element el:
  //  - The script element is connected and has a src attribute set where
  //  previously the element had no such attribute.
  if (aElement->IsSVGElement() && ((aNameSpaceID != kNameSpaceID_XLink &&
                                    aNameSpaceID != kNameSpaceID_None) ||
                                   aAttribute != nsGkAtoms::href)) {
    return;
  }
  if (aElement->IsHTMLElement() &&
      (aNameSpaceID != kNameSpaceID_None || aAttribute != nsGkAtoms::src)) {
    return;
  }
  if (mParserCreated == NOT_FROM_PARSER &&
      aModType == MutationEvent_Binding::ADDITION) {
    auto* cont = GetAsContent();
    if (cont->IsInComposedDoc()) {
      MaybeProcessScript();
    }
  }
}

void ScriptElement::ContentAppended(nsIContent* aFirstNewContent) {
  MaybeProcessScript();
}

void ScriptElement::ContentInserted(nsIContent* aChild) {
  MaybeProcessScript();
}

bool ScriptElement::MaybeProcessScript() {
  nsIContent* cont = GetAsContent();

  NS_ASSERTION(cont->DebugGetSlots()->mMutationObservers.contains(this),
               "You forgot to add self as observer");

  // https://html.spec.whatwg.org/#parsing-main-incdata
  // An end tag whose tag name is "script"
  //  - If the active speculative HTML parser is null and the JavaScript
  // execution context stack is empty, then perform a microtask checkpoint.
  nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
      "ScriptElement::MaybeProcessScript", []() { nsAutoMicroTask mt; }));

  if (mAlreadyStarted || !mDoneAddingChildren || !cont->GetComposedDoc() ||
      mMalformed) {
    return false;
  }

  if (!HasScriptContent()) {
    // In the case of an empty, non-external classic script, there is nothing
    // to process. However, we must perform a microtask checkpoint afterwards,
    // as per https://html.spec.whatwg.org/#clean-up-after-running-script
    if (mKind == JS::loader::ScriptKind::eClassic && !mExternal) {
      nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
          "ScriptElement::MaybeProcessScript", []() { nsAutoMicroTask mt; }));
    }
    return false;
  }

  // Check the type attribute to determine language and version. If type exists,
  // it trumps the deprecated 'language='.
  nsAutoString type;
  bool hasType = GetScriptType(type);
  if (!type.IsEmpty()) {
    NS_ENSURE_TRUE(nsContentUtils::IsJavascriptMIMEType(type) ||
                       type.LowerCaseEqualsASCII("module") ||
                       type.LowerCaseEqualsASCII("importmap"),
                   false);
  } else if (!hasType) {
    // "language" is a deprecated attribute of HTML, so we check it only for
    // HTML script elements.
    if (cont->IsHTMLElement()) {
      nsAutoString language;
      cont->AsElement()->GetAttr(nsGkAtoms::language, language);
      if (!language.IsEmpty() &&
          !nsContentUtils::IsJavaScriptLanguage(language)) {
        return false;
      }
    }
  }

  Document* ownerDoc = cont->OwnerDoc();
  FreezeExecutionAttrs(ownerDoc);

  mAlreadyStarted = true;

  nsCOMPtr<nsIParser> parser = ((nsIScriptElement*)this)->GetCreatorParser();
  if (parser) {
    nsCOMPtr<nsIContentSink> sink = parser->GetContentSink();
    if (sink) {
      nsCOMPtr<Document> parserDoc = do_QueryInterface(sink->GetTarget());
      if (ownerDoc != parserDoc) {
        // Refactor this: https://bugzilla.mozilla.org/show_bug.cgi?id=1849107
        return false;
      }
    }
  }

  RefPtr<ScriptLoader> loader = ownerDoc->ScriptLoader();
  return loader->ProcessScriptElement(this, type);
}

bool ScriptElement::GetScriptType(nsAString& aType) {
  Element* element = GetAsContent()->AsElement();

  nsAutoString type;
  if (!element->GetAttr(nsGkAtoms::type, type)) {
    return false;
  }

  // ASCII whitespace https://infra.spec.whatwg.org/#ascii-whitespace:
  // U+0009 TAB, U+000A LF, U+000C FF, U+000D CR, or U+0020 SPACE.
  static const char kASCIIWhitespace[] = "\t\n\f\r ";

  const bool wasEmptyBeforeTrim = type.IsEmpty();
  type.Trim(kASCIIWhitespace);

  // If the value before trim was not empty and the value is now empty, do not
  // trim as we want to retain pure whitespace (by restoring original value)
  // because we need to treat "" and " " (etc) differently.
  if (!wasEmptyBeforeTrim && type.IsEmpty()) {
    return element->GetAttr(nsGkAtoms::type, aType);
  }

  aType.Assign(type);
  return true;
}
