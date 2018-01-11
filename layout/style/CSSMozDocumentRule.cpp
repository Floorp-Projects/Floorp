/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSMozDocumentRule.h"
#include "mozilla/dom/CSSMozDocumentRuleBinding.h"

namespace mozilla {
namespace dom {

using namespace mozilla::css;

/* virtual */ JSObject*
CSSMozDocumentRule::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto)
{
  return CSSMozDocumentRuleBinding::Wrap(aCx, this, aGivenProto);
}

bool
CSSMozDocumentRule::Match(nsIDocument* aDoc,
                          nsIURI* aDocURI,
                          const nsACString& aDocURISpec,
                          const nsACString& aPattern,
                          URLMatchingFunction aUrlMatchingFunction)
{
  switch (aUrlMatchingFunction) {
    case URLMatchingFunction::eURL: {
      if (aDocURISpec == aPattern) {
        return true;
      }
    } break;
    case URLMatchingFunction::eURLPrefix: {
      if (StringBeginsWith(aDocURISpec, aPattern)) {
        return true;
      }
    } break;
    case URLMatchingFunction::eDomain: {
      nsAutoCString host;
      if (aDocURI) {
        aDocURI->GetHost(host);
      }
      int32_t lenDiff = host.Length() - aPattern.Length();
      if (lenDiff == 0) {
        if (host == aPattern) {
          return true;
        }
      } else {
        if (StringEndsWith(host, aPattern) &&
            host.CharAt(lenDiff - 1) == '.') {
          return true;
        }
      }
    } break;
    case URLMatchingFunction::eRegExp: {
      NS_ConvertUTF8toUTF16 spec(aDocURISpec);
      NS_ConvertUTF8toUTF16 regex(aPattern);
      if (nsContentUtils::IsPatternMatching(spec, regex, aDoc)) {
        return true;
      }
    } break;
  }

  return false;
}

} // namespace dom
} // namespace mozilla
