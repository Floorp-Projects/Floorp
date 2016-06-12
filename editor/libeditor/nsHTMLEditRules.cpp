/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLEditRules.h"

#include <stdlib.h>

#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/mozalloc.h"
#include "nsAutoPtr.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsEditorUtils.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsHTMLCSSUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsID.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMText.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsIHTMLDocument.h"
#include "nsINode.h"
#include "nsLiteralString.h"
#include "nsPlaintextEditor.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTextEditUtils.h"
#include "nsThreadUtils.h"
#include "nsUnicharUtils.h"
#include "nsWSRunObject.h"
#include <algorithm>

// Workaround for windows headers
#ifdef SetProp
#undef SetProp
#endif

class nsISupports;
class nsRulesInfo;

using namespace mozilla;
using namespace mozilla::dom;

//const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
//const static char* kMOZEditorBogusNodeValue="TRUE";

enum
{
  kLonely = 0,
  kPrevSib = 1,
  kNextSib = 2,
  kBothSibs = 3
};

/********************************************************
 *  first some helpful functors we will use
 ********************************************************/

static bool IsBlockNode(const nsINode& node)
{
  return nsHTMLEditor::NodeIsBlockStatic(&node);
}

static bool IsInlineNode(const nsINode& node)
{
  return !IsBlockNode(node);
}

static bool
IsStyleCachePreservingAction(EditAction action)
{
  return action == EditAction::deleteSelection ||
         action == EditAction::insertBreak ||
         action == EditAction::makeList ||
         action == EditAction::indent ||
         action == EditAction::outdent ||
         action == EditAction::align ||
         action == EditAction::makeBasicBlock ||
         action == EditAction::removeList ||
         action == EditAction::makeDefListItem ||
         action == EditAction::insertElement ||
         action == EditAction::insertQuotation;
}

class nsTableCellAndListItemFunctor : public nsBoolDomIterFunctor
{
  public:
    // Used to build list of all li's, td's & th's iterator covers
    virtual bool operator()(nsINode* aNode) const
    {
      if (nsHTMLEditUtils::IsTableCell(aNode)) return true;
      if (nsHTMLEditUtils::IsListItem(aNode)) return true;
      return false;
    }
};

class nsBRNodeFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual bool operator()(nsINode* aNode) const
    {
      if (aNode->IsHTMLElement(nsGkAtoms::br)) {
        return true;
      }
      return false;
    }
};

class nsEmptyEditableFunctor : public nsBoolDomIterFunctor
{
  public:
    explicit nsEmptyEditableFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
    virtual bool operator()(nsINode* aNode) const
    {
      if (mHTMLEditor->IsEditable(aNode) &&
          (nsHTMLEditUtils::IsListItem(aNode) ||
           nsHTMLEditUtils::IsTableCellOrCaption(*aNode))) {
        bool bIsEmptyNode;
        nsresult res = mHTMLEditor->IsEmptyNode(aNode, &bIsEmptyNode, false, false);
        NS_ENSURE_SUCCESS(res, false);
        if (bIsEmptyNode)
          return true;
      }
      return false;
    }
  protected:
    nsHTMLEditor* mHTMLEditor;
};

class nsEditableTextFunctor : public nsBoolDomIterFunctor
{
  public:
    explicit nsEditableTextFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
    virtual bool operator()(nsINode* aNode) const
    {
      if (nsEditor::IsTextNode(aNode) && mHTMLEditor->IsEditable(aNode))
      {
        return true;
      }
      return false;
    }
  protected:
    nsHTMLEditor* mHTMLEditor;
};


/********************************************************
 *  Constructor/Destructor
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules()
  : mHTMLEditor(nullptr)
  , mListenerEnabled(false)
  , mReturnInEmptyLIKillsList(false)
  , mDidDeleteSelection(false)
  , mDidRangedDelete(false)
  , mRestoreContentEditableCount(false)
  , mJoinOffset(0)
{
  InitFields();
}

void
nsHTMLEditRules::InitFields()
{
  mHTMLEditor = nullptr;
  mDocChangeRange = nullptr;
  mListenerEnabled = true;
  mReturnInEmptyLIKillsList = true;
  mDidDeleteSelection = false;
  mDidRangedDelete = false;
  mRestoreContentEditableCount = false;
  mUtilRange = nullptr;
  mJoinOffset = 0;
  mNewBlock = nullptr;
  mRangeItem = new nsRangeStore();
  // populate mCachedStyles
  mCachedStyles[0] = StyleCache(nsGkAtoms::b, EmptyString(), EmptyString());
  mCachedStyles[1] = StyleCache(nsGkAtoms::i, EmptyString(), EmptyString());
  mCachedStyles[2] = StyleCache(nsGkAtoms::u, EmptyString(), EmptyString());
  mCachedStyles[3] = StyleCache(nsGkAtoms::font, NS_LITERAL_STRING("face"), EmptyString());
  mCachedStyles[4] = StyleCache(nsGkAtoms::font, NS_LITERAL_STRING("size"), EmptyString());
  mCachedStyles[5] = StyleCache(nsGkAtoms::font, NS_LITERAL_STRING("color"), EmptyString());
  mCachedStyles[6] = StyleCache(nsGkAtoms::tt, EmptyString(), EmptyString());
  mCachedStyles[7] = StyleCache(nsGkAtoms::em, EmptyString(), EmptyString());
  mCachedStyles[8] = StyleCache(nsGkAtoms::strong, EmptyString(), EmptyString());
  mCachedStyles[9] = StyleCache(nsGkAtoms::dfn, EmptyString(), EmptyString());
  mCachedStyles[10] = StyleCache(nsGkAtoms::code, EmptyString(), EmptyString());
  mCachedStyles[11] = StyleCache(nsGkAtoms::samp, EmptyString(), EmptyString());
  mCachedStyles[12] = StyleCache(nsGkAtoms::var, EmptyString(), EmptyString());
  mCachedStyles[13] = StyleCache(nsGkAtoms::cite, EmptyString(), EmptyString());
  mCachedStyles[14] = StyleCache(nsGkAtoms::abbr, EmptyString(), EmptyString());
  mCachedStyles[15] = StyleCache(nsGkAtoms::acronym, EmptyString(), EmptyString());
  mCachedStyles[16] = StyleCache(nsGkAtoms::backgroundColor, EmptyString(), EmptyString());
  mCachedStyles[17] = StyleCache(nsGkAtoms::sub, EmptyString(), EmptyString());
  mCachedStyles[18] = StyleCache(nsGkAtoms::sup, EmptyString(), EmptyString());
}

nsHTMLEditRules::~nsHTMLEditRules()
{
  // remove ourselves as a listener to edit actions
  // In some cases, we have already been removed by
  // ~nsHTMLEditor, in which case we will get a null pointer here
  // which we ignore.  But this allows us to add the ability to
  // switch rule sets on the fly if we want.
  if (mHTMLEditor)
    mHTMLEditor->RemoveEditActionListener(this);
}

/********************************************************
 *  XPCOM Cruft
 ********************************************************/

NS_IMPL_ADDREF_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLEditRules)
  NS_INTERFACE_TABLE_INHERITED(nsHTMLEditRules, nsIEditActionListener)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsTextEditRules)

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsHTMLEditRules, nsTextEditRules,
                                   mDocChangeRange, mUtilRange, mNewBlock,
                                   mRangeItem)

/********************************************************
 *  Public methods
 ********************************************************/

NS_IMETHODIMP
nsHTMLEditRules::Init(nsPlaintextEditor *aEditor)
{
  InitFields();

  mHTMLEditor = static_cast<nsHTMLEditor*>(aEditor);
  nsresult res;

  // call through to base class Init
  res = nsTextEditRules::Init(aEditor);
  NS_ENSURE_SUCCESS(res, res);

  // cache any prefs we care about
  static const char kPrefName[] =
    "editor.html.typing.returnInEmptyListItemClosesList";
  nsAdoptingCString returnInEmptyLIKillsList =
    Preferences::GetCString(kPrefName);

  // only when "false", becomes FALSE.  Otherwise (including empty), TRUE.
  // XXX Why was this pref designed as a string and not bool?
  mReturnInEmptyLIKillsList = !returnInEmptyLIKillsList.EqualsLiteral("false");

  // make a utility range for use by the listenter
  nsCOMPtr<nsINode> node = mHTMLEditor->GetRoot();
  if (!node) {
    node = mHTMLEditor->GetDocument();
  }

  NS_ENSURE_STATE(node);

  mUtilRange = new nsRange(node);

  // set up mDocChangeRange to be whole doc
  // temporarily turn off rules sniffing
  nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
  if (!mDocChangeRange) {
    mDocChangeRange = new nsRange(node);
  }

  if (node->IsElement()) {
    ErrorResult rv;
    mDocChangeRange->SelectNode(*node, rv);
    NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
    AdjustSpecialBreaks();
  }

  // add ourselves as a listener to edit actions
  res = mHTMLEditor->AddEditActionListener(this);

  return res;
}

NS_IMETHODIMP
nsHTMLEditRules::DetachEditor()
{
  if (mHTMLEditor) {
    mHTMLEditor->RemoveEditActionListener(this);
  }
  mHTMLEditor = nullptr;
  return nsTextEditRules::DetachEditor();
}

