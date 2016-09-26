/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLPrototypeResources_h__
#define nsXBLPrototypeResources_h__

#include "mozilla/StyleSheet.h"
#include "nsICSSLoaderObserver.h"

class nsCSSRuleProcessor;
class nsIAtom;
class nsIContent;
class nsXBLPrototypeBinding;
class nsXBLResourceLoader;

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla

// *********************************************************************/
// The XBLPrototypeResources class

class nsXBLPrototypeResources
{
public:
  explicit nsXBLPrototypeResources(nsXBLPrototypeBinding* aBinding);
  ~nsXBLPrototypeResources();

  void LoadResources(bool* aResult);
  void AddResource(nsIAtom* aResourceType, const nsAString& aSrc);
  void AddResourceListener(nsIContent* aElement);
  nsresult FlushSkinSheets();

  nsresult Write(nsIObjectOutputStream* aStream);

  void Traverse(nsCycleCollectionTraversalCallback &cb);
  void Unlink();

  void ClearLoader();

  void AppendStyleSheet(mozilla::StyleSheet* aSheet);
  void RemoveStyleSheet(mozilla::StyleSheet* aSheet);
  void InsertStyleSheetAt(size_t aIndex, mozilla::StyleSheet* aSheet);
  mozilla::StyleSheet* StyleSheetAt(size_t aIndex) const;
  size_t SheetCount() const;
  bool HasStyleSheets() const;
  void AppendStyleSheetsTo(nsTArray<mozilla::StyleSheet*>& aResult) const;

  /**
   * Recreates mRuleProcessor to represent the current list of style sheets
   * stored in mStyleSheetList.  (Named GatherRuleProcessor to parallel
   * nsStyleSet::GatherRuleProcessors.)
   */
  void GatherRuleProcessor();

  nsCSSRuleProcessor* GetRuleProcessor() const { return mRuleProcessor; }

private:
  // A loader object. Exists only long enough to load resources, and then it dies.
  RefPtr<nsXBLResourceLoader> mLoader;

  // A list of loaded stylesheets for this binding.
  nsTArray<RefPtr<mozilla::StyleSheet>> mStyleSheetList;

  // The list of stylesheets converted to a rule processor.
  RefPtr<nsCSSRuleProcessor> mRuleProcessor;
};

#endif
