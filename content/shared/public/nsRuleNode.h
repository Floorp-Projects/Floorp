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

#ifndef nsRuleNode_h___
#define nsRuleNode_h___

#include "nsIPresContext.h"
#include "nsStyleStruct.h"

class nsStyleContext;
struct nsRuleList;
struct PLDHashTable;
class nsILanguageAtomService;
struct nsRuleData;
class nsIStyleRule;
struct nsCSSStruct;
// Copy of typedef that's in nsCSSStruct.h, for compilation speed.
typedef nsCSSStruct nsRuleDataStruct;

struct nsRuleDataFont;
class nsCSSValue;

typedef void (*nsPostResolveFunc)(nsStyleStruct* aStyleStruct, nsRuleData* aData);

struct nsInheritedStyleData
{

#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args) \
  nsStyle##name * m##name##Data;
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

  void* operator new(size_t sz, nsIPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  };

  void ClearInheritedData(PRUint32 aBits) {
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args) \
    if (m##name##Data && (aBits & NS_STYLE_INHERIT_BIT(name))) \
      m##name##Data = nsnull;
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET
  };

  void Destroy(PRUint32 aBits, nsIPresContext* aContext) {
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args) \
    if (m##name##Data && !(aBits & NS_STYLE_INHERIT_BIT(name))) \
      m##name##Data->Destroy(aContext);
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

    aContext->FreeToShell(sizeof(nsInheritedStyleData), this);
  };

  nsInheritedStyleData() {
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args) \
    m##name##Data = nsnull;
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

  };
};

struct nsResetStyleData
{
  nsResetStyleData()
  {
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args) \
    m##name##Data = nsnull;
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_RESET
#undef STYLE_STRUCT_INHERITED
  };

  void* operator new(size_t sz, nsIPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }

  void ClearInheritedData(PRUint32 aBits) {
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args) \
    if (m##name##Data && (aBits & NS_STYLE_INHERIT_BIT(name))) \
      m##name##Data = nsnull;
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_RESET
#undef STYLE_STRUCT_INHERITED
  };

  void Destroy(PRUint32 aBits, nsIPresContext* aContext) {
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args) \
    if (m##name##Data && !(aBits & NS_STYLE_INHERIT_BIT(name))) \
      m##name##Data->Destroy(aContext);
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_RESET
#undef STYLE_STRUCT_INHERITED

    aContext->FreeToShell(sizeof(nsResetStyleData), this);
  };

#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args) \
  nsStyle##name * m##name##Data;
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_RESET
#undef STYLE_STRUCT_INHERITED

};

struct nsCachedStyleData
{
  struct StyleStructInfo {
    ptrdiff_t mCachedStyleDataOffset;
    ptrdiff_t mInheritResetOffset;
    PRBool    mIsReset;
  };

  static StyleStructInfo gInfo[];

  nsInheritedStyleData* mInheritedData;
  nsResetStyleData* mResetData;

  static PRBool IsReset(const nsStyleStructID& aSID) {
    return gInfo[aSID].mIsReset;
  };

  static PRUint32 GetBitForSID(const nsStyleStructID& aSID) {
    return 1 << aSID;
  };