NS_IMETHODIMP
nsHTMLEditRules::BeforeEdit(EditAction action,
                            nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  nsAutoLockRulesSniffing lockIt(this);
  mDidExplicitlySetInterline = false;

  if (!mActionNesting) {
    mActionNesting++;

    // Clear our flag about if just deleted a range
    mDidRangedDelete = false;

    // Remember where our selection was before edit action took place:

    // Get selection
    RefPtr<Selection> selection = mHTMLEditor->GetSelection();

    // Get the selection location
    if (!selection->RangeCount()) {
      return NS_ERROR_UNEXPECTED;
    }
    mRangeItem->startNode = selection->GetRangeAt(0)->GetStartParent();
    mRangeItem->startOffset = selection->GetRangeAt(0)->StartOffset();
    mRangeItem->endNode = selection->GetRangeAt(0)->GetEndParent();
    mRangeItem->endOffset = selection->GetRangeAt(0)->EndOffset();
    nsCOMPtr<nsINode> selStartNode = mRangeItem->startNode;
    nsCOMPtr<nsINode> selEndNode = mRangeItem->endNode;

    // Register with range updater to track this as we perturb the doc
    (mHTMLEditor->mRangeUpdater).RegisterRangeItem(mRangeItem);

    // Clear deletion state bool
    mDidDeleteSelection = false;

    // Clear out mDocChangeRange and mUtilRange
    if (mDocChangeRange) {
      // Clear out our accounting of what changed
      mDocChangeRange->Reset();
    }
    if (mUtilRange) {
      // Ditto for mUtilRange.
      mUtilRange->Reset();
    }

    // Remember current inline styles for deletion and normal insertion ops
    if (action == EditAction::insertText ||
        action == EditAction::insertIMEText ||
        action == EditAction::deleteSelection ||
        IsStyleCachePreservingAction(action)) {
      nsCOMPtr<nsINode> selNode =
        aDirection == nsIEditor::eNext ? selEndNode : selStartNode;
      nsresult rv = CacheInlineStyles(GetAsDOMNode(selNode));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Stabilize the document against contenteditable count changes
    nsCOMPtr<nsIDOMDocument> doc = mHTMLEditor->GetDOMDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
    NS_ENSURE_TRUE(htmlDoc, NS_ERROR_FAILURE);
    if (htmlDoc->GetEditingState() == nsIHTMLDocument::eContentEditable) {
      htmlDoc->ChangeContentEditableCount(nullptr, +1);
      mRestoreContentEditableCount = true;
    }

    // Check that selection is in subtree defined by body node
    ConfirmSelectionInBody();
    // Let rules remember the top level action
    mTheAction = action;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::AfterEdit(EditAction action,
                           nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  nsAutoLockRulesSniffing lockIt(this);

  MOZ_ASSERT(mActionNesting > 0);
  nsresult rv = NS_OK;
  mActionNesting--;
  if (!mActionNesting) {
    // Do all the tricky stuff
    rv = AfterEditInner(action, aDirection);

    // Free up selectionState range item
    (mHTMLEditor->mRangeUpdater).DropRangeItem(mRangeItem);

    // Reset the contenteditable count to its previous value
    if (mRestoreContentEditableCount) {
      nsCOMPtr<nsIDOMDocument> doc = mHTMLEditor->GetDOMDocument();
      NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);
      nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
      NS_ENSURE_TRUE(htmlDoc, NS_ERROR_FAILURE);
      if (htmlDoc->GetEditingState() == nsIHTMLDocument::eContentEditable) {
        htmlDoc->ChangeContentEditableCount(nullptr, -1);
      }
      mRestoreContentEditableCount = false;
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsHTMLEditRules::AfterEditInner(EditAction action,
                                nsIEditor::EDirection aDirection)
{
  ConfirmSelectionInBody();
  if (action == EditAction::ignore) return NS_OK;

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  nsCOMPtr<nsIDOMNode> rangeStartParent, rangeEndParent;
  int32_t rangeStartOffset = 0, rangeEndOffset = 0;
  // do we have a real range to act on?
  bool bDamagedRange = false;
  if (mDocChangeRange)
  {
    mDocChangeRange->GetStartContainer(getter_AddRefs(rangeStartParent));
    mDocChangeRange->GetEndContainer(getter_AddRefs(rangeEndParent));
    mDocChangeRange->GetStartOffset(&rangeStartOffset);
    mDocChangeRange->GetEndOffset(&rangeEndOffset);
    if (rangeStartParent && rangeEndParent)
      bDamagedRange = true;
  }

  nsresult res;
  if (bDamagedRange && !((action == EditAction::undo) || (action == EditAction::redo)))
  {
    // don't let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);

    // expand the "changed doc range" as needed
    PromoteRange(*mDocChangeRange, action);

    // if we did a ranged deletion or handling backspace key, make sure we have
    // a place to put caret.
    // Note we only want to do this if the overall operation was deletion,
    // not if deletion was done along the way for EditAction::loadHTML, EditAction::insertText, etc.
    // That's why this is here rather than DidDeleteSelection().
    if ((action == EditAction::deleteSelection) && mDidRangedDelete)
    {
      res = InsertBRIfNeeded(selection);
      NS_ENSURE_SUCCESS(res, res);
    }

    // add in any needed <br>s, and remove any unneeded ones.
    AdjustSpecialBreaks();

    // merge any adjacent text nodes
    if ( (action != EditAction::insertText &&
         action != EditAction::insertIMEText) )
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->CollapseAdjacentTextNodes(mDocChangeRange);
      NS_ENSURE_SUCCESS(res, res);
    }

    // clean up any empty nodes in the selection
    res = RemoveEmptyNodes();
    NS_ENSURE_SUCCESS(res, res);

    // attempt to transform any unneeded nbsp's into spaces after doing various operations
    if ((action == EditAction::insertText) ||
        (action == EditAction::insertIMEText) ||
        (action == EditAction::deleteSelection) ||
        (action == EditAction::insertBreak) ||
        (action == EditAction::htmlPaste ||
        (action == EditAction::loadHTML)))
    {
      res = AdjustWhitespace(selection);
      NS_ENSURE_SUCCESS(res, res);

      // also do this for original selection endpoints.
      NS_ENSURE_STATE(mHTMLEditor);
      NS_ENSURE_STATE(mRangeItem->startNode);
      NS_ENSURE_STATE(mRangeItem->endNode);
      nsWSRunObject(mHTMLEditor, mRangeItem->startNode,
                    mRangeItem->startOffset).AdjustWhitespace();
      // we only need to handle old selection endpoint if it was different from start
      if (mRangeItem->startNode != mRangeItem->endNode ||
          mRangeItem->startOffset != mRangeItem->endOffset) {
        NS_ENSURE_STATE(mHTMLEditor);
        nsWSRunObject(mHTMLEditor, mRangeItem->endNode,
                      mRangeItem->endOffset).AdjustWhitespace();
      }
    }

    // if we created a new block, make sure selection lands in it
    if (mNewBlock)
    {
      res = PinSelectionToNewBlock(selection);
      mNewBlock = 0;
    }

    // adjust selection for insert text, html paste, and delete actions
    if ((action == EditAction::insertText) ||
        (action == EditAction::insertIMEText) ||
        (action == EditAction::deleteSelection) ||
        (action == EditAction::insertBreak) ||
        (action == EditAction::htmlPaste ||
        (action == EditAction::loadHTML)))
    {
      res = AdjustSelection(selection, aDirection);
      NS_ENSURE_SUCCESS(res, res);
    }

    // check for any styles which were removed inappropriately
    if (action == EditAction::insertText ||
        action == EditAction::insertIMEText ||
        action == EditAction::deleteSelection ||
        IsStyleCachePreservingAction(action)) {
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mTypeInState->UpdateSelState(selection);
      res = ReapplyCachedStyles();
      NS_ENSURE_SUCCESS(res, res);
      ClearCachedStyles();
    }
  }

  NS_ENSURE_STATE(mHTMLEditor);

  res = mHTMLEditor->HandleInlineSpellCheck(action, selection,
                                            GetAsDOMNode(mRangeItem->startNode),
                                            mRangeItem->startOffset,
                                            rangeStartParent, rangeStartOffset,
                                            rangeEndParent, rangeEndOffset);
  NS_ENSURE_SUCCESS(res, res);

  // detect empty doc
  res = CreateBogusNodeIfNeeded(selection);

  // adjust selection HINT if needed
  NS_ENSURE_SUCCESS(res, res);

  if (!mDidExplicitlySetInterline)
  {
    CheckInterlinePosition(*selection);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::WillDoAction(Selection* aSelection,
                              nsRulesInfo* aInfo,
                              bool* aCancel,
                              bool* aHandled)
{
  MOZ_ASSERT(aInfo && aCancel && aHandled);

  *aCancel = false;
  *aHandled = false;

  // my kingdom for dynamic cast
  nsTextRulesInfo *info = static_cast<nsTextRulesInfo*>(aInfo);

  // Deal with actions for which we don't need to check whether the selection is
  // editable.
  if (info->action == EditAction::outputText ||
      info->action == EditAction::undo ||
      info->action == EditAction::redo) {
    return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel, aHandled);
  }

  // Nothing to do if there's no selection to act on
  if (!aSelection) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(aSelection->RangeCount(), NS_OK);

  RefPtr<nsRange> range = aSelection->GetRangeAt(0);
  nsCOMPtr<nsINode> selStartNode = range->GetStartParent();

  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsModifiableNode(selStartNode)) {
    *aCancel = true;
    return NS_OK;
  }

  nsCOMPtr<nsINode> selEndNode = range->GetEndParent();

  if (selStartNode != selEndNode) {
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsModifiableNode(selEndNode)) {
      *aCancel = true;
      return NS_OK;
    }

    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsModifiableNode(range->GetCommonAncestor())) {
      *aCancel = true;
      return NS_OK;
    }
  }

  switch (info->action) {
    case EditAction::insertText:
    case EditAction::insertIMEText:
      UndefineCaretBidiLevel(aSelection);
      return WillInsertText(info->action, aSelection, aCancel, aHandled,
                            info->inString, info->outString, info->maxLength);
    case EditAction::loadHTML:
      return WillLoadHTML(aSelection, aCancel);
    case EditAction::insertBreak:
      UndefineCaretBidiLevel(aSelection);
      return WillInsertBreak(*aSelection, aCancel, aHandled);
    case EditAction::deleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction,
                                 info->stripWrappers, aCancel, aHandled);
    case EditAction::makeList:
      return WillMakeList(aSelection, info->blockType, info->entireList,
                          info->bulletType, aCancel, aHandled);
    case EditAction::indent:
      return WillIndent(aSelection, aCancel, aHandled);
    case EditAction::outdent:
      return WillOutdent(*aSelection, aCancel, aHandled);
    case EditAction::setAbsolutePosition:
      return WillAbsolutePosition(*aSelection, aCancel, aHandled);
    case EditAction::removeAbsolutePosition:
      return WillRemoveAbsolutePosition(aSelection, aCancel, aHandled);
    case EditAction::align:
      return WillAlign(*aSelection, *info->alignType, aCancel, aHandled);
    case EditAction::makeBasicBlock:
      return WillMakeBasicBlock(*aSelection, *info->blockType, aCancel,
                                aHandled);
    case EditAction::removeList:
      return WillRemoveList(aSelection, info->bOrdered, aCancel, aHandled);
    case EditAction::makeDefListItem:
      return WillMakeDefListItem(aSelection, info->blockType, info->entireList,
                                 aCancel, aHandled);
    case EditAction::insertElement:
      WillInsert(*aSelection, aCancel);
      return NS_OK;
    case EditAction::decreaseZIndex:
      return WillRelativeChangeZIndex(aSelection, -1, aCancel, aHandled);
    case EditAction::increaseZIndex:
      return WillRelativeChangeZIndex(aSelection, 1, aCancel, aHandled);
    default:
      return nsTextEditRules::WillDoAction(aSelection, aInfo,
                                           aCancel, aHandled);
  }
}


NS_IMETHODIMP
nsHTMLEditRules::DidDoAction(Selection* aSelection, nsRulesInfo* aInfo,
                             nsresult aResult)
{
  nsTextRulesInfo *info = static_cast<nsTextRulesInfo*>(aInfo);
  switch (info->action)
  {
    case EditAction::insertBreak:
      return DidInsertBreak(aSelection, aResult);
    case EditAction::deleteSelection:
      return DidDeleteSelection(aSelection, info->collapsedAction, aResult);
    case EditAction::makeBasicBlock:
    case EditAction::indent:
    case EditAction::outdent:
    case EditAction::align:
      return DidMakeBasicBlock(aSelection, aInfo, aResult);
    case EditAction::setAbsolutePosition: {
      nsresult rv = DidMakeBasicBlock(aSelection, aInfo, aResult);
      NS_ENSURE_SUCCESS(rv, rv);
      return DidAbsolutePosition();
    }
    default:
      // pass thru to nsTextEditRules
      return nsTextEditRules::DidDoAction(aSelection, aInfo, aResult);
  }
}

nsresult
nsHTMLEditRules::GetListState(bool *aMixed, bool *aOL, bool *aUL, bool *aDL)
{
  NS_ENSURE_TRUE(aMixed && aOL && aUL && aDL, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aOL = false;
  *aUL = false;
  *aDL = false;
  bool bNonList = false;

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult res = GetListActionNodes(arrayOfNodes, EntireList::no,
                                    TouchContent::no);
  NS_ENSURE_SUCCESS(res, res);

  // Examine list type for nodes in selection.
  for (const auto& curNode : arrayOfNodes) {
    if (!curNode->IsElement()) {
      bNonList = true;
    } else if (curNode->IsHTMLElement(nsGkAtoms::ul)) {
      *aUL = true;
    } else if (curNode->IsHTMLElement(nsGkAtoms::ol)) {
      *aOL = true;
    } else if (curNode->IsHTMLElement(nsGkAtoms::li)) {
      if (dom::Element* parent = curNode->GetParentElement()) {
        if (parent->IsHTMLElement(nsGkAtoms::ul)) {
          *aUL = true;
        } else if (parent->IsHTMLElement(nsGkAtoms::ol)) {
          *aOL = true;
        }
      }
    } else if (curNode->IsAnyOfHTMLElements(nsGkAtoms::dl,
                                            nsGkAtoms::dt,
                                            nsGkAtoms::dd)) {
      *aDL = true;
    } else {
      bNonList = true;
    }
  }

  // hokey arithmetic with booleans
  if ((*aUL + *aOL + *aDL + bNonList) > 1) {
    *aMixed = true;
  }

  return NS_OK;
}

nsresult
nsHTMLEditRules::GetListItemState(bool *aMixed, bool *aLI, bool *aDT, bool *aDD)
{
  NS_ENSURE_TRUE(aMixed && aLI && aDT && aDD, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aLI = false;
  *aDT = false;
  *aDD = false;
  bool bNonList = false;

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult res = GetListActionNodes(arrayOfNodes, EntireList::no,
                                    TouchContent::no);
  NS_ENSURE_SUCCESS(res, res);

  // examine list type for nodes in selection
  for (const auto& node : arrayOfNodes) {
    if (!node->IsElement()) {
      bNonList = true;
    } else if (node->IsAnyOfHTMLElements(nsGkAtoms::ul,
                                         nsGkAtoms::ol,
                                         nsGkAtoms::li)) {
      *aLI = true;
    } else if (node->IsHTMLElement(nsGkAtoms::dt)) {
      *aDT = true;
    } else if (node->IsHTMLElement(nsGkAtoms::dd)) {
      *aDD = true;
    } else if (node->IsHTMLElement(nsGkAtoms::dl)) {
      // need to look inside dl and see which types of items it has
      bool bDT, bDD;
      GetDefinitionListItemTypes(node->AsElement(), &bDT, &bDD);
      *aDT |= bDT;
      *aDD |= bDD;
    } else {
      bNonList = true;
    }
  }

  // hokey arithmetic with booleans
  if ( (*aDT + *aDD + bNonList) > 1) *aMixed = true;

  return NS_OK;
}

nsresult
nsHTMLEditRules::GetAlignment(bool* aMixed, nsIHTMLEditor::EAlignment* aAlign)
{
  MOZ_ASSERT(aMixed && aAlign);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // For now, just return first alignment.  We'll lie about if it's mixed.
  // This is for efficiency given that our current ui doesn't care if it's
  // mixed.
  // cmanske: NOT TRUE! We would like to pay attention to mixed state in Format
  // | Align submenu!

  // This routine assumes that alignment is done ONLY via divs

  // Default alignment is left
  *aMixed = false;
  *aAlign = nsIHTMLEditor::eLeft;

  // Get selection
  NS_ENSURE_STATE(mHTMLEditor->GetSelection());
  OwningNonNull<Selection> selection = *mHTMLEditor->GetSelection();

  // Get selection location
  NS_ENSURE_TRUE(mHTMLEditor->GetRoot(), NS_ERROR_FAILURE);
  OwningNonNull<Element> root = *mHTMLEditor->GetRoot();

  int32_t rootOffset = root->GetParentNode() ?
                       root->GetParentNode()->IndexOf(root) : -1;

  NS_ENSURE_STATE(selection->GetRangeAt(0) &&
                  selection->GetRangeAt(0)->GetStartParent());
  OwningNonNull<nsINode> parent = *selection->GetRangeAt(0)->GetStartParent();
  int32_t offset = selection->GetRangeAt(0)->StartOffset();

  // Is the selection collapsed?
  nsCOMPtr<nsINode> nodeToExamine;
  if (selection->Collapsed() || parent->GetAsText()) {
    // If selection is collapsed, we want to look at 'parent' and its ancestors
    // for divs with alignment on them.  If we are in a text node, then that is
    // the node of interest.
    nodeToExamine = parent;
  } else if (parent->IsHTMLElement(nsGkAtoms::html) && offset == rootOffset) {
    // If we have selected the body, let's look at the first editable node
    nodeToExamine = mHTMLEditor->GetNextNode(parent, offset, true);
  } else {
    nsTArray<RefPtr<nsRange>> arrayOfRanges;
    GetPromotedRanges(selection, arrayOfRanges, EditAction::align);

    // Use these ranges to construct a list of nodes to act on.
    nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
    nsresult rv = GetNodesForOperation(arrayOfRanges, arrayOfNodes,
                                       EditAction::align, TouchContent::no);
    NS_ENSURE_SUCCESS(rv, rv);
    nodeToExamine = arrayOfNodes.SafeElementAt(0);
  }

  NS_ENSURE_TRUE(nodeToExamine, NS_ERROR_NULL_POINTER);

  NS_NAMED_LITERAL_STRING(typeAttrName, "align");
  nsCOMPtr<Element> blockParent = mHTMLEditor->GetBlock(*nodeToExamine);

  NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);

  if (mHTMLEditor->IsCSSEnabled() &&
      mHTMLEditor->mHTMLCSSUtils->IsCSSEditableProperty(blockParent, nullptr,
                                                        &typeAttrName)) {
    // We are in CSS mode and we know how to align this element with CSS
    nsAutoString value;
    // Let's get the value(s) of text-align or margin-left/margin-right
    mHTMLEditor->mHTMLCSSUtils->GetCSSEquivalentToHTMLInlineStyleSet(
        blockParent, nullptr, &typeAttrName, value, nsHTMLCSSUtils::eComputed);
    if (value.EqualsLiteral("center") ||
        value.EqualsLiteral("-moz-center") ||
        value.EqualsLiteral("auto auto")) {
      *aAlign = nsIHTMLEditor::eCenter;
      return NS_OK;
    }
    if (value.EqualsLiteral("right") ||
        value.EqualsLiteral("-moz-right") ||
        value.EqualsLiteral("auto 0px")) {
      *aAlign = nsIHTMLEditor::eRight;
      return NS_OK;
    }
    if (value.EqualsLiteral("justify")) {
      *aAlign = nsIHTMLEditor::eJustify;
      return NS_OK;
    }
    *aAlign = nsIHTMLEditor::eLeft;
    return NS_OK;
  }

  // Check up the ladder for divs with alignment
  bool isFirstNodeToExamine = true;
  for (; nodeToExamine; nodeToExamine = nodeToExamine->GetParentNode()) {
    if (!isFirstNodeToExamine &&
        nodeToExamine->IsHTMLElement(nsGkAtoms::table)) {
      // The node to examine is a table and this is not the first node we
      // examine; let's break here to materialize the 'inline-block' behaviour
      // of html tables regarding to text alignment
      return NS_OK;
    }
    if (nsHTMLEditUtils::SupportsAlignAttr(GetAsDOMNode(nodeToExamine))) {
      // Check for alignment
      nsAutoString typeAttrVal;
      nodeToExamine->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::align,
                                          typeAttrVal);
      ToLowerCase(typeAttrVal);
      if (!typeAttrVal.IsEmpty()) {
        if (typeAttrVal.EqualsLiteral("center")) {
          *aAlign = nsIHTMLEditor::eCenter;
        } else if (typeAttrVal.EqualsLiteral("right")) {
          *aAlign = nsIHTMLEditor::eRight;
        } else if (typeAttrVal.EqualsLiteral("justify")) {
          *aAlign = nsIHTMLEditor::eJustify;
        } else {
          *aAlign = nsIHTMLEditor::eLeft;
        }
        return NS_OK;
      }
    }
    isFirstNodeToExamine = false;
  }
  return NS_OK;
}

static nsIAtom& MarginPropertyAtomForIndent(nsHTMLCSSUtils& aHTMLCSSUtils,
                                            nsINode& aNode)
{
  nsAutoString direction;
  aHTMLCSSUtils.GetComputedProperty(aNode, *nsGkAtoms::direction, direction);
  return direction.EqualsLiteral("rtl") ?
    *nsGkAtoms::marginRight : *nsGkAtoms::marginLeft;
}

nsresult
nsHTMLEditRules::GetIndentState(bool *aCanIndent, bool *aCanOutdent)
{
  NS_ENSURE_TRUE(aCanIndent && aCanOutdent, NS_ERROR_FAILURE);
  *aCanIndent = true;
  *aCanOutdent = false;

  // get selection
  NS_ENSURE_STATE(mHTMLEditor && mHTMLEditor->GetSelection());
  OwningNonNull<Selection> selection = *mHTMLEditor->GetSelection();

  // contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult res = GetNodesFromSelection(*selection, EditAction::indent,
                                       arrayOfNodes, TouchContent::no);
  NS_ENSURE_SUCCESS(res, res);

  // examine nodes in selection for blockquotes or list elements;
  // these we can outdent.  Note that we return true for canOutdent
  // if *any* of the selection is outdentable, rather than all of it.
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();
  for (auto& curNode : Reversed(arrayOfNodes)) {
    if (nsHTMLEditUtils::IsNodeThatCanOutdent(GetAsDOMNode(curNode))) {
      *aCanOutdent = true;
      break;
    }
    else if (useCSS) {
      // we are in CSS mode, indentation is done using the margin-left (or margin-right) property
      NS_ENSURE_STATE(mHTMLEditor);
      nsIAtom& marginProperty =
        MarginPropertyAtomForIndent(*mHTMLEditor->mHTMLCSSUtils, curNode);
      nsAutoString value;
      // retrieve its specified value
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(*curNode,
                                                       marginProperty, value);
      float f;
      nsCOMPtr<nsIAtom> unit;
      // get its number part and its unit
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
      // if the number part is strictly positive, outdent is possible
      if (0 < f) {
        *aCanOutdent = true;
        break;
      }
    }
  }

  if (!*aCanOutdent)
  {
    // if we haven't found something to outdent yet, also check the parents
    // of selection endpoints.  We might have a blockquote or list item
    // in the parent hierarchy.

    // gather up info we need for test
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsIDOMNode> parent, tmp, root = do_QueryInterface(mHTMLEditor->GetRoot());
    NS_ENSURE_TRUE(root, NS_ERROR_NULL_POINTER);
    int32_t selOffset;
    NS_ENSURE_STATE(mHTMLEditor);
    RefPtr<Selection> selection = mHTMLEditor->GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    // test start parent hierarchy
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetStartNodeAndOffset(selection, getter_AddRefs(parent), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    while (parent && (parent!=root))
    {
      if (nsHTMLEditUtils::IsNodeThatCanOutdent(parent))
      {
        *aCanOutdent = true;
        break;
      }
      tmp=parent;
      tmp->GetParentNode(getter_AddRefs(parent));
    }

    // test end parent hierarchy
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetEndNodeAndOffset(selection, getter_AddRefs(parent), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    while (parent && (parent!=root))
    {
      if (nsHTMLEditUtils::IsNodeThatCanOutdent(parent))
      {
        *aCanOutdent = true;
        break;
      }
      tmp=parent;
      tmp->GetParentNode(getter_AddRefs(parent));
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::GetParagraphState(bool *aMixed, nsAString &outFormat)
{
  // This routine is *heavily* tied to our ui choices in the paragraph
  // style popup.  I can't see a way around that.
  NS_ENSURE_TRUE(aMixed, NS_ERROR_NULL_POINTER);
  *aMixed = true;
  outFormat.Truncate(0);

  bool bMixed = false;
  // using "x" as an uninitialized value, since "" is meaningful
  nsAutoString formatStr(NS_LITERAL_STRING("x"));

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult res = GetParagraphFormatNodes(arrayOfNodes, TouchContent::no);
  NS_ENSURE_SUCCESS(res, res);

  // post process list.  We need to replace any block nodes that are not format
  // nodes with their content.  This is so we only have to look "up" the hierarchy
  // to find format nodes, instead of both up and down.
  for (int32_t i = arrayOfNodes.Length() - 1; i >= 0; i--) {
    auto& curNode = arrayOfNodes[i];
    nsAutoString format;
    // if it is a known format node we have it easy
    if (IsBlockNode(curNode) && !nsHTMLEditUtils::IsFormatNode(curNode)) {
      // arrayOfNodes.RemoveObject(curNode);
      res = AppendInnerFormatNodes(arrayOfNodes, curNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // we might have an empty node list.  if so, find selection parent
  // and put that on the list
  if (!arrayOfNodes.Length()) {
    nsCOMPtr<nsINode> selNode;
    int32_t selOffset;
    NS_ENSURE_STATE(mHTMLEditor);
    RefPtr<Selection> selection = mHTMLEditor->GetSelection();
    NS_ENSURE_STATE(selection);
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetStartNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(selNode, NS_ERROR_NULL_POINTER);
    arrayOfNodes.AppendElement(*selNode);
  }

  // remember root node
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIDOMElement> rootElem = do_QueryInterface(mHTMLEditor->GetRoot());
  NS_ENSURE_TRUE(rootElem, NS_ERROR_NULL_POINTER);

  // loop through the nodes in selection and examine their paragraph format
  for (auto& curNode : Reversed(arrayOfNodes)) {
    nsAutoString format;
    // if it is a known format node we have it easy
    if (nsHTMLEditUtils::IsFormatNode(curNode)) {
      GetFormatString(GetAsDOMNode(curNode), format);
    } else if (IsBlockNode(curNode)) {
      // this is a div or some other non-format block.
      // we should ignore it.  Its children were appended to this list
      // by AppendInnerFormatNodes() call above.  We will get needed
      // info when we examine them instead.
      continue;
    }
    else
    {
      nsCOMPtr<nsIDOMNode> node, tmp = GetAsDOMNode(curNode);
      tmp->GetParentNode(getter_AddRefs(node));
      while (node)
      {
        if (node == rootElem)
        {
          format.Truncate(0);
          break;
        }
        else if (nsHTMLEditUtils::IsFormatNode(node))
        {
          GetFormatString(node, format);
          break;
        }
        // else keep looking up
        tmp = node;
        tmp->GetParentNode(getter_AddRefs(node));
      }
    }

    // if this is the first node, we've found, remember it as the format
    if (formatStr.EqualsLiteral("x"))
      formatStr = format;
    // else make sure it matches previously found format
    else if (format != formatStr)
    {
      bMixed = true;
      break;
    }
  }

  *aMixed = bMixed;
  outFormat = formatStr;
  return res;
}

nsresult
nsHTMLEditRules::AppendInnerFormatNodes(nsTArray<OwningNonNull<nsINode>>& aArray,
                                        nsINode* aNode)
{
  MOZ_ASSERT(aNode);

  // we only need to place any one inline inside this node onto
  // the list.  They are all the same for purposes of determining
  // paragraph style.  We use foundInline to track this as we are
  // going through the children in the loop below.
  bool foundInline = false;
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    bool isBlock = IsBlockNode(*child);
    bool isFormat = nsHTMLEditUtils::IsFormatNode(child);
    if (isBlock && !isFormat) {
      // if it's a div, etc, recurse
      AppendInnerFormatNodes(aArray, child);
    } else if (isFormat) {
      aArray.AppendElement(*child);
    } else if (!foundInline) {
      // if this is the first inline we've found, use it
      foundInline = true;
      aArray.AppendElement(*child);
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::GetFormatString(nsIDOMNode *aNode, nsAString &outFormat)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  if (nsHTMLEditUtils::IsFormatNode(aNode))
  {
    nsCOMPtr<nsIAtom> atom = nsEditor::GetTag(aNode);
    atom->ToString(outFormat);
  }
  else
    outFormat.Truncate();

  return NS_OK;
}

/********************************************************
 *  Protected rules methods
 ********************************************************/

void
nsHTMLEditRules::WillInsert(Selection& aSelection, bool* aCancel)
{
  MOZ_ASSERT(aCancel);

  nsTextEditRules::WillInsert(aSelection, aCancel);

  NS_ENSURE_TRUE_VOID(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // Adjust selection to prevent insertion after a moz-BR.  This next only
  // works for collapsed selections right now, because selection is a pain to
  // work with when not collapsed.  (no good way to extend start or end of
  // selection), so we ignore those types of selections.
  if (!aSelection.Collapsed()) {
    return;
  }

  // If we are after a mozBR in the same block, then move selection to be
  // before it
  NS_ENSURE_TRUE_VOID(aSelection.GetRangeAt(0) &&
                      aSelection.GetRangeAt(0)->GetStartParent());
  OwningNonNull<nsINode> selNode = *aSelection.GetRangeAt(0)->GetStartParent();
  int32_t selOffset = aSelection.GetRangeAt(0)->StartOffset();

  // Get prior node
  nsCOMPtr<nsIContent> priorNode = mHTMLEditor->GetPriorHTMLNode(selNode,
                                                                 selOffset);
  if (priorNode && nsTextEditUtils::IsMozBR(priorNode)) {
    nsCOMPtr<Element> block1 = mHTMLEditor->GetBlock(selNode);
    nsCOMPtr<Element> block2 = mHTMLEditor->GetBlockNodeParent(priorNode);

    if (block1 && block1 == block2) {
      // If we are here then the selection is right after a mozBR that is in
      // the same block as the selection.  We need to move the selection start
      // to be before the mozBR.
      selNode = priorNode->GetParentNode();
      selOffset = selNode->IndexOf(priorNode);
      nsresult res = aSelection.Collapse(selNode, selOffset);
      NS_ENSURE_SUCCESS_VOID(res);
    }
  }

  if (mDidDeleteSelection &&
      (mTheAction == EditAction::insertText ||
       mTheAction == EditAction::insertIMEText ||
       mTheAction == EditAction::deleteSelection)) {
    nsresult res = ReapplyCachedStyles();
    NS_ENSURE_SUCCESS_VOID(res);
  }
  // For most actions we want to clear the cached styles, but there are
  // exceptions
  if (!IsStyleCachePreservingAction(mTheAction)) {
    ClearCachedStyles();
  }
}

nsresult
nsHTMLEditRules::WillInsertText(EditAction aAction,
                                Selection*       aSelection,
                                bool            *aCancel,
                                bool            *aHandled,
                                const nsAString *inString,
                                nsAString       *outString,
                                int32_t          aMaxLength)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  if (inString->IsEmpty() && aAction != EditAction::insertIMEText) {
    // HACK: this is a fix for bug 19395
    // I can't outlaw all empty insertions
    // because IME transaction depend on them
    // There is more work to do to make the
    // world safe for IME.
    *aCancel = true;
    *aHandled = false;
    return NS_OK;
  }

  // initialize out param
  *aCancel = false;
  *aHandled = true;
  nsresult res;
  // If the selection isn't collapsed, delete it.  Don't delete existing inline
  // tags, because we're hopefully going to insert text (bug 787432).
  if (!aSelection->Collapsed()) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eNoStrip);
    NS_ENSURE_SUCCESS(res, res);
  }

  WillInsert(*aSelection, aCancel);
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;

  // we need to get the doc
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIDocument> doc = mHTMLEditor->GetDocument();
  NS_ENSURE_STATE(doc);

  // for every property that is set, insert a new inline style node
  res = CreateStyleForInsertText(*aSelection, *doc);
  NS_ENSURE_SUCCESS(res, res);

  // get the (collapsed) selection location
  NS_ENSURE_STATE(mHTMLEditor);
  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  nsCOMPtr<nsINode> selNode = aSelection->GetRangeAt(0)->GetStartParent();
  int32_t selOffset = aSelection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_STATE(selNode);

  // dont put text in places that can't have it
  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsTextNode(selNode) &&
      (!mHTMLEditor || !mHTMLEditor->CanContainTag(*selNode,
                                                   *nsGkAtoms::textTagName))) {
    return NS_ERROR_FAILURE;
  }

  if (aAction == EditAction::insertIMEText) {
    // Right now the nsWSRunObject code bails on empty strings, but IME needs
    // the InsertTextImpl() call to still happen since empty strings are meaningful there.
    NS_ENSURE_STATE(mHTMLEditor);
    // If there is one or more IME selections, its minimum offset should be
    // the insertion point.
    int32_t IMESelectionOffset =
      mHTMLEditor->GetIMESelectionStartOffsetIn(selNode);
    if (IMESelectionOffset >= 0) {
      selOffset = IMESelectionOffset;
    }
    if (inString->IsEmpty())
    {
      res = mHTMLEditor->InsertTextImpl(*inString, address_of(selNode),
                                        &selOffset, doc);
    }
    else
    {
      nsWSRunObject wsObj(mHTMLEditor, selNode, selOffset);
      res = wsObj.InsertText(*inString, address_of(selNode), &selOffset, doc);
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  else // aAction == kInsertText
  {
    // find where we are
    nsCOMPtr<nsINode> curNode = selNode;
    int32_t curOffset = selOffset;

    // is our text going to be PREformatted?
    // We remember this so that we know how to handle tabs.
    bool isPRE;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->IsPreformatted(GetAsDOMNode(selNode), &isPRE);
    NS_ENSURE_SUCCESS(res, res);

    // turn off the edit listener: we know how to
    // build the "doc changed range" ourselves, and it's
    // must faster to do it once here than to track all
    // the changes one at a time.
    nsAutoLockListener lockit(&mListenerEnabled);

    // don't spaz my selection in subtransactions
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString tString(*inString);
    const char16_t *unicodeBuf = tString.get();
    int32_t pos = 0;
    NS_NAMED_LITERAL_STRING(newlineStr, LFSTR);

    // for efficiency, break out the pre case separately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE || IsPlaintextEditor())
    {
      while (unicodeBuf && (pos != -1) && (pos < (int32_t)(*inString).Length()))
      {
        int32_t oldPos = pos;
        int32_t subStrLen;
        pos = tString.FindChar(nsCRT::LF, oldPos);

        if (pos != -1)
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);

        // is it a return?
        if (subStr.Equals(newlineStr))
        {
          NS_ENSURE_STATE(mHTMLEditor);
          nsCOMPtr<Element> br =
            mHTMLEditor->CreateBRImpl(address_of(curNode), &curOffset,
                                      nsIEditor::eNone);
          NS_ENSURE_STATE(br);
          pos++;
        }
        else
        {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->InsertTextImpl(subStr, address_of(curNode),
                                            &curOffset, doc);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
    else
    {
      NS_NAMED_LITERAL_STRING(tabStr, "\t");
      NS_NAMED_LITERAL_STRING(spacesStr, "    ");
      char specialChars[] = {TAB, nsCRT::LF, 0};
      while (unicodeBuf && (pos != -1) && (pos < (int32_t)inString->Length()))
      {
        int32_t oldPos = pos;
        int32_t subStrLen;
        pos = tString.FindCharInSet(specialChars, oldPos);

        if (pos != -1)
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        NS_ENSURE_STATE(mHTMLEditor);
        nsWSRunObject wsObj(mHTMLEditor, curNode, curOffset);

        // is it a tab?
        if (subStr.Equals(tabStr))
        {
          res =
            wsObj.InsertText(spacesStr, address_of(curNode), &curOffset, doc);
          NS_ENSURE_SUCCESS(res, res);
          pos++;
        }
        // is it a return?
        else if (subStr.Equals(newlineStr))
        {
          nsCOMPtr<Element> br = wsObj.InsertBreak(address_of(curNode),
                                                   &curOffset,
                                                   nsIEditor::eNone);
          NS_ENSURE_TRUE(br, NS_ERROR_FAILURE);
          pos++;
        }
        else
        {
          res = wsObj.InsertText(subStr, address_of(curNode), &curOffset, doc);
          NS_ENSURE_SUCCESS(res, res);
        }
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    aSelection->SetInterlinePosition(false);
    if (curNode) aSelection->Collapse(curNode, curOffset);
    // manually update the doc changed range so that AfterEdit will clean up
    // the correct portion of the document.
    if (!mDocChangeRange)
    {
      mDocChangeRange = new nsRange(selNode);
    }
    res = mDocChangeRange->SetStart(selNode, selOffset);
    NS_ENSURE_SUCCESS(res, res);
    if (curNode)
      res = mDocChangeRange->SetEnd(curNode, curOffset);
    else
      res = mDocChangeRange->SetEnd(selNode, selOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  return res;
}

nsresult
nsHTMLEditRules::WillLoadHTML(Selection* aSelection, bool* aCancel)
{
  NS_ENSURE_TRUE(aSelection && aCancel, NS_ERROR_NULL_POINTER);

  *aCancel = false;

  // Delete mBogusNode if it exists. If we really need one,
  // it will be added during post-processing in AfterEditInner().

  if (mBogusNode)
  {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nullptr;
  }

  return NS_OK;
}

nsresult
nsHTMLEditRules::WillInsertBreak(Selection& aSelection, bool* aCancel,
                                 bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);
  *aCancel = false;
  *aHandled = false;

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // If the selection isn't collapsed, delete it.
  nsresult res;
  if (!aSelection.Collapsed()) {
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(res, res);
  }

  WillInsert(aSelection, aCancel);

  // Initialize out param.  We want to ignore result of WillInsert().
  *aCancel = false;

  // Split any mailcites in the way.  Should we abort this if we encounter
  // table cell boundaries?
  if (IsMailEditor()) {
    res = SplitMailCites(&aSelection, aHandled);
    NS_ENSURE_SUCCESS(res, res);
    if (*aHandled) {
      return NS_OK;
    }
  }

  // Smart splitting rules
  NS_ENSURE_TRUE(aSelection.GetRangeAt(0) &&
                 aSelection.GetRangeAt(0)->GetStartParent(),
                 NS_ERROR_FAILURE);
  OwningNonNull<nsINode> node = *aSelection.GetRangeAt(0)->GetStartParent();
  int32_t offset = aSelection.GetRangeAt(0)->StartOffset();

  // Do nothing if the node is read-only
  if (!mHTMLEditor->IsModifiableNode(node)) {
    *aCancel = true;
    return NS_OK;
  }

  // Identify the block
  nsCOMPtr<Element> blockParent = mHTMLEditor->GetBlock(node);
  NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);

  // If the active editing host is an inline element, or if the active editing
  // host is the block parent itself, just append a br.
  nsCOMPtr<Element> host = mHTMLEditor->GetActiveEditingHost();
  if (!nsEditorUtils::IsDescendantOf(blockParent, host)) {
    res = StandardBreakImpl(node, offset, aSelection);
    NS_ENSURE_SUCCESS(res, res);
    *aHandled = true;
    return NS_OK;
  }

  // If block is empty, populate with br.  (For example, imagine a div that
  // contains the word "text".  The user selects "text" and types return.
  // "Text" is deleted leaving an empty block.  We want to put in one br to
  // make block have a line.  Then code further below will put in a second br.)
  bool isEmpty;
  IsEmptyBlock(*blockParent, &isEmpty);
  if (isEmpty) {
    nsCOMPtr<Element> br = mHTMLEditor->CreateBR(blockParent,
                                                 blockParent->Length());
    NS_ENSURE_STATE(br);
  }

  nsCOMPtr<Element> listItem = IsInListItem(blockParent);
  if (listItem && listItem != host) {
    ReturnInListItem(aSelection, *listItem, node, offset);
    *aHandled = true;
    return NS_OK;
  } else if (nsHTMLEditUtils::IsHeader(*blockParent)) {
    // Headers: close (or split) header
    ReturnInHeader(aSelection, *blockParent, node, offset);
    *aHandled = true;
    return NS_OK;
  } else if (blockParent->IsHTMLElement(nsGkAtoms::p)) {
    // Paragraphs: special rules to look for <br>s
    res = ReturnInParagraph(&aSelection, GetAsDOMNode(blockParent),
                            GetAsDOMNode(node), offset, aCancel, aHandled);
    NS_ENSURE_SUCCESS(res, res);
    // Fall through, we may not have handled it in ReturnInParagraph()
  }

  // If not already handled then do the standard thing
  if (!(*aHandled)) {
    *aHandled = true;
    return StandardBreakImpl(node, offset, aSelection);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::StandardBreakImpl(nsINode& aNode, int32_t aOffset,
                                   Selection& aSelection)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  nsCOMPtr<Element> brNode;
  bool bAfterBlock = false;
  bool bBeforeBlock = false;
  nsCOMPtr<nsINode> node = &aNode;
  nsresult res;

  if (IsPlaintextEditor()) {
    brNode = mHTMLEditor->CreateBR(node, aOffset);
    NS_ENSURE_STATE(brNode);
  } else {
    nsWSRunObject wsObj(mHTMLEditor, node, aOffset);
    int32_t visOffset = 0;
    WSType wsType;
    nsCOMPtr<nsINode> visNode;
    wsObj.PriorVisibleNode(node, aOffset, address_of(visNode),
                           &visOffset, &wsType);
    if (wsType & WSType::block) {
      bAfterBlock = true;
    }
    wsObj.NextVisibleNode(node, aOffset, address_of(visNode),
                          &visOffset, &wsType);
    if (wsType & WSType::block) {
      bBeforeBlock = true;
    }
    nsCOMPtr<nsIDOMNode> linkDOMNode;
    if (mHTMLEditor->IsInLink(GetAsDOMNode(node), address_of(linkDOMNode))) {
      // Split the link
      nsCOMPtr<Element> linkNode = do_QueryInterface(linkDOMNode);
      NS_ENSURE_STATE(linkNode || !linkDOMNode);
      nsCOMPtr<nsINode> linkParent = linkNode->GetParentNode();
      aOffset = mHTMLEditor->SplitNodeDeep(*linkNode, *node->AsContent(),
                                           aOffset,
                                           nsHTMLEditor::EmptyContainers::no);
      NS_ENSURE_STATE(aOffset != -1);
      node = linkParent;
    }
    brNode = wsObj.InsertBreak(address_of(node), &aOffset, nsIEditor::eNone);
    NS_ENSURE_TRUE(brNode, NS_ERROR_FAILURE);
  }
  node = brNode->GetParentNode();
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  int32_t offset = node->IndexOf(brNode);
  if (bAfterBlock && bBeforeBlock) {
    // We just placed a br between block boundaries.  This is the one case
    // where we want the selection to be before the br we just placed, as the
    // br will be on a new line, rather than at end of prior line.
    aSelection.SetInterlinePosition(true);
    res = aSelection.Collapse(node, offset);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    nsWSRunObject wsObj(mHTMLEditor, node, offset + 1);
    nsCOMPtr<nsINode> secondBR;
    int32_t visOffset = 0;
    WSType wsType;
    wsObj.NextVisibleNode(node, offset + 1, address_of(secondBR),
                          &visOffset, &wsType);
    if (wsType == WSType::br) {
      // The next thing after the break we inserted is another break.  Move the
      // second break to be the first break's sibling.  This will prevent them
      // from being in different inline nodes, which would break
      // SetInterlinePosition().  It will also assure that if the user clicks
      // away and then clicks back on their new blank line, they will still get
      // the style from the line above.
      nsCOMPtr<nsINode> brParent = secondBR->GetParentNode();
      int32_t brOffset = brParent ? brParent->IndexOf(secondBR) : -1;
      if (brParent != node || brOffset != offset + 1) {
        res = mHTMLEditor->MoveNode(secondBR->AsContent(), node, offset + 1);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    // SetInterlinePosition(true) means we want the caret to stick to the
    // content on the "right".  We want the caret to stick to whatever is past
    // the break.  This is because the break is on the same line we were on,
    // but the next content will be on the following line.

    // An exception to this is if the break has a next sibling that is a block
    // node.  Then we stick to the left to avoid an uber caret.
    nsCOMPtr<nsIContent> siblingNode = brNode->GetNextSibling();
    if (siblingNode && IsBlockNode(*siblingNode)) {
      aSelection.SetInterlinePosition(false);
    } else {
      aSelection.SetInterlinePosition(true);
    }
    res = aSelection.Collapse(node, offset + 1);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::DidInsertBreak(Selection* aSelection, nsresult aResult)
{
  return NS_OK;
}


nsresult
nsHTMLEditRules::SplitMailCites(Selection* aSelection, bool* aHandled)
{
  NS_ENSURE_TRUE(aSelection && aHandled, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIContent> leftCite, rightCite;
  nsCOMPtr<nsINode> selNode;
  nsCOMPtr<Element> citeNode;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  citeNode = GetTopEnclosingMailCite(*selNode);
  NS_ENSURE_SUCCESS(res, res);
  if (citeNode)
  {
    // If our selection is just before a break, nudge it to be
    // just after it.  This does two things for us.  It saves us the trouble of having to add
    // a break here ourselves to preserve the "blockness" of the inline span mailquote
    // (in the inline case), and :
    // it means the break won't end up making an empty line that happens to be inside a
    // mailquote (in either inline or block case).
    // The latter can confuse a user if they click there and start typing,
    // because being in the mailquote may affect wrapping behavior, or font color, etc.
    NS_ENSURE_STATE(mHTMLEditor);
    nsWSRunObject wsObj(mHTMLEditor, selNode, selOffset);
    nsCOMPtr<nsINode> visNode;
    int32_t visOffset=0;
    WSType wsType;
    wsObj.NextVisibleNode(selNode, selOffset, address_of(visNode),
                          &visOffset, &wsType);
    if (wsType == WSType::br) {
      // ok, we are just before a break.  is it inside the mailquote?
      if (visNode != citeNode && citeNode->Contains(visNode)) {
        // it is.  so lets reset our selection to be just after it.
        NS_ENSURE_STATE(mHTMLEditor);
        selNode = mHTMLEditor->GetNodeLocation(visNode, &selOffset);
        ++selOffset;
      }
    }

    NS_ENSURE_STATE(mHTMLEditor);
    NS_ENSURE_STATE(selNode->IsContent());
    int32_t newOffset = mHTMLEditor->SplitNodeDeep(*citeNode,
        *selNode->AsContent(), selOffset, nsHTMLEditor::EmptyContainers::no,
        getter_AddRefs(leftCite), getter_AddRefs(rightCite));
    NS_ENSURE_STATE(newOffset != -1);
    selNode = citeNode->GetParentNode();
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> brNode = mHTMLEditor->CreateBR(selNode, newOffset);
    NS_ENSURE_STATE(brNode);
    // want selection before the break, and on same line
    aSelection->SetInterlinePosition(true);
    res = aSelection->Collapse(selNode, newOffset);
    NS_ENSURE_SUCCESS(res, res);
    // if citeNode wasn't a block, we might also want another break before it.
    // We need to examine the content both before the br we just added and also
    // just after it.  If we don't have another br or block boundary adjacent,
    // then we will need a 2nd br added to achieve blank line that user expects.
    if (IsInlineNode(*citeNode)) {
      NS_ENSURE_STATE(mHTMLEditor);
      nsWSRunObject wsObj(mHTMLEditor, selNode, newOffset);
      nsCOMPtr<nsINode> visNode;
      int32_t visOffset=0;
      WSType wsType;
      wsObj.PriorVisibleNode(selNode, newOffset, address_of(visNode),
                             &visOffset, &wsType);
      if (wsType == WSType::normalWS || wsType == WSType::text ||
          wsType == WSType::special) {
        NS_ENSURE_STATE(mHTMLEditor);
        nsWSRunObject wsObjAfterBR(mHTMLEditor, selNode, newOffset+1);
        wsObjAfterBR.NextVisibleNode(selNode, newOffset + 1,
                                     address_of(visNode), &visOffset, &wsType);
        if (wsType == WSType::normalWS || wsType == WSType::text ||
            wsType == WSType::special) {
          NS_ENSURE_STATE(mHTMLEditor);
          brNode = mHTMLEditor->CreateBR(selNode, newOffset);
          NS_ENSURE_STATE(brNode);
        }
      }
    }
    // delete any empty cites
    bool bEmptyCite = false;
    if (leftCite)
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->IsEmptyNode(leftCite, &bEmptyCite, true, false);
      if (NS_SUCCEEDED(res) && bEmptyCite) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(leftCite);
      }
      NS_ENSURE_SUCCESS(res, res);
    }
    if (rightCite)
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->IsEmptyNode(rightCite, &bEmptyCite, true, false);
      if (NS_SUCCEEDED(res) && bEmptyCite) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(rightCite);
      }
      NS_ENSURE_SUCCESS(res, res);
    }
    *aHandled = true;
  }
  return NS_OK;
}


nsresult
nsHTMLEditRules::WillDeleteSelection(Selection* aSelection,
                                     nsIEditor::EDirection aAction,
                                     nsIEditor::EStripWrappers aStripWrappers,
                                     bool* aCancel,
                                     bool* aHandled)
{
  MOZ_ASSERT(aStripWrappers == nsIEditor::eStrip ||
             aStripWrappers == nsIEditor::eNoStrip);

  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  // Initialize out params
  *aCancel = false;
  *aHandled = false;

  // Remember that we did a selection deletion.  Used by CreateStyleForInsertText()
  mDidDeleteSelection = true;

  // If there is only bogus content, cancel the operation
  if (mBogusNode) {
    *aCancel = true;
    return NS_OK;
  }

  // First check for table selection mode.  If so, hand off to table editor.
  nsCOMPtr<nsIDOMElement> cell;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetFirstSelectedCell(nullptr, getter_AddRefs(cell));
  if (NS_SUCCEEDED(res) && cell) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteTableCellContents();
    *aHandled = true;
    return res;
  }
  cell = nullptr;

  // origCollapsed is used later to determine whether we should join blocks. We
  // don't really care about bCollapsed because it will be modified by
  // ExtendSelectionForDelete later. JoinBlocks should happen if the original
  // selection is collapsed and the cursor is at the end of a block element, in
  // which case ExtendSelectionForDelete would always make the selection not
  // collapsed.
  bool bCollapsed = aSelection->Collapsed();
  bool join = false;
  bool origCollapsed = bCollapsed;

  nsCOMPtr<nsINode> selNode;
  int32_t selOffset;

  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  nsCOMPtr<nsINode> startNode = aSelection->GetRangeAt(0)->GetStartParent();
  int32_t startOffset = aSelection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);


  if (bCollapsed) {
    // If we are inside an empty block, delete it.
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> host = mHTMLEditor->GetActiveEditingHost();
    NS_ENSURE_TRUE(host, NS_ERROR_FAILURE);
    res = CheckForEmptyBlock(startNode, host, aSelection, aAction, aHandled);
    NS_ENSURE_SUCCESS(res, res);
    if (*aHandled) {
      return NS_OK;
    }

    // Test for distance between caret and text that will be deleted
    res = CheckBidiLevelForDeletion(aSelection, GetAsDOMNode(startNode),
                                    startOffset, aAction, aCancel);
    NS_ENSURE_SUCCESS(res, res);
    if (*aCancel) {
      return NS_OK;
    }

    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->ExtendSelectionForDelete(aSelection, &aAction);
    NS_ENSURE_SUCCESS(res, res);

    // We should delete nothing.
    if (aAction == nsIEditor::eNone) {
      return NS_OK;
    }

    // ExtendSelectionForDelete() may have changed the selection, update it
    NS_ENSURE_STATE(aSelection->GetRangeAt(0));
    startNode = aSelection->GetRangeAt(0)->GetStartParent();
    startOffset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

    bCollapsed = aSelection->Collapsed();
  }

  if (bCollapsed) {
    // What's in the direction we are deleting?
    NS_ENSURE_STATE(mHTMLEditor);
    nsWSRunObject wsObj(mHTMLEditor, startNode, startOffset);
    nsCOMPtr<nsINode> visNode;
    int32_t visOffset;
    WSType wsType;

    // Find next visible node
    if (aAction == nsIEditor::eNext) {
      wsObj.NextVisibleNode(startNode, startOffset, address_of(visNode),
                            &visOffset, &wsType);
    } else {
      wsObj.PriorVisibleNode(startNode, startOffset, address_of(visNode),
                             &visOffset, &wsType);
    }

    if (!visNode) {
      // Can't find anything to delete!
      *aCancel = true;
      return res;
    }

    if (wsType == WSType::normalWS) {
      // We found some visible ws to delete.  Let ws code handle it.
      if (aAction == nsIEditor::eNext) {
        res = wsObj.DeleteWSForward();
      } else {
        res = wsObj.DeleteWSBackward();
      }
      *aHandled = true;
      NS_ENSURE_SUCCESS(res, res);
      res = InsertBRIfNeeded(aSelection);
      return res;
    }

    if (wsType == WSType::text) {
      // Found normal text to delete.
      OwningNonNull<Text> nodeAsText = *visNode->GetAsText();
      int32_t so = visOffset;
      int32_t eo = visOffset + 1;
      if (aAction == nsIEditor::ePrevious) {
        if (so == 0) {
          return NS_ERROR_UNEXPECTED;
        }
        so--;
        eo--;
        // Bug 1068979: delete both codepoints if surrogate pair
        if (so > 0) {
          const nsTextFragment *text = nodeAsText->GetText();
          if (NS_IS_LOW_SURROGATE(text->CharAt(so)) &&
              NS_IS_HIGH_SURROGATE(text->CharAt(so - 1))) {
            so--;
          }
        }
      } else {
        RefPtr<nsRange> range = aSelection->GetRangeAt(0);
        NS_ENSURE_STATE(range);

        NS_ASSERTION(range->GetStartParent() == visNode,
                     "selection start not in visNode");
        NS_ASSERTION(range->GetEndParent() == visNode,
                     "selection end not in visNode");

        so = range->StartOffset();
        eo = range->EndOffset();
      }
      NS_ENSURE_STATE(mHTMLEditor);
      res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
          address_of(visNode), &so, address_of(visNode), &eo);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteText(nodeAsText, std::min(so, eo),
                                    DeprecatedAbs(eo - so));
      *aHandled = true;
      NS_ENSURE_SUCCESS(res, res);

      DeleteNodeIfCollapsedText(nodeAsText);

      res = InsertBRIfNeeded(aSelection);
      NS_ENSURE_SUCCESS(res, res);

      // Remember that we did a ranged delete for the benefit of
      // AfterEditInner().
      mDidRangedDelete = true;

      return NS_OK;
    }

    if (wsType == WSType::special || wsType == WSType::br ||
        visNode->IsHTMLElement(nsGkAtoms::hr)) {
      // Short circuit for invisible breaks.  delete them and recurse.
      if (visNode->IsHTMLElement(nsGkAtoms::br) &&
          (!mHTMLEditor || !mHTMLEditor->IsVisBreak(visNode))) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(visNode);
        NS_ENSURE_SUCCESS(res, res);
        return WillDeleteSelection(aSelection, aAction, aStripWrappers,
                                   aCancel, aHandled);
      }

      // Special handling for backspace when positioned after <hr>
      if (aAction == nsIEditor::ePrevious &&
          visNode->IsHTMLElement(nsGkAtoms::hr)) {
        // Only if the caret is positioned at the end-of-hr-line position, we
        // want to delete the <hr>.
        //
        // In other words, we only want to delete, if our selection position
        // (indicated by startNode and startOffset) is the position directly
        // after the <hr>, on the same line as the <hr>.
        //
        // To detect this case we check:
        // startNode == parentOfVisNode
        // and
        // startOffset -1 == visNodeOffsetToVisNodeParent
        // and
        // interline position is false (left)
        //
        // In any other case we set the position to startnode -1 and
        // interlineposition to false, only moving the caret to the
        // end-of-hr-line position.
        bool moveOnly = true;

        selNode = visNode->GetParentNode();
        selOffset = selNode ? selNode->IndexOf(visNode) : -1;

        bool interLineIsRight;
        res = aSelection->GetInterlinePosition(&interLineIsRight);
        NS_ENSURE_SUCCESS(res, res);

        if (startNode == selNode && startOffset - 1 == selOffset &&
            !interLineIsRight) {
          moveOnly = false;
        }

        if (moveOnly) {
          // Go to the position after the <hr>, but to the end of the <hr> line
          // by setting the interline position to left.
          ++selOffset;
          aSelection->Collapse(selNode, selOffset);
          aSelection->SetInterlinePosition(false);
          mDidExplicitlySetInterline = true;
          *aHandled = true;

          // There is one exception to the move only case.  If the <hr> is
          // followed by a <br> we want to delete the <br>.

          WSType otherWSType;
          nsCOMPtr<nsINode> otherNode;
          int32_t otherOffset;

          wsObj.NextVisibleNode(startNode, startOffset, address_of(otherNode),
                                &otherOffset, &otherWSType);

          if (otherWSType == WSType::br) {
            // Delete the <br>

            NS_ENSURE_STATE(mHTMLEditor);
            nsCOMPtr<nsIContent> otherContent(do_QueryInterface(otherNode));
            res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, otherContent);
            NS_ENSURE_SUCCESS(res, res);
            NS_ENSURE_STATE(mHTMLEditor);
            res = mHTMLEditor->DeleteNode(otherNode);
            NS_ENSURE_SUCCESS(res, res);
          }

          return NS_OK;
        }
        // Else continue with normal delete code
      }

      // Found break or image, or hr.
      NS_ENSURE_STATE(mHTMLEditor);
      NS_ENSURE_STATE(visNode->IsContent());
      res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor,
                                               visNode->AsContent());
      NS_ENSURE_SUCCESS(res, res);
      // Remember sibling to visnode, if any
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<nsIContent> sibling = mHTMLEditor->GetPriorHTMLSibling(visNode);
      // Delete the node, and join like nodes if appropriate
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(visNode);
      NS_ENSURE_SUCCESS(res, res);
      // We did something, so let's say so.
      *aHandled = true;
      // Is there a prior node and are they siblings?
      nsCOMPtr<nsINode> stepbrother;
      if (sibling) {
        NS_ENSURE_STATE(mHTMLEditor);
        stepbrother = mHTMLEditor->GetNextHTMLSibling(sibling);
      }
      // Are they both text nodes?  If so, join them!
      if (startNode == stepbrother && startNode->GetAsText() &&
          sibling->GetAsText()) {
        ::DOMPoint pt = JoinNodesSmart(*sibling, *startNode->AsContent());
        NS_ENSURE_STATE(pt.node);
        // Fix up selection
        res = aSelection->Collapse(pt.node, pt.offset);
        NS_ENSURE_SUCCESS(res, res);
      }
      res = InsertBRIfNeeded(aSelection);
      NS_ENSURE_SUCCESS(res, res);
      return NS_OK;
    }

    if (wsType == WSType::otherBlock) {
      // Make sure it's not a table element.  If so, cancel the operation
      // (translation: users cannot backspace or delete across table cells)
      if (nsHTMLEditUtils::IsTableElement(visNode)) {
        *aCancel = true;
        return NS_OK;
      }

      // Next to a block.  See if we are between a block and a br.  If so, we
      // really want to delete the br.  Else join content at selection to the
      // block.
      bool bDeletedBR = false;
      WSType otherWSType;
      nsCOMPtr<nsINode> otherNode;
      int32_t otherOffset;

      // Find node in other direction
      if (aAction == nsIEditor::eNext) {
        wsObj.PriorVisibleNode(startNode, startOffset, address_of(otherNode),
                               &otherOffset, &otherWSType);
      } else {
        wsObj.NextVisibleNode(startNode, startOffset, address_of(otherNode),
                              &otherOffset, &otherWSType);
      }

      // First find the adjacent node in the block
      nsCOMPtr<nsIContent> leafNode;
      nsCOMPtr<nsINode> leftNode, rightNode;
      if (aAction == nsIEditor::ePrevious) {
        NS_ENSURE_STATE(mHTMLEditor);
        leafNode = mHTMLEditor->GetLastEditableLeaf(*visNode);
        leftNode = leafNode;
        rightNode = startNode;
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        leafNode = mHTMLEditor->GetFirstEditableLeaf(*visNode);
        leftNode = startNode;
        rightNode = leafNode;
      }

      if (otherNode->IsHTMLElement(nsGkAtoms::br)) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(otherNode);
        NS_ENSURE_SUCCESS(res, res);
        *aHandled = true;
        bDeletedBR = true;
      }

      // Don't cross table boundaries
      if (leftNode && rightNode &&
          InDifferentTableElements(leftNode, rightNode)) {
        return NS_OK;
      }

      if (bDeletedBR) {
        // Put selection at edge of block and we are done.
        NS_ENSURE_STATE(leafNode);
        ::DOMPoint newSel = GetGoodSelPointForNode(*leafNode, aAction);
        NS_ENSURE_STATE(newSel.node);
        aSelection->Collapse(newSel.node, newSel.offset);
        return NS_OK;
      }

      // Else we are joining content to block

      nsCOMPtr<nsINode> selPointNode = startNode;
      int32_t selPointOffset = startOffset;
      {
        NS_ENSURE_STATE(mHTMLEditor);
        nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                    address_of(selPointNode), &selPointOffset);
        NS_ENSURE_STATE(leftNode && leftNode->IsContent() &&
                        rightNode && rightNode->IsContent());
        res = JoinBlocks(*leftNode->AsContent(), *rightNode->AsContent(),
                         aCancel);
        *aHandled = true;
        NS_ENSURE_SUCCESS(res, res);
      }
      aSelection->Collapse(selPointNode, selPointOffset);
      return NS_OK;
    }

    if (wsType == WSType::thisBlock) {
      // At edge of our block.  Look beside it and see if we can join to an
      // adjacent block

      // Make sure it's not a table element.  If so, cancel the operation
      // (translation: users cannot backspace or delete across table cells)
      if (nsHTMLEditUtils::IsTableElement(visNode)) {
        *aCancel = true;
        return NS_OK;
      }

      // First find the relevant nodes
      nsCOMPtr<nsINode> leftNode, rightNode;
      if (aAction == nsIEditor::ePrevious) {
        NS_ENSURE_STATE(mHTMLEditor);
        leftNode = mHTMLEditor->GetPriorHTMLNode(visNode);
        rightNode = startNode;
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        rightNode = mHTMLEditor->GetNextHTMLNode(visNode);
        leftNode = startNode;
      }

      // Nothing to join
      if (!leftNode || !rightNode) {
        *aCancel = true;
        return NS_OK;
      }

      // Don't cross table boundaries -- cancel it
      if (InDifferentTableElements(leftNode, rightNode)) {
        *aCancel = true;
        return NS_OK;
      }

      nsCOMPtr<nsINode> selPointNode = startNode;
      int32_t selPointOffset = startOffset;
      {
        NS_ENSURE_STATE(mHTMLEditor);
        nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                    address_of(selPointNode), &selPointOffset);
        NS_ENSURE_STATE(leftNode->IsContent() && rightNode->IsContent());
        res = JoinBlocks(*leftNode->AsContent(), *rightNode->AsContent(),
                         aCancel);
        *aHandled = true;
        NS_ENSURE_SUCCESS(res, res);
      }
      aSelection->Collapse(selPointNode, selPointOffset);
      return NS_OK;
    }
  }


  // Else we have a non-collapsed selection.  First adjust the selection.
  res = ExpandSelectionForDeletion(*aSelection);
  NS_ENSURE_SUCCESS(res, res);

  // Remember that we did a ranged delete for the benefit of AfterEditInner().
  mDidRangedDelete = true;

  // Refresh start and end points
  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  startNode = aSelection->GetRangeAt(0)->GetStartParent();
  startOffset = aSelection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  nsCOMPtr<nsINode> endNode = aSelection->GetRangeAt(0)->GetEndParent();
  int32_t endOffset = aSelection->GetRangeAt(0)->EndOffset();
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  // Figure out if the endpoints are in nodes that can be merged.  Adjust
  // surrounding whitespace in preparation to delete selection.
  if (!IsPlaintextEditor()) {
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                           address_of(startNode), &startOffset,
                                           address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(res, res);
  }

  {
    // Track location of where we are deleting
    NS_ENSURE_STATE(mHTMLEditor);
    nsAutoTrackDOMPoint startTracker(mHTMLEditor->mRangeUpdater,
                                     address_of(startNode), &startOffset);
    nsAutoTrackDOMPoint endTracker(mHTMLEditor->mRangeUpdater,
                                   address_of(endNode), &endOffset);
    // We are handling all ranged deletions directly now.
    *aHandled = true;

    if (endNode == startNode) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // Figure out mailcite ancestors
      nsCOMPtr<Element> startCiteNode = GetTopEnclosingMailCite(*startNode);
      nsCOMPtr<Element> endCiteNode = GetTopEnclosingMailCite(*endNode);

      // If we only have a mailcite at one of the two endpoints, set the
      // directionality of the deletion so that the selection will end up
      // outside the mailcite.
      if (startCiteNode && !endCiteNode) {
        aAction = nsIEditor::eNext;
      } else if (!startCiteNode && endCiteNode) {
        aAction = nsIEditor::ePrevious;
      }

      // Figure out block parents
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<Element> leftParent = mHTMLEditor->GetBlock(*startNode);
      nsCOMPtr<Element> rightParent = mHTMLEditor->GetBlock(*endNode);

      // Are endpoint block parents the same?  Use default deletion
      if (leftParent && leftParent == rightParent) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
      } else {
        // Deleting across blocks.  Are the blocks of same type?
        NS_ENSURE_STATE(leftParent && rightParent);

        // Are the blocks siblings?
        nsCOMPtr<nsINode> leftBlockParent = leftParent->GetParentNode();
        nsCOMPtr<nsINode> rightBlockParent = rightParent->GetParentNode();

        // MOOSE: this could conceivably screw up a table.. fix me.
        NS_ENSURE_STATE(mHTMLEditor);
        if (leftBlockParent == rightBlockParent &&
            mHTMLEditor->NodesSameType(GetAsDOMNode(leftParent),
                                       GetAsDOMNode(rightParent))) {
          if (leftParent->IsHTMLElement(nsGkAtoms::p)) {
            // First delete the selection
            NS_ENSURE_STATE(mHTMLEditor);
            res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
            NS_ENSURE_SUCCESS(res, res);
            // Then join paragraphs, insert break
            NS_ENSURE_STATE(mHTMLEditor);
            ::DOMPoint pt = mHTMLEditor->JoinNodeDeep(*leftParent, *rightParent);
            NS_ENSURE_STATE(pt.node);
            // Fix up selection
            res = aSelection->Collapse(pt.node, pt.offset);
            NS_ENSURE_SUCCESS(res, res);
            return NS_OK;
          }
          if (nsHTMLEditUtils::IsListItem(leftParent) ||
              nsHTMLEditUtils::IsHeader(*leftParent)) {
            // First delete the selection
            NS_ENSURE_STATE(mHTMLEditor);
            res = mHTMLEditor->DeleteSelectionImpl(aAction, aStripWrappers);
            NS_ENSURE_SUCCESS(res, res);
            // Join blocks
            NS_ENSURE_STATE(mHTMLEditor);
            ::DOMPoint pt = mHTMLEditor->JoinNodeDeep(*leftParent, *rightParent);
            NS_ENSURE_STATE(pt.node);
            // Fix up selection
            res = aSelection->Collapse(pt.node, pt.offset);
            NS_ENSURE_SUCCESS(res, res);
            return NS_OK;
          }
        }

        // Else blocks not same type, or not siblings.  Delete everything
        // except table elements.
        join = true;

        uint32_t rangeCount = aSelection->RangeCount();
        for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
          OwningNonNull<nsRange> range = *aSelection->GetRangeAt(rangeIdx);

          // Build a list of nodes in the range
          nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
          nsTrivialFunctor functor;
          nsDOMSubtreeIterator iter;
          nsresult res = iter.Init(*range);
          NS_ENSURE_SUCCESS(res, res);
          iter.AppendList(functor, arrayOfNodes);

          // Now that we have the list, delete non-table elements
          int32_t listCount = arrayOfNodes.Length();
          for (int32_t j = 0; j < listCount; j++) {
            nsCOMPtr<nsINode> somenode = do_QueryInterface(arrayOfNodes[0]);
            NS_ENSURE_STATE(somenode);
            DeleteNonTableElements(somenode);
            arrayOfNodes.RemoveElementAt(0);
            // If something visible is deleted, no need to join.  Visible means
            // all nodes except non-visible textnodes and breaks.
            if (join && origCollapsed) {
              if (!somenode->IsContent()) {
                join = false;
                continue;
              }
              nsCOMPtr<nsIContent> content = somenode->AsContent();
              if (content->NodeType() == nsIDOMNode::TEXT_NODE) {
                NS_ENSURE_STATE(mHTMLEditor);
                mHTMLEditor->IsVisTextNode(content, &join, true);
              } else {
                NS_ENSURE_STATE(mHTMLEditor);
                join = content->IsHTMLElement(nsGkAtoms::br) &&
                       !mHTMLEditor->IsVisBreak(somenode);
              }
            }
          }
        }

        // Check endpoints for possible text deletion.  We can assume that if
        // text node is found, we can delete to end or to begining as
        // appropriate, since the case where both sel endpoints in same text
        // node was already handled (we wouldn't be here)
        if (startNode->GetAsText() &&
            startNode->Length() > uint32_t(startOffset)) {
          // Delete to last character
          OwningNonNull<nsGenericDOMDataNode> dataNode =
            *static_cast<nsGenericDOMDataNode*>(startNode.get());
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->DeleteText(dataNode, startOffset,
              startNode->Length() - startOffset);
          NS_ENSURE_SUCCESS(res, res);
        }
        if (endNode->GetAsText() && endOffset) {
          // Delete to first character
          NS_ENSURE_STATE(mHTMLEditor);
          OwningNonNull<nsGenericDOMDataNode> dataNode =
            *static_cast<nsGenericDOMDataNode*>(endNode.get());
          res = mHTMLEditor->DeleteText(dataNode, 0, endOffset);
          NS_ENSURE_SUCCESS(res, res);
        }

        if (join) {
          res = JoinBlocks(*leftParent, *rightParent, aCancel);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
  }

  // We might have left only collapsed whitespace in the start/end nodes
  {
    nsAutoTrackDOMPoint startTracker(mHTMLEditor->mRangeUpdater,
                                     address_of(startNode), &startOffset);
    nsAutoTrackDOMPoint endTracker(mHTMLEditor->mRangeUpdater,
                                   address_of(endNode), &endOffset);

    DeleteNodeIfCollapsedText(*startNode);
    DeleteNodeIfCollapsedText(*endNode);
  }

  // If we're joining blocks: if deleting forward the selection should be
  // collapsed to the end of the selection, if deleting backward the selection
  // should be collapsed to the beginning of the selection. But if we're not
  // joining then the selection should collapse to the beginning of the
  // selection if we'redeleting forward, because the end of the selection will
  // still be in the next block. And same thing for deleting backwards
  // (selection should collapse to the end, because the beginning will still be
  // in the first block). See Bug 507936
  if (aAction == (join ? nsIEditor::eNext : nsIEditor::ePrevious)) {
    res = aSelection->Collapse(endNode, endOffset);
  } else {
    res = aSelection->Collapse(startNode, startOffset);
  }
  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

/**
 * If aNode is a text node that contains only collapsed whitespace, delete it.
 * It doesn't serve any useful purpose, and we don't want it to confuse code
 * that doesn't correctly skip over it.
 *
 * If deleting the node fails (like if it's not editable), the caller should
 * proceed as usual, so don't return any errors.
 */
void
nsHTMLEditRules::DeleteNodeIfCollapsedText(nsINode& aNode)
{
  if (!aNode.GetAsText()) {
    return;
  }
  bool empty;
  nsresult res = mHTMLEditor->IsVisTextNode(aNode.AsContent(), &empty, false);
  NS_ENSURE_SUCCESS_VOID(res);
  if (empty) {
    mHTMLEditor->DeleteNode(&aNode);
  }
}


/*****************************************************************************************************
*    InsertBRIfNeeded: determines if a br is needed for current selection to not be spastic.
*    If so, it inserts one.  Callers responsibility to only call with collapsed selection.
*         Selection* aSelection      the collapsed selection
*/
nsresult
nsHTMLEditRules::InsertBRIfNeeded(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);

  // get selection
  nsCOMPtr<nsINode> node;
  int32_t offset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(node), &offset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  // inline elements don't need any br
  if (!IsBlockNode(*node)) {
    return NS_OK;
  }

  // examine selection
  NS_ENSURE_STATE(mHTMLEditor);
  nsWSRunObject wsObj(mHTMLEditor, node, offset);
  if (((wsObj.mStartReason & WSType::block) ||
       (wsObj.mStartReason & WSType::br)) &&
      (wsObj.mEndReason & WSType::block)) {
    // if we are tucked between block boundaries then insert a br
    // first check that we are allowed to
    NS_ENSURE_STATE(mHTMLEditor);
    if (mHTMLEditor->CanContainTag(*node, *nsGkAtoms::br)) {
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<Element> br =
        mHTMLEditor->CreateBR(node, offset, nsIEditor::ePrevious);
      return br ? NS_OK : NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

/**
 *  GetGoodSelPointForNode: Finds where at a node you would want to set the
 *  selection if you were trying to have a caret next to it.
 *       nsINode& aNode                   the node
 *       nsIEditor::EDirection aAction    which edge to find: eNext indicates
 *                                        beginning, ePrevious ending
 */
::DOMPoint
nsHTMLEditRules::GetGoodSelPointForNode(nsINode& aNode,
                                        nsIEditor::EDirection aAction)
{
  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  if (aNode.GetAsText() || mHTMLEditor->IsContainer(&aNode)) {
    return ::DOMPoint(&aNode,
                      aAction == nsIEditor::ePrevious ? aNode.Length() : 0);
  }

  ::DOMPoint ret;
  ret.node = aNode.GetParentNode();
  ret.offset = ret.node ? ret.node->IndexOf(&aNode) : -1;
  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  if ((!aNode.IsHTMLElement(nsGkAtoms::br) ||
       mHTMLEditor->IsVisBreak(&aNode)) &&
      aAction == nsIEditor::ePrevious) {
    ret.offset++;
  }
  return ret;
}


/**
 * This method is used to join two block elements.  The right element is always
 * joined to the left element.  If the elements are the same type and not
 * nested within each other, JoinNodesSmart is called (example, joining two
 * list items together into one).  If the elements are not the same type, or
 * one is a descendant of the other, we instead destroy the right block placing
 * its children into leftblock.  DTD containment rules are followed throughout.
 */
nsresult
nsHTMLEditRules::JoinBlocks(nsIContent& aLeftNode, nsIContent& aRightNode,
                            bool* aCanceled)
{
  MOZ_ASSERT(aCanceled);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  nsCOMPtr<Element> leftBlock = mHTMLEditor->GetBlock(aLeftNode);
  nsCOMPtr<Element> rightBlock = mHTMLEditor->GetBlock(aRightNode);

  // Sanity checks
  NS_ENSURE_TRUE(leftBlock && rightBlock, NS_ERROR_NULL_POINTER);
  NS_ENSURE_STATE(leftBlock != rightBlock);

  if (nsHTMLEditUtils::IsTableElement(leftBlock) ||
      nsHTMLEditUtils::IsTableElement(rightBlock)) {
    // Do not try to merge table elements
    *aCanceled = true;
    return NS_OK;
  }

  // Make sure we don't try to move things into HR's, which look like blocks
  // but aren't containers
  if (leftBlock->IsHTMLElement(nsGkAtoms::hr)) {
    leftBlock = mHTMLEditor->GetBlockNodeParent(leftBlock);
  }
  if (rightBlock->IsHTMLElement(nsGkAtoms::hr)) {
    rightBlock = mHTMLEditor->GetBlockNodeParent(rightBlock);
  }
  NS_ENSURE_STATE(leftBlock && rightBlock);

  // Bail if both blocks the same
  if (leftBlock == rightBlock) {
    *aCanceled = true;
    return NS_OK;
  }

  // Joining a list item to its parent is a NOP.
  if (nsHTMLEditUtils::IsList(leftBlock) &&
      nsHTMLEditUtils::IsListItem(rightBlock) &&
      rightBlock->GetParentNode() == leftBlock) {
    return NS_OK;
  }

  // Special rule here: if we are trying to join list items, and they are in
  // different lists, join the lists instead.
  bool mergeLists = false;
  nsIAtom* existingList = nsGkAtoms::_empty;
  int32_t offset;
  nsCOMPtr<Element> leftList, rightList;
  if (nsHTMLEditUtils::IsListItem(leftBlock) &&
      nsHTMLEditUtils::IsListItem(rightBlock)) {
    leftList = leftBlock->GetParentElement();
    rightList = rightBlock->GetParentElement();
    if (leftList && rightList && leftList != rightList &&
        !nsEditorUtils::IsDescendantOf(leftList, rightBlock, &offset) &&
        !nsEditorUtils::IsDescendantOf(rightList, leftBlock, &offset)) {
      // There are some special complications if the lists are descendants of
      // the other lists' items.  Note that it is okay for them to be
      // descendants of the other lists themselves, which is the usual case for
      // sublists in our implementation.
      leftBlock = leftList;
      rightBlock = rightList;
      mergeLists = true;
      existingList = leftList->NodeInfo()->NameAtom();
    }
  }

  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);

  nsresult res = NS_OK;
  int32_t rightOffset = 0;
  int32_t leftOffset = -1;

  // offset below is where you find yourself in rightBlock when you traverse
  // upwards from leftBlock
  if (nsEditorUtils::IsDescendantOf(leftBlock, rightBlock, &rightOffset)) {
    // Tricky case.  Left block is inside right block.  Do ws adjustment.  This
    // just destroys non-visible ws at boundaries we will be joining.
    rightOffset++;
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor,
                                            nsWSRunObject::kBlockEnd,
                                            leftBlock);
    NS_ENSURE_SUCCESS(res, res);

    {
      // We can't just track rightBlock because it's an Element.
      nsCOMPtr<nsINode> trackingRightBlock(rightBlock);
      nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                  address_of(trackingRightBlock),
                                  &rightOffset);
      res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor,
                                              nsWSRunObject::kAfterBlock,
                                              rightBlock, rightOffset);
      NS_ENSURE_SUCCESS(res, res);
      if (trackingRightBlock->IsElement()) {
        rightBlock = trackingRightBlock->AsElement();
      } else {
        NS_ENSURE_STATE(trackingRightBlock->GetParentElement());
        rightBlock = trackingRightBlock->GetParentElement();
      }
    }
    // Do br adjustment.
    nsCOMPtr<Element> brNode =
      CheckForInvisibleBR(*leftBlock, BRLocation::blockEnd);
    if (mergeLists) {
      // The idea here is to take all children in rightList that are past
      // offset, and pull them into leftlist.
      for (nsCOMPtr<nsIContent> child = rightList->GetChildAt(offset);
           child; child = rightList->GetChildAt(rightOffset)) {
        res = mHTMLEditor->MoveNode(child, leftList, -1);
        NS_ENSURE_SUCCESS(res, res);
      }
    } else {
      res = MoveBlock(*leftBlock, *rightBlock, leftOffset, rightOffset);
    }
    if (brNode) {
      mHTMLEditor->DeleteNode(brNode);
    }
  // Offset below is where you find yourself in leftBlock when you traverse
  // upwards from rightBlock
  } else if (nsEditorUtils::IsDescendantOf(rightBlock, leftBlock,
                                           &leftOffset)) {
    // Tricky case.  Right block is inside left block.  Do ws adjustment.  This
    // just destroys non-visible ws at boundaries we will be joining.
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor,
                                            nsWSRunObject::kBlockStart,
                                            rightBlock);
    NS_ENSURE_SUCCESS(res, res);
    {
      // We can't just track leftBlock because it's an Element, so track
      // something else.
      nsCOMPtr<nsINode> trackingLeftBlock(leftBlock);
      nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                  address_of(trackingLeftBlock), &leftOffset);
      res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor,
                                              nsWSRunObject::kBeforeBlock,
                                              leftBlock, leftOffset);
      NS_ENSURE_SUCCESS(res, res);
      if (trackingLeftBlock->IsElement()) {
        leftBlock = trackingLeftBlock->AsElement();
      } else {
        NS_ENSURE_STATE(trackingLeftBlock->GetParentElement());
        leftBlock = trackingLeftBlock->GetParentElement();
      }
    }
    // Do br adjustment.
    nsCOMPtr<Element> brNode =
      CheckForInvisibleBR(*leftBlock, BRLocation::beforeBlock, leftOffset);
    if (mergeLists) {
      res = MoveContents(*rightList, *leftList, &leftOffset);
    } else {
      // Left block is a parent of right block, and the parent of the previous
      // visible content.  Right block is a child and contains the contents we
      // want to move.

      int32_t previousContentOffset;
      nsCOMPtr<nsINode> previousContentParent;

      if (&aLeftNode == leftBlock) {
        // We are working with valid HTML, aLeftNode is a block node, and is
        // therefore allowed to contain rightBlock.  This is the simple case,
        // we will simply move the content in rightBlock out of its block.
        previousContentParent = leftBlock;
        previousContentOffset = leftOffset;
      } else {
        // We try to work as well as possible with HTML that's already invalid.
        // Although "right block" is a block, and a block must not be contained
        // in inline elements, reality is that broken documents do exist.  The
        // DIRECT parent of "left NODE" might be an inline element.  Previous
        // versions of this code skipped inline parents until the first block
        // parent was found (and used "left block" as the destination).
        // However, in some situations this strategy moves the content to an
        // unexpected position.  (see bug 200416) The new idea is to make the
        // moving content a sibling, next to the previous visible content.

        previousContentParent = aLeftNode.GetParentNode();
        previousContentOffset = previousContentParent ?
          previousContentParent->IndexOf(&aLeftNode) : -1;

        // We want to move our content just after the previous visible node.
        previousContentOffset++;
      }

      // Because we don't want the moving content to receive the style of the
      // previous content, we split the previous content's style.

      nsCOMPtr<Element> editorRoot = mHTMLEditor->GetEditorRoot();
      if (!editorRoot || &aLeftNode != editorRoot) {
        nsCOMPtr<nsIContent> splittedPreviousContent;
        res = mHTMLEditor->SplitStyleAbovePoint(address_of(previousContentParent),
                                                &previousContentOffset,
                                                nullptr, nullptr, nullptr,
                                                getter_AddRefs(splittedPreviousContent));
        NS_ENSURE_SUCCESS(res, res);

        if (splittedPreviousContent) {
          previousContentParent = splittedPreviousContent->GetParentNode();
          previousContentOffset = previousContentParent ?
            previousContentParent->IndexOf(splittedPreviousContent) : -1;
        }
      }

      NS_ENSURE_TRUE(previousContentParent, NS_ERROR_NULL_POINTER);

      res = MoveBlock(*previousContentParent->AsElement(), *rightBlock,
          previousContentOffset, rightOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
    if (brNode) {
      mHTMLEditor->DeleteNode(brNode);
    }
  } else {
    // Normal case.  Blocks are siblings, or at least close enough.  An example
    // of the latter is <p>paragraph</p><ul><li>one<li>two<li>three</ul>.  The
    // first li and the p are not true siblings, but we still want to join them
    // if you backspace from li into p.

    // Adjust whitespace at block boundaries
    res = nsWSRunObject::PrepareToJoinBlocks(mHTMLEditor, leftBlock, rightBlock);
    NS_ENSURE_SUCCESS(res, res);
    // Do br adjustment.
    nsCOMPtr<Element> brNode =
      CheckForInvisibleBR(*leftBlock, BRLocation::blockEnd);
    if (mergeLists || leftBlock->NodeInfo()->NameAtom() ==
                      rightBlock->NodeInfo()->NameAtom()) {
      // Nodes are same type.  merge them.
      ::DOMPoint pt = JoinNodesSmart(*leftBlock, *rightBlock);
      if (pt.node && mergeLists) {
        nsCOMPtr<Element> newBlock;
        res = ConvertListType(rightBlock, getter_AddRefs(newBlock),
                              existingList, nsGkAtoms::li);
      }
    } else {
      // Nodes are dissimilar types.
      res = MoveBlock(*leftBlock, *rightBlock, leftOffset, rightOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
    if (brNode) {
      res = mHTMLEditor->DeleteNode(brNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}


/**
 * Moves the content from aRightBlock starting from aRightOffset into
 * aLeftBlock at aLeftOffset. Note that the "block" might merely be inline
 * nodes between <br>s, or between blocks, etc.  DTD containment rules are
 * followed throughout.
 */
nsresult
nsHTMLEditRules::MoveBlock(Element& aLeftBlock, Element& aRightBlock,
                           int32_t aLeftOffset, int32_t aRightOffset)
{
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  // GetNodesFromPoint is the workhorse that figures out what we wnat to move.
  nsresult res = GetNodesFromPoint(::DOMPoint(&aRightBlock, aRightOffset),
                                   EditAction::makeList, arrayOfNodes,
                                   TouchContent::yes);
  NS_ENSURE_SUCCESS(res, res);
  for (uint32_t i = 0; i < arrayOfNodes.Length(); i++) {
    // get the node to act on
    if (IsBlockNode(arrayOfNodes[i])) {
      // For block nodes, move their contents only, then delete block.
      res = MoveContents(*arrayOfNodes[i]->AsElement(), aLeftBlock,
                         &aLeftOffset);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(arrayOfNodes[i]);
    } else {
      // Otherwise move the content as is, checking against the DTD.
      res = MoveNodeSmart(*arrayOfNodes[i]->AsContent(), aLeftBlock,
                          &aLeftOffset);
    }
  }

  // XXX We're only checking return value of the last iteration
  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

/**
 * This method is used to move node aNode to (aDestElement, aInOutDestOffset).
 * DTD containment rules are followed throughout.  aInOutDestOffset is updated
 * to point _after_ inserted content.
 */
nsresult
nsHTMLEditRules::MoveNodeSmart(nsIContent& aNode, Element& aDestElement,
                               int32_t* aInOutDestOffset)
{
  MOZ_ASSERT(aInOutDestOffset);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);
  nsresult res;

  // Check if this node can go into the destination node
  if (mHTMLEditor->CanContain(aDestElement, aNode)) {
    // If it can, move it there
    res = mHTMLEditor->MoveNode(&aNode, &aDestElement, *aInOutDestOffset);
    NS_ENSURE_SUCCESS(res, res);
    if (*aInOutDestOffset != -1) {
      (*aInOutDestOffset)++;
    }
  } else {
    // If it can't, move its children (if any), and then delete it.
    if (aNode.IsElement()) {
      res = MoveContents(*aNode.AsElement(), aDestElement, aInOutDestOffset);
      NS_ENSURE_SUCCESS(res, res);
    }

    res = mHTMLEditor->DeleteNode(&aNode);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

/**
 * Moves the _contents_ of aElement to (aDestElement, aInOutDestOffset).  DTD
 * containment rules are followed throughout.  aInOutDestOffset is updated to
 * point _after_ inserted content.
 */
nsresult
nsHTMLEditRules::MoveContents(Element& aElement, Element& aDestElement,
                              int32_t* aInOutDestOffset)
{
  MOZ_ASSERT(aInOutDestOffset);

  NS_ENSURE_TRUE(&aElement != &aDestElement, NS_ERROR_ILLEGAL_VALUE);

  while (aElement.GetFirstChild()) {
    nsresult res = MoveNodeSmart(*aElement.GetFirstChild(), aDestElement,
                                 aInOutDestOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}


nsresult
nsHTMLEditRules::DeleteNonTableElements(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  if (!nsHTMLEditUtils::IsTableElementButNotTable(aNode)) {
    NS_ENSURE_STATE(mHTMLEditor);
    return mHTMLEditor->DeleteNode(aNode->AsDOMNode());
  }

  for (int32_t i = aNode->GetChildCount() - 1; i >= 0; --i) {
    nsresult rv = DeleteNonTableElements(aNode->GetChildAt(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::DidDeleteSelection(Selection* aSelection,
                                    nsIEditor::EDirection aDir,
                                    nsresult aResult)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }

  // find where we are
  nsCOMPtr<nsINode> startNode;
  int32_t startOffset;
  nsresult res = mEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(startNode), &startOffset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

  // find any enclosing mailcite
  nsCOMPtr<Element> citeNode = GetTopEnclosingMailCite(*startNode);
  if (citeNode) {
    bool isEmpty = true, seenBR = false;
    NS_ENSURE_STATE(mHTMLEditor);
    mHTMLEditor->IsEmptyNodeImpl(citeNode, &isEmpty, true, true, false,
                                 &seenBR);
    if (isEmpty)
    {
      int32_t offset;
      nsCOMPtr<nsINode> parent = nsEditor::GetNodeLocation(citeNode, &offset);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(citeNode);
      NS_ENSURE_SUCCESS(res, res);
      if (parent && seenBR)
      {
        NS_ENSURE_STATE(mHTMLEditor);
        nsCOMPtr<Element> brNode = mHTMLEditor->CreateBR(parent, offset);
        NS_ENSURE_STATE(brNode);
        aSelection->Collapse(parent, offset);
      }
    }
  }

  // call through to base class
  return nsTextEditRules::DidDeleteSelection(aSelection, aDir, aResult);
}

nsresult
nsHTMLEditRules::WillMakeList(Selection* aSelection,
                              const nsAString* aListType,
                              bool aEntireList,
                              const nsAString* aBulletType,
                              bool* aCancel,
                              bool* aHandled,
                              const nsAString* aItemType)
{
  if (!aSelection || !aListType || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  OwningNonNull<nsIAtom> listType = NS_Atomize(*aListType);

  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = false;

  // deduce what tag to use for list items
  nsCOMPtr<nsIAtom> itemType;
  if (aItemType) {
    itemType = NS_Atomize(*aItemType);
    NS_ENSURE_TRUE(itemType, NS_ERROR_OUT_OF_MEMORY);
  } else if (listType == nsGkAtoms::dl) {
    itemType = nsGkAtoms::dd;
  } else {
    itemType = nsGkAtoms::li;
  }

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range

  *aHandled = true;

  nsresult res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  res = GetListActionNodes(arrayOfNodes,
                           aEntireList ? EntireList::yes : EntireList::no);
  NS_ENSURE_SUCCESS(res, res);

  // check if all our nodes are <br>s, or empty inlines
  bool bOnlyBreaks = true;
  for (auto& curNode : arrayOfNodes) {
    // if curNode is not a Break or empty inline, we're done
    if (!nsTextEditUtils::IsBreak(curNode) &&
        !IsEmptyInline(curNode)) {
      bOnlyBreaks = false;
      break;
    }
  }

  // if no nodes, we make empty list.  Ditto if the user tried to make a list
  // of some # of breaks.
  if (!arrayOfNodes.Length() || bOnlyBreaks) {
    // if only breaks, delete them
    if (bOnlyBreaks) {
      for (auto& node : arrayOfNodes) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->DeleteNode(node);
        NS_ENSURE_SUCCESS(res, res);
      }
    }

    // get selection location
    NS_ENSURE_STATE(aSelection->RangeCount());
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);

    // make sure we can put a list here
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->CanContainTag(*parent, listType)) {
      *aCancel = true;
      return NS_OK;
    }
    res = SplitAsNeeded(listType, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theList =
      mHTMLEditor->CreateNode(listType, parent, offset);
    NS_ENSURE_STATE(theList);

    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theListItem =
      mHTMLEditor->CreateNode(itemType, theList, 0);
    NS_ENSURE_STATE(theListItem);

    // remember our new block for postprocessing
    mNewBlock = theListItem;
    // put selection in new list item
    res = aSelection->Collapse(theListItem, 0);
    // to prevent selection resetter from overriding us
    selectionResetter.Abort();
    *aHandled = true;
    return res;
  }

  // if there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.

  LookInsideDivBQandList(arrayOfNodes);

  // Ok, now go through all the nodes and put then in the list,
  // or whatever is approriate.  Wohoo!

  uint32_t listCount = arrayOfNodes.Length();
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<Element> curList, prevListItem;

  for (uint32_t i = 0; i < listCount; i++) {
    // here's where we actually figure out what to do
    nsCOMPtr<Element> newBlock;
    NS_ENSURE_STATE(arrayOfNodes[i]->IsContent());
    OwningNonNull<nsIContent> curNode = *arrayOfNodes[i]->AsContent();
    int32_t offset;
    curParent = nsEditor::GetNodeLocation(curNode, &offset);

    // make sure we don't assemble content that is in different table cells
    // into the same list.  respect table cell boundaries when listifying.
    if (curList && InDifferentTableElements(curList, curNode)) {
      curList = nullptr;
    }

    // if curNode is a Break, delete it, and quit remembering prev list item
    if (nsTextEditUtils::IsBreak(curNode)) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      prevListItem = 0;
      continue;
    } else if (IsEmptyInline(curNode)) {
      // if curNode is an empty inline container, delete it
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      continue;
    }

    if (nsHTMLEditUtils::IsList(curNode)) {
      // do we have a curList already?
      if (curList && !nsEditorUtils::IsDescendantOf(curNode, curList)) {
        // move all of our children into curList.  cheezy way to do it: move
        // whole list and then RemoveContainer() on the list.  ConvertListType
        // first: that routine handles converting the list item types, if
        // needed
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        res = ConvertListType(curNode->AsElement(), getter_AddRefs(newBlock),
                              listType, itemType);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RemoveBlockContainer(*newBlock);
        NS_ENSURE_SUCCESS(res, res);
      } else {
        // replace list with new list type
        res = ConvertListType(curNode->AsElement(), getter_AddRefs(newBlock),
                              listType, itemType);
        NS_ENSURE_SUCCESS(res, res);
        curList = newBlock;
      }
      prevListItem = 0;
      continue;
    }

    if (nsHTMLEditUtils::IsListItem(curNode)) {
      NS_ENSURE_STATE(mHTMLEditor);
      if (!curParent->IsHTMLElement(listType)) {
        // list item is in wrong type of list. if we don't have a curList,
        // split the old list and make a new list of correct type.
        if (!curList || nsEditorUtils::IsDescendantOf(curNode, curList)) {
          NS_ENSURE_STATE(mHTMLEditor);
          NS_ENSURE_STATE(curParent->IsContent());
          ErrorResult rv;
          nsCOMPtr<nsIContent> splitNode =
            mHTMLEditor->SplitNode(*curParent->AsContent(), offset, rv);
          NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
          newBlock = splitNode ? splitNode->AsElement() : nullptr;
          int32_t offset;
          nsCOMPtr<nsINode> parent = nsEditor::GetNodeLocation(curParent,
                                                               &offset);
          NS_ENSURE_STATE(mHTMLEditor);
          curList = mHTMLEditor->CreateNode(listType, parent, offset);
          NS_ENSURE_STATE(curList);
        }
        // move list item to new list
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // convert list item type if needed
        NS_ENSURE_STATE(mHTMLEditor);
        if (!curNode->IsHTMLElement(itemType)) {
          NS_ENSURE_STATE(mHTMLEditor);
          newBlock = mHTMLEditor->ReplaceContainer(curNode->AsElement(),
                                                   itemType);
          NS_ENSURE_STATE(newBlock);
        }
      } else {
        // item is in right type of list.  But we might still have to move it.
        // and we might need to convert list item types.
        if (!curList) {
          curList = curParent->AsElement();
        } else if (curParent != curList) {
          // move list item to new list
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->MoveNode(curNode, curList, -1);
          NS_ENSURE_SUCCESS(res, res);
        }
        NS_ENSURE_STATE(mHTMLEditor);
        if (!curNode->IsHTMLElement(itemType)) {
          NS_ENSURE_STATE(mHTMLEditor);
          newBlock = mHTMLEditor->ReplaceContainer(curNode->AsElement(),
                                                   itemType);
          NS_ENSURE_STATE(newBlock);
        }
      }
      nsCOMPtr<nsIDOMElement> curElement = do_QueryInterface(curNode);
      NS_NAMED_LITERAL_STRING(typestr, "type");
      if (aBulletType && !aBulletType->IsEmpty()) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->SetAttribute(curElement, typestr, *aBulletType);
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RemoveAttribute(curElement, typestr);
      }
      NS_ENSURE_SUCCESS(res, res);
      continue;
    }

    // if we hit a div clear our prevListItem, insert divs contents
    // into our node array, and remove the div
    if (curNode->IsHTMLElement(nsGkAtoms::div)) {
      prevListItem = nullptr;
      int32_t j = i + 1;
      GetInnerContent(*curNode, arrayOfNodes, &j);
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->RemoveContainer(curNode);
      NS_ENSURE_SUCCESS(res, res);
      listCount = arrayOfNodes.Length();
      continue;
    }

    // need to make a list to put things in if we haven't already,
    if (!curList) {
      res = SplitAsNeeded(listType, curParent, offset);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      curList = mHTMLEditor->CreateNode(listType, curParent, offset);
      NS_ENSURE_SUCCESS(res, res);
      // remember our new block for postprocessing
      mNewBlock = curList;
      // curList is now the correct thing to put curNode in
      prevListItem = 0;
    }

    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<Element> listItem;
    if (!nsHTMLEditUtils::IsListItem(curNode)) {
      if (IsInlineNode(curNode) && prevListItem) {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, prevListItem, -1);
        NS_ENSURE_SUCCESS(res, res);
      } else {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (curNode->IsHTMLElement(nsGkAtoms::p)) {
          NS_ENSURE_STATE(mHTMLEditor);
          listItem = mHTMLEditor->ReplaceContainer(curNode->AsElement(),
                                                   itemType);
          NS_ENSURE_STATE(listItem);
        } else {
          NS_ENSURE_STATE(mHTMLEditor);
          listItem = mHTMLEditor->InsertContainerAbove(curNode, itemType);
          NS_ENSURE_STATE(listItem);
        }
        if (IsInlineNode(curNode)) {
          prevListItem = listItem;
        } else {
          prevListItem = nullptr;
        }
      }
    } else {
      listItem = curNode->AsElement();
    }

    if (listItem) {
      // if we made a new list item, deal with it: tuck the listItem into the
      // end of the active list
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(listItem, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::WillRemoveList(Selection* aSelection,
                                bool aOrdered,
                                bool *aCancel,
                                bool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = false;
  *aHandled = true;

  nsresult res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  nsTArray<RefPtr<nsRange>> arrayOfRanges;
  GetPromotedRanges(*aSelection, arrayOfRanges, EditAction::makeList);

  // use these ranges to contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  res = GetListActionNodes(arrayOfNodes, EntireList::no);
  NS_ENSURE_SUCCESS(res, res);

  // Remove all non-editable nodes.  Leave them be.
  int32_t listCount = arrayOfNodes.Length();
  int32_t i;
  for (i=listCount-1; i>=0; i--)
  {
    OwningNonNull<nsINode> testNode = arrayOfNodes[i];
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(testNode))
    {
      arrayOfNodes.RemoveElementAt(i);
    }
  }

  // reset list count
  listCount = arrayOfNodes.Length();

  // Only act on lists or list items in the array
  for (auto& curNode : arrayOfNodes) {
    // here's where we actually figure out what to do
    if (nsHTMLEditUtils::IsListItem(curNode))  // unlist this listitem
    {
      bool bOutOfList;
      do
      {
        res = PopListItem(GetAsDOMNode(curNode), &bOutOfList);
        NS_ENSURE_SUCCESS(res, res);
      } while (!bOutOfList); // keep popping it out until it's not in a list anymore
    }
    else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, move list items out
    {
      res = RemoveListStructure(*curNode->AsElement());
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillMakeDefListItem(Selection* aSelection,
                                     const nsAString *aItemType,
                                     bool aEntireList,
                                     bool *aCancel,
                                     bool *aHandled)
{
  // for now we let WillMakeList handle this
  NS_NAMED_LITERAL_STRING(listType, "dl");
  return WillMakeList(aSelection, &listType, aEntireList, nullptr, aCancel, aHandled, aItemType);
}

nsresult
nsHTMLEditRules::WillMakeBasicBlock(Selection& aSelection,
                                    const nsAString& aBlockType,
                                    bool* aCancel,
                                    bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  OwningNonNull<nsIAtom> blockType = NS_Atomize(aBlockType);

  WillInsert(aSelection, aCancel);
  // We want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = false;

  nsresult res = NormalizeSelection(&aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(&aSelection, mHTMLEditor);
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  *aHandled = true;

  // Contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  res = GetNodesFromSelection(aSelection, EditAction::makeBasicBlock,
                              arrayOfNodes);
  NS_ENSURE_SUCCESS(res, res);

  // Remove all non-editable nodes.  Leave them be.
  for (int32_t i = arrayOfNodes.Length() - 1; i >= 0; i--) {
    if (!mHTMLEditor->IsEditable(arrayOfNodes[i])) {
      arrayOfNodes.RemoveElementAt(i);
    }
  }

  // If nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    // Get selection location
    NS_ENSURE_STATE(aSelection.GetRangeAt(0) &&
                    aSelection.GetRangeAt(0)->GetStartParent());
    OwningNonNull<nsINode> parent =
      *aSelection.GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection.GetRangeAt(0)->StartOffset();

    if (blockType == nsGkAtoms::normal ||
        blockType == nsGkAtoms::_empty) {
      // We are removing blocks (going to "body text")
      NS_ENSURE_TRUE(mHTMLEditor->GetBlock(parent), NS_ERROR_NULL_POINTER);
      OwningNonNull<Element> curBlock = *mHTMLEditor->GetBlock(parent);
      if (nsHTMLEditUtils::IsFormatNode(curBlock)) {
        // If the first editable node after selection is a br, consume it.
        // Otherwise it gets pushed into a following block after the split,
        // which is visually bad.
        nsCOMPtr<nsIContent> brNode =
          mHTMLEditor->GetNextHTMLNode(parent, offset);
        if (brNode && brNode->IsHTMLElement(nsGkAtoms::br)) {
          res = mHTMLEditor->DeleteNode(brNode);
          NS_ENSURE_SUCCESS(res, res);
        }
        // Do the splits!
        offset = mHTMLEditor->SplitNodeDeep(curBlock, *parent->AsContent(),
                                            offset,
                                            nsHTMLEditor::EmptyContainers::no);
        NS_ENSURE_STATE(offset != -1);
        // Put a br at the split point
        brNode = mHTMLEditor->CreateBR(curBlock->GetParentNode(), offset);
        NS_ENSURE_STATE(brNode);
        // Put selection at the split point
        res = aSelection.Collapse(curBlock->GetParentNode(), offset);
        // To prevent selection resetter from overriding us.
        selectionResetter.Abort();
        *aHandled = true;
        NS_ENSURE_SUCCESS(res, res);
      }
      // Else nothing to do!
    } else {
      // We are making a block.  Consume a br, if needed.
      nsCOMPtr<nsIContent> brNode =
        mHTMLEditor->GetNextHTMLNode(parent, offset, true);
      if (brNode && brNode->IsHTMLElement(nsGkAtoms::br)) {
        res = mHTMLEditor->DeleteNode(brNode);
        NS_ENSURE_SUCCESS(res, res);
        // We don't need to act on this node any more
        arrayOfNodes.RemoveElement(brNode);
      }
      // Make sure we can put a block here
      res = SplitAsNeeded(blockType, parent, offset);
      NS_ENSURE_SUCCESS(res, res);
      nsCOMPtr<Element> block =
        mHTMLEditor->CreateNode(blockType, parent, offset);
      NS_ENSURE_STATE(block);
      // Remember our new block for postprocessing
      mNewBlock = block;
      // Delete anything that was in the list of nodes
      while (!arrayOfNodes.IsEmpty()) {
        OwningNonNull<nsINode> curNode = arrayOfNodes[0];
        res = mHTMLEditor->DeleteNode(curNode);
        NS_ENSURE_SUCCESS(res, res);
        arrayOfNodes.RemoveElementAt(0);
      }
      // Put selection in new block
      res = aSelection.Collapse(block, 0);
      // To prevent selection resetter from overriding us.
      selectionResetter.Abort();
      *aHandled = true;
      NS_ENSURE_SUCCESS(res, res);
    }
    return NS_OK;
  }
  // Okay, now go through all the nodes and make the right kind of blocks, or
  // whatever is approriate.  Woohoo!  Note: blockquote is handled a little
  // differently.
  if (blockType == nsGkAtoms::blockquote) {
    res = MakeBlockquote(arrayOfNodes);
    NS_ENSURE_SUCCESS(res, res);
  } else if (blockType == nsGkAtoms::normal ||
             blockType == nsGkAtoms::_empty) {
    res = RemoveBlockStyle(arrayOfNodes);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    res = ApplyBlockStyle(arrayOfNodes, blockType);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::DidMakeBasicBlock(Selection* aSelection,
                                   nsRulesInfo *aInfo, nsresult aResult)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  // check for empty block.  if so, put a moz br in it.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  NS_ENSURE_STATE(aSelection->GetRangeAt(0) &&
                  aSelection->GetRangeAt(0)->GetStartParent());
  nsresult res =
    InsertMozBRIfNeeded(*aSelection->GetRangeAt(0)->GetStartParent());
  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

nsresult
nsHTMLEditRules::WillIndent(Selection* aSelection,
                            bool* aCancel, bool* aHandled)
{
  nsresult res;
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsCSSEnabled()) {
    res = WillCSSIndent(aSelection, aCancel, aHandled);
  }
  else {
    res = WillHTMLIndent(aSelection, aCancel, aHandled);
  }
  return res;
}

nsresult
nsHTMLEditRules::WillCSSIndent(Selection* aSelection,
                               bool* aCancel, bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsresult res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsTArray<OwningNonNull<nsRange>> arrayOfRanges;
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;

  // short circuit: detect case of collapsed selection inside an <li>.
  // just sublist that <li>.  This prevents bug 97797.

  nsCOMPtr<Element> liNode;
  if (aSelection->Collapsed()) {
    nsCOMPtr<nsINode> node;
    int32_t offset;
    NS_ENSURE_STATE(mHTMLEditor);
    nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(node), &offset);
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<Element> block = mHTMLEditor->GetBlock(*node);
    if (block && nsHTMLEditUtils::IsListItem(block))
      liNode = block;
  }

  if (liNode)
  {
    arrayOfNodes.AppendElement(*liNode);
  }
  else
  {
    // convert the selection ranges into "promoted" selection ranges:
    // this basically just expands the range to include the immediate
    // block parent, and then further expands to include any ancestors
    // whose children are all in the range
    res = GetNodesFromSelection(*aSelection, EditAction::indent, arrayOfNodes);
    NS_ENSURE_SUCCESS(res, res);
  }

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    // get selection location
    NS_ENSURE_STATE(aSelection->RangeCount());
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);

    // make sure we can put a block here
    res = SplitAsNeeded(*nsGkAtoms::div, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theBlock = mHTMLEditor->CreateNode(nsGkAtoms::div,
                                                         parent, offset);
    NS_ENSURE_STATE(theBlock);
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    ChangeIndentation(*theBlock, Change::plus);
    // delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      arrayOfNodes.RemoveElementAt(0);
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = true;
    return res;
  }

  // Ok, now go through all the nodes and put them in a blockquote,
  // or whatever is appropriate.  Wohoo!
  int32_t i;
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<Element> curList, curQuote;
  nsCOMPtr<nsIContent> sibling;
  int32_t listCount = arrayOfNodes.Length();
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    NS_ENSURE_STATE(arrayOfNodes[i]->IsContent());
    nsCOMPtr<nsIContent> curNode = arrayOfNodes[i]->AsContent();

    // Ignore all non-editable nodes.  Leave them be.
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    curParent = curNode->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;

    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      sibling = nullptr;

      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetNextHTMLSibling(curNode);
      if (sibling && nsHTMLEditUtils::IsList(sibling))
      {
        if (curParent->NodeInfo()->NameAtom() == sibling->NodeInfo()->NameAtom() &&
            curParent->NodeInfo()->NamespaceID() == sibling->NodeInfo()->NamespaceID()) {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->MoveNode(curNode, sibling, 0);
          NS_ENSURE_SUCCESS(res, res);
          continue;
        }
      }
      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      if (sibling && nsHTMLEditUtils::IsList(sibling))
      {
        if (curParent->NodeInfo()->NameAtom() == sibling->NodeInfo()->NameAtom() &&
            curParent->NodeInfo()->NamespaceID() == sibling->NodeInfo()->NamespaceID()) {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->MoveNode(curNode, sibling, -1);
          NS_ENSURE_SUCCESS(res, res);
          continue;
        }
      }
      sibling = nullptr;

      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList) {
        NS_ENSURE_STATE(mHTMLEditor);
        sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        // create a new nested list of correct type
        res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                            offset);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                          curParent, offset);
        NS_ENSURE_STATE(curList);
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      uint32_t listLen = curList->Length();
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(curNode, curList, listLen);
      NS_ENSURE_SUCCESS(res, res);
    }

    else // not a list item
    {
      if (curNode && IsBlockNode(*curNode)) {
        ChangeIndentation(*curNode->AsElement(), Change::plus);
        curQuote = nullptr;
      }
      else {
        if (!curQuote)
        {
          // First, check that our element can contain a div.
          if (!mEditor->CanContainTag(*curParent, *nsGkAtoms::div)) {
            return NS_OK; // cancelled
          }

          res = SplitAsNeeded(*nsGkAtoms::div, curParent, offset);
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_STATE(mHTMLEditor);
          curQuote = mHTMLEditor->CreateNode(nsGkAtoms::div, curParent,
                                             offset);
          NS_ENSURE_STATE(curQuote);
          ChangeIndentation(*curQuote, Change::plus);
          // remember our new block for postprocessing
          mNewBlock = curQuote;
          // curQuote is now the correct thing to put curNode in
        }

        // tuck the node into the end of the active blockquote
        uint32_t quoteLen = curQuote->Length();
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curQuote, quoteLen);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }
  return res;
}

nsresult
nsHTMLEditRules::WillHTMLIndent(Selection* aSelection,
                                bool* aCancel, bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsresult res = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range

  nsTArray<RefPtr<nsRange>> arrayOfRanges;
  GetPromotedRanges(*aSelection, arrayOfRanges, EditAction::indent);

  // use these ranges to contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes, EditAction::indent);
  NS_ENSURE_SUCCESS(res, res);

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    // get selection location
    NS_ENSURE_STATE(aSelection->RangeCount());
    nsCOMPtr<nsINode> parent = aSelection->GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(parent);

    // make sure we can put a block here
    res = SplitAsNeeded(*nsGkAtoms::blockquote, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> theBlock = mHTMLEditor->CreateNode(nsGkAtoms::blockquote,
                                                         parent, offset);
    NS_ENSURE_STATE(theBlock);
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    // delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      arrayOfNodes.RemoveElementAt(0);
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = true;
    return res;
  }

  // Ok, now go through all the nodes and put them in a blockquote,
  // or whatever is appropriate.  Wohoo!
  int32_t i;
  nsCOMPtr<nsINode> curParent;
  nsCOMPtr<nsIContent> sibling;
  nsCOMPtr<Element> curList, curQuote, indentedLI;
  int32_t listCount = arrayOfNodes.Length();
  for (i=0; i<listCount; i++)
  {
    // here's where we actually figure out what to do
    NS_ENSURE_STATE(arrayOfNodes[i]->IsContent());
    nsCOMPtr<nsIContent> curNode = arrayOfNodes[i]->AsContent();

    // Ignore all non-editable nodes.  Leave them be.
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    curParent = curNode->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;

    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      sibling = nullptr;

      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetNextHTMLSibling(curNode);
      if (sibling && nsHTMLEditUtils::IsList(sibling) &&
          curParent->NodeInfo()->NameAtom() == sibling->NodeInfo()->NameAtom() &&
          curParent->NodeInfo()->NamespaceID() == sibling->NodeInfo()->NamespaceID()) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, sibling, 0);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }

      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      if (sibling && nsHTMLEditUtils::IsList(sibling) &&
          curParent->NodeInfo()->NameAtom() == sibling->NodeInfo()->NameAtom() &&
          curParent->NodeInfo()->NamespaceID() == sibling->NodeInfo()->NamespaceID()) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, sibling, -1);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }

      sibling = nullptr;

      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList) {
        NS_ENSURE_STATE(mHTMLEditor);
        sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList) )
      {
        // create a new nested list of correct type
        res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                            offset);
        NS_ENSURE_SUCCESS(res, res);
        NS_ENSURE_STATE(mHTMLEditor);
        curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                          curParent, offset);
        NS_ENSURE_STATE(curList);
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(curNode, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
      // forget curQuote, if any
      curQuote = nullptr;
    }

    else // not a list item, use blockquote?
    {
      // if we are inside a list item, we don't want to blockquote, we want
      // to sublist the list item.  We may have several nodes listed in the
      // array of nodes to act on, that are in the same list item.  Since
      // we only want to indent that li once, we must keep track of the most
      // recent indented list item, and not indent it if we find another node
      // to act on that is still inside the same li.
      nsCOMPtr<Element> listItem = IsInListItem(curNode);
      if (listItem) {
        if (indentedLI == listItem) {
          // already indented this list item
          continue;
        }
        curParent = listItem->GetParentNode();
        offset = curParent ? curParent->IndexOf(listItem) : -1;
        // check to see if curList is still appropriate.  Which it is if
        // curNode is still right after it in the same list.
        if (curList)
        {
          sibling = nullptr;
          NS_ENSURE_STATE(mHTMLEditor);
          sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
        }

        if (!curList || (sibling && sibling != curList) )
        {
          // create a new nested list of correct type
          res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                              offset);
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_STATE(mHTMLEditor);
          curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                            curParent, offset);
          NS_ENSURE_STATE(curList);
        }
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(listItem, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // remember we indented this li
        indentedLI = listItem;
      }

      else
      {
        // need to make a blockquote to put things in if we haven't already,
        // or if this node doesn't go in blockquote we used earlier.
        // One reason it might not go in prio blockquote is if we are now
        // in a different table cell.
        if (curQuote && InDifferentTableElements(curQuote, curNode)) {
          curQuote = nullptr;
        }

        if (!curQuote)
        {
          // First, check that our element can contain a blockquote.
          if (!mEditor->CanContainTag(*curParent, *nsGkAtoms::blockquote)) {
            return NS_OK; // cancelled
          }

          res = SplitAsNeeded(*nsGkAtoms::blockquote, curParent, offset);
          NS_ENSURE_SUCCESS(res, res);
          NS_ENSURE_STATE(mHTMLEditor);
          curQuote = mHTMLEditor->CreateNode(nsGkAtoms::blockquote, curParent,
                                             offset);
          NS_ENSURE_STATE(curQuote);
          // remember our new block for postprocessing
          mNewBlock = curQuote;
          // curQuote is now the correct thing to put curNode in
        }

        // tuck the node into the end of the active blockquote
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->MoveNode(curNode, curQuote, -1);
        NS_ENSURE_SUCCESS(res, res);
        // forget curList, if any
        curList = nullptr;
      }
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillOutdent(Selection& aSelection,
                             bool* aCancel, bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);
  *aCancel = false;
  *aHandled = true;
  nsCOMPtr<nsIContent> rememberedLeftBQ, rememberedRightBQ;
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  nsresult res = NormalizeSelection(&aSelection);
  NS_ENSURE_SUCCESS(res, res);

  // Some scoping for selection resetting - we may need to tweak it
  {
    nsAutoSelectionReset selectionResetter(&aSelection, mHTMLEditor);

    // Convert the selection ranges into "promoted" selection ranges: this
    // basically just expands the range to include the immediate block parent,
    // and then further expands to include any ancestors whose children are all
    // in the range
    nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
    res = GetNodesFromSelection(aSelection, EditAction::outdent,
                                arrayOfNodes);
    NS_ENSURE_SUCCESS(res, res);

    // Okay, now go through all the nodes and remove a level of blockquoting,
    // or whatever is appropriate.  Wohoo!

    nsCOMPtr<Element> curBlockQuote;
    nsCOMPtr<nsIContent> firstBQChild, lastBQChild;
    bool curBlockQuoteIsIndentedWithCSS = false;
    for (uint32_t i = 0; i < arrayOfNodes.Length(); i++) {
      if (!arrayOfNodes[i]->IsContent()) {
        continue;
      }
      OwningNonNull<nsIContent> curNode = *arrayOfNodes[i]->AsContent();

      // Here's where we actually figure out what to do
      nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
      int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;

      // Is it a blockquote?
      if (curNode->IsHTMLElement(nsGkAtoms::blockquote)) {
        // If it is a blockquote, remove it.  So we need to finish up dealng
        // with any curBlockQuote first.
        if (curBlockQuote) {
          res = OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                                   curBlockQuoteIsIndentedWithCSS,
                                   getter_AddRefs(rememberedLeftBQ),
                                   getter_AddRefs(rememberedRightBQ));
          NS_ENSURE_SUCCESS(res, res);
          curBlockQuote = nullptr;
          firstBQChild = nullptr;
          lastBQChild = nullptr;
          curBlockQuoteIsIndentedWithCSS = false;
        }
        res = mHTMLEditor->RemoveBlockContainer(curNode);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }
      // Is it a block with a 'margin' property?
      if (useCSS && IsBlockNode(curNode)) {
        nsIAtom& marginProperty =
          MarginPropertyAtomForIndent(*mHTMLEditor->mHTMLCSSUtils, curNode);
        nsAutoString value;
        mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(curNode,
                                                         marginProperty,
                                                         value);
        float f;
        nsCOMPtr<nsIAtom> unit;
        NS_ENSURE_STATE(mHTMLEditor);
        mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
        if (f > 0) {
          ChangeIndentation(*curNode->AsElement(), Change::minus);
          continue;
        }
      }
      // Is it a list item?
      if (nsHTMLEditUtils::IsListItem(curNode)) {
        // If it is a list item, that means we are not outdenting whole list.
        // So we need to finish up dealing with any curBlockQuote, and then pop
        // this list item.
        if (curBlockQuote) {
          res = OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                                   curBlockQuoteIsIndentedWithCSS,
                                   getter_AddRefs(rememberedLeftBQ),
                                   getter_AddRefs(rememberedRightBQ));
          NS_ENSURE_SUCCESS(res, res);
          curBlockQuote = nullptr;
          firstBQChild = nullptr;
          lastBQChild = nullptr;
          curBlockQuoteIsIndentedWithCSS = false;
        }
        bool unused;
        res = PopListItem(GetAsDOMNode(curNode), &unused);
        NS_ENSURE_SUCCESS(res, res);
        continue;
      }
      // Do we have a blockquote that we are already committed to removing?
      if (curBlockQuote) {
        // If so, is this node a descendant?
        if (nsEditorUtils::IsDescendantOf(curNode, curBlockQuote)) {
          lastBQChild = curNode;
          // Then we don't need to do anything different for this node
          continue;
        } else {
          // Otherwise, we have progressed beyond end of curBlockQuote, so
          // let's handle it now.  We need to remove the portion of
          // curBlockQuote that contains [firstBQChild - lastBQChild].
          res = OutdentPartOfBlock(*curBlockQuote, *firstBQChild,
                                   *lastBQChild,
                                   curBlockQuoteIsIndentedWithCSS,
                                   getter_AddRefs(rememberedLeftBQ),
                                   getter_AddRefs(rememberedRightBQ));
          NS_ENSURE_SUCCESS(res, res);
          curBlockQuote = nullptr;
          firstBQChild = nullptr;
          lastBQChild = nullptr;
          curBlockQuoteIsIndentedWithCSS = false;
          // Fall out and handle curNode
        }
      }

      // Are we inside a blockquote?
      OwningNonNull<nsINode> n = curNode;
      curBlockQuoteIsIndentedWithCSS = false;
      // Keep looking up the hierarchy as long as we don't hit the body or the
      // active editing host or a table element (other than an entire table)
      while (!n->IsHTMLElement(nsGkAtoms::body) && 
             mHTMLEditor->IsDescendantOfEditorRoot(n) &&
             (n->IsHTMLElement(nsGkAtoms::table) ||
              !nsHTMLEditUtils::IsTableElement(n))) {
        if (!n->GetParentNode()) {
          break;
        }
        n = *n->GetParentNode();
        if (n->IsHTMLElement(nsGkAtoms::blockquote)) {
          // If so, remember it and the first node we are taking out of it.
          curBlockQuote = n->AsElement();
          firstBQChild = curNode;
          lastBQChild = curNode;
          break;
        } else if (useCSS) {
          nsIAtom& marginProperty =
            MarginPropertyAtomForIndent(*mHTMLEditor->mHTMLCSSUtils, curNode);
          nsAutoString value;
          mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(*n, marginProperty,
                                                           value);
          float f;
          nsCOMPtr<nsIAtom> unit;
          mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
          if (f > 0 && !(nsHTMLEditUtils::IsList(curParent) &&
                         nsHTMLEditUtils::IsList(curNode))) {
            curBlockQuote = n->AsElement();
            firstBQChild = curNode;
            lastBQChild = curNode;
            curBlockQuoteIsIndentedWithCSS = true;
            break;
          }
        }
      }

      if (!curBlockQuote) {
        // Couldn't find enclosing blockquote.  Handle list cases.
        if (nsHTMLEditUtils::IsList(curParent)) {
          // Move node out of list
          if (nsHTMLEditUtils::IsList(curNode)) {
            // Just unwrap this sublist
            res = mHTMLEditor->RemoveBlockContainer(curNode);
            NS_ENSURE_SUCCESS(res, res);
          }
          // handled list item case above
        } else if (nsHTMLEditUtils::IsList(curNode)) {
          // node is a list, but parent is non-list: move list items out
          nsCOMPtr<nsIContent> child = curNode->GetLastChild();
          while (child) {
            if (nsHTMLEditUtils::IsListItem(child)) {
              bool unused;
              res = PopListItem(GetAsDOMNode(child), &unused);
              NS_ENSURE_SUCCESS(res, res);
            } else if (nsHTMLEditUtils::IsList(child)) {
              // We have an embedded list, so move it out from under the parent
              // list. Be sure to put it after the parent list because this
              // loop iterates backwards through the parent's list of children.

              res = mHTMLEditor->MoveNode(child, curParent, offset + 1);
              NS_ENSURE_SUCCESS(res, res);
            } else {
              // Delete any non-list items for now
              res = mHTMLEditor->DeleteNode(child);
              NS_ENSURE_SUCCESS(res, res);
            }
            child = curNode->GetLastChild();
          }
          // Delete the now-empty list
          res = mHTMLEditor->RemoveBlockContainer(curNode);
          NS_ENSURE_SUCCESS(res, res);
        } else if (useCSS) {
          nsCOMPtr<Element> element;
          nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(curNode);
          if (curNode->GetAsText()) {
            // We want to outdent the parent of text nodes
            element = curNode->GetParentElement();
          } else if (curNode->IsElement()) {
            element = curNode->AsElement();
          }
          if (element) {
            ChangeIndentation(*element, Change::minus);
          }
        }
      }
    }
    if (curBlockQuote) {
      // We have a blockquote we haven't finished handling
      res = OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                               curBlockQuoteIsIndentedWithCSS,
                               getter_AddRefs(rememberedLeftBQ),
                               getter_AddRefs(rememberedRightBQ));
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  // Make sure selection didn't stick to last piece of content in old bq (only
  // a problem for collapsed selections)
  if (rememberedLeftBQ || rememberedRightBQ) {
    if (aSelection.Collapsed()) {
      // Push selection past end of rememberedLeftBQ
      NS_ENSURE_TRUE(aSelection.GetRangeAt(0), NS_OK);
      nsCOMPtr<nsINode> startNode = aSelection.GetRangeAt(0)->GetStartParent();
      int32_t startOffset = aSelection.GetRangeAt(0)->StartOffset();
      if (rememberedLeftBQ &&
          (startNode == rememberedLeftBQ ||
           nsEditorUtils::IsDescendantOf(startNode, rememberedLeftBQ))) {
        // Selection is inside rememberedLeftBQ - push it past it.
        startNode = rememberedLeftBQ->GetParentNode();
        startOffset = startNode ? 1 + startNode->IndexOf(rememberedLeftBQ) : 0;
        aSelection.Collapse(startNode, startOffset);
      }
      // And pull selection before beginning of rememberedRightBQ
      startNode = aSelection.GetRangeAt(0)->GetStartParent();
      startOffset = aSelection.GetRangeAt(0)->StartOffset();
      if (rememberedRightBQ &&
          (startNode == rememberedRightBQ ||
           nsEditorUtils::IsDescendantOf(startNode, rememberedRightBQ))) {
        // Selection is inside rememberedRightBQ - push it before it.
        startNode = rememberedRightBQ->GetParentNode();
        startOffset = startNode ? startNode->IndexOf(rememberedRightBQ) : -1;
        aSelection.Collapse(startNode, startOffset);
      }
    }
    return NS_OK;
  }
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// RemovePartOfBlock: Split aBlock and move aStartChild to aEndChild out of
//                    aBlock.
nsresult
nsHTMLEditRules::RemovePartOfBlock(Element& aBlock,
                                   nsIContent& aStartChild,
                                   nsIContent& aEndChild)
{
  SplitBlock(aBlock, aStartChild, aEndChild);
  // Get rid of part of blockquote we are outdenting

  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->RemoveBlockContainer(aBlock);
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}

void
nsHTMLEditRules::SplitBlock(Element& aBlock,
                            nsIContent& aStartChild,
                            nsIContent& aEndChild,
                            nsIContent** aOutLeftNode,
                            nsIContent** aOutRightNode,
                            nsIContent** aOutMiddleNode)
{
  // aStartChild and aEndChild must be exclusive descendants of aBlock
  MOZ_ASSERT(nsEditorUtils::IsDescendantOf(&aStartChild, &aBlock) &&
             nsEditorUtils::IsDescendantOf(&aEndChild, &aBlock));
  NS_ENSURE_TRUE_VOID(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // Get split point location
  OwningNonNull<nsIContent> startParent = *aStartChild.GetParent();
  int32_t startOffset = startParent->IndexOf(&aStartChild);

  // Do the splits!
  nsCOMPtr<nsIContent> newMiddleNode1;
  mHTMLEditor->SplitNodeDeep(aBlock, startParent, startOffset,
                             nsHTMLEditor::EmptyContainers::no,
                             aOutLeftNode, getter_AddRefs(newMiddleNode1));

  // Get split point location
  OwningNonNull<nsIContent> endParent = *aEndChild.GetParent();
  // +1 because we want to be after the child
  int32_t endOffset = 1 + endParent->IndexOf(&aEndChild);

  // Do the splits!
  nsCOMPtr<nsIContent> newMiddleNode2;
  mHTMLEditor->SplitNodeDeep(aBlock, endParent, endOffset,
                             nsHTMLEditor::EmptyContainers::no,
                             getter_AddRefs(newMiddleNode2), aOutRightNode);

  if (aOutMiddleNode) {
    if (newMiddleNode2) {
      newMiddleNode2.forget(aOutMiddleNode);
    } else {
      newMiddleNode1.forget(aOutMiddleNode);
    }
  }
}

nsresult
nsHTMLEditRules::OutdentPartOfBlock(Element& aBlock,
                                    nsIContent& aStartChild,
                                    nsIContent& aEndChild,
                                    bool aIsBlockIndentedWithCSS,
                                    nsIContent** aOutLeftNode,
                                    nsIContent** aOutRightNode)
{
  MOZ_ASSERT(aOutLeftNode && aOutRightNode);

  nsCOMPtr<nsIContent> middleNode;
  SplitBlock(aBlock, aStartChild, aEndChild, aOutLeftNode, aOutRightNode,
             getter_AddRefs(middleNode));

  NS_ENSURE_STATE(middleNode);

  if (!aIsBlockIndentedWithCSS) {
    NS_ENSURE_STATE(mHTMLEditor);
    nsresult res =
      mHTMLEditor->RemoveBlockContainer(*middleNode);
    NS_ENSURE_SUCCESS(res, res);
  } else if (middleNode->IsElement()) {
    // We do nothing if middleNode isn't an element
    nsresult res = ChangeIndentation(*middleNode->AsElement(), Change::minus);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// ConvertListType:  convert list type and list item type.
//
//
nsresult
nsHTMLEditRules::ConvertListType(Element* aList,
                                 Element** aOutList,
                                 nsIAtom* aListType,
                                 nsIAtom* aItemType)
{
  MOZ_ASSERT(aList);
  MOZ_ASSERT(aOutList);
  MOZ_ASSERT(aListType);
  MOZ_ASSERT(aItemType);

  nsCOMPtr<nsINode> child = aList->GetFirstChild();
  while (child)
  {
    if (child->IsElement()) {
      dom::Element* element = child->AsElement();
      if (nsHTMLEditUtils::IsListItem(element) &&
          !element->IsHTMLElement(aItemType)) {
        child = mHTMLEditor->ReplaceContainer(element, aItemType);
        NS_ENSURE_STATE(child);
      } else if (nsHTMLEditUtils::IsList(element) &&
                 !element->IsHTMLElement(aListType)) {
        nsCOMPtr<dom::Element> temp;
        nsresult rv = ConvertListType(child->AsElement(), getter_AddRefs(temp),
                                      aListType, aItemType);
        NS_ENSURE_SUCCESS(rv, rv);
        child = temp.forget();
      }
    }
    child = child->GetNextSibling();
  }

  if (aList->IsHTMLElement(aListType)) {
    nsCOMPtr<dom::Element> list = aList->AsElement();
    list.forget(aOutList);
    return NS_OK;
  }

  *aOutList = mHTMLEditor->ReplaceContainer(aList, aListType).take();
  NS_ENSURE_STATE(aOutList);

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// CreateStyleForInsertText:  take care of clearing and setting appropriate
//                            style nodes for text insertion.
//
//
nsresult
nsHTMLEditRules::CreateStyleForInsertText(Selection& aSelection,
                                          nsIDocument& aDoc)
{
  MOZ_ASSERT(mHTMLEditor->mTypeInState);

  nsresult res;
  bool weDidSomething = false;
  NS_ENSURE_STATE(aSelection.GetRangeAt(0));
  nsCOMPtr<nsINode> node = aSelection.GetRangeAt(0)->GetStartParent();
  int32_t offset = aSelection.GetRangeAt(0)->StartOffset();

  // next examine our present style and make sure default styles are either
  // present or explicitly overridden.  If neither, add the default style to
  // the TypeInState
  int32_t length = mHTMLEditor->mDefaultStyles.Length();
  for (int32_t j = 0; j < length; j++) {
    PropItem* propItem = mHTMLEditor->mDefaultStyles[j];
    MOZ_ASSERT(propItem);
    bool bFirst, bAny, bAll;

    // GetInlineProperty also examine TypeInState.  The only gotcha here is
    // that a cleared property looks like an unset property.  For now I'm
    // assuming that's not a problem: that default styles will always be
    // multivalue styles (like font face or size) where clearing the style
    // means we want to go back to the default.  If we ever wanted a "toggle"
    // style like bold for a default, though, I'll have to add code to detect
    // the difference between unset and explicitly cleared, else user would
    // never be able to unbold, for instance.
    nsAutoString curValue;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetInlinePropertyBase(*propItem->tag, &propItem->attr,
                                             nullptr, &bFirst, &bAny, &bAll,
                                             &curValue, false);
    NS_ENSURE_SUCCESS(res, res);

    if (!bAny) {
      // no style set for this prop/attr
      mHTMLEditor->mTypeInState->SetProp(propItem->tag, propItem->attr,
                                         propItem->value);
    }
  }

  nsCOMPtr<Element> rootElement = aDoc.GetRootElement();
  NS_ENSURE_STATE(rootElement);

  // process clearing any styles first
  nsAutoPtr<PropItem> item(mHTMLEditor->mTypeInState->TakeClearProperty());
  while (item && node != rootElement) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->ClearStyle(address_of(node), &offset,
                                  item->tag, &item->attr);
    NS_ENSURE_SUCCESS(res, res);
    item = mHTMLEditor->mTypeInState->TakeClearProperty();
    weDidSomething = true;
  }

  // then process setting any styles
  int32_t relFontSize = mHTMLEditor->mTypeInState->TakeRelativeFontSize();
  item = mHTMLEditor->mTypeInState->TakeSetProperty();

  if (item || relFontSize) {
    // we have at least one style to add; make a new text node to insert style
    // nodes above.
    if (RefPtr<Text> text = node->GetAsText()) {
      // if we are in a text node, split it
      NS_ENSURE_STATE(mHTMLEditor);
      offset = mHTMLEditor->SplitNodeDeep(*text, *text, offset);
      NS_ENSURE_STATE(offset != -1);
      node = node->GetParentNode();
    }
    if (!mHTMLEditor->IsContainer(node)) {
      return NS_OK;
    }
    OwningNonNull<Text> newNode = aDoc.CreateTextNode(EmptyString());
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->InsertNode(newNode, *node, offset);
    NS_ENSURE_SUCCESS(res, res);
    node = newNode;
    offset = 0;
    weDidSomething = true;

    if (relFontSize) {
      // dir indicated bigger versus smaller.  1 = bigger, -1 = smaller
      nsHTMLEditor::FontSize dir = relFontSize > 0 ?
        nsHTMLEditor::FontSize::incr : nsHTMLEditor::FontSize::decr;
      for (int32_t j = 0; j < DeprecatedAbs(relFontSize); j++) {
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RelativeFontChangeOnTextNode(dir, newNode, 0, -1);
        NS_ENSURE_SUCCESS(res, res);
      }
    }

    while (item) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetInlinePropertyOnNode(*node->AsContent(),
                                                 *item->tag, &item->attr,
                                                 item->value);
      NS_ENSURE_SUCCESS(res, res);
      item = mHTMLEditor->mTypeInState->TakeSetProperty();
    }
  }
  if (weDidSomething) {
    return aSelection.Collapse(node, offset);
  }

  return NS_OK;
}


/**
 * Figure out if aNode is (or is inside) an empty block.  A block can have
 * children and still be considered empty, if the children are empty or
 * non-editable.
 */
nsresult
nsHTMLEditRules::IsEmptyBlock(Element& aNode,
                              bool* aOutIsEmptyBlock,
                              MozBRCounts aMozBRCounts)
{
  MOZ_ASSERT(aOutIsEmptyBlock);
  *aOutIsEmptyBlock = true;

  NS_ENSURE_TRUE(IsBlockNode(aNode), NS_ERROR_NULL_POINTER);

  return mHTMLEditor->IsEmptyNode(aNode.AsDOMNode(), aOutIsEmptyBlock,
                                  aMozBRCounts == MozBRCounts::yes ? false
                                                                   : true);
}


nsresult
nsHTMLEditRules::WillAlign(Selection& aSelection,
                           const nsAString& aAlignType,
                           bool* aCancel,
                           bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  WillInsert(aSelection, aCancel);

  // Initialize out param.  We want to ignore result of WillInsert().
  *aCancel = false;
  *aHandled = false;

  nsresult rv = NormalizeSelection(&aSelection);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoSelectionReset selectionResetter(&aSelection, mHTMLEditor);

  // Convert the selection ranges into "promoted" selection ranges: This
  // basically just expands the range to include the immediate block parent,
  // and then further expands to include any ancestors whose children are all
  // in the range
  *aHandled = true;
  nsTArray<OwningNonNull<nsINode>> nodeArray;
  rv = GetNodesFromSelection(aSelection, EditAction::align, nodeArray);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we don't have any nodes, or we have only a single br, then we are
  // creating an empty alignment div.  We have to do some different things for
  // these.
  bool emptyDiv = nodeArray.IsEmpty();
  if (nodeArray.Length() == 1) {
    OwningNonNull<nsINode> node = nodeArray[0];

    if (nsHTMLEditUtils::SupportsAlignAttr(GetAsDOMNode(node))) {
      // The node is a table element, an hr, a paragraph, a div or a section
      // header; in HTML 4, it can directly carry the ALIGN attribute and we
      // don't need to make a div! If we are in CSS mode, all the work is done
      // in AlignBlock
      rv = AlignBlock(*node->AsElement(), aAlignType, ContentsOnly::yes);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }

    if (nsTextEditUtils::IsBreak(node)) {
      // The special case emptyDiv code (below) that consumes BRs can cause
      // tables to split if the start node of the selection is not in a table
      // cell or caption, for example parent is a <tr>.  Avoid this unnecessary
      // splitting if possible by leaving emptyDiv FALSE so that we fall
      // through to the normal case alignment code.
      //
      // XXX: It seems a little error prone for the emptyDiv special case code
      // to assume that the start node of the selection is the parent of the
      // single node in the nodeArray, as the paragraph above points out. Do we
      // rely on the selection start node because of the fact that nodeArray
      // can be empty?  We should probably revisit this issue. - kin

      NS_ENSURE_STATE(aSelection.GetRangeAt(0) &&
                      aSelection.GetRangeAt(0)->GetStartParent());
      OwningNonNull<nsINode> parent =
        *aSelection.GetRangeAt(0)->GetStartParent();

      emptyDiv = !nsHTMLEditUtils::IsTableElement(parent) ||
                 nsHTMLEditUtils::IsTableCellOrCaption(parent);
    }
  }
  if (emptyDiv) {
    nsCOMPtr<nsINode> parent =
      aSelection.GetRangeAt(0) ? aSelection.GetRangeAt(0)->GetStartParent()
                               : nullptr;
    NS_ENSURE_STATE(parent);
    int32_t offset = aSelection.GetRangeAt(0)->StartOffset();

    rv = SplitAsNeeded(*nsGkAtoms::div, parent, offset);
    NS_ENSURE_SUCCESS(rv, rv);
    // Consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    nsCOMPtr<nsIContent> brContent =
      mHTMLEditor->GetNextHTMLNode(parent, offset);
    if (brContent && nsTextEditUtils::IsBreak(brContent)) {
      // Making use of html structure... if next node after where we are
      // putting our div is not a block, then the br we found is in same block
      // we are, so it's safe to consume it.
      nsCOMPtr<nsIContent> sibling = mHTMLEditor->GetNextHTMLSibling(parent,
                                                                     offset);
      if (sibling && !IsBlockNode(*sibling)) {
        rv = mHTMLEditor->DeleteNode(brContent);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    nsCOMPtr<Element> div = mHTMLEditor->CreateNode(nsGkAtoms::div, parent,
                                                    offset);
    NS_ENSURE_STATE(div);
    // Remember our new block for postprocessing
    mNewBlock = div;
    // Set up the alignment on the div, using HTML or CSS
    rv = AlignBlock(*div, aAlignType, ContentsOnly::yes);
    NS_ENSURE_SUCCESS(rv, rv);
    *aHandled = true;
    // Put in a moz-br so that it won't get deleted
    rv = CreateMozBR(div->AsDOMNode(), 0);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aSelection.Collapse(div, 0);
    // Don't reset our selection in this case.
    selectionResetter.Abort();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.

  nsTArray<bool> transitionList;
  MakeTransitionList(nodeArray, transitionList);

  // Okay, now go through all the nodes and give them an align attrib or put
  // them in a div, or whatever is appropriate.  Woohoo!

  nsCOMPtr<Element> curDiv;
  bool useCSS = mHTMLEditor->IsCSSEnabled();
  for (size_t i = 0; i < nodeArray.Length(); i++) {
    auto& curNode = nodeArray[i];
    // Here's where we actually figure out what to do

    // Ignore all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(curNode)) {
      continue;
    }

    // The node is a table element, an hr, a paragraph, a div or a section
    // header; in HTML 4, it can directly carry the ALIGN attribute and we
    // don't need to nest it, just set the alignment.  In CSS, assign the
    // corresponding CSS styles in AlignBlock
    if (nsHTMLEditUtils::SupportsAlignAttr(GetAsDOMNode(curNode))) {
      rv = AlignBlock(*curNode->AsElement(), aAlignType, ContentsOnly::no);
      NS_ENSURE_SUCCESS(rv, rv);
      // Clear out curDiv so that we don't put nodes after this one into it
      curDiv = nullptr;
      continue;
    }

    nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;

    // Skip insignificant formatting text nodes to prevent unnecessary
    // structure splitting!
    bool isEmptyTextNode = false;
    if (curNode->GetAsText() &&
        ((nsHTMLEditUtils::IsTableElement(curParent) &&
          !nsHTMLEditUtils::IsTableCellOrCaption(*curParent)) ||
         nsHTMLEditUtils::IsList(curParent) ||
         (NS_SUCCEEDED(mHTMLEditor->IsEmptyNode(curNode, &isEmptyTextNode)) &&
          isEmptyTextNode))) {
      continue;
    }

    // If it's a list item, or a list inside a list, forget any "current" div,
    // and instead put divs inside the appropriate block (td, li, etc)
    if (nsHTMLEditUtils::IsListItem(curNode) ||
        nsHTMLEditUtils::IsList(curNode)) {
      rv = RemoveAlignment(GetAsDOMNode(curNode), aAlignType, true);
      NS_ENSURE_SUCCESS(rv, rv);
      if (useCSS) {
        mHTMLEditor->mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(
            curNode->AsElement(), nullptr, &NS_LITERAL_STRING("align"),
            &aAlignType, false);
        curDiv = nullptr;
        continue;
      } else if (nsHTMLEditUtils::IsList(curParent)) {
        // If we don't use CSS, add a contraint to list element: they have to
        // be inside another list, i.e., >= second level of nesting
        rv = AlignInnerBlocks(*curNode, &aAlignType);
        NS_ENSURE_SUCCESS(rv, rv);
        curDiv = nullptr;
        continue;
      }
      // Clear out curDiv so that we don't put nodes after this one into it
    }

    // Need to make a div to put things in if we haven't already, or if this
    // node doesn't go in div we used earlier.
    if (!curDiv || transitionList[i]) {
      // First, check that our element can contain a div.
      if (!mEditor->CanContainTag(*curParent, *nsGkAtoms::div)) {
        // Cancelled
        return NS_OK;
      }

      rv = SplitAsNeeded(*nsGkAtoms::div, curParent, offset);
      NS_ENSURE_SUCCESS(rv, rv);
      curDiv = mHTMLEditor->CreateNode(nsGkAtoms::div, curParent, offset);
      NS_ENSURE_STATE(curDiv);
      // Remember our new block for postprocessing
      mNewBlock = curDiv;
      // Set up the alignment on the div
      rv = AlignBlock(*curDiv, aAlignType, ContentsOnly::yes);
    }

    NS_ENSURE_STATE(curNode->IsContent());

    // Tuck the node into the end of the active div
    rv = mHTMLEditor->MoveNode(curNode->AsContent(), curDiv, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// AlignInnerBlocks: Align inside table cells or list items
//
nsresult
nsHTMLEditRules::AlignInnerBlocks(nsINode& aNode, const nsAString* alignType)
{
  NS_ENSURE_TRUE(alignType, NS_ERROR_NULL_POINTER);

  // Gather list of table cells or list items
  nsTArray<OwningNonNull<nsINode>> nodeArray;
  nsTableCellAndListItemFunctor functor;
  nsDOMIterator iter(aNode);
  iter.AppendList(functor, nodeArray);

  // Now that we have the list, align their contents as requested
  for (auto& node : nodeArray) {
    nsresult res = AlignBlockContents(GetAsDOMNode(node), alignType);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// AlignBlockContents: align contents of a block element
//
nsresult
nsHTMLEditRules::AlignBlockContents(nsIDOMNode *aNode, const nsAString *alignType)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node && alignType, NS_ERROR_NULL_POINTER);
  nsresult res = NS_OK;
  nsCOMPtr<nsIContent> firstChild, lastChild;
  nsCOMPtr<Element> divNode;

  bool useCSS = mHTMLEditor->IsCSSEnabled();

  NS_ENSURE_STATE(mHTMLEditor);
  firstChild = mHTMLEditor->GetFirstEditableChild(*node);
  NS_ENSURE_STATE(mHTMLEditor);
  lastChild = mHTMLEditor->GetLastEditableChild(*node);
  NS_NAMED_LITERAL_STRING(attr, "align");
  if (!firstChild)
  {
    // this cell has no content, nothing to align
  } else if (firstChild == lastChild &&
             firstChild->IsHTMLElement(nsGkAtoms::div)) {
    // the cell already has a div containing all of its content: just
    // act on this div.
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(firstChild);
    if (useCSS) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttributeOrEquivalent(divElem, attr, *alignType, false);
    }
    else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  else
  {
    // else we need to put in a div, set the alignment, and toss in all the children
    NS_ENSURE_STATE(mHTMLEditor);
    divNode = mHTMLEditor->CreateNode(nsGkAtoms::div, node, 0);
    NS_ENSURE_STATE(divNode);
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(divNode);
    if (useCSS) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttributeOrEquivalent(divElem, attr, *alignType, false);
    }
    else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    }
    NS_ENSURE_SUCCESS(res, res);
    // tuck the children into the end of the active div
    while (lastChild && (lastChild != divNode))
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->MoveNode(lastChild, divNode, 0);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      lastChild = mHTMLEditor->GetLastEditableChild(*node);
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// CheckForEmptyBlock: Called by WillDeleteSelection to detect and handle
//                     case of deleting from inside an empty block.
//
nsresult
nsHTMLEditRules::CheckForEmptyBlock(nsINode* aStartNode,
                                    Element* aBodyNode,
                                    Selection* aSelection,
                                    nsIEditor::EDirection aAction,
                                    bool* aHandled)
{
  // If the editing host is an inline element, bail out early.
  if (aBodyNode && IsInlineNode(*aBodyNode)) {
    return NS_OK;
  }
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // If we are inside an empty block, delete it.  Note: do NOT delete table
  // elements this way.
  nsCOMPtr<Element> block = mHTMLEditor->GetBlock(*aStartNode);
  bool bIsEmptyNode;
  nsCOMPtr<Element> emptyBlock;
  nsresult res;
  if (block && block != aBodyNode) {
    // Efficiency hack, avoiding IsEmptyNode() call when in body
    res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
    NS_ENSURE_SUCCESS(res, res);
    while (block && bIsEmptyNode && !nsHTMLEditUtils::IsTableElement(block) &&
           block != aBodyNode) {
      emptyBlock = block;
      block = mHTMLEditor->GetBlockNodeParent(emptyBlock);
      if (block) {
        res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
  }

  if (emptyBlock && emptyBlock->IsEditable()) {
    nsCOMPtr<nsINode> blockParent = emptyBlock->GetParentNode();
    NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);
    int32_t offset = blockParent->IndexOf(emptyBlock);

    if (nsHTMLEditUtils::IsListItem(emptyBlock)) {
      // Are we the first list item in the list?
      bool bIsFirst;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->IsFirstEditableChild(GetAsDOMNode(emptyBlock),
                                              &bIsFirst);
      NS_ENSURE_SUCCESS(res, res);
      if (bIsFirst) {
        nsCOMPtr<nsINode> listParent = blockParent->GetParentNode();
        NS_ENSURE_TRUE(listParent, NS_ERROR_FAILURE);
        int32_t listOffset = listParent->IndexOf(blockParent);
        // If we are a sublist, skip the br creation
        if (!nsHTMLEditUtils::IsList(listParent)) {
          // Create a br before list
          NS_ENSURE_STATE(mHTMLEditor);
          nsCOMPtr<Element> br =
            mHTMLEditor->CreateBR(listParent, listOffset);
          NS_ENSURE_STATE(br);
          // Adjust selection to be right before it
          res = aSelection->Collapse(listParent, listOffset);
          NS_ENSURE_SUCCESS(res, res);
        }
        // Else just let selection percolate up.  We'll adjust it in
        // AfterEdit()
      }
    } else {
      if (aAction == nsIEditor::eNext) {
        // Adjust selection to be right after it.
        res = aSelection->Collapse(blockParent, offset + 1);
        NS_ENSURE_SUCCESS(res, res);

        // Move to the start of the next node if it's a text.
        nsCOMPtr<nsIContent> nextNode = mHTMLEditor->GetNextNode(blockParent,
                                                                 offset + 1, true);
        if (nextNode && mHTMLEditor->IsTextNode(nextNode)) {
          res = aSelection->Collapse(nextNode, 0);
          NS_ENSURE_SUCCESS(res, res);
        }
      } else {
        // Move to the end of the previous node if it's a text.
        nsCOMPtr<nsIContent> priorNode = mHTMLEditor->GetPriorNode(blockParent,
                                                                   offset,
                                                                   true);
        if (priorNode && mHTMLEditor->IsTextNode(priorNode)) {
          res = aSelection->Collapse(priorNode, priorNode->TextLength());
          NS_ENSURE_SUCCESS(res, res);
        } else {
          res = aSelection->Collapse(blockParent, offset + 1);
          NS_ENSURE_SUCCESS(res, res);
        }
      }
    }
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteNode(emptyBlock);
    *aHandled = true;
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

Element*
nsHTMLEditRules::CheckForInvisibleBR(Element& aBlock, BRLocation aWhere,
                                     int32_t aOffset)
{
  nsCOMPtr<nsINode> testNode;
  int32_t testOffset = 0;

  if (aWhere == BRLocation::blockEnd) {
    // No block crossing
    nsCOMPtr<nsIContent> rightmostNode =
      mHTMLEditor->GetRightmostChild(&aBlock, true);

    if (!rightmostNode) {
      return nullptr;
    }

    testNode = rightmostNode->GetParentNode();
    // Use offset + 1, so last node is included in our evaluation
    testOffset = testNode->IndexOf(rightmostNode) + 1;
  } else if (aOffset) {
    testNode = &aBlock;
    // We'll check everything to the left of the input position
    testOffset = aOffset;
  } else {
    return nullptr;
  }

  nsWSRunObject wsTester(mHTMLEditor, testNode, testOffset);
  if (WSType::br == wsTester.mStartReason) {
    return wsTester.mStartReasonNode->AsElement();
  }

  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// GetInnerContent: aLists and aTables allow the caller to specify what kind of
//                  content to "look inside".  If aTables is Tables::yes, look
//                  inside any table content, and insert the inner content into
//                  the supplied issupportsarray at offset aIndex.  Similarly
//                  with aLists and list content.  aIndex is updated to point
//                  past inserted elements.
//
void
nsHTMLEditRules::GetInnerContent(nsINode& aNode,
                                 nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                                 int32_t* aIndex, Lists aLists, Tables aTables)
{
  MOZ_ASSERT(aIndex);

  for (nsCOMPtr<nsIContent> node = mHTMLEditor->GetFirstEditableChild(aNode);
       node; node = node->GetNextSibling()) {
    if ((aLists == Lists::yes && (nsHTMLEditUtils::IsList(node) ||
                                  nsHTMLEditUtils::IsListItem(node))) ||
        (aTables == Tables::yes && nsHTMLEditUtils::IsTableElement(node))) {
      GetInnerContent(*node, aOutArrayOfNodes, aIndex, aLists, aTables);
    } else {
      aOutArrayOfNodes.InsertElementAt(*aIndex, *node);
      (*aIndex)++;
    }
  }
}

/**
 * Promotes selection to include blocks that have all their children selected.
 */
nsresult
nsHTMLEditRules::ExpandSelectionForDeletion(Selection& aSelection)
{
  // Don't need to touch collapsed selections
  if (aSelection.Collapsed()) {
    return NS_OK;
  }

  // We don't need to mess with cell selections, and we assume multirange
  // selections are those.
  if (aSelection.RangeCount() != 1) {
    return NS_OK;
  }

  // Find current sel start and end
  NS_ENSURE_TRUE(aSelection.GetRangeAt(0), NS_ERROR_NULL_POINTER);
  OwningNonNull<nsRange> range = *aSelection.GetRangeAt(0);

  nsCOMPtr<nsINode> selStartNode = range->GetStartParent();
  int32_t selStartOffset = range->StartOffset();
  nsCOMPtr<nsINode> selEndNode = range->GetEndParent();
  int32_t selEndOffset = range->EndOffset();

  // Find current selection common block parent
  nsCOMPtr<Element> selCommon =
    nsHTMLEditor::GetBlock(*range->GetCommonAncestor());
  NS_ENSURE_STATE(selCommon);

  // Set up for loops and cache our root element
  nsCOMPtr<nsINode> firstBRParent;
  nsCOMPtr<nsINode> unused;
  int32_t visOffset = 0, firstBROffset = 0;
  WSType wsType;
  nsCOMPtr<Element> root = mHTMLEditor->GetActiveEditingHost();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  // Find previous visible thingy before start of selection
  if (selStartNode != selCommon && selStartNode != root) {
    while (true) {
      nsWSRunObject wsObj(mHTMLEditor, selStartNode, selStartOffset);
      wsObj.PriorVisibleNode(selStartNode, selStartOffset, address_of(unused),
                             &visOffset, &wsType);
      if (wsType != WSType::thisBlock) {
        break;
      }
      // We want to keep looking up.  But stop if we are crossing table
      // element boundaries, or if we hit the root.
      if (nsHTMLEditUtils::IsTableElement(wsObj.mStartReasonNode) ||
          selCommon == wsObj.mStartReasonNode ||
          root == wsObj.mStartReasonNode) {
        break;
      }
      selStartNode = wsObj.mStartReasonNode->GetParentNode();
      selStartOffset = selStartNode ?
        selStartNode->IndexOf(wsObj.mStartReasonNode) : -1;
    }
  }

  // Find next visible thingy after end of selection
  if (selEndNode != selCommon && selEndNode != root) {
    while (true) {
      nsWSRunObject wsObj(mHTMLEditor, selEndNode, selEndOffset);
      wsObj.NextVisibleNode(selEndNode, selEndOffset, address_of(unused),
                            &visOffset, &wsType);
      if (wsType == WSType::br) {
        if (mHTMLEditor->IsVisBreak(wsObj.mEndReasonNode)) {
          break;
        }
        if (!firstBRParent) {
          firstBRParent = selEndNode;
          firstBROffset = selEndOffset;
        }
        selEndNode = wsObj.mEndReasonNode->GetParentNode();
        selEndOffset = selEndNode
          ? selEndNode->IndexOf(wsObj.mEndReasonNode) + 1 : 0;
      } else if (wsType == WSType::thisBlock) {
        // We want to keep looking up.  But stop if we are crossing table
        // element boundaries, or if we hit the root.
        if (nsHTMLEditUtils::IsTableElement(wsObj.mEndReasonNode) ||
            selCommon == wsObj.mEndReasonNode ||
            root == wsObj.mEndReasonNode) {
          break;
        }
        selEndNode = wsObj.mEndReasonNode->GetParentNode();
        selEndOffset = 1 + selEndNode->IndexOf(wsObj.mEndReasonNode);
      } else {
        break;
      }
    }
  }
  // Now set the selection to the new range
  aSelection.Collapse(selStartNode, selStartOffset);

  // Expand selection endpoint only if we didn't pass a br, or if we really
  // needed to pass that br (i.e., its block is now totally selected)
  nsresult res;
  bool doEndExpansion = true;
  if (firstBRParent) {
    // Find block node containing br
    nsCOMPtr<Element> brBlock = nsHTMLEditor::GetBlock(*firstBRParent);
    bool nodeBefore = false, nodeAfter = false;

    // Create a range that represents expanded selection
    RefPtr<nsRange> range = new nsRange(selStartNode);
    res = range->SetStart(selStartNode, selStartOffset);
    NS_ENSURE_SUCCESS(res, res);
    res = range->SetEnd(selEndNode, selEndOffset);
    NS_ENSURE_SUCCESS(res, res);

    // Check if block is entirely inside range
    if (brBlock) {
      nsRange::CompareNodeToRange(brBlock, range, &nodeBefore, &nodeAfter);
    }

    // If block isn't contained, forgo grabbing the br in expanded selection
    if (nodeBefore || nodeAfter) {
      doEndExpansion = false;
    }
  }
  if (doEndExpansion) {
    res = aSelection.Extend(selEndNode, selEndOffset);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    // Only expand to just before br
    res = aSelection.Extend(firstBRParent, firstBROffset);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// NormalizeSelection:  tweak non-collapsed selections to be more "natural".
//    Idea here is to adjust selection endpoint so that they do not cross
//    breaks or block boundaries unless something editable beyond that boundary
//    is also selected.  This adjustment makes it much easier for the various
//    block operations to determine what nodes to act on.
//
nsresult
nsHTMLEditRules::NormalizeSelection(Selection* inSelection)
{
  NS_ENSURE_TRUE(inSelection, NS_ERROR_NULL_POINTER);

  // don't need to touch collapsed selections
  if (inSelection->Collapsed()) {
    return NS_OK;
  }

  int32_t rangeCount;
  nsresult res = inSelection->GetRangeCount(&rangeCount);
  NS_ENSURE_SUCCESS(res, res);

  // we don't need to mess with cell selections, and we assume multirange selections are those.
  if (rangeCount != 1) return NS_OK;

  RefPtr<nsRange> range = inSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  int32_t startOffset, endOffset;
  nsCOMPtr<nsIDOMNode> newStartNode, newEndNode;
  int32_t newStartOffset, newEndOffset;

  res = range->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetStartOffset(&startOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetEndContainer(getter_AddRefs(endNode));
  NS_ENSURE_SUCCESS(res, res);
  res = range->GetEndOffset(&endOffset);
  NS_ENSURE_SUCCESS(res, res);

  // adjusted values default to original values
  newStartNode = startNode;
  newStartOffset = startOffset;
  newEndNode = endNode;
  newEndOffset = endOffset;

  // some locals we need for whitespace code
  nsCOMPtr<nsINode> unused;
  int32_t offset;
  WSType wsType;

  // let the whitespace code do the heavy lifting
  nsWSRunObject wsEndObj(mHTMLEditor, endNode, endOffset);
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  nsCOMPtr<nsINode> endNode_(do_QueryInterface(endNode));
  wsEndObj.PriorVisibleNode(endNode_, endOffset, address_of(unused),
                            &offset, &wsType);
  if (wsType != WSType::text && wsType != WSType::normalWS) {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsEndObj.mStartReason == WSType::otherBlock) {
      // endpoint is just after the close of a block.
      nsCOMPtr<nsIDOMNode> child =
        GetAsDOMNode(mHTMLEditor->GetRightmostChild(wsEndObj.mStartReasonNode,
                                                    true));
      if (child)
      {
        newEndNode = nsEditor::GetNodeLocation(child, &newEndOffset);
        ++newEndOffset; // offset *after* child
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsEndObj.mStartReason == WSType::thisBlock) {
      // endpoint is just after start of this block
      nsCOMPtr<nsIDOMNode> child;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetPriorHTMLNode(endNode, endOffset, address_of(child));
      if (child)
      {
        newEndNode = nsEditor::GetNodeLocation(child, &newEndOffset);
        ++newEndOffset; // offset *after* child
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsEndObj.mStartReason == WSType::br) {
      // endpoint is just after break.  lets adjust it to before it.
      newEndNode = nsEditor::GetNodeLocation(GetAsDOMNode(wsEndObj.mStartReasonNode),
                                             &newEndOffset);
    }
  }


  // similar dealio for start of range
  nsWSRunObject wsStartObj(mHTMLEditor, startNode, startOffset);
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  nsCOMPtr<nsINode> startNode_(do_QueryInterface(startNode));
  wsStartObj.NextVisibleNode(startNode_, startOffset, address_of(unused),
                             &offset, &wsType);
  if (wsType != WSType::text && wsType != WSType::normalWS) {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsStartObj.mEndReason == WSType::otherBlock) {
      // startpoint is just before the start of a block.
      nsCOMPtr<nsIDOMNode> child =
        GetAsDOMNode(mHTMLEditor->GetLeftmostChild(wsStartObj.mEndReasonNode,
                                                   true));
      if (child)
      {
        newStartNode = nsEditor::GetNodeLocation(child, &newStartOffset);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsStartObj.mEndReason == WSType::thisBlock) {
      // startpoint is just before end of this block
      nsCOMPtr<nsIDOMNode> child;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLNode(startNode, startOffset, address_of(child));
      if (child)
      {
        newStartNode = nsEditor::GetNodeLocation(child, &newStartOffset);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsStartObj.mEndReason == WSType::br) {
      // startpoint is just before a break.  lets adjust it to after it.
      newStartNode = nsEditor::GetNodeLocation(GetAsDOMNode(wsStartObj.mEndReasonNode),
                                               &newStartOffset);
      ++newStartOffset; // offset *after* break
    }
  }

  // there is a demented possiblity we have to check for.  We might have a very strange selection
  // that is not collapsed and yet does not contain any editable content, and satisfies some of the
  // above conditions that cause tweaking.  In this case we don't want to tweak the selection into
  // a block it was never in, etc.  There are a variety of strategies one might use to try to
  // detect these cases, but I think the most straightforward is to see if the adjusted locations
  // "cross" the old values: ie, new end before old start, or new start after old end.  If so
  // then just leave things alone.

  int16_t comp;
  comp = nsContentUtils::ComparePoints(startNode, startOffset,
                                       newEndNode, newEndOffset);
  if (comp == 1) return NS_OK;  // new end before old start
  comp = nsContentUtils::ComparePoints(newStartNode, newStartOffset,
                                       endNode, endOffset);
  if (comp == 1) return NS_OK;  // new start after old end

  // otherwise set selection to new values.
  inSelection->Collapse(newStartNode, newStartOffset);
  inSelection->Extend(newEndNode, newEndOffset);
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetPromotedPoint: figure out where a start or end point for a block
//                   operation really is
void
nsHTMLEditRules::GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode* aNode,
                                  int32_t aOffset,
                                  EditAction actionID,
                                  nsCOMPtr<nsIDOMNode>* outNode,
                                  int32_t* outOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  MOZ_ASSERT(node && outNode && outOffset);

  // default values
  *outNode = node->AsDOMNode();
  *outOffset = aOffset;

  // we do one thing for text actions, something else entirely for other
  // actions
  if (actionID == EditAction::insertText ||
      actionID == EditAction::insertIMEText ||
      actionID == EditAction::insertBreak ||
      actionID == EditAction::deleteText) {
    bool isSpace, isNBSP;
    nsCOMPtr<nsIContent> content = do_QueryInterface(node), temp;
    // for text actions, we want to look backwards (or forwards, as
    // appropriate) for additional whitespace or nbsp's.  We may have to act on
    // these later even though they are outside of the initial selection.  Even
    // if they are in another node!
    while (content) {
      int32_t offset;
      if (aWhere == kStart) {
        NS_ENSURE_TRUE(mHTMLEditor, /* void */);
        mHTMLEditor->IsPrevCharInNodeWhitespace(content, *outOffset,
                                                &isSpace, &isNBSP,
                                                getter_AddRefs(temp), &offset);
      } else {
        NS_ENSURE_TRUE(mHTMLEditor, /* void */);
        mHTMLEditor->IsNextCharInNodeWhitespace(content, *outOffset,
                                                &isSpace, &isNBSP,
                                                getter_AddRefs(temp), &offset);
      }
      if (isSpace || isNBSP) {
        content = temp;
        *outOffset = offset;
      } else {
        break;
      }
    }

    *outNode = content->AsDOMNode();
    return;
  }

  int32_t offset = aOffset;

  // else not a text section.  In this case we want to see if we should grab
  // any adjacent inline nodes and/or parents and other ancestors
  if (aWhere == kStart) {
    // some special casing for text nodes
    if (node->IsNodeOfType(nsINode::eTEXT)) {
      if (!node->GetParentNode()) {
        // Okay, can't promote any further
        return;
      }
      offset = node->GetParentNode()->IndexOf(node);
      node = node->GetParentNode();
    }

    // look back through any further inline nodes that aren't across a <br>
    // from us, and that are enclosed in the same block.
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    nsCOMPtr<nsINode> priorNode =
      mHTMLEditor->GetPriorHTMLNode(node, offset, true);

    while (priorNode && priorNode->GetParentNode() &&
           mHTMLEditor && !mHTMLEditor->IsVisBreak(priorNode) &&
           !IsBlockNode(*priorNode)) {
      offset = priorNode->GetParentNode()->IndexOf(priorNode);
      node = priorNode->GetParentNode();
      NS_ENSURE_TRUE(mHTMLEditor, /* void */);
      priorNode = mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    }

    // finding the real start for this point.  look up the tree for as long as
    // we are the first node in the container, and as long as we haven't hit
    // the body node.
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    nsCOMPtr<nsIContent> nearNode =
      mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    while (!nearNode && !node->IsHTMLElement(nsGkAtoms::body) &&
           node->GetParentNode()) {
      // some cutoffs are here: we don't need to also include them in the
      // aWhere == kEnd case.  as long as they are in one or the other it will
      // work.  special case for outdent: don't keep looking up if we have
      // found a blockquote element to act on
      if (actionID == EditAction::outdent &&
          node->IsHTMLElement(nsGkAtoms::blockquote)) {
        break;
      }

      int32_t parentOffset = node->GetParentNode()->IndexOf(node);
      nsCOMPtr<nsINode> parent = node->GetParentNode();

      // Don't walk past the editable section. Note that we need to check
      // before walking up to a parent because we need to return the parent
      // object, so the parent itself might not be in the editable area, but
      // it's OK if we're not performing a block-level action.
      bool blockLevelAction = actionID == EditAction::indent ||
                              actionID == EditAction::outdent ||
                              actionID == EditAction::align ||
                              actionID == EditAction::makeBasicBlock;
      NS_ENSURE_TRUE(mHTMLEditor, /* void */);
      if (!mHTMLEditor->IsDescendantOfEditorRoot(parent) &&
          (blockLevelAction || !mHTMLEditor ||
           !mHTMLEditor->IsDescendantOfEditorRoot(node))) {
        NS_ENSURE_TRUE(mHTMLEditor, /* void */);
        break;
      }

      node = parent;
      offset = parentOffset;
      NS_ENSURE_TRUE(mHTMLEditor, /* void */);
      nearNode = mHTMLEditor->GetPriorHTMLNode(node, offset, true);
    }
    *outNode = node->AsDOMNode();
    *outOffset = offset;
    return;
  }

  // aWhere == kEnd
  // some special casing for text nodes
  if (node->IsNodeOfType(nsINode::eTEXT)) {
    if (!node->GetParentNode()) {
      // Okay, can't promote any further
      return;
    }
    // want to be after the text node
    offset = 1 + node->GetParentNode()->IndexOf(node);
    node = node->GetParentNode();
  }

  // look ahead through any further inline nodes that aren't across a <br> from
  // us, and that are enclosed in the same block.
  NS_ENSURE_TRUE(mHTMLEditor, /* void */);
  nsCOMPtr<nsIContent> nextNode =
    mHTMLEditor->GetNextHTMLNode(node, offset, true);

  while (nextNode && !IsBlockNode(*nextNode) && nextNode->GetParentNode()) {
    offset = 1 + nextNode->GetParentNode()->IndexOf(nextNode);
    node = nextNode->GetParentNode();
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    if (mHTMLEditor->IsVisBreak(nextNode)) {
      break;
    }

    // Check for newlines in pre-formatted text nodes.
    bool isPRE;
    mHTMLEditor->IsPreformatted(nextNode->AsDOMNode(), &isPRE);
    if (isPRE) {
      nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(nextNode);
      if (textNode) {
        nsAutoString tempString;
        textNode->GetData(tempString);
        int32_t newlinePos = tempString.FindChar(nsCRT::LF);
        if (newlinePos >= 0) {
          if ((uint32_t)newlinePos + 1 == tempString.Length()) {
            // No need for special processing if the newline is at the end.
            break;
          }
          *outNode = nextNode->AsDOMNode();
          *outOffset = newlinePos + 1;
          return;
        }
      }
    }
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    nextNode = mHTMLEditor->GetNextHTMLNode(node, offset, true);
  }

  // finding the real end for this point.  look up the tree for as long as we
  // are the last node in the container, and as long as we haven't hit the body
  // node.
  NS_ENSURE_TRUE(mHTMLEditor, /* void */);
  nsCOMPtr<nsIContent> nearNode =
    mHTMLEditor->GetNextHTMLNode(node, offset, true);
  while (!nearNode && !node->IsHTMLElement(nsGkAtoms::body) &&
         node->GetParentNode()) {
    int32_t parentOffset = node->GetParentNode()->IndexOf(node);
    nsCOMPtr<nsINode> parent = node->GetParentNode();

    // Don't walk past the editable section. Note that we need to check before
    // walking up to a parent because we need to return the parent object, so
    // the parent itself might not be in the editable area, but it's OK.
    if ((!mHTMLEditor || !mHTMLEditor->IsDescendantOfEditorRoot(node)) &&
        (!mHTMLEditor || !mHTMLEditor->IsDescendantOfEditorRoot(parent))) {
      NS_ENSURE_TRUE(mHTMLEditor, /* void */);
      break;
    }

    node = parent;
    // we want to be AFTER nearNode
    offset = parentOffset + 1;
    NS_ENSURE_TRUE(mHTMLEditor, /* void */);
    nearNode = mHTMLEditor->GetNextHTMLNode(node, offset, true);
  }
  *outNode = node->AsDOMNode();
  *outOffset = offset;
}


///////////////////////////////////////////////////////////////////////////////
// GetPromotedRanges: Run all the selection range endpoint through
//                    GetPromotedPoint()
//
void
nsHTMLEditRules::GetPromotedRanges(Selection& aSelection,
                                   nsTArray<RefPtr<nsRange>>& outArrayOfRanges,
                                   EditAction inOperationType)
{
  uint32_t rangeCount = aSelection.RangeCount();

  for (uint32_t i = 0; i < rangeCount; i++) {
    RefPtr<nsRange> selectionRange = aSelection.GetRangeAt(i);
    MOZ_ASSERT(selectionRange);

    // Clone range so we don't muck with actual selection ranges
    RefPtr<nsRange> opRange = selectionRange->CloneRange();

    // Make a new adjusted range to represent the appropriate block content.
    // The basic idea is to push out the range endpoints to truly enclose the
    // blocks that we will affect.  This call alters opRange.
    PromoteRange(*opRange, inOperationType);

    // Stuff new opRange into array
    outArrayOfRanges.AppendElement(opRange);
  }
}


///////////////////////////////////////////////////////////////////////////////
// PromoteRange: Expand a range to include any parents for which all editable
//               children are already in range.
//
void
nsHTMLEditRules::PromoteRange(nsRange& aRange, EditAction aOperationType)
{
  NS_ENSURE_TRUE(mHTMLEditor, );
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  nsCOMPtr<nsINode> startNode = aRange.GetStartParent();
  nsCOMPtr<nsINode> endNode = aRange.GetEndParent();
  int32_t startOffset = aRange.StartOffset();
  int32_t endOffset = aRange.EndOffset();

  // MOOSE major hack:
  // GetPromotedPoint doesn't really do the right thing for collapsed ranges
  // inside block elements that contain nothing but a solo <br>.  It's easier
  // to put a workaround here than to revamp GetPromotedPoint.  :-(
  if (startNode == endNode && startOffset == endOffset) {
    nsCOMPtr<Element> block = mHTMLEditor->GetBlock(*startNode);
    if (block) {
      bool bIsEmptyNode = false;
      nsCOMPtr<nsIContent> root = mHTMLEditor->GetActiveEditingHost();
      // Make sure we don't go higher than our root element in the content tree
      NS_ENSURE_TRUE(root, );
      if (!nsContentUtils::ContentIsDescendantOf(root, block)) {
        mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
      }
      if (bIsEmptyNode) {
        startNode = block;
        endNode = block;
        startOffset = 0;
        endOffset = block->Length();
      }
    }
  }

  // Make a new adjusted range to represent the appropriate block content.
  // This is tricky.  The basic idea is to push out the range endpoints to
  // truly enclose the blocks that we will affect.

  nsCOMPtr<nsIDOMNode> opStartNode;
  nsCOMPtr<nsIDOMNode> opEndNode;
  int32_t opStartOffset, opEndOffset;
  RefPtr<nsRange> opRange;

  GetPromotedPoint(kStart, GetAsDOMNode(startNode), startOffset,
                   aOperationType, address_of(opStartNode), &opStartOffset);
  GetPromotedPoint(kEnd, GetAsDOMNode(endNode), endOffset, aOperationType,
                   address_of(opEndNode), &opEndOffset);

  // Make sure that the new range ends up to be in the editable section.
  if (!mHTMLEditor->IsDescendantOfEditorRoot(
        nsEditor::GetNodeAtRangeOffsetPoint(opStartNode, opStartOffset)) ||
      !mHTMLEditor->IsDescendantOfEditorRoot(
        nsEditor::GetNodeAtRangeOffsetPoint(opEndNode, opEndOffset - 1))) {
    return;
  }

  DebugOnly<nsresult> res = aRange.SetStart(opStartNode, opStartOffset);
  MOZ_ASSERT(NS_SUCCEEDED(res));
  res = aRange.SetEnd(opEndNode, opEndOffset);
  MOZ_ASSERT(NS_SUCCEEDED(res));
}

class nsUniqueFunctor : public nsBoolDomIterFunctor
{
public:
  explicit nsUniqueFunctor(nsTArray<OwningNonNull<nsINode>> &aArray) : mArray(aArray)
  {
  }
  // used to build list of all nodes iterator covers
  virtual bool operator()(nsINode* aNode) const
  {
    return !mArray.Contains(aNode);
  }

private:
  nsTArray<OwningNonNull<nsINode>>& mArray;
};

///////////////////////////////////////////////////////////////////////////////
// GetNodesForOperation: Run through the ranges in the array and construct a
//                       new array of nodes to be acted on.
//
nsresult
nsHTMLEditRules::GetNodesForOperation(nsTArray<RefPtr<nsRange>>& aArrayOfRanges,
                                      nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                                      EditAction aOperationType,
                                      TouchContent aTouchContent)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  int32_t rangeCount = aArrayOfRanges.Length();
  nsresult res = NS_OK;

  if (aTouchContent == TouchContent::yes) {
    // Split text nodes. This is necessary, since GetPromotedPoint() may return a
    // range ending in a text node in case where part of a pre-formatted
    // elements needs to be moved.
    for (int32_t i = 0; i < rangeCount; i++) {
      RefPtr<nsRange> r = aArrayOfRanges[i];
      nsCOMPtr<nsIContent> endParent = do_QueryInterface(r->GetEndParent());
      if (!mHTMLEditor->IsTextNode(endParent)) {
        continue;
      }
      nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(endParent);
      if (textNode) {
        int32_t offset = r->EndOffset();
        nsAutoString tempString;
        textNode->GetData(tempString);

        if (0 < offset && offset < (int32_t)(tempString.Length())) {
          // Split the text node.
          nsCOMPtr<nsIDOMNode> tempNode;
          res = mHTMLEditor->SplitNode(endParent->AsDOMNode(), offset,
                                       getter_AddRefs(tempNode));
          NS_ENSURE_SUCCESS(res, res);

          // Correct the range.
          // The new end parent becomes the parent node of the text.
          nsCOMPtr<nsIContent> newParent = endParent->GetParent();
          r->SetEnd(newParent, newParent->IndexOf(endParent));
        }
      }
    }
  }

  // Bust up any inlines that cross our range endpoints, but only if we are
  // allowed to touch content.

  if (aTouchContent == TouchContent::yes) {
    nsTArray<OwningNonNull<nsRangeStore>> rangeItemArray;
    rangeItemArray.AppendElements(rangeCount);

    // First register ranges for special editor gravity
    for (int32_t i = 0; i < rangeCount; i++) {
      rangeItemArray[i] = new nsRangeStore();
      rangeItemArray[i]->StoreRange(aArrayOfRanges[0]);
      mHTMLEditor->mRangeUpdater.RegisterRangeItem(rangeItemArray[i]);
      aArrayOfRanges.RemoveElementAt(0);
    }
    // Now bust up inlines.
    for (auto& item : Reversed(rangeItemArray)) {
      res = BustUpInlinesAtRangeEndpoints(*item);
      if (NS_FAILED(res)) {
        break;
      }
    }
    // Then unregister the ranges
    for (auto& item : rangeItemArray) {
      mHTMLEditor->mRangeUpdater.DropRangeItem(item);
      aArrayOfRanges.AppendElement(item->GetRange());
    }
    NS_ENSURE_SUCCESS(res, res);
  }
  // Gather up a list of all the nodes
  for (auto& range : aArrayOfRanges) {
    nsDOMSubtreeIterator iter;
    res = iter.Init(*range);
    NS_ENSURE_SUCCESS(res, res);
    if (aOutArrayOfNodes.Length() == 0) {
      iter.AppendList(nsTrivialFunctor(), aOutArrayOfNodes);
    } else {
      // We don't want duplicates in aOutArrayOfNodes, so we use an
      // iterator/functor that only return nodes that are not already in
      // aOutArrayOfNodes.
      nsTArray<OwningNonNull<nsINode>> nodes;
      iter.AppendList(nsUniqueFunctor(aOutArrayOfNodes), nodes);
      aOutArrayOfNodes.AppendElements(nodes);
    }
  }

  // Certain operations should not act on li's and td's, but rather inside
  // them.  Alter the list as needed.
  if (aOperationType == EditAction::makeBasicBlock) {
    for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
      OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
      if (nsHTMLEditUtils::IsListItem(node)) {
        int32_t j = i;
        aOutArrayOfNodes.RemoveElementAt(i);
        GetInnerContent(*node, aOutArrayOfNodes, &j);
      }
    }
  // Indent/outdent already do something special for list items, but we still
  // need to make sure we don't act on table elements
  } else if (aOperationType == EditAction::outdent ||
             aOperationType == EditAction::indent ||
             aOperationType == EditAction::setAbsolutePosition) {
    for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
      OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
      if (nsHTMLEditUtils::IsTableElementButNotTable(node)) {
        int32_t j = i;
        aOutArrayOfNodes.RemoveElementAt(i);
        GetInnerContent(*node, aOutArrayOfNodes, &j);
      }
    }
  }
  // Outdent should look inside of divs.
  if (aOperationType == EditAction::outdent &&
      !mHTMLEditor->IsCSSEnabled()) {
    for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
      OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
      if (node->IsHTMLElement(nsGkAtoms::div)) {
        int32_t j = i;
        aOutArrayOfNodes.RemoveElementAt(i);
        GetInnerContent(*node, aOutArrayOfNodes, &j, Lists::no, Tables::no);
      }
    }
  }


  // Post-process the list to break up inline containers that contain br's, but
  // only for operations that might care, like making lists or paragraphs
  if (aOperationType == EditAction::makeBasicBlock ||
      aOperationType == EditAction::makeList ||
      aOperationType == EditAction::align ||
      aOperationType == EditAction::setAbsolutePosition ||
      aOperationType == EditAction::indent ||
      aOperationType == EditAction::outdent) {
    for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
      OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
      if (aTouchContent == TouchContent::yes && IsInlineNode(node) &&
          mHTMLEditor->IsContainer(node) && !mHTMLEditor->IsTextNode(node)) {
        nsTArray<OwningNonNull<nsINode>> arrayOfInlines;
        res = BustUpInlinesAtBRs(*node->AsContent(), arrayOfInlines);
        NS_ENSURE_SUCCESS(res, res);

        // Put these nodes in aOutArrayOfNodes, replacing the current node
        aOutArrayOfNodes.RemoveElementAt(i);
        aOutArrayOfNodes.InsertElementsAt(i, arrayOfInlines);
      }
    }
  }
  return NS_OK;
}


void
nsHTMLEditRules::GetChildNodesForOperation(nsINode& aNode,
    nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes)
{
  for (nsCOMPtr<nsIContent> child = aNode.GetFirstChild();
       child; child = child->GetNextSibling()) {
    outArrayOfNodes.AppendElement(*child);
  }
}


nsresult
nsHTMLEditRules::GetListActionNodes(nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                                    EntireList aEntireList,
                                    TouchContent aTouchContent)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  // Added this in so that ui code can ask to change an entire list, even if
  // selection is only in part of it.  used by list item dialog.
  if (aEntireList == EntireList::yes) {
    uint32_t rangeCount = selection->RangeCount();
    for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
      RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
      for (nsCOMPtr<nsINode> parent = range->GetCommonAncestor();
           parent; parent = parent->GetParentNode()) {
        if (nsHTMLEditUtils::IsList(parent)) {
          aOutArrayOfNodes.AppendElement(*parent);
          break;
        }
      }
    }
    // If we didn't find any nodes this way, then try the normal way.  Perhaps
    // the selection spans multiple lists but with no common list parent.
    if (aOutArrayOfNodes.Length()) {
      return NS_OK;
    }
  }

  {
    // We don't like other people messing with our selection!
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);

    // contruct a list of nodes to act on.
    nsresult res = GetNodesFromSelection(*selection, EditAction::makeList,
                                         aOutArrayOfNodes, aTouchContent);
    NS_ENSURE_SUCCESS(res, res);
  }

  // Pre-process our list of nodes
  for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsINode> testNode = aOutArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(testNode)) {
      aOutArrayOfNodes.RemoveElementAt(i);
      continue;
    }

    // Scan for table elements and divs.  If we find table elements other than
    // table, replace it with a list of any editable non-table content.
    if (nsHTMLEditUtils::IsTableElementButNotTable(testNode)) {
      int32_t j = i;
      aOutArrayOfNodes.RemoveElementAt(i);
      GetInnerContent(*testNode, aOutArrayOfNodes, &j, Lists::no);
    }
  }

  // If there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.
  LookInsideDivBQandList(aOutArrayOfNodes);

  return NS_OK;
}


void
nsHTMLEditRules::LookInsideDivBQandList(nsTArray<OwningNonNull<nsINode>>& aNodeArray)
{
  NS_ENSURE_TRUE(mHTMLEditor, );
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // If there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.
  int32_t listCount = aNodeArray.Length();
  if (listCount != 1) {
    return;
  }

  OwningNonNull<nsINode> curNode = aNodeArray[0];

  while (curNode->IsHTMLElement(nsGkAtoms::div) ||
         nsHTMLEditUtils::IsList(curNode) ||
         curNode->IsHTMLElement(nsGkAtoms::blockquote)) {
    // Dive as long as there's only one child, and it's a list, div, blockquote
    uint32_t numChildren = mHTMLEditor->CountEditableChildren(curNode);
    if (numChildren != 1) {
      break;
    }

    // Keep diving!  XXX One would expect to dive into the one editable node.
    nsCOMPtr<nsIContent> child = curNode->GetFirstChild();
    if (!child->IsHTMLElement(nsGkAtoms::div) &&
        !nsHTMLEditUtils::IsList(child) &&
        !child->IsHTMLElement(nsGkAtoms::blockquote)) {
      break;
    }

    // check editability XXX floppy moose
    curNode = child;
  }

  // We've found innermost list/blockquote/div: replace the one node in the
  // array with these nodes
  aNodeArray.RemoveElementAt(0);
  if (curNode->IsAnyOfHTMLElements(nsGkAtoms::div,
                                   nsGkAtoms::blockquote)) {
    int32_t j = 0;
    GetInnerContent(*curNode, aNodeArray, &j, Lists::no, Tables::no);
    return;
  }

  aNodeArray.AppendElement(*curNode);
}


///////////////////////////////////////////////////////////////////////////
// GetDefinitionListItemTypes:
//
void
nsHTMLEditRules::GetDefinitionListItemTypes(dom::Element* aElement, bool* aDT, bool* aDD)
{
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(aElement->IsHTMLElement(nsGkAtoms::dl));
  MOZ_ASSERT(aDT);
  MOZ_ASSERT(aDD);

  *aDT = *aDD = false;
  for (nsIContent* child = aElement->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::dt)) {
      *aDT = true;
    } else if (child->IsHTMLElement(nsGkAtoms::dd)) {
      *aDD = true;
    }
  }
}

nsresult
nsHTMLEditRules::GetParagraphFormatNodes(nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                                         TouchContent aTouchContent)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  // Contruct a list of nodes to act on.
  nsresult res = GetNodesFromSelection(*selection, EditAction::makeBasicBlock,
                                       outArrayOfNodes, aTouchContent);
  NS_ENSURE_SUCCESS(res, res);

  // Pre-process our list of nodes
  for (int32_t i = outArrayOfNodes.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsINode> testNode = outArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(testNode)) {
      outArrayOfNodes.RemoveElementAt(i);
      continue;
    }

    // Scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.  Ditto for
    // list elements.
    if (nsHTMLEditUtils::IsTableElement(testNode) ||
        nsHTMLEditUtils::IsList(testNode) ||
        nsHTMLEditUtils::IsListItem(testNode)) {
      int32_t j = i;
      outArrayOfNodes.RemoveElementAt(i);
      GetInnerContent(testNode, outArrayOfNodes, &j);
    }
  }
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// BustUpInlinesAtRangeEndpoints:
//
nsresult
nsHTMLEditRules::BustUpInlinesAtRangeEndpoints(nsRangeStore &item)
{
  bool isCollapsed = ((item.startNode == item.endNode) && (item.startOffset == item.endOffset));

  nsCOMPtr<nsIContent> endInline = GetHighestInlineParent(*item.endNode);

  // if we have inline parents above range endpoints, split them
  if (endInline && !isCollapsed)
  {
    nsCOMPtr<nsINode> resultEndNode = endInline->GetParentNode();
    NS_ENSURE_STATE(mHTMLEditor);
    // item.endNode must be content if endInline isn't null
    int32_t resultEndOffset =
      mHTMLEditor->SplitNodeDeep(*endInline, *item.endNode->AsContent(),
                                 item.endOffset,
                                 nsEditor::EmptyContainers::no);
    NS_ENSURE_TRUE(resultEndOffset != -1, NS_ERROR_FAILURE);
    // reset range
    item.endNode = resultEndNode;
    item.endOffset = resultEndOffset;
  }

  nsCOMPtr<nsIContent> startInline = GetHighestInlineParent(*item.startNode);

  if (startInline)
  {
    nsCOMPtr<nsINode> resultStartNode = startInline->GetParentNode();
    NS_ENSURE_STATE(mHTMLEditor);
    int32_t resultStartOffset =
      mHTMLEditor->SplitNodeDeep(*startInline, *item.startNode->AsContent(),
                                 item.startOffset,
                                 nsEditor::EmptyContainers::no);
    NS_ENSURE_TRUE(resultStartOffset != -1, NS_ERROR_FAILURE);
    // reset range
    item.startNode = resultStartNode;
    item.startOffset = resultStartOffset;
  }

  return NS_OK;
}



nsresult
nsHTMLEditRules::BustUpInlinesAtBRs(nsIContent& aNode,
                                    nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // First build up a list of all the break nodes inside the inline container.
  nsTArray<OwningNonNull<nsINode>> arrayOfBreaks;
  nsBRNodeFunctor functor;
  nsDOMIterator iter(aNode);
  iter.AppendList(functor, arrayOfBreaks);

  // If there aren't any breaks, just put inNode itself in the array
  if (!arrayOfBreaks.Length()) {
    aOutArrayOfNodes.AppendElement(aNode);
    return NS_OK;
  }

  // Else we need to bust up inNode along all the breaks
  nsCOMPtr<nsINode> inlineParentNode = aNode.GetParentNode();
  nsCOMPtr<nsIContent> splitDeepNode = &aNode;
  nsCOMPtr<nsIContent> leftNode, rightNode;

  for (uint32_t i = 0; i < arrayOfBreaks.Length(); i++) {
    OwningNonNull<Element> breakNode = *arrayOfBreaks[i]->AsElement();
    NS_ENSURE_TRUE(splitDeepNode, NS_ERROR_NULL_POINTER);
    NS_ENSURE_TRUE(breakNode->GetParent(), NS_ERROR_NULL_POINTER);
    OwningNonNull<nsIContent> splitParentNode = *breakNode->GetParent();
    int32_t splitOffset = splitParentNode->IndexOf(breakNode);

    int32_t resultOffset =
      mHTMLEditor->SplitNodeDeep(*splitDeepNode, splitParentNode, splitOffset,
                                 nsHTMLEditor::EmptyContainers::yes,
                                 getter_AddRefs(leftNode),
                                 getter_AddRefs(rightNode));
    NS_ENSURE_STATE(resultOffset != -1);

    // Put left node in node list
    if (leftNode) {
      // Might not be a left node.  A break might have been at the very
      // beginning of inline container, in which case SplitNodeDeep would not
      // actually split anything
      aOutArrayOfNodes.AppendElement(*leftNode);
    }
    // Move break outside of container and also put in node list
    nsresult res = mHTMLEditor->MoveNode(breakNode, inlineParentNode,
                                         resultOffset);
    NS_ENSURE_SUCCESS(res, res);
    aOutArrayOfNodes.AppendElement(*breakNode);

    // Now rightNode becomes the new node to split
    splitDeepNode = rightNode;
  }
  // Now tack on remaining rightNode, if any, to the list
  if (rightNode) {
    aOutArrayOfNodes.AppendElement(*rightNode);
  }
  return NS_OK;
}


nsIContent*
nsHTMLEditRules::GetHighestInlineParent(nsINode& aNode)
{
  if (!aNode.IsContent() || IsBlockNode(aNode)) {
    return nullptr;
  }
  OwningNonNull<nsIContent> node = *aNode.AsContent();

  while (node->GetParent() && IsInlineNode(*node->GetParent())) {
    node = *node->GetParent();
  }
  return node;
}


///////////////////////////////////////////////////////////////////////////////
// GetNodesFromPoint: Given a particular operation, construct a list of nodes
//                    from a point that will be operated on.
//
nsresult
nsHTMLEditRules::GetNodesFromPoint(::DOMPoint aPoint,
                                   EditAction aOperation,
                                   nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                                   TouchContent aTouchContent)
{
  NS_ENSURE_STATE(aPoint.node);
  RefPtr<nsRange> range = new nsRange(aPoint.node);
  nsresult res = range->SetStart(aPoint.node, aPoint.offset);
  MOZ_ASSERT(NS_SUCCEEDED(res));

  // Expand the range to include adjacent inlines
  PromoteRange(*range, aOperation);

  // Make array of ranges
  nsTArray<RefPtr<nsRange>> arrayOfRanges;

  // Stuff new opRange into array
  arrayOfRanges.AppendElement(range);

  // Use these ranges to contruct a list of nodes to act on
  res = GetNodesForOperation(arrayOfRanges, outArrayOfNodes, aOperation,
                             aTouchContent);
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// GetNodesFromSelection: Given a particular operation, construct a list of
//                        nodes from the selection that will be operated on.
//
nsresult
nsHTMLEditRules::GetNodesFromSelection(Selection& aSelection,
                                       EditAction aOperation,
                                       nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                                       TouchContent aTouchContent)
{
  // Promote selection ranges
  nsTArray<RefPtr<nsRange>> arrayOfRanges;
  GetPromotedRanges(aSelection, arrayOfRanges, aOperation);

  // Use these ranges to contruct a list of nodes to act on.
  nsresult res = GetNodesForOperation(arrayOfRanges, outArrayOfNodes,
                                      aOperation, aTouchContent);
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// MakeTransitionList: Detect all the transitions in the array, where a
//                     transition means that adjacent nodes in the array don't
//                     have the same parent.
void
nsHTMLEditRules::MakeTransitionList(nsTArray<OwningNonNull<nsINode>>& aNodeArray,
                                    nsTArray<bool>& aTransitionArray)
{
  nsCOMPtr<nsINode> prevParent;

  aTransitionArray.EnsureLengthAtLeast(aNodeArray.Length());
  for (uint32_t i = 0; i < aNodeArray.Length(); i++) {
    if (aNodeArray[i]->GetParentNode() != prevParent) {
      // Different parents: transition point
      aTransitionArray[i] = true;
    } else {
      // Same parents: these nodes grew up together
      aTransitionArray[i] = false;
    }
    prevParent = aNodeArray[i]->GetParentNode();
  }
}



/********************************************************
 *  main implementation methods
 ********************************************************/

///////////////////////////////////////////////////////////////////////////
// IsInListItem: if aNode is the descendant of a listitem, return that li.
//               But table element boundaries are stoppers on the search.
//               Also stops on the active editor host (contenteditable).
//               Also test if aNode is an li itself.
//
Element*
nsHTMLEditRules::IsInListItem(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, nullptr);
  if (nsHTMLEditUtils::IsListItem(aNode)) {
    return aNode->AsElement();
  }

  Element* parent = aNode->GetParentElement();
  while (parent && mHTMLEditor && mHTMLEditor->IsDescendantOfEditorRoot(parent) &&
         !nsHTMLEditUtils::IsTableElement(parent)) {
    if (nsHTMLEditUtils::IsListItem(parent)) {
      return parent;
    }
    parent = parent->GetParentElement();
  }
  return nullptr;
}


/**
 * ReturnInHeader: do the right thing for returns pressed in headers
 */
nsresult
nsHTMLEditRules::ReturnInHeader(Selection& aSelection,
                                Element& aHeader,
                                nsINode& aNode,
                                int32_t aOffset)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // Remember where the header is
  nsCOMPtr<nsINode> headerParent = aHeader.GetParentNode();
  int32_t offset = headerParent ? headerParent->IndexOf(&aHeader) : -1;

  // Get ws code to adjust any ws
  nsCOMPtr<nsINode> node = &aNode;
  nsresult res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor,
                                                           address_of(node),
                                                           &aOffset);
  NS_ENSURE_SUCCESS(res, res);

  // Split the header
  NS_ENSURE_STATE(node->IsContent());
  mHTMLEditor->SplitNodeDeep(aHeader, *node->AsContent(), aOffset);

  // If the left-hand heading is empty, put a mozbr in it
  nsCOMPtr<nsIContent> prevItem = mHTMLEditor->GetPriorHTMLSibling(&aHeader);
  if (prevItem && nsHTMLEditUtils::IsHeader(*prevItem)) {
    bool isEmptyNode;
    res = mHTMLEditor->IsEmptyNode(prevItem, &isEmptyNode);
    NS_ENSURE_SUCCESS(res, res);
    if (isEmptyNode) {
      res = CreateMozBR(prevItem->AsDOMNode(), 0);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // If the new (righthand) header node is empty, delete it
  bool isEmpty;
  res = IsEmptyBlock(aHeader, &isEmpty, MozBRCounts::no);
  NS_ENSURE_SUCCESS(res, res);
  if (isEmpty) {
    res = mHTMLEditor->DeleteNode(&aHeader);
    NS_ENSURE_SUCCESS(res, res);
    // Layout tells the caret to blink in a weird place if we don't place a
    // break after the header.
    nsCOMPtr<nsIContent> sibling =
      mHTMLEditor->GetNextHTMLSibling(headerParent, offset + 1);
    if (!sibling || !sibling->IsHTMLElement(nsGkAtoms::br)) {
      ClearCachedStyles();
      mHTMLEditor->mTypeInState->ClearAllProps();

      // Create a paragraph
      nsCOMPtr<Element> pNode =
        mHTMLEditor->CreateNode(nsGkAtoms::p, headerParent, offset + 1);
      NS_ENSURE_STATE(pNode);

      // Append a <br> to it
      nsCOMPtr<Element> brNode = mHTMLEditor->CreateBR(pNode, 0);
      NS_ENSURE_STATE(brNode);

      // Set selection to before the break
      res = aSelection.Collapse(pNode, 0);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      headerParent = sibling->GetParentNode();
      offset = headerParent ? headerParent->IndexOf(sibling) : -1;
      // Put selection after break
      res = aSelection.Collapse(headerParent, offset + 1);
      NS_ENSURE_SUCCESS(res, res);
    }
  } else {
    // Put selection at front of righthand heading
    res = aSelection.Collapse(&aHeader, 0);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// ReturnInParagraph: do the right thing for returns pressed in paragraphs
//
nsresult
nsHTMLEditRules::ReturnInParagraph(Selection* aSelection,
                                   nsIDOMNode* aPara,
                                   nsIDOMNode* aNode,
                                   int32_t aOffset,
                                   bool* aCancel,
                                   bool* aHandled)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  if (!aSelection || !aPara || !node || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  *aCancel = false;
  *aHandled = false;
  nsresult res;

  int32_t offset;
  nsCOMPtr<nsINode> parent = nsEditor::GetNodeLocation(node, &offset);

  NS_ENSURE_STATE(mHTMLEditor);
  bool doesCRCreateNewP = mHTMLEditor->GetReturnInParagraphCreatesNewParagraph();

  bool newBRneeded = false;
  bool newSelNode = false;
  nsCOMPtr<nsIContent> sibling;
  nsCOMPtr<nsIDOMNode> selNode = aNode;
  int32_t selOffset = aOffset;

  NS_ENSURE_STATE(mHTMLEditor);
  if (aNode == aPara && doesCRCreateNewP) {
    // we are at the edges of the block, newBRneeded not needed!
    sibling = node->AsContent();
  } else if (mHTMLEditor->IsTextNode(aNode)) {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aNode);
    uint32_t strLength;
    res = textNode->GetLength(&strLength);
    NS_ENSURE_SUCCESS(res, res);

    // at beginning of text node?
    if (!aOffset) {
      // is there a BR prior to it?
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetPriorHTMLSibling(node);
      if (!sibling || !mHTMLEditor || !mHTMLEditor->IsVisBreak(sibling) ||
          nsTextEditUtils::HasMozAttr(GetAsDOMNode(sibling))) {
        NS_ENSURE_STATE(mHTMLEditor);
        newBRneeded = true;
      }
    } else if (aOffset == (int32_t)strLength) {
      // we're at the end of text node...
      // is there a BR after to it?
      NS_ENSURE_STATE(mHTMLEditor);
      sibling = mHTMLEditor->GetNextHTMLSibling(node);
      if (!sibling || !mHTMLEditor || !mHTMLEditor->IsVisBreak(sibling) ||
          nsTextEditUtils::HasMozAttr(GetAsDOMNode(sibling))) {
        NS_ENSURE_STATE(mHTMLEditor);
        newBRneeded = true;
        offset++;
      }
    } else {
      if (doesCRCreateNewP) {
        nsCOMPtr<nsIDOMNode> tmp;
        res = mEditor->SplitNode(aNode, aOffset, getter_AddRefs(tmp));
        NS_ENSURE_SUCCESS(res, res);
        selNode = tmp;
      }

      newBRneeded = true;
      offset++;
    }
  } else {
    // not in a text node.
    // is there a BR prior to it?
    nsCOMPtr<nsIContent> nearNode;
    NS_ENSURE_STATE(mHTMLEditor);
    nearNode = mHTMLEditor->GetPriorHTMLNode(node, aOffset);
    NS_ENSURE_STATE(mHTMLEditor);
    if (!nearNode || !mHTMLEditor->IsVisBreak(nearNode) ||
        nsTextEditUtils::HasMozAttr(GetAsDOMNode(nearNode))) {
      // is there a BR after it?
      NS_ENSURE_STATE(mHTMLEditor);
      nearNode = mHTMLEditor->GetNextHTMLNode(node, aOffset);
      NS_ENSURE_STATE(mHTMLEditor);
      if (!nearNode || !mHTMLEditor->IsVisBreak(nearNode) ||
          nsTextEditUtils::HasMozAttr(GetAsDOMNode(nearNode))) {
        newBRneeded = true;
        parent = node;
        offset = aOffset;
        newSelNode = true;
      }
    }
    if (!newBRneeded) {
      sibling = nearNode;
    }
  }
  if (newBRneeded) {
    // if CR does not create a new P, default to BR creation
    NS_ENSURE_TRUE(doesCRCreateNewP, NS_OK);

    NS_ENSURE_STATE(mHTMLEditor);
    sibling = mHTMLEditor->CreateBR(parent, offset);
    if (newSelNode) {
      // We split the parent after the br we've just inserted.
      selNode = GetAsDOMNode(parent);
      selOffset = offset + 1;
    }
  }
  *aHandled = true;
  return SplitParagraph(aPara, sibling, aSelection, address_of(selNode), &selOffset);
}

///////////////////////////////////////////////////////////////////////////
// SplitParagraph: split a paragraph at selection point, possibly deleting a br
//
nsresult
nsHTMLEditRules::SplitParagraph(nsIDOMNode *aPara,
                                nsIContent* aBRNode,
                                Selection* aSelection,
                                nsCOMPtr<nsIDOMNode> *aSelNode,
                                int32_t *aOffset)
{
  nsCOMPtr<Element> para = do_QueryInterface(aPara);
  NS_ENSURE_TRUE(para && aBRNode && aSelNode && *aSelNode && aOffset &&
                 aSelection, NS_ERROR_NULL_POINTER);
  nsresult res = NS_OK;

  // split para
  // get ws code to adjust any ws
  nsCOMPtr<nsIContent> leftPara, rightPara;
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsINode> selNode(do_QueryInterface(*aSelNode));
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), aOffset);
  *aSelNode = GetAsDOMNode(selNode);
  NS_ENSURE_SUCCESS(res, res);
  // split the paragraph
  NS_ENSURE_STATE(mHTMLEditor);
  NS_ENSURE_STATE(selNode->IsContent());
  mHTMLEditor->SplitNodeDeep(*para, *selNode->AsContent(), *aOffset,
                             nsHTMLEditor::EmptyContainers::yes,
                             getter_AddRefs(leftPara),
                             getter_AddRefs(rightPara));
  // get rid of the break, if it is visible (otherwise it may be needed to prevent an empty p)
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsVisBreak(aBRNode))
  {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->DeleteNode(aBRNode);
    NS_ENSURE_SUCCESS(res, res);
  }

  // remove ID attribute on the paragraph we just created
  nsCOMPtr<nsIDOMElement> rightElt = do_QueryInterface(rightPara);
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->RemoveAttribute(rightElt, NS_LITERAL_STRING("id"));
  NS_ENSURE_SUCCESS(res, res);

  // check both halves of para to see if we need mozBR
  res = InsertMozBRIfNeeded(*leftPara);
  NS_ENSURE_SUCCESS(res, res);
  res = InsertMozBRIfNeeded(*rightPara);
  NS_ENSURE_SUCCESS(res, res);

  // selection to beginning of right hand para;
  // look inside any containers that are up front.
  nsCOMPtr<nsINode> rightParaNode = do_QueryInterface(rightPara);
  NS_ENSURE_STATE(mHTMLEditor && rightParaNode);
  nsCOMPtr<nsIDOMNode> child =
    GetAsDOMNode(mHTMLEditor->GetLeftmostChild(rightParaNode, true));
  if (mHTMLEditor->IsTextNode(child) ||
      mHTMLEditor->IsContainer(child))
  {
    aSelection->Collapse(child,0);
  }
  else
  {
    int32_t offset;
    nsCOMPtr<nsIDOMNode> parent = nsEditor::GetNodeLocation(child, &offset);
    aSelection->Collapse(parent,offset);
  }
  return res;
}


/**
 * ReturnInListItem: do the right thing for returns pressed in list items
 */
nsresult
nsHTMLEditRules::ReturnInListItem(Selection& aSelection,
                                  Element& aListItem,
                                  nsINode& aNode,
                                  int32_t aOffset)
{
  MOZ_ASSERT(nsHTMLEditUtils::IsListItem(&aListItem));

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // Get the item parent and the active editing host.
  nsCOMPtr<Element> root = mHTMLEditor->GetActiveEditingHost();

  nsCOMPtr<Element> list = aListItem.GetParentElement();
  int32_t itemOffset = list ? list->IndexOf(&aListItem) : -1;

  // If we are in an empty item, then we want to pop up out of the list, but
  // only if prefs say it's okay and if the parent isn't the active editing
  // host.
  bool isEmpty;
  nsresult res = IsEmptyBlock(aListItem, &isEmpty, MozBRCounts::no);
  NS_ENSURE_SUCCESS(res, res);
  if (isEmpty && root != list && mReturnInEmptyLIKillsList) {
    // Get the list offset now -- before we might eventually split the list
    nsCOMPtr<nsINode> listParent = list->GetParentNode();
    int32_t offset = listParent ? listParent->IndexOf(list) : -1;

    // Are we the last list item in the list?
    bool isLast;
    res = mHTMLEditor->IsLastEditableChild(aListItem.AsDOMNode(), &isLast);
    NS_ENSURE_SUCCESS(res, res);
    if (!isLast) {
      // We need to split the list!
      ErrorResult rv;
      mHTMLEditor->SplitNode(*list, itemOffset, rv);
      NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
    }

    // Are we in a sublist?
    if (nsHTMLEditUtils::IsList(listParent)) {
      // If so, move item out of this list and into the grandparent list
      res = mHTMLEditor->MoveNode(&aListItem, listParent, offset + 1);
      NS_ENSURE_SUCCESS(res, res);
      res = aSelection.Collapse(&aListItem, 0);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // Otherwise kill this item
      res = mHTMLEditor->DeleteNode(&aListItem);
      NS_ENSURE_SUCCESS(res, res);

      // Time to insert a paragraph
      nsCOMPtr<Element> pNode =
        mHTMLEditor->CreateNode(nsGkAtoms::p, listParent, offset + 1);
      NS_ENSURE_STATE(pNode);

      // Append a <br> to it
      nsCOMPtr<Element> brNode = mHTMLEditor->CreateBR(pNode, 0);
      NS_ENSURE_STATE(brNode);

      // Set selection to before the break
      res = aSelection.Collapse(pNode, 0);
      NS_ENSURE_SUCCESS(res, res);
    }
    return NS_OK;
  }

  // Else we want a new list item at the same list level.  Get ws code to
  // adjust any ws.
  nsCOMPtr<nsINode> selNode = &aNode;
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor,
                                                  address_of(selNode),
                                                  &aOffset);
  NS_ENSURE_SUCCESS(res, res);
  // Now split list item
  NS_ENSURE_STATE(selNode->IsContent());
  mHTMLEditor->SplitNodeDeep(aListItem, *selNode->AsContent(), aOffset);

  // Hack: until I can change the damaged doc range code back to being
  // extra-inclusive, I have to manually detect certain list items that may be
  // left empty.
  nsCOMPtr<nsIContent> prevItem = mHTMLEditor->GetPriorHTMLSibling(&aListItem);
  if (prevItem && nsHTMLEditUtils::IsListItem(prevItem)) {
    bool isEmptyNode;
    res = mHTMLEditor->IsEmptyNode(prevItem, &isEmptyNode);
    NS_ENSURE_SUCCESS(res, res);
    if (isEmptyNode) {
      res = CreateMozBR(prevItem->AsDOMNode(), 0);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      res = mHTMLEditor->IsEmptyNode(&aListItem, &isEmptyNode, true);
      NS_ENSURE_SUCCESS(res, res);
      if (isEmptyNode) {
        nsCOMPtr<nsIAtom> nodeAtom = aListItem.NodeInfo()->NameAtom();
        if (nodeAtom == nsGkAtoms::dd || nodeAtom == nsGkAtoms::dt) {
          nsCOMPtr<nsINode> list = aListItem.GetParentNode();
          int32_t itemOffset = list ? list->IndexOf(&aListItem) : -1;

          nsIAtom* listAtom = nodeAtom == nsGkAtoms::dt ? nsGkAtoms::dd
                                                        : nsGkAtoms::dt;
          nsCOMPtr<Element> newListItem =
            mHTMLEditor->CreateNode(listAtom, list, itemOffset + 1);
          NS_ENSURE_STATE(newListItem);
          res = mEditor->DeleteNode(&aListItem);
          NS_ENSURE_SUCCESS(res, res);
          res = aSelection.Collapse(newListItem, 0);
          NS_ENSURE_SUCCESS(res, res);

          return NS_OK;
        }

        nsCOMPtr<Element> brNode;
        res = mHTMLEditor->CopyLastEditableChildStyles(GetAsDOMNode(prevItem),
          GetAsDOMNode(&aListItem), getter_AddRefs(brNode));
        NS_ENSURE_SUCCESS(res, res);
        if (brNode) {
          nsCOMPtr<nsINode> brParent = brNode->GetParentNode();
          int32_t offset = brParent ? brParent->IndexOf(brNode) : -1;
          res = aSelection.Collapse(brParent, offset);
          NS_ENSURE_SUCCESS(res, res);

          return NS_OK;
        }
      } else {
        nsWSRunObject wsObj(mHTMLEditor, &aListItem, 0);
        nsCOMPtr<nsINode> visNode;
        int32_t visOffset = 0;
        WSType wsType;
        wsObj.NextVisibleNode(&aListItem, 0, address_of(visNode),
                              &visOffset, &wsType);
        if (wsType == WSType::special || wsType == WSType::br ||
            visNode->IsHTMLElement(nsGkAtoms::hr)) {
          nsCOMPtr<nsINode> parent = visNode->GetParentNode();
          int32_t offset = parent ? parent->IndexOf(visNode) : -1;
          res = aSelection.Collapse(parent, offset);
          NS_ENSURE_SUCCESS(res, res);

          return NS_OK;
        } else {
          res = aSelection.Collapse(visNode, visOffset);
          NS_ENSURE_SUCCESS(res, res);

          return NS_OK;
        }
      }
    }
  }
  res = aSelection.Collapse(&aListItem, 0);
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// MakeBlockquote: Put the list of nodes into one or more blockquotes.
//
nsresult
nsHTMLEditRules::MakeBlockquote(nsTArray<OwningNonNull<nsINode>>& aNodeArray)
{
  // The idea here is to put the nodes into a minimal number of blockquotes.
  // When the user blockquotes something, they expect one blockquote.  That may
  // not be possible (for instance, if they have two table cells selected, you
  // need two blockquotes inside the cells).
  nsresult res;
  nsCOMPtr<Element> curBlock;
  nsCOMPtr<nsINode> prevParent;

  for (auto& curNode : aNodeArray) {
    // Get the node to act on, and its location
    NS_ENSURE_STATE(curNode->IsContent());

    // If the node is a table element or list item, dive inside
    if (nsHTMLEditUtils::IsTableElementButNotTable(curNode) ||
        nsHTMLEditUtils::IsListItem(curNode)) {
      // Forget any previous block
      curBlock = nullptr;
      // Recursion time
      nsTArray<OwningNonNull<nsINode>> childArray;
      GetChildNodesForOperation(*curNode, childArray);
      res = MakeBlockquote(childArray);
      NS_ENSURE_SUCCESS(res, res);
    }

    // If the node has different parent than previous node, further nodes in a
    // new parent
    if (prevParent) {
      if (prevParent != curNode->GetParentNode()) {
        // Forget any previous blockquote node we were using
        curBlock = nullptr;
        prevParent = curNode->GetParentNode();
      }
    } else {
      prevParent = curNode->GetParentNode();
    }

    // If no curBlock, make one
    if (!curBlock) {
      nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
      int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;
      res = SplitAsNeeded(*nsGkAtoms::blockquote, curParent, offset);
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_STATE(mHTMLEditor);
      curBlock = mHTMLEditor->CreateNode(nsGkAtoms::blockquote, curParent,
                                         offset);
      NS_ENSURE_STATE(curBlock);
      // remember our new block for postprocessing
      mNewBlock = curBlock;
      // note: doesn't matter if we set mNewBlock multiple times.
    }

    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->MoveNode(curNode->AsContent(), curBlock, -1);
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// RemoveBlockStyle: Make the nodes have no special block type.
nsresult
nsHTMLEditRules::RemoveBlockStyle(nsTArray<OwningNonNull<nsINode>>& aNodeArray)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // Intent of this routine is to be used for converting to/from headers,
  // paragraphs, pre, and address.  Those blocks that pretty much just contain
  // inline things...
  nsresult res;

  nsCOMPtr<Element> curBlock;
  nsCOMPtr<nsIContent> firstNode, lastNode;
  for (auto& curNode : aNodeArray) {
    // If curNode is a address, p, header, address, or pre, remove it
    if (nsHTMLEditUtils::IsFormatNode(curNode)) {
      // Process any partial progress saved
      if (curBlock) {
        res = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
        NS_ENSURE_SUCCESS(res, res);
        firstNode = lastNode = curBlock = nullptr;
      }
      // Remove current block
      res = mHTMLEditor->RemoveBlockContainer(*curNode->AsContent());
      NS_ENSURE_SUCCESS(res, res);
    } else if (curNode->IsAnyOfHTMLElements(nsGkAtoms::table,
                                            nsGkAtoms::tr,
                                            nsGkAtoms::tbody,
                                            nsGkAtoms::td,
                                            nsGkAtoms::li,
                                            nsGkAtoms::blockquote,
                                            nsGkAtoms::div) ||
                nsHTMLEditUtils::IsList(curNode)) {
      // Process any partial progress saved
      if (curBlock) {
        res = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
        NS_ENSURE_SUCCESS(res, res);
        firstNode = lastNode = curBlock = nullptr;
      }
      // Recursion time
      nsTArray<OwningNonNull<nsINode>> childArray;
      GetChildNodesForOperation(*curNode, childArray);
      res = RemoveBlockStyle(childArray);
      NS_ENSURE_SUCCESS(res, res);
    } else if (IsInlineNode(curNode)) {
      if (curBlock) {
        // If so, is this node a descendant?
        if (nsEditorUtils::IsDescendantOf(curNode, curBlock)) {
          // Then we don't need to do anything different for this node
          lastNode = curNode->AsContent();
          continue;
        } else {
          // Otherwise, we have progressed beyond end of curBlock, so let's
          // handle it now.  We need to remove the portion of curBlock that
          // contains [firstNode - lastNode].
          res = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
          NS_ENSURE_SUCCESS(res, res);
          firstNode = lastNode = curBlock = nullptr;
          // Fall out and handle curNode
        }
      }
      curBlock = mHTMLEditor->GetBlockNodeParent(curNode);
      if (curBlock && nsHTMLEditUtils::IsFormatNode(curBlock)) {
        firstNode = lastNode = curNode->AsContent();
      } else {
        // Not a block kind that we care about.
        curBlock = nullptr;
      }
    } else if (curBlock) {
      // Some node that is already sans block style.  Skip over it and process
      // any partial progress saved.
      res = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
      NS_ENSURE_SUCCESS(res, res);
      firstNode = lastNode = curBlock = nullptr;
    }
  }
  // Process any partial progress saved
  if (curBlock) {
    res = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
    NS_ENSURE_SUCCESS(res, res);
    firstNode = lastNode = curBlock = nullptr;
  }
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// ApplyBlockStyle: Do whatever it takes to make the list of nodes into one or
//                  more blocks of type aBlockTag.
nsresult
nsHTMLEditRules::ApplyBlockStyle(nsTArray<OwningNonNull<nsINode>>& aNodeArray,
                                 nsIAtom& aBlockTag)
{
  // Intent of this routine is to be used for converting to/from headers,
  // paragraphs, pre, and address.  Those blocks that pretty much just contain
  // inline things...
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  nsresult res;

  // Remove all non-editable nodes.  Leave them be.
  for (int32_t i = aNodeArray.Length() - 1; i >= 0; i--) {
    if (!mHTMLEditor->IsEditable(aNodeArray[i])) {
      aNodeArray.RemoveElementAt(i);
    }
  }

  nsCOMPtr<Element> newBlock;

  nsCOMPtr<Element> curBlock;
  for (auto& curNode : aNodeArray) {
    nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;

    // Is it already the right kind of block?
    if (curNode->IsHTMLElement(&aBlockTag)) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      // Do nothing to this block
      continue;
    }

    // If curNode is a address, p, header, address, or pre, replace it with a
    // new block of correct type.
    // XXX: pre can't hold everything the others can
    if (nsHTMLEditUtils::IsMozDiv(curNode) ||
        nsHTMLEditUtils::IsFormatNode(curNode)) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      newBlock = mHTMLEditor->ReplaceContainer(curNode->AsElement(),
                                               &aBlockTag, nullptr, nullptr,
                                               nsEditor::eCloneAttributes);
      NS_ENSURE_STATE(newBlock);
    } else if (nsHTMLEditUtils::IsTable(curNode) ||
               nsHTMLEditUtils::IsList(curNode) ||
               curNode->IsAnyOfHTMLElements(nsGkAtoms::tbody,
                                            nsGkAtoms::tr,
                                            nsGkAtoms::td,
                                            nsGkAtoms::li,
                                            nsGkAtoms::blockquote,
                                            nsGkAtoms::div)) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      // Recursion time
      nsTArray<OwningNonNull<nsINode>> childArray;
      GetChildNodesForOperation(*curNode, childArray);
      if (childArray.Length()) {
        res = ApplyBlockStyle(childArray, aBlockTag);
        NS_ENSURE_SUCCESS(res, res);
      } else {
        // Make sure we can put a block here
        res = SplitAsNeeded(aBlockTag, curParent, offset);
        NS_ENSURE_SUCCESS(res, res);
        nsCOMPtr<Element> theBlock =
          mHTMLEditor->CreateNode(&aBlockTag, curParent, offset);
        NS_ENSURE_STATE(theBlock);
        // Remember our new block for postprocessing
        mNewBlock = theBlock;
      }
    } else if (curNode->IsHTMLElement(nsGkAtoms::br)) {
      // If the node is a break, we honor it by putting further nodes in a new
      // parent
      if (curBlock) {
        // Forget any previous block used for previous inline nodes
        curBlock = nullptr;
        res = mHTMLEditor->DeleteNode(curNode);
        NS_ENSURE_SUCCESS(res, res);
      } else {
        // The break is the first (or even only) node we encountered.  Create a
        // block for it.
        res = SplitAsNeeded(aBlockTag, curParent, offset);
        NS_ENSURE_SUCCESS(res, res);
        curBlock = mHTMLEditor->CreateNode(&aBlockTag, curParent, offset);
        NS_ENSURE_STATE(curBlock);
        // Remember our new block for postprocessing
        mNewBlock = curBlock;
        // Note: doesn't matter if we set mNewBlock multiple times.
        res = mHTMLEditor->MoveNode(curNode->AsContent(), curBlock, -1);
        NS_ENSURE_SUCCESS(res, res);
      }
    } else if (IsInlineNode(curNode)) {
      // If curNode is inline, pull it into curBlock.  Note: it's assumed that
      // consecutive inline nodes in aNodeArray are actually members of the
      // same block parent.  This happens to be true now as a side effect of
      // how aNodeArray is contructed, but some additional logic should be
      // added here if that should change
      //
      // If curNode is a non editable, drop it if we are going to <pre>.
      if (&aBlockTag == nsGkAtoms::pre && !mHTMLEditor->IsEditable(curNode)) {
        // Do nothing to this block
        continue;
      }

      // If no curBlock, make one
      if (!curBlock) {
        res = SplitAsNeeded(aBlockTag, curParent, offset);
        NS_ENSURE_SUCCESS(res, res);
        curBlock = mHTMLEditor->CreateNode(&aBlockTag, curParent, offset);
        NS_ENSURE_STATE(curBlock);
        // Remember our new block for postprocessing
        mNewBlock = curBlock;
        // Note: doesn't matter if we set mNewBlock multiple times.
      }

      // XXX If curNode is a br, replace it with a return if going to <pre>

      // This is a continuation of some inline nodes that belong together in
      // the same block item.  Use curBlock.
      res = mHTMLEditor->MoveNode(curNode->AsContent(), curBlock, -1);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// SplitAsNeeded: Given a tag name, split inOutParent up to the point where we
//                can insert the tag.  Adjust inOutParent and inOutOffset to
//                point to new location for tag.
nsresult
nsHTMLEditRules::SplitAsNeeded(nsIAtom& aTag,
                               OwningNonNull<nsINode>& aInOutParent,
                               int32_t& aInOutOffset)
{
  // XXX Is there a better way to do this?
  nsCOMPtr<nsINode> parent = aInOutParent.forget();
  nsresult res = SplitAsNeeded(aTag, parent, aInOutOffset);
  aInOutParent = parent.forget();
  return res;
}

nsresult
nsHTMLEditRules::SplitAsNeeded(nsIAtom& aTag,
                               nsCOMPtr<nsINode>& inOutParent,
                               int32_t& inOutOffset)
{
  NS_ENSURE_TRUE(inOutParent, NS_ERROR_NULL_POINTER);

  // Check that we have a place that can legally contain the tag
  nsCOMPtr<nsINode> tagParent, splitNode;
  for (nsCOMPtr<nsINode> parent = inOutParent; parent;
       parent = parent->GetParentNode()) {
    // Sniffing up the parent tree until we find a legal place for the block

    // Don't leave the active editing host
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsDescendantOfEditorRoot(parent)) {
      NS_ENSURE_STATE(mHTMLEditor);
      if (parent != mHTMLEditor->GetActiveEditingHost()) {
        return NS_ERROR_FAILURE;
      }
    }

    NS_ENSURE_STATE(mHTMLEditor);
    if (mHTMLEditor->CanContainTag(*parent, aTag)) {
      // Success
      tagParent = parent;
      break;
    }

    splitNode = parent;
  }
  if (!tagParent) {
    // Could not find a place to build tag!
    return NS_ERROR_FAILURE;
  }
  if (splitNode && splitNode->IsContent() && inOutParent->IsContent()) {
    // We found a place for block, but above inOutParent. We need to split.
    NS_ENSURE_STATE(mHTMLEditor);
    int32_t offset = mHTMLEditor->SplitNodeDeep(*splitNode->AsContent(),
                                                *inOutParent->AsContent(),
                                                inOutOffset);
    NS_ENSURE_STATE(offset != -1);
    inOutParent = tagParent;
    inOutOffset = offset;
  }
  return NS_OK;
}

/**
 * JoinNodesSmart: Join two nodes, doing whatever makes sense for their
 * children (which often means joining them, too).  aNodeLeft & aNodeRight must
 * be same type of node.
 *
 * Returns the point where they're merged, or (nullptr, -1) on failure.
 */
::DOMPoint
nsHTMLEditRules::JoinNodesSmart(nsIContent& aNodeLeft, nsIContent& aNodeRight)
{
  // Caller responsible for left and right node being the same type
  nsCOMPtr<nsINode> parent = aNodeLeft.GetParentNode();
  NS_ENSURE_TRUE(parent, ::DOMPoint());
  int32_t parOffset = parent->IndexOf(&aNodeLeft);
  nsCOMPtr<nsINode> rightParent = aNodeRight.GetParentNode();

  // If they don't have the same parent, first move the right node to after the
  // left one
  nsresult res;
  if (parent != rightParent) {
    NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
    res = mHTMLEditor->MoveNode(&aNodeRight, parent, parOffset);
    NS_ENSURE_SUCCESS(res, ::DOMPoint());
  }

  ::DOMPoint ret(&aNodeRight, aNodeLeft.Length());

  // Separate join rules for differing blocks
  if (nsHTMLEditUtils::IsList(&aNodeLeft) || aNodeLeft.GetAsText()) {
    // For lists, merge shallow (wouldn't want to combine list items)
    res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight);
    NS_ENSURE_SUCCESS(res, ::DOMPoint());
    return ret;
  }

  // Remember the last left child, and first right child
  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  nsCOMPtr<nsIContent> lastLeft = mHTMLEditor->GetLastEditableChild(aNodeLeft);
  NS_ENSURE_TRUE(lastLeft, ::DOMPoint());

  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  nsCOMPtr<nsIContent> firstRight = mHTMLEditor->GetFirstEditableChild(aNodeRight);
  NS_ENSURE_TRUE(firstRight, ::DOMPoint());

  // For list items, divs, etc., merge smart
  NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
  res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight);
  NS_ENSURE_SUCCESS(res, ::DOMPoint());

  if (lastLeft && firstRight && mHTMLEditor &&
      mHTMLEditor->AreNodesSameType(lastLeft, firstRight) &&
      (lastLeft->GetAsText() || !mHTMLEditor ||
       (lastLeft->IsElement() && firstRight->IsElement() &&
        mHTMLEditor->mHTMLCSSUtils->ElementsSameStyle(lastLeft->AsElement(),
                                                  firstRight->AsElement())))) {
    NS_ENSURE_TRUE(mHTMLEditor, ::DOMPoint());
    return JoinNodesSmart(*lastLeft, *firstRight);
  }
  return ret;
}


Element*
nsHTMLEditRules::GetTopEnclosingMailCite(nsINode& aNode)
{
  nsCOMPtr<Element> ret;

  for (nsCOMPtr<nsINode> node = &aNode; node; node = node->GetParentNode()) {
    if ((IsPlaintextEditor() && node->IsHTMLElement(nsGkAtoms::pre)) ||
        nsHTMLEditUtils::IsMailCite(node)) {
      ret = node->AsElement();
    }
    if (node->IsHTMLElement(nsGkAtoms::body)) {
      break;
    }
  }

  return ret;
}


nsresult
nsHTMLEditRules::CacheInlineStyles(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  for (int32_t j = 0; j < SIZE_STYLE_TABLE; ++j)
  {
    bool isSet = false;
    nsAutoString outValue;
    // Don't use CSS for <font size>, we don't support it usefully (bug 780035)
    if (!useCSS || (mCachedStyles[j].tag == nsGkAtoms::font &&
                    mCachedStyles[j].attr.EqualsLiteral("size"))) {
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->IsTextPropertySetByContent(aNode, mCachedStyles[j].tag,
                                              &(mCachedStyles[j].attr), nullptr,
                                              isSet, &outValue);
    }
    else
    {
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(aNode,
        mCachedStyles[j].tag, &(mCachedStyles[j].attr), isSet, outValue,
        nsHTMLCSSUtils::eComputed);
    }
    if (isSet)
    {
      mCachedStyles[j].mPresent = true;
      mCachedStyles[j].value.Assign(outValue);
    }
  }
  return NS_OK;
}


nsresult
nsHTMLEditRules::ReapplyCachedStyles()
{
  // The idea here is to examine our cached list of styles and see if any have
  // been removed.  If so, add typeinstate for them, so that they will be
  // reinserted when new content is added.

  // remember if we are in css mode
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  // get selection point; if it doesn't exist, we have nothing to do
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  if (!selection) {
    // If the document is removed from its parent document during executing an
    // editor operation with DOMMutationEvent or something, there may be no
    // selection.
    return NS_OK;
  }
  if (!selection->RangeCount()) {
    // Nothing to do
    return NS_OK;
  }
  nsCOMPtr<nsIContent> selNode =
    do_QueryInterface(selection->GetRangeAt(0)->GetStartParent());
  if (!selNode) {
    // Nothing to do
    return NS_OK;
  }

  for (int32_t i = 0; i < SIZE_STYLE_TABLE; ++i) {
    if (mCachedStyles[i].mPresent) {
      bool bFirst, bAny, bAll;
      bFirst = bAny = bAll = false;

      nsAutoString curValue;
      if (useCSS) {
        // check computed style first in css case
        NS_ENSURE_STATE(mHTMLEditor);
        bAny = mHTMLEditor->mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(
          selNode, mCachedStyles[i].tag, &(mCachedStyles[i].attr), curValue,
          nsHTMLCSSUtils::eComputed);
      }
      if (!bAny) {
        // then check typeinstate and html style
        NS_ENSURE_STATE(mHTMLEditor);
        nsresult res = mHTMLEditor->GetInlinePropertyBase(
                                                     *mCachedStyles[i].tag,
                                                     &(mCachedStyles[i].attr),
                                                     &(mCachedStyles[i].value),
                                                     &bFirst, &bAny, &bAll,
                                                     &curValue, false);
        NS_ENSURE_SUCCESS(res, res);
      }
      // this style has disappeared through deletion.  Add to our typeinstate:
      if (!bAny || IsStyleCachePreservingAction(mTheAction)) {
        NS_ENSURE_STATE(mHTMLEditor);
        mHTMLEditor->mTypeInState->SetProp(mCachedStyles[i].tag,
                                           mCachedStyles[i].attr,
                                           mCachedStyles[i].value);
      }
    }
  }

  return NS_OK;
}


void
nsHTMLEditRules::ClearCachedStyles()
{
  // clear the mPresent bits in mCachedStyles array
  for (uint32_t j = 0; j < SIZE_STYLE_TABLE; j++) {
    mCachedStyles[j].mPresent = false;
    mCachedStyles[j].value.Truncate();
  }
}


void
nsHTMLEditRules::AdjustSpecialBreaks()
{
  NS_ENSURE_TRUE(mHTMLEditor, );

  // Gather list of empty nodes
  nsTArray<OwningNonNull<nsINode>> nodeArray;
  nsEmptyEditableFunctor functor(mHTMLEditor);
  nsDOMIterator iter;
  nsresult res = iter.Init(*mDocChangeRange);
  NS_ENSURE_SUCCESS(res, );
  iter.AppendList(functor, nodeArray);

  // Put moz-br's into these empty li's and td's
  for (auto& node : nodeArray) {
    // Need to put br at END of node.  It may have empty containers in it and
    // still pass the "IsEmptyNode" test, and we want the br's to be after
    // them.  Also, we want the br to be after the selection if the selection
    // is in this node.
    nsresult res = CreateMozBR(node->AsDOMNode(), (int32_t)node->Length());
    NS_ENSURE_SUCCESS(res, );
  }
}

nsresult
nsHTMLEditRules::AdjustWhitespace(Selection* aSelection)
{
  // get selection point
  nsCOMPtr<nsIDOMNode> selNode;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);

  // ask whitespace object to tweak nbsp's
  NS_ENSURE_STATE(mHTMLEditor);
  return nsWSRunObject(mHTMLEditor, selNode, selOffset).AdjustWhitespace();
}

nsresult
nsHTMLEditRules::PinSelectionToNewBlock(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;

  // use ranges and sRangeHelper to compare sel point to new block
  nsCOMPtr<nsINode> node = do_QueryInterface(selNode);
  NS_ENSURE_STATE(node);
  RefPtr<nsRange> range = new nsRange(node);
  res = range->SetStart(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = range->SetEnd(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  nsCOMPtr<nsIContent> block = mNewBlock.get();
  NS_ENSURE_TRUE(block, NS_ERROR_NO_INTERFACE);
  bool nodeBefore, nodeAfter;
  res = nsRange::CompareNodeToRange(block, range, &nodeBefore, &nodeAfter);
  NS_ENSURE_SUCCESS(res, res);

  if (nodeBefore && nodeAfter)
    return NS_OK;  // selection is inside block
  else if (nodeBefore)
  {
    // selection is after block.  put at end of block.
    nsCOMPtr<nsIDOMNode> tmp = GetAsDOMNode(mNewBlock);
    NS_ENSURE_STATE(mHTMLEditor);
    tmp = GetAsDOMNode(mHTMLEditor->GetLastEditableChild(*block));
    uint32_t endPoint;
    if (mHTMLEditor->IsTextNode(tmp) ||
        mHTMLEditor->IsContainer(tmp))
    {
      res = nsEditor::GetLengthOfDOMNode(tmp, endPoint);
      NS_ENSURE_SUCCESS(res, res);
    }
    else
    {
      tmp = nsEditor::GetNodeLocation(tmp, (int32_t*)&endPoint);
      endPoint++;  // want to be after this node
    }
    return aSelection->Collapse(tmp, (int32_t)endPoint);
  }
  else
  {
    // selection is before block.  put at start of block.
    nsCOMPtr<nsIDOMNode> tmp = GetAsDOMNode(mNewBlock);
    NS_ENSURE_STATE(mHTMLEditor);
    tmp = GetAsDOMNode(mHTMLEditor->GetFirstEditableChild(*block));
    int32_t offset;
    if (mHTMLEditor->IsTextNode(tmp) ||
        mHTMLEditor->IsContainer(tmp))
    {
      tmp = nsEditor::GetNodeLocation(tmp, &offset);
    }
    return aSelection->Collapse(tmp, 0);
  }
}

void
nsHTMLEditRules::CheckInterlinePosition(Selection& aSelection)
{
  // If the selection isn't collapsed, do nothing.
  if (!aSelection.Collapsed()) {
    return;
  }

  NS_ENSURE_TRUE_VOID(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // Get the (collapsed) selection location
  NS_ENSURE_TRUE_VOID(aSelection.GetRangeAt(0) &&
                      aSelection.GetRangeAt(0)->GetStartParent());
  OwningNonNull<nsINode> selNode = *aSelection.GetRangeAt(0)->GetStartParent();
  int32_t selOffset = aSelection.GetRangeAt(0)->StartOffset();

  // First, let's check to see if we are after a <br>.  We take care of this
  // special-case first so that we don't accidentally fall through into one of
  // the other conditionals.
  nsCOMPtr<nsIContent> node =
    mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, true);
  if (node && node->IsHTMLElement(nsGkAtoms::br)) {
    aSelection.SetInterlinePosition(true);
    return;
  }

  // Are we after a block?  If so try set caret to following content
  node = mHTMLEditor->GetPriorHTMLSibling(selNode, selOffset);
  if (node && IsBlockNode(*node)) {
    aSelection.SetInterlinePosition(true);
    return;
  }

  // Are we before a block?  If so try set caret to prior content
  node = mHTMLEditor->GetNextHTMLSibling(selNode, selOffset);
  if (node && IsBlockNode(*node)) {
    aSelection.SetInterlinePosition(false);
  }
}

nsresult
nsHTMLEditRules::AdjustSelection(Selection* aSelection,
                                 nsIEditor::EDirection aAction)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);

  // if the selection isn't collapsed, do nothing.
  // moose: one thing to do instead is check for the case of
  // only a single break selected, and collapse it.  Good thing?  Beats me.
  if (!aSelection->Collapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  nsCOMPtr<nsINode> selNode, temp;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;

  // are we in an editable node?
  NS_ENSURE_STATE(mHTMLEditor);
  while (!mHTMLEditor->IsEditable(selNode))
  {
    // scan up the tree until we find an editable place to be
    selNode = nsEditor::GetNodeLocation(temp, &selOffset);
    NS_ENSURE_TRUE(selNode, NS_ERROR_FAILURE);
    temp = selNode;
    NS_ENSURE_STATE(mHTMLEditor);
  }

  // make sure we aren't in an empty block - user will see no cursor.  If this
  // is happening, put a <br> in the block if allowed.
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<Element> theblock = mHTMLEditor->GetBlock(*selNode);

  if (theblock && mHTMLEditor->IsEditable(theblock)) {
    bool bIsEmptyNode;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->IsEmptyNode(theblock, &bIsEmptyNode, false, false);
    NS_ENSURE_SUCCESS(res, res);
    // check if br can go into the destination node
    NS_ENSURE_STATE(mHTMLEditor);
    if (bIsEmptyNode && mHTMLEditor->CanContainTag(*selNode, *nsGkAtoms::br)) {
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<Element> rootNode = mHTMLEditor->GetRoot();
      NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);
      if (selNode == rootNode)
      {
        // Our root node is completely empty. Don't add a <br> here.
        // AfterEditInner() will add one for us when it calls
        // CreateBogusNodeIfNeeded()!
        return NS_OK;
      }

      // we know we can skip the rest of this routine given the cirumstance
      return CreateMozBR(GetAsDOMNode(selNode), selOffset);
    }
  }

  // are we in a text node?
  nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(selNode);
  if (textNode)
    return NS_OK; // we LIKE it when we are in a text node.  that RULZ

  // do we need to insert a special mozBR?  We do if we are:
  // 1) prior node is in same block where selection is AND
  // 2) prior node is a br AND
  // 3) that br is not visible

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIContent> nearNode =
    mHTMLEditor->GetPriorHTMLNode(selNode, selOffset);
  if (nearNode)
  {
    // is nearNode also a descendant of same block?
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> block = mHTMLEditor->GetBlock(*selNode);
    nsCOMPtr<Element> nearBlock = mHTMLEditor->GetBlockNodeParent(nearNode);
    if (block && block == nearBlock) {
      if (nearNode && nsTextEditUtils::IsBreak(nearNode) )
      {
        NS_ENSURE_STATE(mHTMLEditor);
        if (!mHTMLEditor->IsVisBreak(nearNode))
        {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  Also, things
          // like table cells won't grow in height.
          nsCOMPtr<nsIDOMNode> brNode;
          res = CreateMozBR(GetAsDOMNode(selNode), selOffset,
                            getter_AddRefs(brNode));
          NS_ENSURE_SUCCESS(res, res);
          nsCOMPtr<nsIDOMNode> brParent =
            nsEditor::GetNodeLocation(brNode, &selOffset);
          // selection stays *before* moz-br, sticking to it
          aSelection->SetInterlinePosition(true);
          res = aSelection->Collapse(brParent, selOffset);
          NS_ENSURE_SUCCESS(res, res);
        }
        else
        {
          NS_ENSURE_STATE(mHTMLEditor);
          nsCOMPtr<nsIContent> nextNode =
            mHTMLEditor->GetNextHTMLNode(nearNode, true);
          if (nextNode && nsTextEditUtils::IsMozBR(nextNode))
          {
            // selection between br and mozbr.  make it stick to mozbr
            // so that it will be on blank line.
            aSelection->SetInterlinePosition(true);
          }
        }
      }
    }
  }

  // we aren't in a textnode: are we adjacent to text or a break or an image?
  NS_ENSURE_STATE(mHTMLEditor);
  nearNode = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, true);
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode) ||
                   nsEditor::IsTextNode(nearNode) ||
                   nsHTMLEditUtils::IsImage(nearNode) ||
                   nearNode->IsHTMLElement(nsGkAtoms::hr))) {
    // this is a good place for the caret to be
    return NS_OK;
  }
  NS_ENSURE_STATE(mHTMLEditor);
  nearNode = mHTMLEditor->GetNextHTMLNode(selNode, selOffset, true);
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode) ||
                   nsEditor::IsTextNode(nearNode) ||
                   nearNode->IsAnyOfHTMLElements(nsGkAtoms::img,
                                                 nsGkAtoms::hr))) {
    return NS_OK; // this is a good place for the caret to be
  }

  // look for a nearby text node.
  // prefer the correct direction.
  nsCOMPtr<nsIDOMNode> nearNodeDOM = GetAsDOMNode(nearNode);
  res = FindNearSelectableNode(GetAsDOMNode(selNode), selOffset, aAction,
                               address_of(nearNodeDOM));
  NS_ENSURE_SUCCESS(res, res);
  nearNode = do_QueryInterface(nearNodeDOM);

  if (nearNode)
  {
    // is the nearnode a text node?
    textNode = do_QueryInterface(nearNode);
    if (textNode)
    {
      int32_t offset = 0;
      // put selection in right place:
      if (aAction == nsIEditor::ePrevious)
        textNode->GetLength((uint32_t*)&offset);
      res = aSelection->Collapse(nearNode,offset);
    }
    else  // must be break or image
    {
      selNode = nsEditor::GetNodeLocation(nearNode, &selOffset);
      if (aAction == nsIEditor::ePrevious) selOffset++;  // want to be beyond it if we backed up to it
      res = aSelection->Collapse(selNode, selOffset);
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::FindNearSelectableNode(nsIDOMNode *aSelNode,
                                        int32_t aSelOffset,
                                        nsIEditor::EDirection &aDirection,
                                        nsCOMPtr<nsIDOMNode> *outSelectableNode)
{
  NS_ENSURE_TRUE(aSelNode && outSelectableNode, NS_ERROR_NULL_POINTER);
  *outSelectableNode = nullptr;
  nsresult res = NS_OK;

  nsCOMPtr<nsIDOMNode> nearNode, curNode;
  if (aDirection == nsIEditor::ePrevious) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  } else {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  }
  NS_ENSURE_SUCCESS(res, res);

  if (!nearNode) // try the other direction then
  {
    if (aDirection == nsIEditor::ePrevious)
      aDirection = nsIEditor::eNext;
    else
      aDirection = nsIEditor::ePrevious;

    if (aDirection == nsIEditor::ePrevious) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
    } else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
    }
    NS_ENSURE_SUCCESS(res, res);
  }

  // scan in the right direction until we find an eligible text node,
  // but don't cross any breaks, images, or table elements.
  NS_ENSURE_STATE(mHTMLEditor);
  while (nearNode && !(mHTMLEditor->IsTextNode(nearNode)
                       || nsTextEditUtils::IsBreak(nearNode)
                       || nsHTMLEditUtils::IsImage(nearNode)))
  {
    curNode = nearNode;
    if (aDirection == nsIEditor::ePrevious) {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetPriorHTMLNode(curNode, address_of(nearNode));
    } else {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLNode(curNode, address_of(nearNode));
    }
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_STATE(mHTMLEditor);
  }

  if (nearNode)
  {
    // don't cross any table elements
    if (InDifferentTableElements(nearNode, aSelNode)) {
      return NS_OK;
    }

    // otherwise, ok, we have found a good spot to put the selection
    *outSelectableNode = do_QueryInterface(nearNode);
  }
  return res;
}


