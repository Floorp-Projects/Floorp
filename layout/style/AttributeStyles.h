/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing data from presentational
 * HTML attributes
 */

#ifndef mozilla_AttributeStyles_h_
#define mozilla_AttributeStyles_h_

#include "nsColor.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "PLDHashTable.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "nsTHashMap.h"
#include "nsString.h"

struct MiscContainer;
namespace mozilla {
struct StyleLockedDeclarationBlock;
namespace dom {
class Document;
}  // namespace dom

class AttributeStyles final {
 public:
  explicit AttributeStyles(dom::Document* aDocument);
  void SetOwningDocument(dom::Document* aDocument);

  NS_INLINE_DECL_REFCOUNTING(AttributeStyles)

  size_t DOMSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  void Reset();
  nsresult SetLinkColor(nscolor aColor);
  nsresult SetActiveLinkColor(nscolor aColor);
  nsresult SetVisitedLinkColor(nscolor aColor);

  const StyleLockedDeclarationBlock* GetServoUnvisitedLinkDecl() const {
    return mServoUnvisitedLinkDecl;
  }
  const StyleLockedDeclarationBlock* GetServoVisitedLinkDecl() const {
    return mServoVisitedLinkDecl;
  }
  const StyleLockedDeclarationBlock* GetServoActiveLinkDecl() const {
    return mServoActiveLinkDecl;
  }

  void CacheStyleAttr(const nsAString& aSerialized, MiscContainer* aValue) {
    mCachedStyleAttrs.InsertOrUpdate(aSerialized, aValue);
  }
  void EvictStyleAttr(const nsAString& aSerialized, MiscContainer* aValue) {
    NS_ASSERTION(LookupStyleAttr(aSerialized) == aValue,
                 "Cached value doesn't match?");
    mCachedStyleAttrs.Remove(aSerialized);
  }
  MiscContainer* LookupStyleAttr(const nsAString& aSerialized) {
    return mCachedStyleAttrs.Get(aSerialized);
  }

  AttributeStyles(const AttributeStyles& aCopy) = delete;
  AttributeStyles& operator=(const AttributeStyles& aCopy) = delete;

 private:
  ~AttributeStyles();

  // Implementation of SetLink/VisitedLink/ActiveLinkColor
  nsresult ImplLinkColorSetter(RefPtr<StyleLockedDeclarationBlock>& aDecl,
                               nscolor aColor);

  dom::Document* mDocument;
  RefPtr<StyleLockedDeclarationBlock> mServoUnvisitedLinkDecl;
  RefPtr<StyleLockedDeclarationBlock> mServoVisitedLinkDecl;
  RefPtr<StyleLockedDeclarationBlock> mServoActiveLinkDecl;
  nsTHashMap<nsStringHashKey, MiscContainer*> mCachedStyleAttrs;
};

}  // namespace mozilla

#endif