  NS_HIDDEN_(nsStyleStruct*) NS_FASTCALL GetStyleData(const nsStyleStructID& aSID) {
    // Each struct is stored at this.m##type##Data->m##name##Data where
    // |type| is either Inherit or Reset, and |name| is the name of the
    // style struct.  The |gInfo| stores the offset of the appropriate
    // m##type##Data for the struct within nsCachedStyleData (|this|)
    // and the offset of the appropriate m##name##Data within the
    // m##type##Data.  Note that if we don't have any reset structs,
    // then mResetData is null, and likewise for mInheritedData.  This
    // saves us from having to go through the long if-else cascade into
    // which most compilers will turn a case statement.

    // NOTE:  nsStyleContext::SetStyle works roughly the same way.

    const StyleStructInfo& info = gInfo[aSID];

    // Get either &mInheritedData or &mResetData.
    char* resetOrInheritSlot = NS_REINTERPRET_CAST(char*, this) + info.mCachedStyleDataOffset;

    // Get either mInheritedData or mResetData.
    char* resetOrInherit = NS_REINTERPRET_CAST(char*, *NS_REINTERPRET_CAST(void**, resetOrInheritSlot));

    nsStyleStruct* data = nsnull;
    if (resetOrInherit) {
      // If we have the mInheritedData or mResetData, then we might have
      // the struct, so get it.
      char* dataSlot = resetOrInherit + info.mInheritResetOffset;
      data = *NS_REINTERPRET_CAST(nsStyleStruct**, dataSlot);
    }
    return data;
  };

  NS_HIDDEN_(void) ClearInheritedData(PRUint32 aBits) {
    if (mResetData)
      mResetData->ClearInheritedData(aBits);
    if (mInheritedData)
      mInheritedData->ClearInheritedData(aBits);
  }

  NS_HIDDEN_(void) Destroy(PRUint32 aBits, nsIPresContext* aContext) {
    if (mResetData)
      mResetData->Destroy(aBits, aContext);
    if (mInheritedData)
      mInheritedData->Destroy(aBits, aContext);
    mResetData = nsnull;
    mInheritedData = nsnull;
  }

  nsCachedStyleData() :mInheritedData(nsnull), mResetData(nsnull) {};
  ~nsCachedStyleData() {};
};

class nsRuleNode {
public:
    // for purposes of the RuleDetail (and related code),
    //  * a non-inherited value is one that is specified as a
    //    non-"inherit" value or as an "inherit" value that is reflected
    //    in the struct and to the user of the style system with an
    //    eCSSUnit_Inherit value
    //  * an inherited value is one that is specified as "inherit" and
    //    where the inheritance is computed by the style system
  enum RuleDetail {
    eRuleNone, // No props have been specified at all.
    eRulePartialReset, // At least one prop with a non-inherited value
                       // has been specified.  No props have been
                       // specified with an inherited value.  At least
                       // one prop remains unspecified.
    eRulePartialMixed, // At least one prop with a non-inherited value
                       // has been specified.  Some props may also have
                       // been specified with an inherited value.  At
                       // least one prop remains unspecified.
    eRulePartialInherited, // Only props with inherited values have
                           // have been specified.  At least one prop
                           // remains unspecified.
    eRuleFullReset, // All props have been specified.  None has an
                    // inherited value.
    eRuleFullMixed, // All props have been specified.  At least one has
                    // a non-inherited value.
    eRuleFullInherited, // All props have been specified with inherited
                        // values.
    eRuleUnknown // Information unknown (used as a result from a check
                 // callback to trigger the normal checking codepath)
  };

private:
  nsIPresContext* mPresContext; // Our pres context.

  nsRuleNode* mParent; // A pointer to the parent node in the tree.
                       // This enables us to walk backwards from the
                       // most specific rule matched to the least
                       // specific rule (which is the optimal order to
                       // use for lookups of style properties.
  nsIStyleRule* mRule; // [STRONG] A pointer to our specific rule.

  // The children of this node are stored in either a hashtable or list
  // that maps from rules to our nsRuleNode children.  When matching
  // rules, we use this mapping to transition from node to node
  // (constructing new nodes as needed to flesh out the tree).

  void *mChildrenTaggedPtr; // Accessed only through the methods below.

  enum {
    kTypeMask = 0x1,
    kListType = 0x0,
    kHashType = 0x1
  };
  enum {
    // Maximum to have in a list before converting to a hashtable.
    // XXX Need to optimize this.
    kMaxChildrenInList = 32
  };

