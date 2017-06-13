/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A unique per-element set of attributes that is used as an
 * nsIStyleRule; used to implement presentational attributes.
 */

#ifndef nsMappedAttributes_h___
#define nsMappedAttributes_h___

#include "nsAttrAndChildArray.h"
#include "nsMappedAttributeElement.h"
#include "nsIStyleRule.h"
#include "mozilla/Attributes.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/MemoryReporting.h"

class nsIAtom;
class nsHTMLStyleSheet;

class nsMappedAttributes final : public nsIStyleRule
{
public:
  nsMappedAttributes(nsHTMLStyleSheet* aSheet,
                     nsMapRuleToAttributesFunc aMapRuleFunc);

  // Do not return null.
  void* operator new(size_t size, uint32_t aAttrCount = 1) CPP_THROW_NEW;
  nsMappedAttributes* Clone(bool aWillAddAttr);

  NS_DECL_ISUPPORTS

  void SetAndSwapAttr(nsIAtom* aAttrName, nsAttrValue& aValue,
                      bool* aValueWasSet);
  const nsAttrValue* GetAttr(nsIAtom* aAttrName) const;
  const nsAttrValue* GetAttr(const nsAString& aAttrName) const;

  uint32_t Count() const
  {
    return mAttrCount;
  }

  bool Equals(const nsMappedAttributes* aAttributes) const;
  uint32_t HashValue() const;

  void DropStyleSheetReference()
  {
    mSheet = nullptr;
  }
  void SetStyleSheet(nsHTMLStyleSheet* aSheet);
  nsHTMLStyleSheet* GetStyleSheet()
  {
    return mSheet;
  }

  const nsAttrName* NameAt(uint32_t aPos) const
  {
    NS_ASSERTION(aPos < mAttrCount, "out-of-bounds");
    return &Attrs()[aPos].mName;
  }
  const nsAttrValue* AttrAt(uint32_t aPos) const
  {
    NS_ASSERTION(aPos < mAttrCount, "out-of-bounds");
    return &Attrs()[aPos].mValue;
  }
  // Remove the attr at position aPos.  The value of the attr is placed in
  // aValue; any value that was already in aValue is destroyed.
  void RemoveAttrAt(uint32_t aPos, nsAttrValue& aValue);
  const nsAttrName* GetExistingAttrNameFromQName(const nsAString& aName) const;
  int32_t IndexOfAttr(nsIAtom* aLocalName) const;

  // Apply the contained mapper to the contained set of servo rules,
  // unless the servo rules have already been initialized.
  void LazilyResolveServoDeclaration(nsPresContext* aPresContext);

  // Obtain the contained servo declaration block
  // May return null if called before the inner block
  // has been (lazily) resolved
  const RefPtr<RawServoDeclarationBlock>& GetServoStyle() const
  {
    return mServoStyle;
  }

  void ClearServoStyle() {
    MOZ_ASSERT(NS_IsMainThread());
    mServoStyle = nullptr;
  }

  // nsIStyleRule
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
  virtual bool MightMapInheritedStyleData() override;
  virtual bool GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                             nsCSSValue* aValue) override;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  static void Shutdown();
private:

  void LastRelease();

  nsMappedAttributes(const nsMappedAttributes& aCopy);
  ~nsMappedAttributes();

  struct InternalAttr
  {
    nsAttrName mName;
    nsAttrValue mValue;
  };

  /**
   * Due to a compiler bug in VisualAge C++ for AIX, we need to return the
   * address of the first index into mAttrs here, instead of simply
   * returning mAttrs itself.
   *
   * See Bug 231104 for more information.
   */
  const InternalAttr* Attrs() const
  {
    return reinterpret_cast<const InternalAttr*>(&(mAttrs[0]));
  }
  InternalAttr* Attrs()
  {
    return reinterpret_cast<InternalAttr*>(&(mAttrs[0]));
  }

  uint16_t mAttrCount;
#ifdef DEBUG
  uint16_t mBufferSize;
#endif
  nsHTMLStyleSheet* mSheet; //weak
  nsMapRuleToAttributesFunc mRuleMapper;
  RefPtr<RawServoDeclarationBlock> mServoStyle;
  void* mAttrs[1];

  static bool sShuttingDown;

  // We're caching some memory to avoid trashing the allocator.
  // The memory stored at index N can hold N attribute values.
  static nsTArray<void*>* sCachedMappedAttributeAllocations;
};

#endif /* nsMappedAttributes_h___ */
