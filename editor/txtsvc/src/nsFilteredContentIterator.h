/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFilteredContentIterator_h__
#define nsFilteredContentIterator_h__

#include "nsIContentIterator.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsITextServicesFilter.h"
#include "nsIDOMNSRange.h"
#include "nsIRangeUtils.h"

/**
 * 
 */
class nsFilteredContentIterator : public nsIContentIterator
{
public:

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  nsFilteredContentIterator(nsITextServicesFilter* aFilter);

  virtual ~nsFilteredContentIterator();

  /* nsIContentIterator */
  NS_IMETHOD Init(nsIContent* aRoot);
  NS_IMETHOD Init(nsIDOMRange* aRange);
  NS_IMETHOD First();
  NS_IMETHOD Last();
  NS_IMETHOD Next();
  NS_IMETHOD Prev();
  NS_IMETHOD CurrentNode(nsIContent **aNode);
  NS_IMETHOD IsDone();
  NS_IMETHOD PositionAt(nsIContent* aCurNode);

  /* Helpers */
  PRPackedBool DidSkip()      { return mDidSkip; }
  void         ClearDidSkip() {  mDidSkip = PR_FALSE; }

protected:
  nsFilteredContentIterator() { }

  // enum to give us the direction
  typedef enum {eDirNotSet, eForward, eBackward} eDirectionType;
  nsresult AdvanceNode(nsIDOMNode* aNode, nsIDOMNode*& aNewNode, eDirectionType aDir);
  void CheckAdvNode(nsIDOMNode* aNode, PRPackedBool& aDidSkip, eDirectionType aDir);
  nsresult SwitchDirections(PRPackedBool aChangeToForward);

  nsCOMPtr<nsIContentIterator> mCurrentIterator;
  nsCOMPtr<nsIContentIterator> mIterator;
  nsCOMPtr<nsIContentIterator> mPreIterator;

  nsCOMPtr<nsIAtom> mBlockQuoteAtom;
  nsCOMPtr<nsIAtom> mScriptAtom;
  nsCOMPtr<nsIAtom> mTextAreaAtom;
  nsCOMPtr<nsIAtom> mSelectAreaAtom;
  nsCOMPtr<nsIAtom> mMapAtom;

  nsCOMPtr<nsITextServicesFilter> mFilter;
  nsCOMPtr<nsIDOMNSRange>         mRange;
  PRPackedBool                    mDidSkip;
  PRPackedBool                    mIsOutOfRange;
  eDirectionType                  mDirection;
};

#endif
