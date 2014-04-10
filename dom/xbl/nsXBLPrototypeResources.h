/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLPrototypeResources_h__
#define nsXBLPrototypeResources_h__

#include "nsAutoPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsIStyleRuleProcessor.h"

class nsIContent;
class nsIAtom;
class nsXBLResourceLoader;
class nsXBLPrototypeBinding;
class nsCSSStyleSheet;

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

  void Traverse(nsCycleCollectionTraversalCallback &cb) const;

  void ClearLoader();

  typedef nsTArray<nsRefPtr<nsCSSStyleSheet> > sheet_array_type;

private:
  // A loader object. Exists only long enough to load resources, and then it dies.
  nsXBLResourceLoader* mLoader;

public:
  // A list of loaded stylesheets for this binding.
  sheet_array_type mStyleSheetList;

  // The list of stylesheets converted to a rule processor.
  nsCOMPtr<nsIStyleRuleProcessor> mRuleProcessor;
};

#endif

