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
      Destroy();
      return 0;
    }
    return mRefCnt;
  }

  nsIPresContext* PresContext() const { return mRuleNode->GetPresContext(); }

  nsStyleContext* GetParent() const { return mParent; }

  nsStyleContext* GetFirstChild() const { return mChild; }

  nsIAtom* GetPseudoType() const { return mPseudoTag; }

  already_AddRefed<nsStyleContext> 
  FindChildWithRules(const nsIAtom* aPseudoTag, nsRuleNode* aRules);

  PRBool    Equals(const nsStyleContext* aOther) const;
  PRBool    HasTextDecorations() { return mBits & NS_STYLE_HAS_TEXT_DECORATIONS; };

  void GetBorderPaddingFor(nsStyleBorderPadding& aBorderPadding);

  void SetStyle(nsStyleStructID aSID, nsStyleStruct* aStruct);

  nsRuleNode* GetRuleNode() { return mRuleNode; }
  void AddStyleBit(const PRUint32& aBit) { mBits |= aBit; }
  void GetStyleBits(PRUint32* aBits) { *aBits = mBits; }

  /*
   * Mark this style context's rule node (and its ancestors) to prevent
   * it from being garbage collected.
   */
  void Mark();

  /*
   * Get the style data for a style struct.  This is the most important
   * member function of nsIStyleContext.  It fills in a const pointer
   * to a style data struct that is appropriate for the style context's
   * frame.  This struct may be shared with other contexts (either in
   * the rule tree or the style context tree), so it should not be
   * modified.
   *
   * This function will NOT return null (even when out of memory) when
   * given a valid style struct ID, so the result does not need to be
   * null-checked.
   *
   * The typesafe functions below are preferred to the use of this
   * function.
   *
   * See also |nsIFrame::GetStyleData| and the other global
   * |GetStyleData| in nsIFrame.h.
   */
  const nsStyleStruct* GetStyleData(nsStyleStructID aSID);

  /**
   * Define typesafe getter functions for each style struct by
   * preprocessing the list of style structs.  These functions are the
   * preferred way to get style data.  The macro creates functions like:
   *   const nsStyleBorder* GetStyleBorder();
   *   const nsStyleColor* GetStyleColor();
   */

  #define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)                      \
    const nsStyle##name_ * GetStyle##name_() {                                \
      return NS_STATIC_CAST(const nsStyle##name_*,                            \
                            GetStyleData(eStyleStruct_##name_));              \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT


  const nsStyleStruct* PeekStyleData(nsStyleStructID aSID);

  nsStyleStruct* GetUniqueStyleData(const nsStyleStructID& aSID);

  void ClearStyleData(nsIPresContext* aPresContext);

  nsChangeHint CalcStyleDifference(nsStyleContext* aOther);

#ifdef DEBUG
  void DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent);

  void List(FILE* out, PRInt32 aIndent);
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
#endif
