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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#include "nsINameSpaceManager.h"
#include "nsIHTMLStyleSheet.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "pldhash.h"
#include "nsMappedAttributes.h"
#include "nsILink.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIStyleRule.h"
#include "nsIFrame.h"
#include "nsStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsIPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIDocument.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsLayoutAtoms.h"
#include "nsLayoutCID.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsCSSAnonBoxes.h"

#include "nsRuleWalker.h"

class HTMLColorRule : public nsIStyleRule {
public:
  HTMLColorRule(nsIHTMLStyleSheet* aSheet);
  virtual ~HTMLColorRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  nscolor             mColor;
  nsIHTMLStyleSheet*  mSheet;
};

HTMLColorRule::HTMLColorRule(nsIHTMLStyleSheet* aSheet)
  : mSheet(aSheet)
{
}

HTMLColorRule::~HTMLColorRule()
{
}

NS_IMPL_ISUPPORTS1(HTMLColorRule, nsIStyleRule)

NS_IMETHODIMP
HTMLColorRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

NS_IMETHODIMP
HTMLColorRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSID == eStyleStruct_Color) {
    if (aRuleData->mColorData->mColor.GetUnit() == eCSSUnit_Null)
      aRuleData->mColorData->mColor = nsCSSValue(mColor);
  }
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
HTMLColorRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}
#endif

class GenericTableRule: public nsIStyleRule {
public:
  GenericTableRule(nsIHTMLStyleSheet* aSheet);
  virtual ~GenericTableRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  nsIHTMLStyleSheet*  mSheet; // not ref-counted, cleared by content
};

GenericTableRule::GenericTableRule(nsIHTMLStyleSheet* aSheet)
{
  mSheet = aSheet;
}

GenericTableRule::~GenericTableRule()
{
}

NS_IMPL_ISUPPORTS1(GenericTableRule, nsIStyleRule)

NS_IMETHODIMP
GenericTableRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  aSheet = mSheet;
  return NS_OK;
}

NS_IMETHODIMP
GenericTableRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  // Nothing to do.
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
GenericTableRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}
#endif

// -----------------------------------------------------------
// this rule handles <th> inheritance
// -----------------------------------------------------------
class TableTHRule: public GenericTableRule {
public:
  TableTHRule(nsIHTMLStyleSheet* aSheet);
  virtual ~TableTHRule();

  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
};

TableTHRule::TableTHRule(nsIHTMLStyleSheet* aSheet)
: GenericTableRule(aSheet)
{
}

TableTHRule::~TableTHRule()
{
}

static void PostResolveCallback(nsStyleStruct* aStyleStruct, nsRuleData* aRuleData)
{
  nsStyleText* text = (nsStyleText*)aStyleStruct;
  if (text->mTextAlign == NS_STYLE_TEXT_ALIGN_DEFAULT) {
    nsStyleContext* parentContext = aRuleData->mStyleContext->GetParent();

    if (parentContext) {
      const nsStyleText* parentStyleText = parentContext->GetStyleText();
      PRUint8 parentAlign = parentStyleText->mTextAlign;
      text->mTextAlign = (NS_STYLE_TEXT_ALIGN_DEFAULT == parentAlign)
                              ? NS_STYLE_TEXT_ALIGN_CENTER : parentAlign;
    }
  }
}

NS_IMETHODIMP
TableTHRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData && aRuleData->mSID == eStyleStruct_Text) {
    aRuleData->mCanStoreInRuleTree = PR_FALSE;
    aRuleData->mPostResolveCallback = &PostResolveCallback;
  }
  return NS_OK;
}

