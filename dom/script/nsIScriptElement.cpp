
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIScriptElement.h"

#include "js/loader/ScriptKind.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "nsIParser.h"
#include "nsIWeakReference.h"

using JS::loader::ScriptKind;

bool nsIScriptElement::IsClassicNonAsyncDefer() {
  return mKind == ScriptKind::eClassic && !mAsync && !mDefer;
}

void nsIScriptElement::SetCreatorParser(nsIParser* aParser) {
  mCreatorParser = do_GetWeakReference(aParser);
}

void nsIScriptElement::UnblockParser() {
  if (!IsClassicNonAsyncDefer()) {
    MOZ_ASSERT_UNREACHABLE(
        "Tried to unblock parser for a script type that cannot block "
        "the parser.");
    return;
  }
  nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
  if (parser) {
    parser->UnblockParser();
  }
}

void nsIScriptElement::ContinueParserAsync() {
  if (!IsClassicNonAsyncDefer()) {
    MOZ_ASSERT_UNREACHABLE(
        "Tried to continue after a script type that cannot block the parser.");
    return;
  }
  nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
  if (parser) {
    parser->ContinueInterruptedParsingAsync();
  }
}

void nsIScriptElement::BeginEvaluating() {
  if (!IsClassicNonAsyncDefer()) {
    return;
  }
  nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
  if (parser) {
    parser->IncrementScriptNestingLevel();
  }
}

void nsIScriptElement::EndEvaluating() {
  if (!IsClassicNonAsyncDefer()) {
    return;
  }
  nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
  if (parser) {
    parser->DecrementScriptNestingLevel();
  }
}

already_AddRefed<nsIParser> nsIScriptElement::GetCreatorParser() {
  nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
  return parser.forget();
}

mozilla::dom::ReferrerPolicy nsIScriptElement::GetReferrerPolicy() {
  return mozilla::dom::ReferrerPolicy::_empty;
}

void nsIScriptElement::DetermineKindFromType(
    const mozilla::dom::Document* aOwnerDoc) {
  MOZ_ASSERT((mKind != ScriptKind::eModule) &&
             (mKind != ScriptKind::eImportMap) && !mAsync && !mDefer &&
             !mExternal);

  nsAutoString type;
  GetScriptType(type);

  if (!type.IsEmpty()) {
    if (type.LowerCaseEqualsASCII("module")) {
      mKind = ScriptKind::eModule;
    }

    // https://html.spec.whatwg.org/multipage/scripting.html#prepare-the-script-element
    // Step 11. Otherwise, if the script block's type string is an ASCII
    // case-insensitive match for the string "importmap", then set el's type to
    // "importmap".
    if (aOwnerDoc->ImportMapsEnabled() &&
        type.LowerCaseEqualsASCII("importmap")) {
      mKind = ScriptKind::eImportMap;
    }
  }
}
