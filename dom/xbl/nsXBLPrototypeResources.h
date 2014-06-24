/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLPrototypeResources_h__
#define nsXBLPrototypeResources_h__

#include "nsAutoPtr.h"
#include "nsICSSLoaderObserver.h"

class nsCSSRuleProcessor;
class nsCSSStyleSheet;
class nsIAtom;
class nsIContent;
class nsXBLPrototypeBinding;
class nsXBLResourceLoader;

// *********************************************************************/
// The XBLPrototypeResources class

class nsXBLPrototypeResources
{
public:
  nsXBLPrototypeResources(nsXBLPrototypeBinding* aBinding);
  ~nsXBLPrototypeResources();

  void LoadResources(bool* aResult);
  void AddResource(nsIAtom* aResourceType, const nsAString& aSrc);
  void AddResourceListener(nsIContent* aElement);
  nsresult FlushSkinSheets();

  nsresult Write(nsIObjectOutputStream* aStream);

  void Traverse(nsCycleCollectionTraversalCallback &cb);
  void Unlink();

  void ClearLoader();

  void AppendStyleSheet(nsCSSStyleSheet* aSheet);
  void RemoveStyleSheet(nsCSSStyleSheet* aSheet);
  void InsertStyleSheetAt(size_t aIndex, nsCSSStyleSheet* aSheet);
  nsCSSStyleSheet* StyleSheetAt(size_t aIndex) const;
  size_t SheetCount() const;
  bool HasStyleSheets() const;
  void AppendStyleSheetsTo(nsTArray<nsCSSStyleSheet*>& aResult) const;

  /**
   * Recreates mRuleProcessor to represent the current list of style sheets
   * stored in mStyleSheetList.  (Named GatherRuleProcessor to parallel
   * nsStyleSet::GatherRuleProcessors.)
   */
  void GatherRuleProcessor();

  nsCSSRuleProcessor* GetRuleProcessor() const { return mRuleProcessor; }

private:
  // A loader object. Exists only long enough to load resources, and then it dies.
  nsRefPtr<nsXBLResourceLoader> mLoader;

  // A list of loaded stylesheets for this binding.
  nsTArray<nsRefPtr<nsCSSStyleSheet>> mStyleSheetList;

  // The list of stylesheets converted to a rule processor.
  nsRefPtr<nsCSSRuleProcessor> mRuleProcessor;
};

#endif