static void 
ProcessTableRulesAttribute(nsStyleStruct* aStyleStruct, 
                           nsRuleData*    aRuleData,
                           PRUint8        aSide,
                           PRBool         aGroup,
                           PRUint8        aRulesArg1,
                           PRUint8        aRulesArg2,
                           PRUint8        aRulesArg3)
{
  if (!aStyleStruct || !aRuleData || !aRuleData->mPresContext) return;

  nsStyleContext* tableContext = aRuleData->mStyleContext->GetParent();
  if (!tableContext)
    return;
  if (!aGroup) {
    tableContext = tableContext->GetParent();
    if (!tableContext)
      return;
  } 
  
  const nsStyleTable* tableData = tableContext->GetStyleTable();
  if (aRulesArg1 == tableData->mRules ||
      aRulesArg2 == tableData->mRules ||
      aRulesArg3 == tableData->mRules) {
    const nsStyleBorder* tableBorderData = tableContext->GetStyleBorder();
    PRUint8 tableBorderStyle = tableBorderData->GetBorderStyle(aSide);

    nsStyleBorder* borderData = (nsStyleBorder*)aStyleStruct;
    if (!borderData)
      return;
    PRUint8 borderStyle = borderData->GetBorderStyle(aSide);
    // XXX It appears that the style system erronously applies the custom style rule after css style, 
    // consequently it does not properly fit into the casade. For now, assume that a border style of none
    // implies that the style has not been set.
    // XXXldb No, there's nothing wrong with the style system.  The problem
    // is that the author of all these table rules made them work as
    // post-resolve callbacks, which is an override mechanism that was meant
    // to be used for other things.  They should instead map their rule data
    // normally (see nsIStyleRule.h).
    if (NS_STYLE_BORDER_STYLE_NONE == borderStyle) {
      // use the table's border style if it is dashed or dotted, otherwise use solid
      PRUint8 bStyle = ((NS_STYLE_BORDER_STYLE_NONE != tableBorderStyle) &&
                        (NS_STYLE_BORDER_STYLE_HIDDEN != tableBorderStyle)) 
                        ? tableBorderStyle : NS_STYLE_BORDER_STYLE_SOLID;
      if ((NS_STYLE_BORDER_STYLE_DASHED != bStyle) && 
          (NS_STYLE_BORDER_STYLE_DOTTED != bStyle) && 
          (NS_STYLE_BORDER_STYLE_SOLID  != bStyle)) {
        bStyle = NS_STYLE_BORDER_STYLE_SOLID;
      }
      bStyle |= NS_STYLE_BORDER_STYLE_RULES_MASK;
      borderData->SetBorderStyle(aSide, bStyle);

      nscolor borderColor;
      PRBool transparent, foreground;
      borderData->GetBorderColor(aSide, borderColor, transparent, foreground);
      if (transparent || foreground) {
        // use the table's border color if it is set, otherwise use black
        nscolor tableBorderColor;
        tableBorderData->GetBorderColor(aSide, tableBorderColor, transparent, foreground);
        borderColor = (transparent || foreground) ? NS_RGB(0,0,0) : tableBorderColor;
        borderData->SetBorderColor(aSide, borderColor);
      }
      // set the border width to be 1 pixel
      float p2t;
      aRuleData->mPresContext->GetScaledPixelsToTwips(&p2t);
      nscoord onePixel = NSToCoordRound(p2t);
      nsStyleCoord coord(onePixel);
      switch(aSide) {
      case NS_SIDE_TOP:
        borderData->mBorder.SetTop(coord);
        break;
      case NS_SIDE_RIGHT:
        borderData->mBorder.SetRight(coord);
        break;
      case NS_SIDE_BOTTOM:
        borderData->mBorder.SetBottom(coord);
        break;
      default: // NS_SIDE_LEFT
        borderData->mBorder.SetLeft(coord);
        break;
      }
    }
  }
}

// -----------------------------------------------------------
// this rule handles borders on a <thead>, <tbody>, <tfoot> when rules is set on its <table>
// -----------------------------------------------------------
class TableTbodyRule: public GenericTableRule {
public:
  TableTbodyRule(nsIHTMLStyleSheet* aSheet);
  virtual ~TableTbodyRule();

  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
};

TableTbodyRule::TableTbodyRule(nsIHTMLStyleSheet* aSheet)
: GenericTableRule(aSheet)
{
}

TableTbodyRule::~TableTbodyRule()
{
}

static void TbodyPostResolveCallback(nsStyleStruct* aStyleStruct, nsRuleData* aRuleData)
{
  ::ProcessTableRulesAttribute(aStyleStruct, aRuleData, NS_SIDE_TOP, PR_TRUE, NS_STYLE_TABLE_RULES_ALL,
                               NS_STYLE_TABLE_RULES_GROUPS, NS_STYLE_TABLE_RULES_ROWS);
  ::ProcessTableRulesAttribute(aStyleStruct, aRuleData, NS_SIDE_BOTTOM, PR_TRUE, NS_STYLE_TABLE_RULES_ALL,
                               NS_STYLE_TABLE_RULES_GROUPS, NS_STYLE_TABLE_RULES_ROWS);
}

