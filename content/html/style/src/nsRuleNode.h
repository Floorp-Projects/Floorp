/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

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

  enum RuleDetail {
    eRuleNone, // No props have been specified at all.
    eRulePartialMixed,  // At least one prop with a non-inherited val has been specified.  Some props
                        // may also have been specified with a val of "inherit".  At least one
                        // prop remains unspecified.
    eRulePartialInherited,  // Only props with vals of "inherit" have been specified.  At least
                            // one prop remains unspecified.
    eRuleFullMixed, // All props have been specified.  At least one has a non-inherited val.
    eRuleFullInherited // All props have been specified with a val of "inherit"
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
 
  PRBool InheritsFromParentRule(const nsStyleStructID& aSID);
  
  const nsStyleStruct* SetDefaultOnRoot(const nsStyleStructID& aSID, nsIStyleContext* aContext);

  const nsStyleStruct* WalkRuleTree(const nsStyleStructID& aSID, nsIStyleContext* aContext, 
                                    nsRuleData* aRuleData,
                                    nsCSSStruct* aSpecificData);

  const nsStyleStruct* ComputeStyleData(const nsStyleStructID& aSID, nsStyleStruct* aStartStruct, 
                                        const nsCSSStruct& aStartData, 
                                        nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                        const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeDisplayData(nsStyleDisplay* aStartDisplay, const nsCSSDisplay& aDisplayData, 
                                          nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeVisibilityData(nsStyleVisibility* aStartVisibility, const nsCSSDisplay& aDisplayData, 
                                             nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                             const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeFontData(nsStyleFont* aStartFont, const nsCSSFont& aFontData, 
                                       nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                       const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeColorData(nsStyleColor* aStartColor, const nsCSSColor& aColorData, 
                                        nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                        const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeBackgroundData(nsStyleBackground* aStartBackground, const nsCSSColor& aColorData, 
                                             nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                             const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeMarginData(nsStyleMargin* aStartMargin, const nsCSSMargin& aMarginData, 
                                         nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                         const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeBorderData(nsStyleBorder* aStartBorder, const nsCSSMargin& aMarginData, 
                                         nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                         const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputePaddingData(nsStylePadding* aStartPadding, const nsCSSMargin& aMarginData, 
                                          nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeOutlineData(nsStyleOutline* aStartOutline, const nsCSSMargin& aMarginData, 
                                          nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeListData(nsStyleList* aStartList, const nsCSSList& aListData, 
                                       nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                       const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputePositionData(nsStylePosition* aStartPosition, const nsCSSPosition& aPositionData, 
                                           nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                           const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeTableData(nsStyleTable* aStartTable, const nsCSSTable& aTableData, 
                                        nsIStyleContext* aContext, nsRuleNode* aHighestNode,
                                        const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeTableBorderData(nsStyleTableBorder* aStartTable, const nsCSSTable& aTableData, 
                                              nsIStyleContext* aContext,  
                                              nsRuleNode* aHighestNode,
                                              const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeContentData(nsStyleContent* aStartContent, const nsCSSContent& aData, 
                                          nsIStyleContext* aContext,  
                                          nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeQuotesData(nsStyleQuotes* aStartQuotes, const nsCSSContent& aData, 
                                         nsIStyleContext* aContext,  
                                         nsRuleNode* aHighestNode,
                                         const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeTextData(nsStyleText* aStartData, const nsCSSText& aData, 
                                       nsIStyleContext* aContext,  
                                       nsRuleNode* aHighestNode,
                                       const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeTextResetData(nsStyleTextReset* aStartData, const nsCSSText& aData, 
                                            nsIStyleContext* aContext,  
                                            nsRuleNode* aHighestNode,
                                            const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeUIData(nsStyleUserInterface* aStartData, const nsCSSUserInterface& aData, 
                                     nsIStyleContext* aContext,  
                                     nsRuleNode* aHighestNode,
                                     const RuleDetail& aRuleDetail, PRBool aInherited);
  const nsStyleStruct* ComputeUIResetData(nsStyleUIReset* aStartData, const nsCSSUserInterface& aData, 
                                          nsIStyleContext* aContext,  
                                          nsRuleNode* aHighestNode,
                                          const RuleDetail& aRuleDetail, PRBool aInherited);
#ifdef INCLUDE_XUL
  const nsStyleStruct* ComputeXULData(nsStyleXUL* aStartXUL, const nsCSSXUL& aXULData, 
                                      nsIStyleContext* aContext,  
                                      nsRuleNode* aHighestNode,
                                      const RuleDetail& aRuleDetail, PRBool aInherited);
#endif

  RuleDetail CheckSpecifiedProperties(const nsStyleStructID& aSID, const nsCSSStruct& aCSSStruct);
  RuleDetail CheckDisplayProperties(const nsCSSDisplay& aDisplay);
  RuleDetail CheckVisibilityProperties(const nsCSSDisplay& aDisplay);
  RuleDetail CheckFontProperties(const nsCSSFont& aFont);
  RuleDetail CheckColorProperties(const nsCSSColor& aColor);
  RuleDetail CheckBackgroundProperties(const nsCSSColor& aColor);
  RuleDetail CheckMarginProperties(const nsCSSMargin& aMargin);
  RuleDetail CheckBorderProperties(const nsCSSMargin& aMargin);
  RuleDetail CheckPaddingProperties(const nsCSSMargin& aMargin);
  RuleDetail CheckOutlineProperties(const nsCSSMargin& aMargin);
  RuleDetail CheckListProperties(const nsCSSList& aList);
  RuleDetail CheckPositionProperties(const nsCSSPosition& aPosition);
  RuleDetail CheckTableProperties(const nsCSSTable& aTable);
  RuleDetail CheckTableBorderProperties(const nsCSSTable& aTable);
  RuleDetail CheckContentProperties(const nsCSSContent& aContent);
  RuleDetail CheckQuotesProperties(const nsCSSContent& aContent);
  RuleDetail CheckTextProperties(const nsCSSText& aText);
  RuleDetail CheckTextResetProperties(const nsCSSText& aText);
  RuleDetail CheckUIProperties(const nsCSSUserInterface& aUI);
  RuleDetail CheckUIResetProperties(const nsCSSUserInterface& aUI);

#ifdef INCLUDE_XUL
  RuleDetail CheckXULProperties(const nsCSSXUL& aXUL);
#endif

  const nsStyleStruct* GetParentData(const nsStyleStructID& aSID);
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

public:
  nsRuleNode(nsIPresContext* aPresContext, nsIStyleRule* aRule=nsnull, nsRuleNode* aParent=nsnull);
  virtual ~nsRuleNode();

  static void CreateRootNode(nsIPresContext* aPresContext, nsIRuleNode** aResult);

  // The nsIRuleNode Interface
  NS_IMETHOD Transition(nsIStyleRule* aRule, nsIRuleNode** aResult);
  NS_IMETHOD GetParent(nsIRuleNode** aResult);
  NS_IMETHOD IsRoot(PRBool* aResult);
  NS_IMETHOD GetRule(nsIStyleRule** aResult);
  NS_IMETHOD ClearCachedData(nsIStyleRule* aRule);
  NS_IMETHOD ClearCachedDataInSubtree(nsIStyleRule* aRule);
  NS_IMETHOD GetPresContext(nsIPresContext** aResult);
  const nsStyleStruct* GetStyleData(nsStyleStructID aSID, 
                                    nsIStyleContext* aContext);
};

#endif