  PRBool HaveChildren() {
    return mChildrenTaggedPtr != nsnull;
  }
  PRBool ChildrenAreHashed() {
    return (PRWord(mChildrenTaggedPtr) & kTypeMask) == kHashType;
  }
  nsRuleList* ChildrenList() {
    return NS_REINTERPRET_CAST(nsRuleList*, mChildrenTaggedPtr);
  }
  nsRuleList** ChildrenListPtr() {
    return NS_REINTERPRET_CAST(nsRuleList**, &mChildrenTaggedPtr);
  }
  PLDHashTable* ChildrenHash() {
    return (PLDHashTable*) (PRWord(mChildrenTaggedPtr) & ~PRWord(kTypeMask));
  }
  void SetChildrenList(nsRuleList *aList) {
    NS_ASSERTION(!(PRWord(aList) & kTypeMask),
                 "pointer not 2-byte aligned");
    mChildrenTaggedPtr = aList;
  }
  void SetChildrenHash(PLDHashTable *aHashtable) {
    NS_ASSERTION(!(PRWord(aHashtable) & kTypeMask),
                 "pointer not 2-byte aligned");
    mChildrenTaggedPtr = (void*)(PRWord(aHashtable) | kHashType);
  }
  void ConvertChildrenToHash();

  nsCachedStyleData mStyleData;   // Any data we cached on the rule node.

  PRUint32 mDependentBits; // Used to cache the fact that we can look up
                           // cached data under a parent rule.

  PRUint32 mNoneBits; // Used to cache the fact that the branch to this
                      // node specifies no non-inherited data for a
                      // given struct type.  (This usually implies that
                      // the entire branch specifies no non-inherited
                      // data, although not necessarily, if a
                      // non-inherited value is overridden by an
                      // explicit 'inherit' value.)  For example, if an
                      // entire rule branch specifies no color
                      // information, then a bit will be set along every
                      // rule node on that branch, so that you can break
                      // out of the rule tree early and just inherit
                      // from the parent style context.  The presence of
                      // this bit means we should just get inherited
                      // data from the parent style context, and it is
                      // never used for reset structs since their
                      // Compute*Data functions don't initialize from
                      // inherited data.

friend struct nsRuleList;

public:
  // Overloaded new operator. Initializes the memory to 0 and relies on an arena
  // (which comes from the presShell) to perform the allocation.
  NS_HIDDEN_(void*) operator new(size_t sz, nsIPresContext* aContext) CPP_THROW_NEW;
  NS_HIDDEN_(void) Destroy();
  static NS_HIDDEN_(nsILanguageAtomService*) gLangService;

protected:
  NS_HIDDEN_(void) PropagateDependentBit(PRUint32 aBit,
                                         nsRuleNode* aHighestNode);
  NS_HIDDEN_(void) PropagateNoneBit(PRUint32 aBit, nsRuleNode* aHighestNode);
  
  NS_HIDDEN_(const nsStyleStruct*) SetDefaultOnRoot(const nsStyleStructID aSID,
                                                    nsStyleContext* aContext);