bool nsHTMLEditRules::InDifferentTableElements(nsIDOMNode* aNode1,
                                               nsIDOMNode* aNode2)
{
  nsCOMPtr<nsINode> node1 = do_QueryInterface(aNode1);
  nsCOMPtr<nsINode> node2 = do_QueryInterface(aNode2);
  return InDifferentTableElements(node1, node2);
}

bool
nsHTMLEditRules::InDifferentTableElements(nsINode* aNode1, nsINode* aNode2)
{
  MOZ_ASSERT(aNode1 && aNode2);

  while (aNode1 && !nsHTMLEditUtils::IsTableElement(aNode1)) {
    aNode1 = aNode1->GetParentNode();
  }

  while (aNode2 && !nsHTMLEditUtils::IsTableElement(aNode2)) {
    aNode2 = aNode2->GetParentNode();
  }

  return aNode1 != aNode2;
}


nsresult
nsHTMLEditRules::RemoveEmptyNodes()
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  // Some general notes on the algorithm used here: the goal is to examine all
  // the nodes in mDocChangeRange, and remove the empty ones.  We do this by
  // using a content iterator to traverse all the nodes in the range, and
  // placing the empty nodes into an array.  After finishing the iteration, we
  // delete the empty nodes in the array.  (They cannot be deleted as we find
  // them because that would invalidate the iterator.)
  //
  // Since checking to see if a node is empty can be costly for nodes with many
  // descendants, there are some optimizations made.  I rely on the fact that
  // the iterator is post-order: it will visit children of a node before
  // visiting the parent node.  So if I find that a child node is not empty, I
  // know that its parent is not empty without even checking.  So I put the
  // parent on a "skipList" which is just a voidArray of nodes I can skip the
  // empty check on.  If I encounter a node on the skiplist, i skip the
  // processing for that node and replace its slot in the skiplist with that
  // node's parent.
  //
  // An interesting idea is to go ahead and regard parent nodes that are NOT on
  // the skiplist as being empty (without even doing the IsEmptyNode check) on
  // the theory that if they weren't empty, we would have encountered a
  // non-empty child earlier and thus put this parent node on the skiplist.
  //
  // Unfortunately I can't use that strategy here, because the range may
  // include some children of a node while excluding others.  Thus I could find
  // all the _examined_ children empty, but still not have an empty parent.

  // need an iterator
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();

  nsresult res = iter->Init(mDocChangeRange);
  NS_ENSURE_SUCCESS(res, res);

  nsTArray<OwningNonNull<nsINode>> arrayOfEmptyNodes, arrayOfEmptyCites, skipList;

  // Check for empty nodes
  while (!iter->IsDone()) {
    OwningNonNull<nsINode> node = *iter->GetCurrentNode();

    nsCOMPtr<nsINode> parent = node->GetParentNode();

    size_t idx = skipList.IndexOf(node);
    if (idx != skipList.NoIndex) {
      // This node is on our skip list.  Skip processing for this node, and
      // replace its value in the skip list with the value of its parent
      skipList[idx] = parent;
    } else {
      bool bIsCandidate = false;
      bool bIsEmptyNode = false;
      bool bIsMailCite = false;

      if (node->IsElement()) {
        if (node->IsHTMLElement(nsGkAtoms::body)) {
          // Don't delete the body
        } else if ((bIsMailCite = nsHTMLEditUtils::IsMailCite(node)) ||
                   node->IsHTMLElement(nsGkAtoms::a) ||
                   nsHTMLEditUtils::IsInlineStyle(node) ||
                   nsHTMLEditUtils::IsList(node) ||
                   node->IsHTMLElement(nsGkAtoms::div)) {
          // Only consider certain nodes to be empty for purposes of removal
          bIsCandidate = true;
        } else if (nsHTMLEditUtils::IsFormatNode(node) ||
                   nsHTMLEditUtils::IsListItem(node) ||
                   node->IsHTMLElement(nsGkAtoms::blockquote)) {
          // These node types are candidates if selection is not in them.  If
          // it is one of these, don't delete if selection inside.  This is so
          // we can create empty headings, etc., for the user to type into.
          bool bIsSelInNode;
          res = SelectionEndpointInNode(node, &bIsSelInNode);
          NS_ENSURE_SUCCESS(res, res);
          if (!bIsSelInNode) {
            bIsCandidate = true;
          }
        }
      }

      if (bIsCandidate) {
        // We delete mailcites even if they have a solo br in them.  Other
        // nodes we require to be empty.
        res = mHTMLEditor->IsEmptyNode(node->AsDOMNode(), &bIsEmptyNode,
                                       bIsMailCite, true);
        NS_ENSURE_SUCCESS(res, res);
        if (bIsEmptyNode) {
          if (bIsMailCite) {
            // mailcites go on a separate list from other empty nodes
            arrayOfEmptyCites.AppendElement(*node);
          } else {
            arrayOfEmptyNodes.AppendElement(*node);
          }
        }
      }

      if (!bIsEmptyNode) {
        // put parent on skip list
        skipList.AppendElement(*parent);
      }
    }

    iter->Next();
  }

  // now delete the empty nodes
  for (auto& delNode : arrayOfEmptyNodes) {
    if (mHTMLEditor->IsModifiableNode(delNode)) {
      res = mHTMLEditor->DeleteNode(delNode);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // Now delete the empty mailcites.  This is a separate step because we want
  // to pull out any br's and preserve them.
  for (auto& delNode : arrayOfEmptyCites) {
    bool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(delNode, &bIsEmptyNode, false, true);
    NS_ENSURE_SUCCESS(res, res);
    if (!bIsEmptyNode) {
      // We are deleting a cite that has just a br.  We want to delete cite,
      // but preserve br.
      nsCOMPtr<nsINode> parent = delNode->GetParentNode();
      int32_t offset = parent ? parent->IndexOf(delNode) : -1;
      nsCOMPtr<Element> br = mHTMLEditor->CreateBR(parent, offset);
      NS_ENSURE_STATE(br);
    }
    res = mHTMLEditor->DeleteNode(delNode);
    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}

nsresult
nsHTMLEditRules::SelectionEndpointInNode(nsINode* aNode, bool* aResult)
{
  NS_ENSURE_TRUE(aNode && aResult, NS_ERROR_NULL_POINTER);

  nsIDOMNode* node = aNode->AsDOMNode();

  *aResult = false;

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  uint32_t rangeCount = selection->RangeCount();
  for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
    RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
    nsCOMPtr<nsIDOMNode> startParent, endParent;
    range->GetStartContainer(getter_AddRefs(startParent));
    if (startParent)
    {
      if (node == startParent) {
        *aResult = true;
        return NS_OK;
      }
      if (nsEditorUtils::IsDescendantOf(startParent, node)) {
        *aResult = true;
        return NS_OK;
      }
    }
    range->GetEndContainer(getter_AddRefs(endParent));
    if (startParent == endParent) continue;
    if (endParent)
    {
      if (node == endParent) {
        *aResult = true;
        return NS_OK;
      }
      if (nsEditorUtils::IsDescendantOf(endParent, node)) {
        *aResult = true;
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

/**
 * IsEmptyInline: Return true if aNode is an empty inline container
 */
bool
nsHTMLEditRules::IsEmptyInline(nsINode& aNode)
{
  NS_ENSURE_TRUE(mHTMLEditor, false);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  if (IsInlineNode(aNode) && mHTMLEditor->IsContainer(&aNode)) {
    bool isEmpty = true;
    mHTMLEditor->IsEmptyNode(&aNode, &isEmpty);
    return isEmpty;
  }
  return false;
}


bool
nsHTMLEditRules::ListIsEmptyLine(nsTArray<OwningNonNull<nsINode>>& aArrayOfNodes)
{
  // We have a list of nodes which we are candidates for being moved into a new
  // block.  Determine if it's anything more than a blank line.  Look for
  // editable content above and beyond one single BR.
  NS_ENSURE_TRUE(aArrayOfNodes.Length(), true);

  NS_ENSURE_TRUE(mHTMLEditor, false);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  int32_t brCount = 0;

  for (auto& node : aArrayOfNodes) {
    if (!mHTMLEditor->IsEditable(node)) {
      continue;
    }
    if (nsTextEditUtils::IsBreak(node)) {
      // First break doesn't count
      if (brCount) {
        return false;
      }
      brCount++;
    } else if (IsEmptyInline(node)) {
      // Empty inline, keep looking
    } else {
      return false;
    }
  }
  return true;
}


nsresult
nsHTMLEditRules::PopListItem(nsIDOMNode *aListItem, bool *aOutOfList)
{
  nsCOMPtr<Element> listItem = do_QueryInterface(aListItem);
  // check parms
  NS_ENSURE_TRUE(listItem && aOutOfList, NS_ERROR_NULL_POINTER);

  // init out params
  *aOutOfList = false;

  nsCOMPtr<nsINode> curParent = listItem->GetParentNode();
  int32_t offset = curParent ? curParent->IndexOf(listItem) : -1;

  if (!nsHTMLEditUtils::IsListItem(listItem)) {
    return NS_ERROR_FAILURE;
  }

  // if it's first or last list item, don't need to split the list
  // otherwise we do.
  nsCOMPtr<nsINode> curParPar = curParent->GetParentNode();
  int32_t parOffset = curParPar ? curParPar->IndexOf(curParent) : -1;

  bool bIsFirstListItem;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->IsFirstEditableChild(aListItem,
                                                   &bIsFirstListItem);
  NS_ENSURE_SUCCESS(res, res);

  bool bIsLastListItem;
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->IsLastEditableChild(aListItem, &bIsLastListItem);
  NS_ENSURE_SUCCESS(res, res);

  if (!bIsFirstListItem && !bIsLastListItem)
  {
    // split the list
    nsCOMPtr<nsIDOMNode> newBlock;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->SplitNode(GetAsDOMNode(curParent), offset,
                                 getter_AddRefs(newBlock));
    NS_ENSURE_SUCCESS(res, res);
  }

  if (!bIsFirstListItem) parOffset++;

  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->MoveNode(listItem, curParPar, parOffset);
  NS_ENSURE_SUCCESS(res, res);

  // unwrap list item contents if they are no longer in a list
  if (!nsHTMLEditUtils::IsList(curParPar) &&
      nsHTMLEditUtils::IsListItem(listItem)) {
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->RemoveBlockContainer(*listItem);
    NS_ENSURE_SUCCESS(res, res);
    *aOutOfList = true;
  }
  return res;
}

nsresult
nsHTMLEditRules::RemoveListStructure(Element& aList)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);
  nsresult res;

  while (aList.GetFirstChild()) {
    OwningNonNull<nsIContent> child = *aList.GetFirstChild();

    if (nsHTMLEditUtils::IsListItem(child)) {
      bool isOutOfList;
      // Keep popping it out until it's not in a list anymore
      do {
        res = PopListItem(child->AsDOMNode(), &isOutOfList);
        NS_ENSURE_SUCCESS(res, res);
      } while (!isOutOfList);
    } else if (nsHTMLEditUtils::IsList(child)) {
      res = RemoveListStructure(*child->AsElement());
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // Delete any non-list items for now
      res = mHTMLEditor->DeleteNode(child);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // Delete the now-empty list
  res = mHTMLEditor->RemoveBlockContainer(aList);
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}


nsresult
nsHTMLEditRules::ConfirmSelectionInBody()
{
  // get the body
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(mHTMLEditor->GetRoot());
  NS_ENSURE_TRUE(rootElement, NS_ERROR_UNEXPECTED);

  // get the selection
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  // get the selection start location
  nsCOMPtr<nsIDOMNode> selNode, temp, parent;
  int32_t selOffset;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(selection,
                                                    getter_AddRefs(selNode),
                                                    &selOffset);
  if (NS_FAILED(res)) {
    return res;
  }

  temp = selNode;

  // check that selNode is inside body
  while (temp && !nsTextEditUtils::IsBody(temp))
  {
    res = temp->GetParentNode(getter_AddRefs(parent));
    temp = parent;
  }

  // if we aren't in the body, force the issue
  if (!temp)
  {
//    uncomment this to see when we get bad selections
//    NS_NOTREACHED("selection not in body");
    selection->Collapse(rootElement, 0);
  }

  // get the selection end location
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->GetEndNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  temp = selNode;

  // check that selNode is inside body
  while (temp && !nsTextEditUtils::IsBody(temp))
  {
    res = temp->GetParentNode(getter_AddRefs(parent));
    temp = parent;
  }

  // if we aren't in the body, force the issue
  if (!temp)
  {
//    uncomment this to see when we get bad selections
//    NS_NOTREACHED("selection not in body");
    selection->Collapse(rootElement, 0);
  }

  return res;
}


nsresult
nsHTMLEditRules::UpdateDocChangeRange(nsRange* aRange)
{
  nsresult res = NS_OK;

  // first make sure aRange is in the document.  It might not be if
  // portions of our editting action involved manipulating nodes
  // prior to placing them in the document (e.g., populating a list item
  // before placing it in its list)
  nsCOMPtr<nsIDOMNode> startNode;
  res = aRange->GetStartContainer(getter_AddRefs(startNode));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsDescendantOfRoot(startNode)) {
    // just return - we don't need to adjust mDocChangeRange in this case
    return NS_OK;
  }

  if (!mDocChangeRange)
  {
    // clone aRange.
    mDocChangeRange = aRange->CloneRange();
  }
  else
  {
    int16_t result;

    // compare starts of ranges
    res = mDocChangeRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, aRange, &result);
    if (res == NS_ERROR_NOT_INITIALIZED) {
      // This will happen is mDocChangeRange is non-null, but the range is
      // uninitialized. In this case we'll set the start to aRange start.
      // The same test won't be needed further down since after we've set
      // the start the range will be collapsed to that point.
      result = 1;
      res = NS_OK;
    }
    NS_ENSURE_SUCCESS(res, res);
    if (result > 0)  // positive result means mDocChangeRange start is after aRange start
    {
      int32_t startOffset;
      res = aRange->GetStartOffset(&startOffset);
      NS_ENSURE_SUCCESS(res, res);
      res = mDocChangeRange->SetStart(startNode, startOffset);
      NS_ENSURE_SUCCESS(res, res);
    }

    // compare ends of ranges
    res = mDocChangeRange->CompareBoundaryPoints(nsIDOMRange::END_TO_END, aRange, &result);
    NS_ENSURE_SUCCESS(res, res);
    if (result < 0)  // negative result means mDocChangeRange end is before aRange end
    {
      nsCOMPtr<nsIDOMNode> endNode;
      int32_t endOffset;
      res = aRange->GetEndContainer(getter_AddRefs(endNode));
      NS_ENSURE_SUCCESS(res, res);
      res = aRange->GetEndOffset(&endOffset);
      NS_ENSURE_SUCCESS(res, res);
      res = mDocChangeRange->SetEnd(endNode, endOffset);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return res;
}

nsresult
nsHTMLEditRules::InsertMozBRIfNeeded(nsINode& aNode)
{
  if (!IsBlockNode(aNode)) {
    return NS_OK;
  }

  bool isEmpty;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->IsEmptyNode(&aNode, &isEmpty);
  NS_ENSURE_SUCCESS(res, res);
  if (!isEmpty) {
    return NS_OK;
  }

  return CreateMozBR(aNode.AsDOMNode(), 0);
}

NS_IMETHODIMP
nsHTMLEditRules::WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, int32_t aPosition)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditRules::DidCreateNode(const nsAString& aTag,
                               nsIDOMNode *aNode,
                               nsIDOMNode *aParent,
                               int32_t aPosition,
                               nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SelectNode(aNode);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aPosition)
{
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::DidInsertNode(nsIDOMNode *aNode,
                               nsIDOMNode *aParent,
                               int32_t aPosition,
                               nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsresult res = mUtilRange->SelectNode(aNode);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::WillDeleteNode(nsIDOMNode *aChild)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsresult res = mUtilRange->SelectNode(aChild);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::DidDeleteNode(nsIDOMNode *aChild, nsresult aResult)
{
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::WillSplitNode(nsIDOMNode *aExistingRightNode, int32_t aOffset)
{
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::DidSplitNode(nsIDOMNode *aExistingRightNode,
                              int32_t aOffset,
                              nsIDOMNode *aNewLeftNode,
                              nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsresult res = mUtilRange->SetStart(aNewLeftNode, 0);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(aExistingRightNode, 0);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  // remember split point
  nsresult res = nsEditor::GetLengthOfDOMNode(aLeftNode, mJoinOffset);
  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::DidJoinNodes(nsIDOMNode  *aLeftNode,
                              nsIDOMNode *aRightNode,
                              nsIDOMNode *aParent,
                              nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SetStart(aRightNode, mJoinOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(aRightNode, mJoinOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::WillInsertText(nsIDOMCharacterData *aTextNode, int32_t aOffset, const nsAString &aString)
{
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::DidInsertText(nsIDOMCharacterData *aTextNode,
                                  int32_t aOffset,
                                  const nsAString &aString,
                                  nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  int32_t length = aString.Length();
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(theNode, aOffset+length);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::WillDeleteText(nsIDOMCharacterData *aTextNode, int32_t aOffset, int32_t aLength)
{
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::DidDeleteText(nsIDOMCharacterData *aTextNode,
                                  int32_t aOffset,
                                  int32_t aLength,
                                  nsresult aResult)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(theNode, aOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;
}

NS_IMETHODIMP
nsHTMLEditRules::WillDeleteSelection(nsISelection* aSelection)
{
  if (!mListenerEnabled) {
    return NS_OK;
  }
  RefPtr<Selection> selection = static_cast<Selection*>(aSelection);
  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  int32_t selOffset;

  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(selection,
                                                    getter_AddRefs(selNode),
                                                    &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetStart(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->GetEndNodeAndOffset(selection, getter_AddRefs(selNode),
                                         &selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = mUtilRange->SetEnd(selNode, selOffset);
  NS_ENSURE_SUCCESS(res, res);
  res = UpdateDocChangeRange(mUtilRange);
  return res;
}

NS_IMETHODIMP
nsHTMLEditRules::DidDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}

// Let's remove all alignment hints in the children of aNode; it can
// be an ALIGN attribute (in case we just remove it) or a CENTER
// element (here we have to remove the container and keep its
// children). We break on tables and don't look at their children.
nsresult
nsHTMLEditRules::RemoveAlignment(nsIDOMNode * aNode, const nsAString & aAlignType, bool aChildrenOnly)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsTextNode(aNode) || nsHTMLEditUtils::IsTable(aNode)) return NS_OK;
  nsresult res = NS_OK;

  nsCOMPtr<nsIDOMNode> child = aNode,tmp;
  if (aChildrenOnly)
  {
    aNode->GetFirstChild(getter_AddRefs(child));
  }
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  while (child)
  {
    if (aChildrenOnly) {
      // get the next sibling right now because we could have to remove child
      child->GetNextSibling(getter_AddRefs(tmp));
    }
    else
    {
      tmp = nullptr;
    }
    bool isBlock;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->NodeIsBlockStatic(child, &isBlock);
    NS_ENSURE_SUCCESS(res, res);

    if (nsEditor::NodeIsType(child, nsGkAtoms::center)) {
      // the current node is a CENTER element
      // first remove children's alignment
      res = RemoveAlignment(child, aAlignType, true);
      NS_ENSURE_SUCCESS(res, res);

      // we may have to insert BRs in first and last position of element's children
      // if the nodes before/after are not blocks and not BRs
      res = MakeSureElemStartsOrEndsOnCR(child);
      NS_ENSURE_SUCCESS(res, res);

      // now remove the CENTER container
      NS_ENSURE_STATE(mHTMLEditor);
      nsCOMPtr<Element> childAsElement = do_QueryInterface(child);
      NS_ENSURE_STATE(childAsElement);
      res = mHTMLEditor->RemoveContainer(childAsElement);
      NS_ENSURE_SUCCESS(res, res);
    }
    else if (isBlock || nsHTMLEditUtils::IsHR(child))
    {
      // the current node is a block element
      nsCOMPtr<nsIDOMElement> curElem = do_QueryInterface(child);
      if (nsHTMLEditUtils::SupportsAlignAttr(child))
      {
        // remove the ALIGN attribute if this element can have it
        NS_ENSURE_STATE(mHTMLEditor);
        res = mHTMLEditor->RemoveAttribute(curElem, NS_LITERAL_STRING("align"));
        NS_ENSURE_SUCCESS(res, res);
      }
      if (useCSS)
      {
        if (nsHTMLEditUtils::IsTable(child) || nsHTMLEditUtils::IsHR(child))
        {
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->SetAttributeOrEquivalent(curElem, NS_LITERAL_STRING("align"), aAlignType, false);
        }
        else
        {
          nsAutoString dummyCssValue;
          NS_ENSURE_STATE(mHTMLEditor);
          res = mHTMLEditor->mHTMLCSSUtils->RemoveCSSInlineStyle(child,
            nsGkAtoms::textAlign, dummyCssValue);
        }
        NS_ENSURE_SUCCESS(res, res);
      }
      if (!nsHTMLEditUtils::IsTable(child))
      {
        // unless this is a table, look at children
        res = RemoveAlignment(child, aAlignType, true);
        NS_ENSURE_SUCCESS(res, res);
      }
    }
    child = tmp;
  }
  return NS_OK;
}

// Let's insert a BR as first (resp. last) child of aNode if its
// first (resp. last) child is not a block nor a BR, and if the
// previous (resp. next) sibling is not a block nor a BR
nsresult
nsHTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode, bool aStarts)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMNode> child;
  nsresult res;
  if (aStarts)
  {
    NS_ENSURE_STATE(mHTMLEditor);
    child = GetAsDOMNode(mHTMLEditor->GetFirstEditableChild(*node));
  }
  else
  {
    NS_ENSURE_STATE(mHTMLEditor);
    child = GetAsDOMNode(mHTMLEditor->GetLastEditableChild(*node));
  }
  NS_ENSURE_TRUE(child, NS_OK);
  bool isChildBlock;
  NS_ENSURE_STATE(mHTMLEditor);
  res = mHTMLEditor->NodeIsBlockStatic(child, &isChildBlock);
  NS_ENSURE_SUCCESS(res, res);
  bool foundCR = false;
  if (isChildBlock || nsTextEditUtils::IsBreak(child))
  {
    foundCR = true;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> sibling;
    if (aStarts)
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetPriorHTMLSibling(aNode, address_of(sibling));
    }
    else
    {
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->GetNextHTMLSibling(aNode, address_of(sibling));
    }
    NS_ENSURE_SUCCESS(res, res);
    if (sibling)
    {
      bool isBlock;
      NS_ENSURE_STATE(mHTMLEditor);
      res = mHTMLEditor->NodeIsBlockStatic(sibling, &isBlock);
      NS_ENSURE_SUCCESS(res, res);
      if (isBlock || nsTextEditUtils::IsBreak(sibling))
      {
        foundCR = true;
      }
    }
    else
    {
      foundCR = true;
    }
  }
  if (!foundCR)
  {
    int32_t offset = 0;
    if (!aStarts) {
      nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
      NS_ENSURE_STATE(node);
      offset = node->GetChildCount();
    }
    nsCOMPtr<nsIDOMNode> brNode;
    NS_ENSURE_STATE(mHTMLEditor);
    res = mHTMLEditor->CreateBR(aNode, offset, address_of(brNode));
    NS_ENSURE_SUCCESS(res, res);
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode)
{
  nsresult res = MakeSureElemStartsOrEndsOnCR(aNode, false);
  NS_ENSURE_SUCCESS(res, res);
  res = MakeSureElemStartsOrEndsOnCR(aNode, true);
  return res;
}

nsresult
nsHTMLEditRules::AlignBlock(Element& aElement, const nsAString& aAlignType,
                            ContentsOnly aContentsOnly)
{
  if (!IsBlockNode(aElement) && !aElement.IsHTMLElement(nsGkAtoms::hr)) {
    // We deal only with blocks; early way out
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  nsresult res = RemoveAlignment(aElement.AsDOMNode(), aAlignType,
                                 aContentsOnly == ContentsOnly::yes);
  NS_ENSURE_SUCCESS(res, res);
  NS_NAMED_LITERAL_STRING(attr, "align");
  if (mHTMLEditor->IsCSSEnabled()) {
    // Let's use CSS alignment; we use margin-left and margin-right for tables
    // and text-align for other block-level elements
    res = mHTMLEditor->SetAttributeOrEquivalent(
      static_cast<nsIDOMElement*>(aElement.AsDOMNode()), attr, aAlignType,
      false);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    // HTML case; this code is supposed to be called ONLY if the element
    // supports the align attribute but we'll never know...
    if (nsHTMLEditUtils::SupportsAlignAttr(aElement.AsDOMNode())) {
      res =
        mHTMLEditor->SetAttribute(static_cast<nsIDOMElement*>(aElement.AsDOMNode()),
                                  attr, aAlignType);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::ChangeIndentation(Element& aElement, Change aChange)
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  nsIAtom& marginProperty =
    MarginPropertyAtomForIndent(*mHTMLEditor->mHTMLCSSUtils, aElement);
  nsAutoString value;
  mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(aElement, marginProperty,
                                                   value);
  float f;
  nsCOMPtr<nsIAtom> unit;
  mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, getter_AddRefs(unit));
  if (0 == f) {
    nsAutoString defaultLengthUnit;
    mHTMLEditor->mHTMLCSSUtils->GetDefaultLengthUnit(defaultLengthUnit);
    unit = NS_Atomize(defaultLengthUnit);
  }
  int8_t multiplier = aChange == Change::plus ? +1 : -1;
  if        (nsGkAtoms::in == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_IN * multiplier;
  } else if (nsGkAtoms::cm == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_CM * multiplier;
  } else if (nsGkAtoms::mm == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_MM * multiplier;
  } else if (nsGkAtoms::pt == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_PT * multiplier;
  } else if (nsGkAtoms::pc == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_PC * multiplier;
  } else if (nsGkAtoms::em == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_EM * multiplier;
  } else if (nsGkAtoms::ex == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_EX * multiplier;
  } else if (nsGkAtoms::px == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_PX * multiplier;
  } else if (nsGkAtoms::percentage == unit) {
            f += NS_EDITOR_INDENT_INCREMENT_PERCENT * multiplier;
  }

  if (0 < f) {
    nsAutoString newValue;
    newValue.AppendFloat(f);
    newValue.Append(nsDependentAtomString(unit));
    mHTMLEditor->mHTMLCSSUtils->SetCSSProperty(aElement, marginProperty,
                                               newValue);
    return NS_OK;
  }

  mHTMLEditor->mHTMLCSSUtils->RemoveCSSProperty(aElement, marginProperty,
                                                value);

  // Remove unnecessary divs
  if (!aElement.IsHTMLElement(nsGkAtoms::div) ||
      &aElement == mHTMLEditor->GetActiveEditingHost() ||
      !mHTMLEditor->IsDescendantOfEditorRoot(&aElement) ||
      nsHTMLEditor::HasAttributes(&aElement)) {
    return NS_OK;
  }

  nsresult res = mHTMLEditor->RemoveContainer(&aElement);
  NS_ENSURE_SUCCESS(res, res);

  return NS_OK;
}

//
// Support for Absolute Positioning
//

nsresult
nsHTMLEditRules::WillAbsolutePosition(Selection& aSelection,
                                      bool* aCancel, bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIEditor> kungFuDeathGrip(mHTMLEditor);

  WillInsert(aSelection, aCancel);

  // We want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsCOMPtr<Element> focusElement = mHTMLEditor->GetSelectionContainer();
  if (focusElement && nsHTMLEditUtils::IsImage(focusElement)) {
    mNewBlock = focusElement;
    return NS_OK;
  }

  nsresult res = NormalizeSelection(&aSelection);
  NS_ENSURE_SUCCESS(res, res);
  nsAutoSelectionReset selectionResetter(&aSelection, mHTMLEditor);

  // Convert the selection ranges into "promoted" selection ranges: this
  // basically just expands the range to include the immediate block parent,
  // and then further expands to include any ancestors whose children are all
  // in the range.

  nsTArray<RefPtr<nsRange>> arrayOfRanges;
  GetPromotedRanges(aSelection, arrayOfRanges,
                    EditAction::setAbsolutePosition);

  // Use these ranges to contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes,
                             EditAction::setAbsolutePosition);
  NS_ENSURE_SUCCESS(res, res);

  // If nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    // Get selection location
    NS_ENSURE_STATE(aSelection.GetRangeAt(0) &&
                    aSelection.GetRangeAt(0)->GetStartParent());
    OwningNonNull<nsINode> parent =
      *aSelection.GetRangeAt(0)->GetStartParent();
    int32_t offset = aSelection.GetRangeAt(0)->StartOffset();

    // Make sure we can put a block here
    res = SplitAsNeeded(*nsGkAtoms::div, parent, offset);
    NS_ENSURE_SUCCESS(res, res);
    nsCOMPtr<Element> positionedDiv =
      mHTMLEditor->CreateNode(nsGkAtoms::div, parent, offset);
    NS_ENSURE_STATE(positionedDiv);
    // Remember our new block for postprocessing
    mNewBlock = positionedDiv;
    // Delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      res = mHTMLEditor->DeleteNode(curNode);
      NS_ENSURE_SUCCESS(res, res);
      arrayOfNodes.RemoveElementAt(0);
    }
    // Put selection in new block
    res = aSelection.Collapse(positionedDiv, 0);
    // Prevent selection resetter from overriding us.
    selectionResetter.Abort();
    *aHandled = true;
    NS_ENSURE_SUCCESS(res, res);
    return NS_OK;
  }

  // Okay, now go through all the nodes and put them in a blockquote, or
  // whatever is appropriate.  Woohoo!
  nsCOMPtr<Element> curList, curPositionedDiv, indentedLI;
  for (uint32_t i = 0; i < arrayOfNodes.Length(); i++) {
    // Here's where we actually figure out what to do
    NS_ENSURE_STATE(arrayOfNodes[i]->IsContent());
    OwningNonNull<nsIContent> curNode = *arrayOfNodes[i]->AsContent();

    // Ignore all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(curNode)) {
      continue;
    }

    nsCOMPtr<nsIContent> sibling;

    nsCOMPtr<nsINode> curParent = curNode->GetParentNode();
    int32_t offset = curParent ? curParent->IndexOf(curNode) : -1;

    // Some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent)) {
      // Check to see if curList is still appropriate.  Which it is if curNode
      // is still right after it in the same list.
      if (curList) {
        sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        // Create a new nested list of correct type
        res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                            offset);
        NS_ENSURE_SUCCESS(res, res);
        if (!curPositionedDiv) {
          nsCOMPtr<nsINode> curParentParent = curParent->GetParentNode();
          int32_t parentOffset = curParentParent
            ? curParentParent->IndexOf(curParent) : -1;
          curPositionedDiv = mHTMLEditor->CreateNode(nsGkAtoms::div, curParentParent,
                                                     parentOffset);
          mNewBlock = curPositionedDiv;
        }
        curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                          curPositionedDiv, -1);
        NS_ENSURE_STATE(curList);
        // curList is now the correct thing to put curNode in.  Remember our
        // new block for postprocessing.
      }
      // Tuck the node into the end of the active list
      res = mHTMLEditor->MoveNode(curNode, curList, -1);
      NS_ENSURE_SUCCESS(res, res);
    } else {
      // Not a list item, use blockquote?  If we are inside a list item, we
      // don't want to blockquote, we want to sublist the list item.  We may
      // have several nodes listed in the array of nodes to act on, that are in
      // the same list item.  Since we only want to indent that li once, we
      // must keep track of the most recent indented list item, and not indent
      // it if we find another node to act on that is still inside the same li.
      nsCOMPtr<Element> listItem = IsInListItem(curNode);
      if (listItem) {
        if (indentedLI == listItem) {
          // Already indented this list item
          continue;
        }
        curParent = listItem->GetParentNode();
        offset = curParent ? curParent->IndexOf(listItem) : -1;
        // Check to see if curList is still appropriate.  Which it is if
        // curNode is still right after it in the same list.
        if (curList) {
          sibling = mHTMLEditor->GetPriorHTMLSibling(curNode);
        }

        if (!curList || (sibling && sibling != curList)) {
          // Create a new nested list of correct type
          res = SplitAsNeeded(*curParent->NodeInfo()->NameAtom(), curParent,
                              offset);
          NS_ENSURE_SUCCESS(res, res);
          if (!curPositionedDiv) {
            nsCOMPtr<nsINode> curParentParent = curParent->GetParentNode();
            int32_t parentOffset = curParentParent ?
              curParentParent->IndexOf(curParent) : -1;
            curPositionedDiv = mHTMLEditor->CreateNode(nsGkAtoms::div,
                                                       curParentParent,
                                                       parentOffset);
            mNewBlock = curPositionedDiv;
          }
          curList = mHTMLEditor->CreateNode(curParent->NodeInfo()->NameAtom(),
                                            curPositionedDiv, -1);
          NS_ENSURE_STATE(curList);
        }
        res = mHTMLEditor->MoveNode(listItem, curList, -1);
        NS_ENSURE_SUCCESS(res, res);
        // Remember we indented this li
        indentedLI = listItem;
      } else {
        // Need to make a div to put things in if we haven't already

        if (!curPositionedDiv) {
          if (curNode->IsHTMLElement(nsGkAtoms::div)) {
            curPositionedDiv = curNode->AsElement();
            mNewBlock = curPositionedDiv;
            curList = nullptr;
            continue;
          }
          res = SplitAsNeeded(*nsGkAtoms::div, curParent, offset);
          NS_ENSURE_SUCCESS(res, res);
          curPositionedDiv = mHTMLEditor->CreateNode(nsGkAtoms::div, curParent,
                                                     offset);
          NS_ENSURE_STATE(curPositionedDiv);
          // Remember our new block for postprocessing
          mNewBlock = curPositionedDiv;
          // curPositionedDiv is now the correct thing to put curNode in
        }

        // Tuck the node into the end of the active blockquote
        res = mHTMLEditor->MoveNode(curNode, curPositionedDiv, -1);
        NS_ENSURE_SUCCESS(res, res);
        // Forget curList, if any
        curList = nullptr;
      }
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::DidAbsolutePosition()
{
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  nsCOMPtr<nsIDOMElement> elt =
    static_cast<nsIDOMElement*>(GetAsDOMNode(mNewBlock));
  return absPosHTMLEditor->AbsolutelyPositionElement(elt, true);
}

nsresult
nsHTMLEditRules::WillRemoveAbsolutePosition(Selection* aSelection,
                                            bool* aCancel, bool* aHandled) {
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore aCancel from WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsCOMPtr<nsIDOMElement>  elt;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res =
    mHTMLEditor->GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(elt));
  NS_ENSURE_SUCCESS(res, res);

  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  return absPosHTMLEditor->AbsolutelyPositionElement(elt, false);
}

nsresult
nsHTMLEditRules::WillRelativeChangeZIndex(Selection* aSelection,
                                          int32_t aChange,
                                          bool *aCancel,
                                          bool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore aCancel from WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsCOMPtr<nsIDOMElement>  elt;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult res =
    mHTMLEditor->GetAbsolutelyPositionedSelectionContainer(getter_AddRefs(elt));
  NS_ENSURE_SUCCESS(res, res);

  NS_ENSURE_STATE(mHTMLEditor);
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsIHTMLAbsPosEditor> absPosHTMLEditor = mHTMLEditor;
  int32_t zIndex;
  return absPosHTMLEditor->RelativeChangeElementZIndex(elt, aChange, &zIndex);
}

NS_IMETHODIMP
nsHTMLEditRules::DocumentModified()
{
  nsContentUtils::AddScriptRunner(NewRunnableMethod(this, &nsHTMLEditRules::DocumentModifiedWorker));
  return NS_OK;
}

void
nsHTMLEditRules::DocumentModifiedWorker()
{
  if (!mHTMLEditor) {
    return;
  }

  // DeleteNode below may cause a flush, which could destroy the editor
  nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;

  nsCOMPtr<nsIHTMLEditor> kungFuDeathGrip(mHTMLEditor);
  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  if (!selection) {
    return;
  }

  // Delete our bogus node, if we have one, since the document might not be
  // empty any more.
  if (mBogusNode) {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nullptr;
  }

  // Try to recreate the bogus node if needed.
  CreateBogusNodeIfNeeded(selection);
}
