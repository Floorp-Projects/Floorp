
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIScriptElement.h"

#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "nsIParser.h"
#include "nsIWeakReference.h"

void nsIScriptElement::SetCreatorParser(nsIParser* aParser) {
  mCreatorParser = do_GetWeakReference(aParser);
}

void nsIScriptElement::UnblockParser() {
  nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
  if (parser) {
    parser->UnblockParser();
  }
}

void nsIScriptElement::ContinueParserAsync() {
  nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
  if (parser) {
    parser->ContinueInterruptedParsingAsync();
  }
}

void nsIScriptElement::BeginEvaluating() {
  nsCOMPtr<nsIParser> parser = do_QueryReferent(mCreatorParser);
  if (parser) {
    parser->IncrementScriptNestingLevel();
  }
}

void nsIScriptElement::EndEvaluating() {
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