NS_IMETHODIMP
TableTbodyRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData && aRuleData->mSID == eStyleStruct_Border) {
    aRuleData->mCanStoreInRuleTree = PR_FALSE;
    aRuleData->mPostResolveCallback = &TbodyPostResolveCallback;
  }
  return NS_OK;
}
// -----------------------------------------------------------

// -----------------------------------------------------------
// this rule handles borders on a <row> when rules is set on its <table>
// -----------------------------------------------------------
class TableRowRule: public GenericTableRule {
public:
  TableRowRule(nsIHTMLStyleSheet* aSheet);
  virtual ~TableRowRule();

  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
};

TableRowRule::TableRowRule(nsIHTMLStyleSheet* aSheet)
: GenericTableRule(aSheet)
{
}

TableRowRule::~TableRowRule()
{
}

static void RowPostResolveCallback(nsStyleStruct* aStyleStruct, nsRuleData* aRuleData)
{
  ::ProcessTableRulesAttribute(aStyleStruct, aRuleData, NS_SIDE_TOP, PR_FALSE, NS_STYLE_TABLE_RULES_ALL,
                               NS_STYLE_TABLE_RULES_ROWS, NS_STYLE_TABLE_RULES_ROWS);
  ::ProcessTableRulesAttribute(aStyleStruct, aRuleData, NS_SIDE_BOTTOM, PR_FALSE, NS_STYLE_TABLE_RULES_ALL,
                               NS_STYLE_TABLE_RULES_ROWS, NS_STYLE_TABLE_RULES_ROWS);
}

NS_IMETHODIMP
TableRowRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData && aRuleData->mSID == eStyleStruct_Border) {
    aRuleData->mCanStoreInRuleTree = PR_FALSE;
    aRuleData->mPostResolveCallback = &RowPostResolveCallback;
  }
  return NS_OK;
}

// -----------------------------------------------------------
// this rule handles borders on a <colgroup> when rules is set on its <table>
// -----------------------------------------------------------
class TableColgroupRule: public GenericTableRule {
public:
  TableColgroupRule(nsIHTMLStyleSheet* aSheet);
  virtual ~TableColgroupRule();

  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
};

TableColgroupRule::TableColgroupRule(nsIHTMLStyleSheet* aSheet)
: GenericTableRule(aSheet)
{
}

TableColgroupRule::~TableColgroupRule()
{
}

static void ColgroupPostResolveCallback(nsStyleStruct* aStyleStruct, nsRuleData* aRuleData)
{
  ::ProcessTableRulesAttribute(aStyleStruct, aRuleData, NS_SIDE_LEFT, PR_TRUE, NS_STYLE_TABLE_RULES_ALL,
                               NS_STYLE_TABLE_RULES_GROUPS, NS_STYLE_TABLE_RULES_COLS);
  ::ProcessTableRulesAttribute(aStyleStruct, aRuleData, NS_SIDE_RIGHT, PR_TRUE, NS_STYLE_TABLE_RULES_ALL,
                               NS_STYLE_TABLE_RULES_GROUPS, NS_STYLE_TABLE_RULES_COLS);
}

NS_IMETHODIMP
TableColgroupRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData && aRuleData->mSID == eStyleStruct_Border) {
    aRuleData->mCanStoreInRuleTree = PR_FALSE;
    aRuleData->mPostResolveCallback = &ColgroupPostResolveCallback;
  }
  return NS_OK;
}

// -----------------------------------------------------------
// this rule handles borders on a <col> when rules is set on its <table>
// -----------------------------------------------------------
class TableColRule: public GenericTableRule {
public:
  TableColRule(nsIHTMLStyleSheet* aSheet);
  virtual ~TableColRule();

  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
};

TableColRule::TableColRule(nsIHTMLStyleSheet* aSheet)
: GenericTableRule(aSheet)
{
}

TableColRule::~TableColRule()
{
}

static void ColPostResolveCallback(nsStyleStruct* aStyleStruct, nsRuleData* aRuleData)
{
  ::ProcessTableRulesAttribute(aStyleStruct, aRuleData, NS_SIDE_LEFT, PR_FALSE, NS_STYLE_TABLE_RULES_ALL,
                               NS_STYLE_TABLE_RULES_COLS, NS_STYLE_TABLE_RULES_COLS);
  ::ProcessTableRulesAttribute(aStyleStruct, aRuleData, NS_SIDE_RIGHT, PR_FALSE, NS_STYLE_TABLE_RULES_ALL,
                               NS_STYLE_TABLE_RULES_COLS, NS_STYLE_TABLE_RULES_COLS);
}

