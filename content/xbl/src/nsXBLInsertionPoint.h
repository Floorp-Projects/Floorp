/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  void ClearInsertionParent() { mParentElement = nsnull; }

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
