/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BindingStyleRule_h__
#define mozilla_BindingStyleRule_h__

#include "nscore.h"
#include "nsString.h"
#include "mozilla/css/Rule.h"
#include "mozilla/NotNull.h"

/**
 * Shared superclass for mozilla::css::StyleRule and mozilla::ServoStyleRule,
 * for use from bindings code.
 */

class nsICSSDeclaration;

namespace mozilla {
class DeclarationBlock;
namespace dom {
class Element;
}

class BindingStyleRule : public css::Rule
{
protected:
  BindingStyleRule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : css::Rule(aLineNumber, aColumnNumber)
  {
  }
  BindingStyleRule(const BindingStyleRule& aCopy)
    : css::Rule(aCopy)
  {
  }
  virtual ~BindingStyleRule() {}

public:
  // This is pure virtual because we have no members, and are an abstract class
  // to start with.  The fact that we have to have this declaration at all is
  // kinda dumb.  :(
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const override MOZ_MUST_OVERRIDE = 0;

  // Likewise for this one.  We have to override our superclass, but don't
  // really need to do anything in this method.
  virtual bool IsCCLeaf() const override MOZ_MUST_OVERRIDE = 0;

  virtual uint32_t GetSelectorCount() = 0;
  virtual nsresult GetSelectorText(uint32_t aSelectorIndex, nsAString& aText) = 0;
  virtual nsresult GetSpecificity(uint32_t aSelectorIndex,
                                  uint64_t* aSpecificity) = 0;
  virtual nsresult SelectorMatchesElement(dom::Element* aElement,
                                          uint32_t aSelectorIndex,
                                          const nsAString& aPseudo,
                                          bool* aMatches) = 0;
  virtual NotNull<DeclarationBlock*> GetDeclarationBlock() const = 0;

  // WebIDL API
  virtual void GetSelectorText(nsAString& aSelectorText) = 0;
  virtual void SetSelectorText(const nsAString& aSelectorText) = 0;
  virtual nsICSSDeclaration* Style() = 0;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace mozilla

#endif // mozilla_BindingStyleRule_h__