NS_IMETHODIMP
TableColRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData && aRuleData->mSID == eStyleStruct_Border) {
    aRuleData->mCanStoreInRuleTree = PR_FALSE;
    aRuleData->mPostResolveCallback = &ColPostResolveCallback;
  }
  return NS_OK;
}
// -----------------------------------------------------------

struct MappedAttrTableEntry : public PLDHashEntryHdr {
  nsMappedAttributes *mAttributes;
};

PR_STATIC_CALLBACK(PLDHashNumber)
MappedAttrTable_HashKey(PLDHashTable *table, const void *key)
{
  nsMappedAttributes *attributes =
    NS_STATIC_CAST(nsMappedAttributes*, NS_CONST_CAST(void*, key));

  return attributes->HashValue();
}

PR_STATIC_CALLBACK(void)
MappedAttrTable_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  MappedAttrTableEntry *entry = NS_STATIC_CAST(MappedAttrTableEntry*, hdr);

  entry->mAttributes->DropStyleSheetReference();
  memset(entry, 0, sizeof(MappedAttrTableEntry));
}

PR_STATIC_CALLBACK(PRBool)
MappedAttrTable_MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                           const void *key)
{
  nsMappedAttributes *attributes =
    NS_STATIC_CAST(nsMappedAttributes*, NS_CONST_CAST(void*, key));
  const MappedAttrTableEntry *entry =
    NS_STATIC_CAST(const MappedAttrTableEntry*, hdr);

  return attributes->Equals(entry->mAttributes);
}

static PLDHashTableOps MappedAttrTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  PL_DHashGetKeyStub,
  MappedAttrTable_HashKey,
  MappedAttrTable_MatchEntry,
  PL_DHashMoveEntryStub,
  MappedAttrTable_ClearEntry,
  PL_DHashFinalizeStub,
  NULL
};

// -----------------------------------------------------------

class HTMLStyleSheetImpl : public nsIHTMLStyleSheet,
                           public nsIStyleRuleProcessor {
public:
  HTMLStyleSheetImpl(void);
  nsresult Init();

  NS_DECL_ISUPPORTS

  // nsIStyleSheet api
  NS_IMETHOD GetURL(nsIURI*& aURL) const;
  NS_IMETHOD GetTitle(nsString& aTitle) const;
  NS_IMETHOD GetType(nsString& aType) const;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const;
  NS_IMETHOD_(PRBool) UseForMedium(nsIAtom* aMedium) const;
  NS_IMETHOD_(PRBool) HasRules() const;

  NS_IMETHOD GetApplicable(PRBool& aApplicable) const;
  
  NS_IMETHOD SetEnabled(PRBool aEnabled);

  NS_IMETHOD GetComplete(PRBool& aComplete) const;
  NS_IMETHOD SetComplete();
  
  // style sheet owner info
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const;  // will be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const;

  NS_IMETHOD SetOwningDocument(nsIDocument* aDocumemt);

  NS_IMETHOD GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                   nsIStyleRuleProcessor* aPrevProcessor);

  // nsIStyleRuleProcessor API
  NS_IMETHOD RulesMatching(ElementRuleProcessorData* aData,
                           nsIAtom* aMedium);

  NS_IMETHOD RulesMatching(PseudoRuleProcessorData* aData,
                           nsIAtom* aMedium);

  NS_IMETHOD HasStateDependentStyle(StateRuleProcessorData* aData,
                                    nsIAtom* aMedium,
                                    nsReStyleHint* aResult);

  NS_IMETHOD HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                                        nsIAtom* aMedium,
                                        nsReStyleHint* aResult);

  // nsIHTMLStyleSheet api
  NS_IMETHOD Init(nsIURI* aURL, nsIDocument* aDocument);
  NS_IMETHOD Reset(nsIURI* aURL);
  NS_IMETHOD GetLinkColor(nscolor& aColor);
  NS_IMETHOD GetActiveLinkColor(nscolor& aColor);
  NS_IMETHOD GetVisitedLinkColor(nscolor& aColor);
  NS_IMETHOD SetLinkColor(nscolor aColor);
  NS_IMETHOD SetActiveLinkColor(nscolor aColor);
  NS_IMETHOD SetVisitedLinkColor(nscolor aColor);

  // Mapped Attribute management methods
  NS_IMETHOD UniqueMappedAttributes(nsMappedAttributes* aMapped,
                                    nsMappedAttributes*& aUniqueMapped);
  NS_IMETHOD DropMappedAttributes(nsMappedAttributes* aMapped);

