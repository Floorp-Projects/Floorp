/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CSSMozDocumentRule.h"
#include "mozilla/dom/CSSMozDocumentRuleBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

using namespace mozilla::css;

/* virtual */ JSObject*
CSSMozDocumentRule::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto)
{
  return CSSMozDocumentRule_Binding::Wrap(aCx, this, aGivenProto);
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

CSSMozDocumentRule::CSSMozDocumentRule(RefPtr<RawServoMozDocumentRule> aRawRule,
                                       StyleSheet* aSheet,
                                       css::Rule* aParentRule,
                                       uint32_t aLine,
                                       uint32_t aColumn)
  : css::ConditionRule(Servo_MozDocumentRule_GetRules(aRawRule).Consume(),
                       aSheet, aParentRule, aLine, aColumn)
  , mRawRule(std::move(aRawRule))
{
}

NS_IMPL_ADDREF_INHERITED(CSSMozDocumentRule, css::ConditionRule)
NS_IMPL_RELEASE_INHERITED(CSSMozDocumentRule, css::ConditionRule)

// QueryInterface implementation for MozDocumentRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CSSMozDocumentRule)
NS_INTERFACE_MAP_END_INHERITING(css::ConditionRule)

#ifdef DEBUG
/* virtual */ void
CSSMozDocumentRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_MozDocumentRule_Debug(mRawRule, &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

void
CSSMozDocumentRule::GetConditionText(nsAString& aConditionText)
{
  Servo_MozDocumentRule_GetConditionText(mRawRule, &aConditionText);
}

void
CSSMozDocumentRule::SetConditionText(const nsAString& aConditionText,
                                     ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

/* virtual */ void
CSSMozDocumentRule::GetCssText(nsAString& aCssText) const
{
  Servo_MozDocumentRule_GetCssText(mRawRule, &aCssText);
}

/* virtual */ size_t
CSSMozDocumentRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  // TODO Implement this!
  return aMallocSizeOf(this);
}

} // namespace dom
} // namespace mozilla
