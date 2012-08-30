/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLInsertionPoint_h__
#define nsXBLInsertionPoint_h__

#include "nsCOMArray.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

class nsXBLInsertionPoint
{
public:
  nsXBLInsertionPoint(nsIContent* aParentElement, uint32_t aIndex, nsIContent* aDefContent);
  ~nsXBLInsertionPoint();

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsXBLInsertionPoint)

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsXBLInsertionPoint)

  nsIContent* GetInsertionParent();
  void ClearInsertionParent() { mParentElement = nullptr; }

  int32_t GetInsertionIndex() { return mIndex; }

  void SetDefaultContent(nsIContent* aDefaultContent) { mDefaultContent = aDefaultContent; }
  nsIContent* GetDefaultContent();

  void SetDefaultContentTemplate(nsIContent* aDefaultContent) { mDefaultContentTemplate = aDefaultContent; }
  nsIContent* GetDefaultContentTemplate();

  void AddChild(nsIContent* aChildElement) { mElements.AppendObject(aChildElement); }
  void InsertChildAt(int32_t aIndex, nsIContent* aChildElement) { mElements.InsertObjectAt(aChildElement, aIndex); }
  void RemoveChild(nsIContent* aChildElement) { mElements.RemoveObject(aChildElement); }
  
  int32_t ChildCount() { return mElements.Count(); }

  nsIContent* ChildAt(uint32_t aIndex);

  int32_t IndexOf(nsIContent* aContent) { return mElements.IndexOf(aContent); }

  bool Matches(nsIContent* aContent, uint32_t aIndex);

  // Unbind all the default content in this insertion point.  Used
  // when the insertion parent is going away.
  void UnbindDefaultContent();

protected:
  nsIContent* mParentElement;            // This ref is weak.  The parent of the <children> element.
  int32_t mIndex;                        // The index of this insertion point. -1 is a pseudo-point.
  nsCOMArray<nsIContent> mElements;      // An array of elements present at the insertion point.
  nsCOMPtr<nsIContent> mDefaultContentTemplate ;           // The template default content that will be cloned if
                                                           // the insertion point is empty.
  nsCOMPtr<nsIContent> mDefaultContent;  // The cloned default content obtained by cloning mDefaultContentTemplate.
};

#endif