#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

private: 
  // These are not supported and are not implemented! 
  HTMLStyleSheetImpl(const HTMLStyleSheetImpl& aCopy); 
  HTMLStyleSheetImpl& operator=(const HTMLStyleSheetImpl& aCopy); 

protected:
  virtual ~HTMLStyleSheetImpl();

protected:
  nsIURI*              mURL;
  nsIDocument*         mDocument;
  HTMLColorRule*       mLinkRule;
  HTMLColorRule*       mVisitedRule;
  HTMLColorRule*       mActiveRule;
  HTMLColorRule*       mDocumentColorRule;
  TableTbodyRule*      mTableTbodyRule;
  TableRowRule*        mTableRowRule;
  TableColgroupRule*   mTableColgroupRule;
  TableColRule*        mTableColRule;
  TableTHRule*         mTableTHRule;

  PLDHashTable         mMappedAttrTable;
};


HTMLStyleSheetImpl::HTMLStyleSheetImpl(void)
  : nsIHTMLStyleSheet(),
    mRefCnt(0),
    mURL(nsnull),
    mDocument(nsnull),
    mLinkRule(nsnull),
    mVisitedRule(nsnull),
    mActiveRule(nsnull),
    mDocumentColorRule(nsnull)
{
  mMappedAttrTable.ops = nsnull;
}

nsresult
HTMLStyleSheetImpl::Init()
{
  mTableTbodyRule = new TableTbodyRule(this);
  if (!mTableTbodyRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mTableTbodyRule);

  mTableRowRule = new TableRowRule(this);
  if (!mTableRowRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mTableRowRule);

  mTableColgroupRule = new TableColgroupRule(this);
  if (!mTableColgroupRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mTableColgroupRule);

  mTableColRule = new TableColRule(this);
  if (!mTableColRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mTableColRule);

  mTableTHRule = new TableTHRule(this);
  if (!mTableTHRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mTableTHRule);

  return NS_OK;
}

HTMLStyleSheetImpl::~HTMLStyleSheetImpl()
{
  NS_IF_RELEASE(mURL);
  if (nsnull != mLinkRule) {
    mLinkRule->mSheet = nsnull;
    NS_RELEASE(mLinkRule);
  }
  if (nsnull != mVisitedRule) {
    mVisitedRule->mSheet = nsnull;
    NS_RELEASE(mVisitedRule);
  }
  if (nsnull != mActiveRule) {
    mActiveRule->mSheet = nsnull;
    NS_RELEASE(mActiveRule);
  }
  if (nsnull != mDocumentColorRule) {
    mDocumentColorRule->mSheet = nsnull;
    NS_RELEASE(mDocumentColorRule);
  }
  if (nsnull != mTableTbodyRule) {
    mTableTbodyRule->mSheet = nsnull;
    NS_RELEASE(mTableTbodyRule);
  }
  if (nsnull != mTableRowRule) {
    mTableRowRule->mSheet = nsnull;
    NS_RELEASE(mTableRowRule);
  }
  if (nsnull != mTableColgroupRule) {
    mTableColgroupRule->mSheet = nsnull;
    NS_RELEASE(mTableColgroupRule);
  }
  if (nsnull != mTableColRule) {
    mTableColRule->mSheet = nsnull;
    NS_RELEASE(mTableColRule);
  }
  if (nsnull != mTableTHRule) {
    mTableTHRule->mSheet = nsnull;
    NS_RELEASE(mTableTHRule);
  }
  if (mMappedAttrTable.ops)
    PL_DHashTableFinish(&mMappedAttrTable);
}

NS_IMPL_ISUPPORTS3(HTMLStyleSheetImpl, nsIHTMLStyleSheet, nsIStyleSheet, nsIStyleRuleProcessor)

NS_IMETHODIMP
HTMLStyleSheetImpl::GetStyleRuleProcessor(nsIStyleRuleProcessor*& aProcessor,
                                          nsIStyleRuleProcessor* /*aPrevProcessor*/)
{
  aProcessor = this;
  NS_ADDREF(aProcessor);
  return NS_OK;
}

