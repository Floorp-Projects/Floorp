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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

#ifndef nsRuleNode_h___
#define nsRuleNode_h___

#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIStyleRule.h"
#include "nsFixedSizeAllocator.h"
#include "nsIRuleNode.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsICSSDeclaration.h"

class nsRuleNode: public nsIRuleNode {
public:
  NS_DECL_ISUPPORTS

    // for purposes of the RuleDetail (and related code),
    //  * a non-inherited value is one that is specified as a
    //    non-"inherit" value or as an "inherit" value that is reflected
    //    in the struct and to the user of the style system with an
    //    eCSSUnit_Inherit value
    //  * an inherited value is one that is specified as "inherit" and
    //    where the inheritance is computed by the style system
  enum RuleDetail {
    eRuleNone, // No props have been specified at all.
    eRulePartialMixed, // At least one prop with a non-inherited value
                       // has been specified.  Some props may also have
                       // been specified with an inherited value.  At
                       // least one prop remains unspecified.
    eRulePartialInherited, // Only props with inherited values have
                           // have been specified.  At least one prop
                           // remains unspecified.
    eRuleFullMixed, // All props have been specified.  At least one has
                    // a non-inherited value.
    eRuleFullInherited, // All props have been specified with inherited
                        // values.
    eRuleUnknown // Information unknown (used as a result from a check
                 // callback to trigger the normal checking codepath)
  };

private:
  nsIPresContext* mPresContext; // Our pres context.

  nsRuleNode* mParent; // A pointer to the parent node in the tree.  This enables us to
                       // walk backwards from the most specific rule matched to the least
                       // specific rule (which is the optimal order to use for lookups
                       // of style properties.
  nsCOMPtr<nsIStyleRule> mRule; // A pointer to our specific rule.

  nsSupportsHashtable* mChildren; // A hashtable that maps from rules to our RuleNode children.
                                  // When matching rules, we use this table to transition from
                                  // node to node (constructing new nodes as needed to flesh out
                                  // the tree).

  nsCachedStyleData mStyleData;   // Any data we cached on the rule node.

  PRUint32 mInheritBits;          // Used to cache the fact that we can look up cached data under a parent
                                  // rule.  This is not the same thing as CSS inheritance.  

  PRUint32 mNoneBits;             // Used to cache the fact that this entire branch specifies no data
                                  // for a given struct type.  For example, if an entire rule branch
                                  // specifies no color information, then a bit will be set along every
                                  // rule node on that branch, so that you can break out of the rule tree
                                  // early.

  static PRUint32 gRefCnt;
 
protected:
  // The callback function for deleting rule nodes from our rule tree.
  static PRBool PR_CALLBACK DeleteChildren(nsHashKey *aKey, void *aData, void *closure);
  
   // Overloaded new operator. Initializes the memory to 0 and relies on an arena
  // (which comes from the presShell) to perform the allocation.
  void* operator new(size_t sz, nsIPresContext* aContext);
  void Destroy();

  void PropagateInheritBit(PRUint32 aBit, nsRuleNode* aHighestNode);
  void PropagateNoneBit(PRUint32 aBit, nsRuleNode* aHighestNode);
 
  PRBool InheritsFromParentRule(const nsStyleStructID aSID);
  
  const nsStyleStruct* SetDefaultOnRoot(const nsStyleStructID aSID, nsIStyleContext* aContext);

  const nsStyleStruct* WalkRuleTree(const nsStyleStructID aSID, nsIStyleContext* aContext, 
                                    nsRuleData* aRuleData,
                                    nsCSSStruct* aSpecificData);

