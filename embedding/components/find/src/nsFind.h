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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Akkana Peck   <akkana@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsFind_h__
#define nsFind_h__

#include "nsIFind.h"

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIDOMRange.h"
#include "nsIContentIterator.h"
#include "nsIWordBreaker.h"

class nsIAtom;
class nsIContent;

#define NS_FIND_CONTRACTID "@mozilla.org/embedcomp/rangefind;1"

#define NS_FIND_CID \
 {0x471f4944, 0x1dd2, 0x11b2, {0x87, 0xac, 0x90, 0xbe, 0x0a, 0x51, 0xd6, 0x09}}

class nsFindContentIterator;

class nsFind : public nsIFind
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFIND

  nsFind();
  virtual ~nsFind();

  static already_AddRefed<nsIDOMRange> CreateRange();

protected:
  // Parameters set from the interface:
  //nsCOMPtr<nsIDOMRange> mRange;   // search only in this range
  bool mFindBackward;
  bool mCaseSensitive;

  nsCOMPtr<nsIWordBreaker> mWordBreaker;

  PRInt32 mIterOffset;
  nsCOMPtr<nsIDOMNode> mIterNode;

  // Last block parent, so that we will notice crossing block boundaries:
  nsCOMPtr<nsIDOMNode> mLastBlockParent;
  nsresult GetBlockParent(nsIDOMNode* aNode, nsIDOMNode** aParent);

  // Utility routines:
  bool IsTextNode(nsIDOMNode* aNode);
  bool IsBlockNode(nsIContent* aNode);
  bool SkipNode(nsIContent* aNode);
  bool IsVisibleNode(nsIDOMNode *aNode);

  // Move in the right direction for our search:
  nsresult NextNode(nsIDOMRange* aSearchRange,
                    nsIDOMRange* aStartPoint, nsIDOMRange* aEndPoint,
                    bool aContinueOk);

  // Reset variables before returning -- don't hold any references.
  void ResetAll();

  // The iterator we use to move through the document:
  nsresult InitIterator(nsIDOMNode* aStartNode, PRInt32 aStartOffset,
                        nsIDOMNode* aEndNode, PRInt32 aEndOffset);
  nsCOMPtr<nsFindContentIterator> mIterator;
};

#endif // nsFind_h__
