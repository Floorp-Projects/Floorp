/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLPrototypeResources_h__
#define nsXBLPrototypeResources_h__

#include "mozilla/MemoryReporting.h"
#include "mozilla/StyleSheet.h"
#include "nsICSSLoaderObserver.h"

class nsCSSRuleProcessor;
class nsAtom;
class nsIContent;
class nsXBLPrototypeBinding;
class nsXBLResourceLoader;
struct RawServoAuthorStyles;

namespace mozilla {
class CSSStyleSheet;
class ServoStyleSet;
class ServoStyleRuleMap;
} // namespace mozilla

// *********************************************************************/
// The XBLPrototypeResources class

class nsXBLPrototypeResources
{
public:
  explicit nsXBLPrototypeResources(nsXBLPrototypeBinding* aBinding);
  ~nsXBLPrototypeResources();

  bool LoadResources(nsIContent* aBoundElement);
  void AddResource(nsAtom* aResourceType, const nsAString& aSrc);
  void AddResourceListener(nsIContent* aElement);
  nsresult FlushSkinSheets();

  nsresult Write(nsIObjectOutputStream* aStream);

  void Traverse(nsCycleCollectionTraversalCallback &cb);
  void Unlink();

  void ClearLoader();

  void AppendStyleSheet(mozilla::StyleSheet* aSheet);
  void RemoveStyleSheet(mozilla::StyleSheet* aSheet);
  void InsertStyleSheetAt(size_t aIndex, mozilla::StyleSheet* aSheet);

  mozilla::StyleSheet* StyleSheetAt(size_t aIndex) const
  {
    return mStyleSheetList[aIndex];
  }

  size_t SheetCount() const
  {
    return mStyleSheetList.Length();
  }

  bool HasStyleSheets() const
  {
    return !mStyleSheetList.IsEmpty();
  }

  void AppendStyleSheetsTo(nsTArray<mozilla::StyleSheet*>& aResult) const;


  const RawServoAuthorStyles* GetServoStyles() const
  {
    return mServoStyles.get();
  }

  void SyncServoStyles();

  mozilla::ServoStyleRuleMap* GetServoStyleRuleMap();

  // Updates the ServoStyleSet object that holds the result of cascading the
  // sheets in mStyleSheetList. Equivalent to GatherRuleProcessor(), but for
  // the Servo style backend.
  void ComputeServoStyles(const mozilla::ServoStyleSet& aMasterStyleSet);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // A loader object. Exists only long enough to load resources, and then it dies.
  RefPtr<nsXBLResourceLoader> mLoader;

  // A list of loaded stylesheets for this binding.
  //
  // FIXME(emilio): Remove when the old style system is gone, defer to
  // mServoStyles.
  nsTArray<RefPtr<mozilla::StyleSheet>> mStyleSheetList;


  // The result of cascading the XBL style sheets like mRuleProcessor, but
  // for the Servo style backend.
  mozilla::UniquePtr<RawServoAuthorStyles> mServoStyles;
  mozilla::UniquePtr<mozilla::ServoStyleRuleMap> mStyleRuleMap;
};

#endif