static nsresult GetBodyColor(nsIPresContext* aPresContext, nscolor* aColor)
{
  nsCOMPtr<nsIDocument> doc;
  nsIPresShell *shell = aPresContext->PresShell();
  shell->GetDocument(getter_AddRefs(doc));
  nsCOMPtr<nsIDOMHTMLDocument> domdoc = do_QueryInterface(doc);
  if (!domdoc)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIDOMHTMLElement> body;
  domdoc->GetBody(getter_AddRefs(body));
  nsCOMPtr<nsIContent> bodyContent = do_QueryInterface(body);
  nsIFrame *bodyFrame;
  shell->GetPrimaryFrameFor(bodyContent, &bodyFrame);
  if (!bodyFrame)
    return NS_ERROR_FAILURE;
  *aColor = bodyFrame->GetStyleColor()->mColor;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::RulesMatching(ElementRuleProcessorData* aData,
                                  nsIAtom* aMedium)
{
  nsIStyledContent *styledContent = aData->mStyledContent;

  if (styledContent) {
    nsRuleWalker *ruleWalker = aData->mRuleWalker;
    if (aData->mIsHTMLContent) {
      nsIAtom* tag = aData->mContentTag;

      // if we have anchor colors, check if this is an anchor with an href
      if (tag == nsHTMLAtoms::a) {
        if (mLinkRule || mVisitedRule || mActiveRule) {
          if (aData->mIsHTMLLink) {
            switch (aData->mLinkState) {
              case eLinkState_Unvisited:
                if (mLinkRule)
                  ruleWalker->Forward(mLinkRule);
                break;
              case eLinkState_Visited:
                if (mVisitedRule)
                  ruleWalker->Forward(mVisitedRule);
                break;
              default:
                break;
            }

            // No need to add to the active rule if it's not a link
            if (mActiveRule && (aData->mEventState & NS_EVENT_STATE_ACTIVE))
              ruleWalker->Forward(mActiveRule);
          }
        } // end link/visited/active rules
      } // end A tag
      // add the rule to handle text-align for a <th>
      else if (tag == nsHTMLAtoms::th) {
        ruleWalker->Forward(mTableTHRule);
      }
      else if (tag == nsHTMLAtoms::tr) {
        ruleWalker->Forward(mTableRowRule);
      }
      else if ((tag == nsHTMLAtoms::thead) || (tag == nsHTMLAtoms::tbody) || (tag == nsHTMLAtoms::tfoot)) {
        ruleWalker->Forward(mTableTbodyRule);
      }
      else if (tag == nsHTMLAtoms::col) {
        ruleWalker->Forward(mTableColRule);
      }
      else if (tag == nsHTMLAtoms::colgroup) {
        ruleWalker->Forward(mTableColgroupRule);
      }
      else if (tag == nsHTMLAtoms::table) {
        if (aData->mCompatMode == eCompatibility_NavQuirks) {
          nscolor bodyColor;
          nsresult rv =
            GetBodyColor(ruleWalker->GetCurrentNode()->GetPresContext(),
                         &bodyColor);
          if (NS_SUCCEEDED(rv) &&
              (!mDocumentColorRule || bodyColor != mDocumentColorRule->mColor)) {
            if (mDocumentColorRule) {
              mDocumentColorRule->mSheet = nsnull;
              NS_RELEASE(mDocumentColorRule);
            }
            mDocumentColorRule = new HTMLColorRule(this);
            if (mDocumentColorRule) {
              NS_ADDREF(mDocumentColorRule);
              mDocumentColorRule->mColor = bodyColor;
            }
          }
          if (mDocumentColorRule)
            ruleWalker->Forward(mDocumentColorRule);
        }
      }
    } // end html element

    // just get the style rules from the content
    styledContent->WalkContentStyleRules(ruleWalker);
  }

  return NS_OK;
}

// Test if style is dependent on content state
NS_IMETHODIMP
HTMLStyleSheetImpl::HasStateDependentStyle(StateRuleProcessorData* aData,
                                           nsIAtom* aMedium,
                                           nsReStyleHint* aResult)
{
  if (mActiveRule &&
      (aData->mStateMask & NS_EVENT_STATE_ACTIVE) &&
      aData->mStyledContent &&
      aData->mIsHTMLContent &&
      aData->mIsHTMLLink &&
      aData->mContentTag == nsHTMLAtoms::a)
    *aResult = eReStyle_Self;
  else
    *aResult = nsReStyleHint(0);

  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                                               nsIAtom* aMedium,
                                               nsReStyleHint* aResult)
{
  // Result is true for |href| changes on HTML links if we have link rules.
  nsIStyledContent *styledContent = aData->mStyledContent;
  if (aData->mAttribute == nsHTMLAtoms::href &&
      (mLinkRule || mVisitedRule || mActiveRule) &&
      styledContent &&
      styledContent->IsContentOfType(nsIContent::eHTML) &&
      aData->mContentTag == nsHTMLAtoms::a) {
    *aResult = eReStyle_Self;
    return NS_OK;
  }

  // Don't worry about the mDocumentColorRule since it only applies
  // to descendants of body, when we're already reresolving.

  // Handle the content style rules.
  if (styledContent && styledContent->IsAttributeMapped(aData->mAttribute)) {
    *aResult = eReStyle_Self;
    return NS_OK;
  }

  *aResult = nsReStyleHint(0);
  return NS_OK;
}


NS_IMETHODIMP
HTMLStyleSheetImpl::RulesMatching(PseudoRuleProcessorData* aData,
                                  nsIAtom* aMedium)
{
  nsIAtom* pseudoTag = aData->mPseudoTag;
  if (pseudoTag == nsCSSAnonBoxes::tableCol) {
    nsRuleWalker *ruleWalker = aData->mRuleWalker;
    if (ruleWalker) {
      ruleWalker->Forward(mTableColRule);
    }
  }
  return NS_OK;
}


  // nsIStyleSheet api
NS_IMETHODIMP
HTMLStyleSheetImpl::GetURL(nsIURI*& aURL) const
{
  aURL = mURL;
  NS_IF_ADDREF(aURL);
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetTitle(nsString& aTitle) const
{
  aTitle.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetType(nsString& aType) const
{
  aType.Assign(NS_LITERAL_STRING("text/html"));
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetMediumCount(PRInt32& aCount) const
{
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const
{
  aMedium = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP_(PRBool)
HTMLStyleSheetImpl::UseForMedium(nsIAtom* aMedium) const
{
  return PR_TRUE; // works for all media
}

NS_IMETHODIMP_(PRBool)
HTMLStyleSheetImpl::HasRules() const
{
  return PR_TRUE; // We have rules at all reasonable times
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetApplicable(PRBool& aApplicable) const
{
  aApplicable = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetEnabled(PRBool aEnabled)
{ // these can't be disabled
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetComplete(PRBool& aComplete) const
{
  aComplete = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetComplete()
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetParentSheet(nsIStyleSheet*& aParent) const
{
  aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetOwningDocument(nsIDocument*& aDocument) const
{
  aDocument = mDocument;
  NS_IF_ADDREF(aDocument);
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument; // not refcounted
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::Init(nsIURI* aURL, nsIDocument* aDocument)
{
  NS_PRECONDITION(aURL && aDocument, "null ptr");
  if (! aURL || ! aDocument)
    return NS_ERROR_NULL_POINTER;

  if (mURL || mDocument)
    return NS_ERROR_ALREADY_INITIALIZED;

  mDocument = aDocument; // not refcounted!
  mURL = aURL;
  NS_ADDREF(mURL);
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::Reset(nsIURI* aURL)
{
  NS_IF_RELEASE(mURL);
  mURL = aURL;
  NS_ADDREF(mURL);

  if (mLinkRule) {
    mLinkRule->mSheet = nsnull;
    NS_RELEASE(mLinkRule);
  }
  if (mVisitedRule) {
    mVisitedRule->mSheet = nsnull;
    NS_RELEASE(mVisitedRule);
  }
  if (mActiveRule) {
    mActiveRule->mSheet = nsnull;
    NS_RELEASE(mActiveRule);
  }
  if (mDocumentColorRule) {
    mDocumentColorRule->mSheet = nsnull;
    NS_RELEASE(mDocumentColorRule);
  }

  if (mMappedAttrTable.ops) {
    PL_DHashTableFinish(&mMappedAttrTable);
    mMappedAttrTable.ops = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetLinkColor(nscolor& aColor)
{
  if (!mLinkRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mLinkRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetActiveLinkColor(nscolor& aColor)
{
  if (!mActiveRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mActiveRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP
HTMLStyleSheetImpl::GetVisitedLinkColor(nscolor& aColor)
{
  if (!mVisitedRule) {
    return NS_HTML_STYLE_PROPERTY_NOT_THERE;
  }
  else {
    aColor = mVisitedRule->mColor;
    return NS_OK;
  }
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetLinkColor(nscolor aColor)
{
  if (mLinkRule) {
    if (mLinkRule->mColor == aColor)
      return NS_OK;
    mLinkRule->mSheet = nsnull;
    NS_RELEASE(mLinkRule);
  }

  mLinkRule = new HTMLColorRule(this);
  if (!mLinkRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mLinkRule);

  mLinkRule->mColor = aColor;
  return NS_OK;
}


NS_IMETHODIMP
HTMLStyleSheetImpl::SetActiveLinkColor(nscolor aColor)
{
  if (mActiveRule) {
    if (mActiveRule->mColor == aColor)
      return NS_OK;
    mActiveRule->mSheet = nsnull;
    NS_RELEASE(mActiveRule);
  }

  mActiveRule = new HTMLColorRule(this);
  if (!mActiveRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mActiveRule);

  mActiveRule->mColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::SetVisitedLinkColor(nscolor aColor)
{
  if (mVisitedRule) {
    if (mVisitedRule->mColor == aColor)
      return NS_OK;
    mVisitedRule->mSheet = nsnull;
    NS_RELEASE(mVisitedRule);
  }

  mVisitedRule = new HTMLColorRule(this);
  if (!mVisitedRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mVisitedRule);

  mVisitedRule->mColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::UniqueMappedAttributes(nsMappedAttributes* aMapped,
                                           nsMappedAttributes*& aUniqueMapped)
{
  aUniqueMapped = nsnull;
  if (!mMappedAttrTable.ops) {
    PRBool res = PL_DHashTableInit(&mMappedAttrTable, &MappedAttrTable_Ops,
                                   nsnull, sizeof(MappedAttrTableEntry), 16);
    if (!res) {
      mMappedAttrTable.ops = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  MappedAttrTableEntry *entry = NS_STATIC_CAST(MappedAttrTableEntry*,
    PL_DHashTableOperate(&mMappedAttrTable, aMapped, PL_DHASH_ADD));
  if (!entry)
    return NS_ERROR_OUT_OF_MEMORY;
  if (!entry->mAttributes) {
    // We added a new entry to the hashtable, so we have a new unique set.
    entry->mAttributes = aMapped;
  }
  aUniqueMapped = entry->mAttributes;
  NS_ADDREF(aUniqueMapped);

  return NS_OK;
}

NS_IMETHODIMP
HTMLStyleSheetImpl::DropMappedAttributes(nsMappedAttributes* aMapped)
{
  NS_ENSURE_TRUE(aMapped, NS_OK);

  NS_ASSERTION(mMappedAttrTable.ops, "table uninitialized");
#ifdef DEBUG
  PRUint32 entryCount = mMappedAttrTable.entryCount - 1;
#endif

  PL_DHashTableOperate(&mMappedAttrTable, aMapped, PL_DHASH_REMOVE);

  NS_ASSERTION(entryCount == mMappedAttrTable.entryCount, "not removed");

  return NS_OK;
}

#ifdef DEBUG
void HTMLStyleSheetImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML Style Sheet: ", out);
  nsCAutoString urlSpec;
  mURL->GetSpec(urlSpec);
  if (!urlSpec.IsEmpty()) {
    fputs(urlSpec.get(), out);
  }
  fputs("\n", out);
}
#endif

// XXX For convenience and backwards compatibility
nsresult
NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult, nsIURI* aURL, 
                     nsIDocument* aDocument)
{
  nsresult rv;
  nsIHTMLStyleSheet* sheet;
  if (NS_FAILED(rv = NS_NewHTMLStyleSheet(&sheet)))
    return rv;

  if (NS_FAILED(rv = sheet->Init(aURL, aDocument))) {
    NS_RELEASE(sheet);
    return rv;
  }

  *aInstancePtrResult = sheet;
  return NS_OK;
}


nsresult
NS_NewHTMLStyleSheet(nsIHTMLStyleSheet** aInstancePtrResult)
{
  NS_ASSERTION(aInstancePtrResult, "null out param");

  HTMLStyleSheetImpl *it = new HTMLStyleSheetImpl();
  if (!it) {
    *aInstancePtrResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);
  nsresult rv = it->Init();
  if (NS_FAILED(rv))
    NS_RELEASE(it);

  *aInstancePtrResult = it; // NS_ADDREF above, or set to null by NS_RELEASE
  return rv;
}
