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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#ifndef _nsStyleContext_h_
#define _nsStyleContext_h_

#include "nsRuleNode.h"

class nsIPresContext;
class nsIAtom;

class nsStyleContext
{
public:
  nsStyleContext(nsStyleContext* aParent, nsIAtom* aPseudoTag, 
                 nsRuleNode* aRuleNode, nsIPresContext* aPresContext);
  ~nsStyleContext();

  void* operator new(size_t sz, nsIPresContext* aPresContext) CPP_THROW_NEW;
  void Destroy();

  nsrefcnt AddRef() {
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsStyleContext", sizeof(nsStyleContext));
    return mRefCnt;
  }

  nsrefcnt Release() {
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsStyleContext");
    if (mRefCnt == 0) {
      mRefCnt = 1;
      Destroy();
      return 0;
    }
    return mRefCnt;
  }

  nsStyleContext* GetParent() const { return mParent; }

  nsStyleContext* GetFirstChild() const { return mChild; }

  already_AddRefed<nsIAtom> GetPseudoType() const;

  already_AddRefed<nsStyleContext> 
  FindChildWithRules(const nsIAtom* aPseudoTag, nsRuleNode* aRules);

  PRBool    Equals(const nsStyleContext* aOther) const;
  PRBool    HasTextDecorations() { return mBits & NS_STYLE_HAS_TEXT_DECORATIONS; };

  void GetBorderPaddingFor(nsStyleBorderPadding& aBorderPadding);

  void SetStyle(nsStyleStructID aSID, nsStyleStruct* aStruct);

  void GetRuleNode(nsRuleNode** aResult) { *aResult = mRuleNode; }
  void AddStyleBit(const PRUint32& aBit) { mBits |= aBit; }
  void GetStyleBits(PRUint32* aBits) { *aBits = mBits; }

  const nsStyleStruct* GetStyleData(nsStyleStructID aSID);
  const nsStyleStruct* PeekStyleData(nsStyleStructID aSID);

  nsStyleStruct* GetUniqueStyleData(nsIPresContext* aPresContext, const nsStyleStructID& aSID);

  void ClearCachedDataForRule(nsIStyleRule* aRule);

  void ClearStyleData(nsIPresContext* aPresContext, nsIStyleRule* aRule);

  nsChangeHint CalcStyleDifference(nsStyleContext* aOther);

#ifdef DEBUG
  void DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent);

  void List(FILE* out, PRInt32 aIndent);

  void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
#endif

protected:
  void AppendChild(nsStyleContext* aChild);
  void RemoveChild(nsStyleContext* aChild);

  void ApplyStyleFixups(nsIPresContext* aPresContext);

  nsStyleContext* mParent;
  nsStyleContext* mChild;
  nsStyleContext* mEmptyChild;
  nsStyleContext* mPrevSibling;
  nsStyleContext* mNextSibling;

  // If this style context is for a pseudo-element, the pseudo-element
  // atom.  Otherwise, null.
  nsCOMPtr<nsIAtom> mPseudoTag;

  nsRuleNode*             mRuleNode;

  // |mCachedStyleData| points to both structs that are owned by this
  // style context and structs that are owned by one of this style
  // context's ancestors (which are indirectly owned since this style
  // context owns a reference to its parent).  If the bit in |mBits| is
  // set for a struct, that means that the pointer for that struct is
  // owned by an ancestor rather than by this style context.
  nsCachedStyleData       mCachedStyleData; // Our cached style data.
  PRUint32                mBits; // Which structs are inherited from the
                                 // parent context.
  PRUint32                mRefCnt;
};

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsRuleNode* aRuleNode,
                   nsIPresContext* aPresContext);


// typesafe way to access style data.  See nsStyleStruct.h and also
// overloaded function in nsIFrame.h.
template <class T>
inline void
GetStyleData(nsStyleContext* aStyleContext, const T** aStyleStruct)
{
    *aStyleStruct = NS_STATIC_CAST(const T*,
                         aStyleContext->GetStyleData(NS_GET_STYLESTRUCTID(T)));
}

#endif
