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
  nsXBLInsertionPoint(nsIContent* aParentElement, PRUint32 aIndex, nsIContent* aDefContent);
  ~nsXBLInsertionPoint();

  NS_INLINE_DECL_REFCOUNTING(nsXBLInsertionPoint)

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsXBLInsertionPoint)

  nsIContent* GetInsertionParent();
  void ClearInsertionParent() { mParentElement = nullptr; }

  PRInt32 GetInsertionIndex() { return mIndex; }

  void SetDefaultContent(nsIContent* aDefaultContent) { mDefaultContent = aDefaultContent; }
  nsIContent* GetDefaultContent();

  void SetDefaultContentTemplate(nsIContent* aDefaultContent) { mDefaultContentTemplate = aDefaultContent; }
  nsIContent* GetDefaultContentTemplate();

  void AddChild(nsIContent* aChildElement) { mElements.AppendObject(aChildElement); }
  void InsertChildAt(PRInt32 aIndex, nsIContent* aChildElement) { mElements.InsertObjectAt(aChildElement, aIndex); }
  void RemoveChild(nsIContent* aChildElement) { mElements.RemoveObject(aChildElement); }
  
  PRInt32 ChildCount() { return mElements.Count(); }

  nsIContent* ChildAt(PRUint32 aIndex);

  PRInt32 IndexOf(nsIContent* aContent) { return mElements.IndexOf(aContent); }

  bool Matches(nsIContent* aContent, PRUint32 aIndex);

  // Unbind all the default content in this insertion point.  Used
  // when the insertion parent is going away.
  void UnbindDefaultContent();

protected:
  nsIContent* mParentElement;            // This ref is weak.  The parent of the <children> element.
  PRInt32 mIndex;                        // The index of this insertion point. -1 is a pseudo-point.
  nsCOMArray<nsIContent> mElements;      // An array of elements present at the insertion point.
  nsCOMPtr<nsIContent> mDefaultContentTemplate ;           // The template default content that will be cloned if
                                                           // the insertion point is empty.
  nsCOMPtr<nsIContent> mDefaultContent;  // The cloned default content obtained by cloning mDefaultContentTemplate.
};

#endif