  NS_HIDDEN_(const nsStyleStruct*)
    WalkRuleTree(const nsStyleStructID aSID, nsStyleContext* aContext, 
                 nsRuleData* aRuleData, nsRuleDataStruct* aSpecificData);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeDisplayData(nsStyleStruct* aStartDisplay,
                       const nsRuleDataStruct& aDisplayData,
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeVisibilityData(nsStyleStruct* aStartVisibility,
                          const nsRuleDataStruct& aDisplayData,
                          nsStyleContext* aContext, nsRuleNode* aHighestNode,
                          const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeFontData(nsStyleStruct* aStartFont,
                    const nsRuleDataStruct& aFontData,
                    nsStyleContext* aContext, nsRuleNode* aHighestNode,
                    const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeColorData(nsStyleStruct* aStartColor,
                     const nsRuleDataStruct& aColorData,
                     nsStyleContext* aContext, nsRuleNode* aHighestNode,
                     const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeBackgroundData(nsStyleStruct* aStartBackground,
                          const nsRuleDataStruct& aColorData, 
                          nsStyleContext* aContext, nsRuleNode* aHighestNode,
                          const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeMarginData(nsStyleStruct* aStartMargin,
                      const nsRuleDataStruct& aMarginData, 
                      nsStyleContext* aContext, nsRuleNode* aHighestNode,
                      const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeBorderData(nsStyleStruct* aStartBorder,
                      const nsRuleDataStruct& aMarginData, 
                      nsStyleContext* aContext, nsRuleNode* aHighestNode,
                      const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputePaddingData(nsStyleStruct* aStartPadding,
                       const nsRuleDataStruct& aMarginData, 
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeOutlineData(nsStyleStruct* aStartOutline,
                       const nsRuleDataStruct& aMarginData, 
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeListData(nsStyleStruct* aStartList,
                    const nsRuleDataStruct& aListData,
                    nsStyleContext* aContext, nsRuleNode* aHighestNode,
                    const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputePositionData(nsStyleStruct* aStartPosition,
                        const nsRuleDataStruct& aPositionData, 
                        nsStyleContext* aContext, nsRuleNode* aHighestNode,
                        const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeTableData(nsStyleStruct* aStartTable,
                     const nsRuleDataStruct& aTableData, 
                     nsStyleContext* aContext, nsRuleNode* aHighestNode,
                     const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeTableBorderData(nsStyleStruct* aStartTable,
                           const nsRuleDataStruct& aTableData, 
                           nsStyleContext* aContext, nsRuleNode* aHighestNode,
                           const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeContentData(nsStyleStruct* aStartContent,
                       const nsRuleDataStruct& aData,
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeQuotesData(nsStyleStruct* aStartQuotes,
                      const nsRuleDataStruct& aData, 
                      nsStyleContext* aContext, nsRuleNode* aHighestNode,
                      const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeTextData(nsStyleStruct* aStartData, const nsRuleDataStruct& aData, 
                    nsStyleContext* aContext, nsRuleNode* aHighestNode,
                    const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeTextResetData(nsStyleStruct* aStartData,
                         const nsRuleDataStruct& aData,
                         nsStyleContext* aContext, nsRuleNode* aHighestNode,
                         const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeUserInterfaceData(nsStyleStruct* aStartData,
                             const nsRuleDataStruct& aData, 
                             nsStyleContext* aContext,
                             nsRuleNode* aHighestNode,
                             const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeUIResetData(nsStyleStruct* aStartData,
                       const nsRuleDataStruct& aData,
                       nsStyleContext* aContext, nsRuleNode* aHighestNode,
                       const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeXULData(nsStyleStruct* aStartXUL, const nsRuleDataStruct& aXULData, 
                   nsStyleContext* aContext, nsRuleNode* aHighestNode,
                   const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeColumnData(nsStyleStruct* aStartColumn, const nsRuleDataStruct& aColumnData,
                      nsStyleContext* aContext, nsRuleNode* aHighestNode,
                      const RuleDetail& aRuleDetail, PRBool aInherited);

#ifdef MOZ_SVG
  NS_HIDDEN_(const nsStyleStruct*)
    ComputeSVGData(nsStyleStruct* aStartSVG, const nsRuleDataStruct& aSVGData, 
                   nsStyleContext* aContext, nsRuleNode* aHighestNode,
                   const RuleDetail& aRuleDetail, PRBool aInherited);

  NS_HIDDEN_(const nsStyleStruct*)
    ComputeSVGResetData(nsStyleStruct* aStartSVG,
                        const nsRuleDataStruct& aSVGData, 
                        nsStyleContext* aContext, nsRuleNode* aHighestNode,
                        const RuleDetail& aRuleDetail, PRBool aInherited);
#endif

  // helpers for |ComputeFontData| that need access to |mNoneBits|:
  static NS_HIDDEN_(void) SetFont(nsIPresContext* aPresContext,
                                  nsStyleContext* aContext,
                                  nscoord aMinFontSize,
                                  PRBool aUseDocumentFonts,
                                  PRBool aIsGeneric,
                                  const nsRuleDataFont& aFontData,
                                  const nsFont& aDefaultFont,
                                  const nsStyleFont* aParentFont,
                                  nsStyleFont* aFont, PRBool& aInherited);

  static NS_HIDDEN_(void) SetGenericFont(nsIPresContext* aPresContext,
                                         nsStyleContext* aContext,
                                         const nsRuleDataFont& aFontData,
                                         PRUint8 aGenericFontID,
                                         nscoord aMinFontSize,
                                         PRBool aUseDocumentFonts,
                                         nsStyleFont* aFont);

  NS_HIDDEN_(void) AdjustLogicalBoxProp(nsStyleContext* aContext,
                                        const nsCSSValue& aLTRSource,
                                        const nsCSSValue& aRTLSource,
                                        const nsCSSValue& aLTRLogicalValue,
                                        const nsCSSValue& aRTLLogicalValue,
                                        const nsStyleSides& aParentRect,
                                        nsStyleSides& aRect,
                                        PRUint8 aSide,
                                        PRInt32 aMask,
                                        PRBool& aInherited);
  
  inline RuleDetail CheckSpecifiedProperties(const nsStyleStructID aSID, const nsRuleDataStruct& aRuleDataStruct);

  NS_HIDDEN_(const nsStyleStruct*) GetParentData(const nsStyleStructID aSID);
  NS_HIDDEN_(const nsStyleStruct*) GetDisplayData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetVisibilityData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetFontData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetColorData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetBackgroundData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetMarginData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetBorderData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetPaddingData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetOutlineData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetListData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetPositionData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetTableData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*)
    GetTableBorderData(nsStyleContext* aContext);

  NS_HIDDEN_(const nsStyleStruct*) GetContentData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetQuotesData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetTextData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetTextResetData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*)
    GetUserInterfaceData(nsStyleContext* aContext);

  NS_HIDDEN_(const nsStyleStruct*) GetUIResetData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetXULData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetColumnData(nsStyleContext* aContext);
#ifdef MOZ_SVG
  NS_HIDDEN_(const nsStyleStruct*) GetSVGData(nsStyleContext* aContext);
  NS_HIDDEN_(const nsStyleStruct*) GetSVGResetData(nsStyleContext* aContext);
#endif

public:
  nsRuleNode(nsIPresContext* aPresContext, nsIStyleRule* aRule,
             nsRuleNode* aParent) NS_HIDDEN;
  virtual ~nsRuleNode() NS_HIDDEN;

  static NS_HIDDEN_(nsRuleNode*) CreateRootNode(nsIPresContext* aPresContext);

  NS_HIDDEN_(nsresult) Transition(nsIStyleRule* aRule, nsRuleNode** aResult);
  nsRuleNode* GetParent() const { return mParent; }
  PRBool IsRoot() const { return mParent == nsnull; }

  // NOTE:  Does not |AddRef|.
  nsIStyleRule* GetRule() const { return mRule; }
  // NOTE: Does not |AddRef|.
  nsIPresContext* GetPresContext() const { return mPresContext; }

  NS_HIDDEN_(nsresult) ClearStyleData();
  NS_HIDDEN_(const nsStyleStruct*) GetStyleData(nsStyleStructID aSID, 
                                                nsStyleContext* aContext,
                                                PRBool aComputeData);

  /*
   * Garbage collection.  Mark walks up the tree, marking any unmarked
   * ancestors until it reaches a marked one.  Sweep recursively sweeps
   * the children, destroys any that are unmarked, and clears marks,
   * returning true if the node on which it was called was destroyed.
   */
  NS_HIDDEN_(void) Mark();
  NS_HIDDEN_(PRBool) Sweep();
};

#endif