  const nsStyleStruct* ComputeDisplayData(nsStyleStruct* aStartDisplay, const nsCSSStruct& aDisplayData, 
                                          nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeVisibilityData(nsStyleStruct* aStartVisibility, const nsCSSStruct& aDisplayData, 
                                             nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                             const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeFontData(nsStyleStruct* aStartFont, const nsCSSStruct& aFontData, 
                                       nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                       const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeColorData(nsStyleStruct* aStartColor, const nsCSSStruct& aColorData, 
                                        nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                        const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeBackgroundData(nsStyleStruct* aStartBackground, const nsCSSStruct& aColorData, 
                                             nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                             const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeMarginData(nsStyleStruct* aStartMargin, const nsCSSStruct& aMarginData, 
                                         nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                         const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeBorderData(nsStyleStruct* aStartBorder, const nsCSSStruct& aMarginData, 
                                         nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                         const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputePaddingData(nsStyleStruct* aStartPadding, const nsCSSStruct& aMarginData, 
                                          nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeOutlineData(nsStyleStruct* aStartOutline, const nsCSSStruct& aMarginData, 
                                          nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeListData(nsStyleStruct* aStartList, const nsCSSStruct& aListData, 
                                       nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                       const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputePositionData(nsStyleStruct* aStartPosition, const nsCSSStruct& aPositionData, 
                                           nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                           const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeTableData(nsStyleStruct* aStartTable, const nsCSSStruct& aTableData, 
                                        nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                        const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeTableBorderData(nsStyleStruct* aStartTable, const nsCSSStruct& aTableData, 
                                              nsIStyleContext* aContext,  
                                              nsRuleNode* aHighestNode,
                                              const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeContentData(nsStyleStruct* aStartContent, const nsCSSStruct& aData, 
                                          nsIStyleContext* aContext,  
                                          nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeQuotesData(nsStyleStruct* aStartQuotes, const nsCSSStruct& aData, 
                                         nsIStyleContext* aContext,  
                                         nsRuleNode* aHighestNode,
                                         const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeTextData(nsStyleStruct* aStartData, const nsCSSStruct& aData, 
                                       nsIStyleContext* aContext,  
                                       nsRuleNode* aHighestNode,
                                       const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeTextResetData(nsStyleStruct* aStartData, const nsCSSStruct& aData, 
                                            nsIStyleContext* aContext,  
                                            nsRuleNode* aHighestNode,
                                            const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeUIData(nsStyleStruct* aStartData, const nsCSSStruct& aData, 
                                     nsIStyleContext* aContext,  
                                     nsRuleNode* aHighestNode,
                                     const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeUIResetData(nsStyleStruct* aStartData, const nsCSSStruct& aData, 
                                          nsIStyleContext* aContext,  
                                          nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
#ifdef INCLUDE_XUL
  const nsStyleStruct* ComputeXULData(nsStyleStruct* aStartXUL, const nsCSSStruct& aXULData, 
                                      nsIStyleContext* aContext,  
                                      nsRuleNode* aHighestNode,
                                      const RuleDetail& aRuleDetail, PRBool aInherited);
#endif

  typedef const nsStyleStruct*
  (nsRuleNode::*ComputeStyleDataFn)(nsStyleStruct* aStartStruct,
                                    const nsCSSStruct& aStartData,
                                    nsIStyleContext* aContext,
                                    nsRuleNode* aHighestNode,
                                    const RuleDetail& aRuleDetail,
                                    PRBool aInherited);

  static ComputeStyleDataFn gComputeStyleDataFn[];

  inline RuleDetail CheckSpecifiedProperties(const nsStyleStructID aSID, const nsCSSStruct& aCSSStruct);

  const nsStyleStruct* GetParentData(const nsStyleStructID aSID);
  const nsStyleStruct* GetDisplayData(nsIStyleContext* aContext);
  const nsStyleStruct* GetVisibilityData(nsIStyleContext* aContext);
  const nsStyleStruct* GetFontData(nsIStyleContext* aContext);
  const nsStyleStruct* GetColorData(nsIStyleContext* aContext);
  const nsStyleStruct* GetBackgroundData(nsIStyleContext* aContext);
  const nsStyleStruct* GetMarginData(nsIStyleContext* aContext);
  const nsStyleStruct* GetBorderData(nsIStyleContext* aContext);
  const nsStyleStruct* GetPaddingData(nsIStyleContext* aContext);
  const nsStyleStruct* GetOutlineData(nsIStyleContext* aContext);
  const nsStyleStruct* GetListData(nsIStyleContext* aContext);
  const nsStyleStruct* GetPositionData(nsIStyleContext* aContext);
  const nsStyleStruct* GetTableData(nsIStyleContext* aContext);
  const nsStyleStruct* GetTableBorderData(nsIStyleContext* aContext);
  const nsStyleStruct* GetContentData(nsIStyleContext* aContext);
  const nsStyleStruct* GetQuotesData(nsIStyleContext* aContext);
  const nsStyleStruct* GetTextData(nsIStyleContext* aContext);
  const nsStyleStruct* GetTextResetData(nsIStyleContext* aContext);
  const nsStyleStruct* GetUIData(nsIStyleContext* aContext);
  const nsStyleStruct* GetUIResetData(nsIStyleContext* aContext);
#ifdef INCLUDE_XUL
  const nsStyleStruct* GetXULData(nsIStyleContext* aContext);
#endif

  typedef const nsStyleStruct* (nsRuleNode::*GetStyleDataFn)(nsIStyleContext*);
  static GetStyleDataFn gGetStyleDataFn[];

public:
  nsRuleNode(nsIPresContext* aPresContext, nsIStyleRule* aRule=nsnull, nsRuleNode* aParent=nsnull);
  virtual ~nsRuleNode();

  static void CreateRootNode(nsIPresContext* aPresContext, nsIRuleNode** aResult);

  // The nsIRuleNode Interface
  NS_IMETHOD GetBits(PRInt32 aType, PRUint32* aResult);
  NS_IMETHOD Transition(nsIStyleRule* aRule, nsIRuleNode** aResult);
  NS_IMETHOD GetParent(nsIRuleNode** aResult);
  NS_IMETHOD IsRoot(PRBool* aResult);
  NS_IMETHOD GetRule(nsIStyleRule** aResult);
  NS_IMETHOD ClearCachedData(nsIStyleRule* aRule);
  NS_IMETHOD ClearCachedDataInSubtree(nsIStyleRule* aRule);
  NS_IMETHOD GetPresContext(nsIPresContext** aResult);
  NS_IMETHOD PathContainsRule(nsIStyleRule* aRule, PRBool* aMatched);
  const nsStyleStruct* GetStyleData(nsStyleStructID aSID, 
                                    nsIStyleContext* aContext);
};

#endif
