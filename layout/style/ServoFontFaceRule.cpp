/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoFontFaceRule.h"

#include "mozilla/dom/CSSFontFaceRuleBinding.h"
#include "mozilla/dom/CSSStyleDeclarationBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------------------
// ServoFontFaceRuleDecl and related routines
//

// QueryInterface implementation for ServoFontFaceRuleDecl
NS_INTERFACE_MAP_BEGIN(ServoFontFaceRuleDecl)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICSSDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  // We forward the cycle collection interfaces to ContainingRule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return ContainingRule()->QueryInterface(aIID, aInstancePtr);
  }
  else
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF_USING_AGGREGATOR(ServoFontFaceRuleDecl, ContainingRule())
NS_IMPL_RELEASE_USING_AGGREGATOR(ServoFontFaceRuleDecl, ContainingRule())

// helper for string GetPropertyValue and RemovePropertyValue
void
ServoFontFaceRuleDecl::GetPropertyValue(nsCSSFontDesc aFontDescID,
                                        nsAString& aResult) const
{
  MOZ_ASSERT(aResult.IsEmpty());
  Servo_FontFaceRule_GetDescriptorCssText(mRawRule, aFontDescID, &aResult);
}

void
ServoFontFaceRuleDecl::GetCssText(nsAString& aCssText)
{
  aCssText.Truncate();
  Servo_FontFaceRule_GetDeclCssText(mRawRule, &aCssText);
}

void
ServoFontFaceRuleDecl::SetCssText(const nsAString& aCssText,
                                  nsIPrincipal* aSubjectPrincipal,
                                  ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED); // bug 443978
}

NS_IMETHODIMP
ServoFontFaceRuleDecl::GetPropertyValue(const nsAString& aPropName,
                                        nsAString& aResult)
{
  aResult.Truncate();
  GetPropertyValue(nsCSSProps::LookupFontDesc(aPropName), aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServoFontFaceRuleDecl::RemoveProperty(const nsAString& aPropName,
                                      nsAString& aResult)
{
  nsCSSFontDesc descID = nsCSSProps::LookupFontDesc(aPropName);
  NS_ASSERTION(descID >= eCSSFontDesc_UNKNOWN &&
               descID < eCSSFontDesc_COUNT,
               "LookupFontDesc returned value out of range");

  aResult.Truncate();
  if (descID != eCSSFontDesc_UNKNOWN) {
    GetPropertyValue(descID, aResult);
    Servo_FontFaceRule_ResetDescriptor(mRawRule, descID);
  }
  return NS_OK;
}

void
ServoFontFaceRuleDecl::GetPropertyPriority(const nsAString& aPropName,
                                           nsAString& aResult)
{
  // font descriptors do not have priorities at present
  aResult.Truncate();
}

NS_IMETHODIMP
ServoFontFaceRuleDecl::SetProperty(const nsAString& aPropName,
                                   const nsAString& aValue,
                                   const nsAString& aPriority,
                                   nsIPrincipal* aSubjectPrincipal)
{
  // FIXME(heycam): If we are changing unicode-range, then a FontFace object
  // representing this rule must have its mUnicodeRange value invalidated.

  return NS_ERROR_NOT_IMPLEMENTED; // bug 443978
}

uint32_t
ServoFontFaceRuleDecl::Length()
{
  return Servo_FontFaceRule_Length(mRawRule);
}

void
ServoFontFaceRuleDecl::IndexedGetter(uint32_t aIndex, bool& aFound,
                                     nsAString& aResult)
{
  nsCSSFontDesc id = Servo_FontFaceRule_IndexGetter(mRawRule, aIndex);
  if (id != eCSSFontDesc_UNKNOWN) {
    aFound = true;
    aResult.AssignASCII(nsCSSProps::GetStringValue(id).get());
  } else {
    aFound = false;
  }
}

css::Rule*
ServoFontFaceRuleDecl::GetParentRule()
{
  return ContainingRule();
}

nsINode*
ServoFontFaceRuleDecl::GetParentObject()
{
  return ContainingRule()->GetDocument();
}

JSObject*
ServoFontFaceRuleDecl::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return CSSStyleDeclarationBinding::Wrap(cx, this, aGivenProto);
}

// -------------------------------------------
// ServoFontFaceRule
//

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoFontFaceRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ServoFontFaceRule,
                                               mozilla::css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecl.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ServoFontFaceRule,
                                                mozilla::css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Unlink the wrapper for our declaraton.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecl.ReleaseWrapper(static_cast<nsISupports*>(p));
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoFontFaceRule,
                                                  mozilla::css::Rule)
  // Keep this in sync with IsCCLeaf.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
ServoFontFaceRule::IsCCLeaf() const
{
  if (!Rule::IsCCLeaf()) {
    return false;
  }

  return !mDecl.PreservingWrapper();
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ServoFontFaceRule, mozilla::css::Rule)

#ifdef DEBUG
void
ServoFontFaceRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t i = 0; i < aIndent; i++) {
    str.AppendLiteral("  ");
  }
  Servo_FontFaceRule_Debug(Raw(), &str);
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

uint16_t
ServoFontFaceRule::Type() const
{
  return CSSRuleBinding::FONT_FACE_RULE;
}

void
ServoFontFaceRule::GetCssText(nsAString& aCssText) const
{
  aCssText.Truncate();
  Servo_FontFaceRule_GetCssText(Raw(), &aCssText);
}

nsICSSDeclaration*
ServoFontFaceRule::Style()
{
  return &mDecl;
}

/* virtual */ size_t
ServoFontFaceRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

/* virtual */ JSObject*
ServoFontFaceRule::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto)
{
  return CSSFontFaceRuleBinding::Wrap(aCx, this, aGivenProto);
}
