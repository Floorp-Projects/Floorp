/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLEditRules.h"

#include <stdlib.h>

#include "HTMLEditUtils.h"
#include "TextEditUtils.h"
#include "WSRunObject.h"
#include "mozilla/Assertions.h"
#include "mozilla/CSSEditUtils.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAlgorithm.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsHTMLDocument.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsID.h"
#include "nsIFrame.h"
#include "nsIHTMLAbsPosEditor.h"
#include "nsINode.h"
#include "nsLiteralString.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTextNode.h"
#include "nsThreadUtils.h"
#include "nsUnicharUtils.h"
#include <algorithm>

// Workaround for windows headers
#ifdef SetProp
#undef SetProp
#endif

class nsISupports;

namespace mozilla {

class RulesInfo;

using namespace dom;

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
  return HTMLEditor::NodeIsBlockStatic(&node);
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

static nsAtom&
ParagraphSeparatorElement(ParagraphSeparator separator)
{
  switch (separator) {
    default:
      MOZ_FALLTHROUGH_ASSERT("Unexpected paragraph separator!");

    case ParagraphSeparator::div:
      return *nsGkAtoms::div;

    case ParagraphSeparator::p:
      return *nsGkAtoms::p;

    case ParagraphSeparator::br:
      return *nsGkAtoms::br;
  }
}

class TableCellAndListItemFunctor final : public BoolDomIterFunctor
{
public:
  // Used to build list of all li's, td's & th's iterator covers
  virtual bool operator()(nsINode* aNode) const override
  {
    return HTMLEditUtils::IsTableCell(aNode) ||
           HTMLEditUtils::IsListItem(aNode);
  }
};

class BRNodeFunctor final : public BoolDomIterFunctor
{
public:
  virtual bool operator()(nsINode* aNode) const override
  {
    return aNode->IsHTMLElement(nsGkAtoms::br);
  }
};

class EmptyEditableFunctor final : public BoolDomIterFunctor
{
public:
  explicit EmptyEditableFunctor(HTMLEditor* aHTMLEditor)
    : mHTMLEditor(aHTMLEditor)
  {}

  virtual bool operator()(nsINode* aNode) const override
  {
    if (mHTMLEditor->IsEditable(aNode) &&
        (HTMLEditUtils::IsListItem(aNode) ||
         HTMLEditUtils::IsTableCellOrCaption(*aNode))) {
      bool bIsEmptyNode;
      nsresult rv =
        mHTMLEditor->IsEmptyNode(aNode, &bIsEmptyNode, false, false);
      NS_ENSURE_SUCCESS(rv, false);
      if (bIsEmptyNode) {
        return true;
      }
    }
    return false;
  }

protected:
  HTMLEditor* mHTMLEditor;
};

/********************************************************
 * mozilla::HTMLEditRules
 ********************************************************/

HTMLEditRules::HTMLEditRules()
  : mHTMLEditor(nullptr)
  , mListenerEnabled(false)
  , mReturnInEmptyLIKillsList(false)
  , mDidDeleteSelection(false)
  , mDidRangedDelete(false)
  , mRestoreContentEditableCount(false)
  , mJoinOffset(0)
{
  mIsHTMLEditRules = true;
  InitFields();
}

void
HTMLEditRules::InitFields()
{
  mHTMLEditor = nullptr;
  mDocChangeRange = nullptr;
  mReturnInEmptyLIKillsList = true;
  mDidDeleteSelection = false;
  mDidRangedDelete = false;
  mRestoreContentEditableCount = false;
  mUtilRange = nullptr;
  mJoinOffset = 0;
  mNewBlock = nullptr;
  mRangeItem = new RangeItem();

  InitStyleCacheArray(mCachedStyles);
}

void
HTMLEditRules::InitStyleCacheArray(StyleCache aStyleCache[SIZE_STYLE_TABLE])
{
  aStyleCache[0] = StyleCache(nsGkAtoms::b, nullptr);
  aStyleCache[1] = StyleCache(nsGkAtoms::i, nullptr);
  aStyleCache[2] = StyleCache(nsGkAtoms::u, nullptr);
  aStyleCache[3] = StyleCache(nsGkAtoms::font, nsGkAtoms::face);
  aStyleCache[4] = StyleCache(nsGkAtoms::font, nsGkAtoms::size);
  aStyleCache[5] = StyleCache(nsGkAtoms::font, nsGkAtoms::color);
  aStyleCache[6] = StyleCache(nsGkAtoms::tt, nullptr);
  aStyleCache[7] = StyleCache(nsGkAtoms::em, nullptr);
  aStyleCache[8] = StyleCache(nsGkAtoms::strong, nullptr);
  aStyleCache[9] = StyleCache(nsGkAtoms::dfn, nullptr);
  aStyleCache[10] = StyleCache(nsGkAtoms::code, nullptr);
  aStyleCache[11] = StyleCache(nsGkAtoms::samp, nullptr);
  aStyleCache[12] = StyleCache(nsGkAtoms::var, nullptr);
  aStyleCache[13] = StyleCache(nsGkAtoms::cite, nullptr);
  aStyleCache[14] = StyleCache(nsGkAtoms::abbr, nullptr);
  aStyleCache[15] = StyleCache(nsGkAtoms::acronym, nullptr);
  aStyleCache[16] = StyleCache(nsGkAtoms::backgroundColor, nullptr);
  aStyleCache[17] = StyleCache(nsGkAtoms::sub, nullptr);
  aStyleCache[18] = StyleCache(nsGkAtoms::sup, nullptr);
}

HTMLEditRules::~HTMLEditRules()
{
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLEditRules, TextEditRules)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLEditRules, TextEditRules,
                                   mDocChangeRange, mUtilRange, mNewBlock,
                                   mRangeItem)

nsresult
HTMLEditRules::Init(TextEditor* aTextEditor)
{
  if (NS_WARN_IF(!aTextEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  InitFields();

  mHTMLEditor = aTextEditor->AsHTMLEditor();
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_INVALID_ARG;
  }

  // call through to base class Init
  nsresult rv = TextEditRules::Init(aTextEditor);
  NS_ENSURE_SUCCESS(rv, rv);

  // cache any prefs we care about
  static const char kPrefName[] =
    "editor.html.typing.returnInEmptyListItemClosesList";
  nsAutoCString returnInEmptyLIKillsList;
  Preferences::GetCString(kPrefName, returnInEmptyLIKillsList);

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
  AutoLockRulesSniffing lockIt(this);
  if (!mDocChangeRange) {
    mDocChangeRange = new nsRange(node);
  }

  if (node->IsElement()) {
    ErrorResult rv;
    mDocChangeRange->SelectNode(*node, rv);
    NS_ENSURE_TRUE(!rv.Failed(), rv.StealNSResult());
    AdjustSpecialBreaks();
  }

  StartToListenToEditActions();

  return NS_OK;
}

nsresult
HTMLEditRules::DetachEditor()
{
  EndListeningToEditActions();
  mHTMLEditor = nullptr;
  return TextEditRules::DetachEditor();
}

nsresult
HTMLEditRules::BeforeEdit(EditAction aAction,
                          nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  AutoLockRulesSniffing lockIt(this);
  mDidExplicitlySetInterline = false;

  if (!mActionNesting) {
    mActionNesting++;

    // Clear our flag about if just deleted a range
    mDidRangedDelete = false;

    // Remember where our selection was before edit action took place:

    // Get selection
    RefPtr<Selection> selection = htmlEditor->GetSelection();

    // Get the selection location
    if (NS_WARN_IF(!selection) || !selection->RangeCount()) {
      return NS_ERROR_UNEXPECTED;
    }
    mRangeItem->mStartContainer = selection->GetRangeAt(0)->GetStartContainer();
    mRangeItem->mStartOffset = selection->GetRangeAt(0)->StartOffset();
    mRangeItem->mEndContainer = selection->GetRangeAt(0)->GetEndContainer();
    mRangeItem->mEndOffset = selection->GetRangeAt(0)->EndOffset();
    nsCOMPtr<nsINode> selStartNode = mRangeItem->mStartContainer;
    nsCOMPtr<nsINode> selEndNode = mRangeItem->mEndContainer;

    // Register with range updater to track this as we perturb the doc
    htmlEditor->mRangeUpdater.RegisterRangeItem(mRangeItem);

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
    if (aAction == EditAction::insertText ||
        aAction == EditAction::insertIMEText ||
        aAction == EditAction::deleteSelection ||
        IsStyleCachePreservingAction(aAction)) {
      nsCOMPtr<nsINode> selNode =
        aDirection == nsIEditor::eNext ? selEndNode : selStartNode;
      nsresult rv = CacheInlineStyles(selNode);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Stabilize the document against contenteditable count changes
    nsHTMLDocument* htmlDoc = htmlEditor->GetHTMLDocument();
    if (NS_WARN_IF(!htmlDoc)) {
      return NS_ERROR_FAILURE;
    }
    if (htmlDoc->GetEditingState() == nsIHTMLDocument::eContentEditable) {
      htmlDoc->ChangeContentEditableCount(nullptr, +1);
      mRestoreContentEditableCount = true;
    }

    // Check that selection is in subtree defined by body node
    ConfirmSelectionInBody();
    // Let rules remember the top level action
    mTheAction = aAction;
  }
  return NS_OK;
}


nsresult
HTMLEditRules::AfterEdit(EditAction aAction,
                         nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  AutoLockRulesSniffing lockIt(this);

  MOZ_ASSERT(mActionNesting > 0);
  nsresult rv = NS_OK;
  mActionNesting--;
  if (!mActionNesting) {
    // Do all the tricky stuff
    rv = AfterEditInner(aAction, aDirection);

    // Free up selectionState range item
    htmlEditor->mRangeUpdater.DropRangeItem(mRangeItem);

    // Reset the contenteditable count to its previous value
    if (mRestoreContentEditableCount) {
      nsHTMLDocument* htmlDoc = htmlEditor->GetHTMLDocument();
      if (NS_WARN_IF(!htmlDoc)) {
        return NS_ERROR_FAILURE;
      }
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
HTMLEditRules::AfterEditInner(EditAction aAction,
                              nsIEditor::EDirection aDirection)
{
  ConfirmSelectionInBody();
  if (aAction == EditAction::ignore) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  nsCOMPtr<nsINode> rangeStartContainer, rangeEndContainer;
  uint32_t rangeStartOffset = 0, rangeEndOffset = 0;
  // do we have a real range to act on?
  bool bDamagedRange = false;
  if (mDocChangeRange) {
    rangeStartContainer = mDocChangeRange->GetStartContainer();
    rangeEndContainer = mDocChangeRange->GetEndContainer();
    rangeStartOffset = mDocChangeRange->StartOffset();
    rangeEndOffset = mDocChangeRange->EndOffset();
    if (rangeStartContainer && rangeEndContainer) {
      bDamagedRange = true;
    }
  }

  if (bDamagedRange && !((aAction == EditAction::undo) ||
                         (aAction == EditAction::redo))) {
    // don't let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    NS_ENSURE_STATE(mHTMLEditor);
    AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);

    // expand the "changed doc range" as needed
    PromoteRange(*mDocChangeRange, aAction);

    // if we did a ranged deletion or handling backspace key, make sure we have
    // a place to put caret.
    // Note we only want to do this if the overall operation was deletion,
    // not if deletion was done along the way for EditAction::loadHTML, EditAction::insertText, etc.
    // That's why this is here rather than DidDeleteSelection().
    if (aAction == EditAction::deleteSelection && mDidRangedDelete) {
      nsresult rv = InsertBRIfNeeded(selection);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // add in any needed <br>s, and remove any unneeded ones.
    AdjustSpecialBreaks();

    // merge any adjacent text nodes
    if (aAction != EditAction::insertText &&
        aAction != EditAction::insertIMEText) {
      NS_ENSURE_STATE(mHTMLEditor);
      nsresult rv = mHTMLEditor->CollapseAdjacentTextNodes(mDocChangeRange);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // clean up any empty nodes in the selection
    nsresult rv = RemoveEmptyNodes();
    NS_ENSURE_SUCCESS(rv, rv);

    // attempt to transform any unneeded nbsp's into spaces after doing various operations
    if (aAction == EditAction::insertText ||
        aAction == EditAction::insertIMEText ||
        aAction == EditAction::deleteSelection ||
        aAction == EditAction::insertBreak ||
        aAction == EditAction::htmlPaste ||
        aAction == EditAction::loadHTML) {
      rv = AdjustWhitespace(selection);
      NS_ENSURE_SUCCESS(rv, rv);

      // also do this for original selection endpoints.
      NS_ENSURE_STATE(mHTMLEditor);
      NS_ENSURE_STATE(mRangeItem->mStartContainer);
      NS_ENSURE_STATE(mRangeItem->mEndContainer);
      WSRunObject(mHTMLEditor, mRangeItem->mStartContainer,
                  mRangeItem->mStartOffset).AdjustWhitespace();
      // we only need to handle old selection endpoint if it was different from start
      if (mRangeItem->mStartContainer != mRangeItem->mEndContainer ||
          mRangeItem->mStartOffset != mRangeItem->mEndOffset) {
        NS_ENSURE_STATE(mHTMLEditor);
        WSRunObject(mHTMLEditor, mRangeItem->mEndContainer,
                    mRangeItem->mEndOffset).AdjustWhitespace();
      }
    }

    // if we created a new block, make sure selection lands in it
    if (mNewBlock) {
      rv = PinSelectionToNewBlock(selection);
      mNewBlock = nullptr;
    }

    // adjust selection for insert text, html paste, and delete actions
    if (aAction == EditAction::insertText ||
        aAction == EditAction::insertIMEText ||
        aAction == EditAction::deleteSelection ||
        aAction == EditAction::insertBreak ||
        aAction == EditAction::htmlPaste ||
        aAction == EditAction::loadHTML) {
      rv = AdjustSelection(selection, aDirection);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // check for any styles which were removed inappropriately
    if (aAction == EditAction::insertText ||
        aAction == EditAction::insertIMEText ||
        aAction == EditAction::deleteSelection ||
        IsStyleCachePreservingAction(aAction)) {
      NS_ENSURE_STATE(mHTMLEditor);
      mHTMLEditor->mTypeInState->UpdateSelState(selection);
      rv = ReapplyCachedStyles();
      NS_ENSURE_SUCCESS(rv, rv);
      ClearCachedStyles();
    }
  }

  NS_ENSURE_STATE(mHTMLEditor);

  nsresult rv =
    mHTMLEditor->HandleInlineSpellCheck(
                   aAction, *selection,
                   mRangeItem->mStartContainer,
                   mRangeItem->mStartOffset,
                   rangeStartContainer,
                   rangeStartOffset,
                   rangeEndContainer,
                   rangeEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // detect empty doc
  rv = CreateBogusNodeIfNeeded(selection);
  NS_ENSURE_SUCCESS(rv, rv);

  // adjust selection HINT if needed
  if (!mDidExplicitlySetInterline) {
    CheckInterlinePosition(*selection);
  }

  return NS_OK;
}

nsresult
HTMLEditRules::WillDoAction(Selection* aSelection,
                            RulesInfo* aInfo,
                            bool* aCancel,
                            bool* aHandled)
{
  MOZ_ASSERT(aInfo && aCancel && aHandled);

  *aCancel = false;
  *aHandled = false;

  // Deal with actions for which we don't need to check whether the selection is
  // editable.
  if (aInfo->action == EditAction::outputText ||
      aInfo->action == EditAction::undo ||
      aInfo->action == EditAction::redo) {
    return TextEditRules::WillDoAction(aSelection, aInfo, aCancel, aHandled);
  }

  // Nothing to do if there's no selection to act on
  if (!aSelection) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(aSelection->RangeCount(), NS_OK);

  RefPtr<nsRange> range = aSelection->GetRangeAt(0);
  nsCOMPtr<nsINode> selStartNode = range->GetStartContainer();

  NS_ENSURE_STATE(mHTMLEditor);
  if (!mHTMLEditor->IsModifiableNode(selStartNode)) {
    *aCancel = true;
    return NS_OK;
  }

  nsCOMPtr<nsINode> selEndNode = range->GetEndContainer();

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

  switch (aInfo->action) {
    case EditAction::insertText:
    case EditAction::insertIMEText:
      UndefineCaretBidiLevel(aSelection);
      return WillInsertText(aInfo->action, aSelection, aCancel, aHandled,
                            aInfo->inString, aInfo->outString,
                            aInfo->maxLength);
    case EditAction::loadHTML:
      return WillLoadHTML(aSelection, aCancel);
    case EditAction::insertBreak:
      UndefineCaretBidiLevel(aSelection);
      return WillInsertBreak(*aSelection, aCancel, aHandled);
    case EditAction::deleteSelection:
      return WillDeleteSelection(aSelection, aInfo->collapsedAction,
                                 aInfo->stripWrappers, aCancel, aHandled);
    case EditAction::makeList:
      return WillMakeList(aSelection, aInfo->blockType, aInfo->entireList,
                          aInfo->bulletType, aCancel, aHandled);
    case EditAction::indent:
      return WillIndent(aSelection, aCancel, aHandled);
    case EditAction::outdent:
      return WillOutdent(*aSelection, aCancel, aHandled);
    case EditAction::setAbsolutePosition:
      return WillAbsolutePosition(*aSelection, aCancel, aHandled);
    case EditAction::removeAbsolutePosition:
      return WillRemoveAbsolutePosition(aSelection, aCancel, aHandled);
    case EditAction::align:
      return WillAlign(*aSelection, *aInfo->alignType, aCancel, aHandled);
    case EditAction::makeBasicBlock:
      return WillMakeBasicBlock(*aSelection, *aInfo->blockType, aCancel,
                                aHandled);
    case EditAction::removeList:
      return WillRemoveList(aSelection, aInfo->bOrdered, aCancel, aHandled);
    case EditAction::makeDefListItem:
      return WillMakeDefListItem(aSelection, aInfo->blockType,
                                 aInfo->entireList, aCancel, aHandled);
    case EditAction::insertElement:
      WillInsert(*aSelection, aCancel);
      return NS_OK;
    case EditAction::decreaseZIndex:
      return WillRelativeChangeZIndex(aSelection, -1, aCancel, aHandled);
    case EditAction::increaseZIndex:
      return WillRelativeChangeZIndex(aSelection, 1, aCancel, aHandled);
    default:
      return TextEditRules::WillDoAction(aSelection, aInfo,
                                         aCancel, aHandled);
  }
}

nsresult
HTMLEditRules::DidDoAction(Selection* aSelection,
                           RulesInfo* aInfo,
                           nsresult aResult)
{
  switch (aInfo->action) {
    case EditAction::insertBreak:
      return DidInsertBreak(aSelection, aResult);
    case EditAction::deleteSelection:
      return DidDeleteSelection(aSelection, aInfo->collapsedAction, aResult);
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
      // pass through to TextEditRules
      return TextEditRules::DidDoAction(aSelection, aInfo, aResult);
  }
}

bool
HTMLEditRules::DocumentIsEmpty()
{
  return !!mBogusNode;
}

nsresult
HTMLEditRules::GetListState(bool* aMixed,
                            bool* aOL,
                            bool* aUL,
                            bool* aDL)
{
  NS_ENSURE_TRUE(aMixed && aOL && aUL && aDL, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aOL = false;
  *aUL = false;
  *aDL = false;
  bool bNonList = false;

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv = GetListActionNodes(arrayOfNodes, EntireList::no,
                                   TouchContent::no);
  NS_ENSURE_SUCCESS(rv, rv);

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
HTMLEditRules::GetListItemState(bool* aMixed,
                                bool* aLI,
                                bool* aDT,
                                bool* aDD)
{
  NS_ENSURE_TRUE(aMixed && aLI && aDT && aDD, NS_ERROR_NULL_POINTER);
  *aMixed = false;
  *aLI = false;
  *aDT = false;
  *aDD = false;
  bool bNonList = false;

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv = GetListActionNodes(arrayOfNodes, EntireList::no,
                                   TouchContent::no);
  NS_ENSURE_SUCCESS(rv, rv);

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
  if (*aDT + *aDD + bNonList > 1) {
    *aMixed = true;
  }

  return NS_OK;
}

nsresult
HTMLEditRules::GetAlignment(bool* aMixed,
                            nsIHTMLEditor::EAlignment* aAlign)
{
  MOZ_ASSERT(aMixed && aAlign);

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

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
  NS_ENSURE_STATE(htmlEditor->GetSelection());
  OwningNonNull<Selection> selection = *htmlEditor->GetSelection();

  // Get selection location
  NS_ENSURE_TRUE(htmlEditor->GetRoot(), NS_ERROR_FAILURE);
  OwningNonNull<Element> root = *htmlEditor->GetRoot();

  int32_t rootOffset = root->GetParentNode() ?
                       root->GetParentNode()->ComputeIndexOf(root) : -1;

  nsRange* firstRange = selection->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }
  EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // Is the selection collapsed?
  nsCOMPtr<nsINode> nodeToExamine;
  if (selection->IsCollapsed() || atStartOfSelection.GetContainerAsText()) {
    // If selection is collapsed, we want to look at the container of selection
    // start and its ancestors for divs with alignment on them.  If we are in a
    // text node, then that is the node of interest.
    nodeToExamine = atStartOfSelection.GetContainer();
  } else if (atStartOfSelection.IsContainerHTMLElement(nsGkAtoms::html) &&
             atStartOfSelection.Offset() == static_cast<uint32_t>(rootOffset)) {
    // If we have selected the body, let's look at the first editable node
    nodeToExamine = htmlEditor->GetNextEditableNode(atStartOfSelection);
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

  nsCOMPtr<Element> blockParent = htmlEditor->GetBlock(*nodeToExamine);

  NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);

  if (htmlEditor->IsCSSEnabled() &&
      CSSEditUtils::IsCSSEditableProperty(blockParent, nullptr,
                                          nsGkAtoms::align)) {
    // We are in CSS mode and we know how to align this element with CSS
    nsAutoString value;
    // Let's get the value(s) of text-align or margin-left/margin-right
    CSSEditUtils::GetCSSEquivalentToHTMLInlineStyleSet(
      blockParent, nullptr, nsGkAtoms::align, value, CSSEditUtils::eComputed);
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

    if (CSSEditUtils::IsCSSEditableProperty(nodeToExamine, nullptr,
                                            nsGkAtoms::align)) {
      nsAutoString value;
      CSSEditUtils::GetSpecifiedProperty(*nodeToExamine,
                                         *nsGkAtoms::textAlign,
                                         value);
      if (!value.IsEmpty()) {
        if (value.EqualsLiteral("center")) {
          *aAlign = nsIHTMLEditor::eCenter;
          return NS_OK;
        }
        if (value.EqualsLiteral("right")) {
          *aAlign = nsIHTMLEditor::eRight;
          return NS_OK;
        }
        if (value.EqualsLiteral("justify")) {
          *aAlign = nsIHTMLEditor::eJustify;
          return NS_OK;
        }
        if (value.EqualsLiteral("left")) {
          *aAlign = nsIHTMLEditor::eLeft;
          return NS_OK;
        }
        // XXX
        // text-align: start and end aren't supported yet
      }
    }

    if (HTMLEditUtils::SupportsAlignAttr(*nodeToExamine)) {
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

static nsAtom&
MarginPropertyAtomForIndent(nsINode& aNode)
{
  nsAutoString direction;
  CSSEditUtils::GetComputedProperty(aNode, *nsGkAtoms::direction, direction);
  return direction.EqualsLiteral("rtl") ?
    *nsGkAtoms::marginRight : *nsGkAtoms::marginLeft;
}

nsresult
HTMLEditRules::GetIndentState(bool* aCanIndent,
                              bool* aCanOutdent)
{
  // XXX Looks like that this is implementation of
  //     nsIHTMLEditor::getIndentState() however nobody calls this method
  //     even with the interface method.
  NS_ENSURE_TRUE(aCanIndent && aCanOutdent, NS_ERROR_FAILURE);
  *aCanIndent = true;
  *aCanOutdent = false;

  // get selection
  NS_ENSURE_STATE(mHTMLEditor && mHTMLEditor->GetSelection());
  OwningNonNull<Selection> selection = *mHTMLEditor->GetSelection();

  // contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  nsresult rv = GetNodesFromSelection(*selection, EditAction::indent,
                                      arrayOfNodes, TouchContent::no);
  NS_ENSURE_SUCCESS(rv, rv);

  // examine nodes in selection for blockquotes or list elements;
  // these we can outdent.  Note that we return true for canOutdent
  // if *any* of the selection is outdentable, rather than all of it.
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();
  for (auto& curNode : Reversed(arrayOfNodes)) {
    if (HTMLEditUtils::IsNodeThatCanOutdent(curNode)) {
      *aCanOutdent = true;
      break;
    } else if (useCSS) {
      // we are in CSS mode, indentation is done using the margin-left (or margin-right) property
      nsAtom& marginProperty = MarginPropertyAtomForIndent(curNode);
      nsAutoString value;
      // retrieve its specified value
      CSSEditUtils::GetSpecifiedProperty(*curNode, marginProperty, value);
      float f;
      RefPtr<nsAtom> unit;
      // get its number part and its unit
      CSSEditUtils::ParseLength(value, &f, getter_AddRefs(unit));
      // if the number part is strictly positive, outdent is possible
      if (0 < f) {
        *aCanOutdent = true;
        break;
      }
    }
  }

  if (*aCanOutdent) {
    return NS_OK;
  }

  // if we haven't found something to outdent yet, also check the parents
  // of selection endpoints.  We might have a blockquote or list item
  // in the parent hierarchy.

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_FAILURE;
  }

  Element* rootElement = mHTMLEditor->GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  // Test selection start container hierarchy.
  EditorRawDOMPoint selectionStartPoint(EditorBase::GetStartPoint(selection));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  for (nsINode* node = selectionStartPoint.GetContainer();
       node && node != rootElement;
       node = node->GetParentNode()) {
    if (HTMLEditUtils::IsNodeThatCanOutdent(node)) {
      *aCanOutdent = true;
      return NS_OK;
    }
  }

  // Test selection end container hierarchy.
  EditorRawDOMPoint selectionEndPoint(EditorBase::GetEndPoint(selection));
  if (NS_WARN_IF(!selectionEndPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  for (nsINode* node = selectionEndPoint.GetContainer();
       node && node != rootElement;
       node = node->GetParentNode()) {
    if (HTMLEditUtils::IsNodeThatCanOutdent(node)) {
      *aCanOutdent = true;
      return NS_OK;
    }
  }
  return NS_OK;
}


nsresult
HTMLEditRules::GetParagraphState(bool* aMixed,
                                 nsAString& outFormat)
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
  nsresult rv = GetParagraphFormatNodes(arrayOfNodes, TouchContent::no);
  NS_ENSURE_SUCCESS(rv, rv);

  // post process list.  We need to replace any block nodes that are not format
  // nodes with their content.  This is so we only have to look "up" the hierarchy
  // to find format nodes, instead of both up and down.
  for (int32_t i = arrayOfNodes.Length() - 1; i >= 0; i--) {
    auto& curNode = arrayOfNodes[i];
    nsAutoString format;
    // if it is a known format node we have it easy
    if (IsBlockNode(curNode) && !HTMLEditUtils::IsFormatNode(curNode)) {
      // arrayOfNodes.RemoveObject(curNode);
      rv = AppendInnerFormatNodes(arrayOfNodes, curNode);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // we might have an empty node list.  if so, find selection parent
  // and put that on the list
  if (arrayOfNodes.IsEmpty()) {
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    Selection* selection = mHTMLEditor->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
    EditorRawDOMPoint selectionStartPoint(EditorBase::GetStartPoint(selection));
    if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    arrayOfNodes.AppendElement(*selectionStartPoint.GetContainer());
  }

  // remember root node
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<Element> rootElem = mHTMLEditor->GetRoot();
  NS_ENSURE_TRUE(rootElem, NS_ERROR_NULL_POINTER);

  // loop through the nodes in selection and examine their paragraph format
  for (auto& curNode : Reversed(arrayOfNodes)) {
    nsAutoString format;
    // if it is a known format node we have it easy
    if (HTMLEditUtils::IsFormatNode(curNode)) {
      GetFormatString(curNode, format);
    } else if (IsBlockNode(curNode)) {
      // this is a div or some other non-format block.
      // we should ignore it.  Its children were appended to this list
      // by AppendInnerFormatNodes() call above.  We will get needed
      // info when we examine them instead.
      continue;
    } else {
      nsINode* node = curNode->GetParentNode();
      while (node) {
        if (node == rootElem) {
          format.Truncate(0);
          break;
        } else if (HTMLEditUtils::IsFormatNode(node)) {
          GetFormatString(node, format);
          break;
        }
        // else keep looking up
        node = node->GetParentNode();
      }
    }

    // if this is the first node, we've found, remember it as the format
    if (formatStr.EqualsLiteral("x")) {
      formatStr = format;
    }
    // else make sure it matches previously found format
    else if (format != formatStr) {
      bMixed = true;
      break;
    }
  }

  *aMixed = bMixed;
  outFormat = formatStr;
  return NS_OK;
}

nsresult
HTMLEditRules::AppendInnerFormatNodes(nsTArray<OwningNonNull<nsINode>>& aArray,
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
    bool isFormat = HTMLEditUtils::IsFormatNode(child);
    if (isBlock && !isFormat) {
      // if it's a div, etc., recurse
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
HTMLEditRules::GetFormatString(nsINode* aNode,
                               nsAString& outFormat)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  if (HTMLEditUtils::IsFormatNode(aNode)) {
    aNode->NodeInfo()->NameAtom()->ToString(outFormat);
  } else {
    outFormat.Truncate();
  }
  return NS_OK;
}

void
HTMLEditRules::WillInsert(Selection& aSelection,
                          bool* aCancel)
{
  MOZ_ASSERT(aCancel);

  TextEditRules::WillInsert(aSelection, aCancel);

  NS_ENSURE_TRUE_VOID(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // Adjust selection to prevent insertion after a moz-BR.  This next only
  // works for collapsed selections right now, because selection is a pain to
  // work with when not collapsed.  (no good way to extend start or end of
  // selection), so we ignore those types of selections.
  if (!aSelection.IsCollapsed()) {
    return;
  }

  // If we are after a mozBR in the same block, then move selection to be
  // before it
  nsRange* firstRange = aSelection.GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return;
  }

  EditorRawDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // Get prior node
  nsCOMPtr<nsIContent> priorNode =
    htmlEditor->GetPreviousEditableHTMLNode(atStartOfSelection);
  if (priorNode && TextEditUtils::IsMozBR(priorNode)) {
    RefPtr<Element> block1 =
      htmlEditor->GetBlock(*atStartOfSelection.GetContainer());
    RefPtr<Element> block2 = htmlEditor->GetBlockNodeParent(priorNode);

    if (block1 && block1 == block2) {
      // If we are here then the selection is right after a mozBR that is in
      // the same block as the selection.  We need to move the selection start
      // to be before the mozBR.
      EditorRawDOMPoint point(priorNode);
      IgnoredErrorResult error;
      aSelection.Collapse(point, error);
      if (NS_WARN_IF(error.Failed())) {
        return;
      }
    }
  }

  if (mDidDeleteSelection &&
      (mTheAction == EditAction::insertText ||
       mTheAction == EditAction::insertIMEText ||
       mTheAction == EditAction::deleteSelection)) {
    nsresult rv = ReapplyCachedStyles();
    NS_ENSURE_SUCCESS_VOID(rv);
  }
  // For most actions we want to clear the cached styles, but there are
  // exceptions
  if (!IsStyleCachePreservingAction(mTheAction)) {
    ClearCachedStyles();
  }
}

nsresult
HTMLEditRules::WillInsertText(EditAction aAction,
                              Selection* aSelection,
                              bool* aCancel,
                              bool* aHandled,
                              const nsAString* inString,
                              nsAString* outString,
                              int32_t aMaxLength)
{
  if (NS_WARN_IF(!aSelection) ||
      NS_WARN_IF(!aCancel) ||
      NS_WARN_IF(!aHandled)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // initialize out param
  *aCancel = false;
  *aHandled = true;
  // If the selection isn't collapsed, delete it.  Don't delete existing inline
  // tags, because we're hopefully going to insert text (bug 787432).
  if (!aSelection->IsCollapsed()) {
    nsresult rv =
      htmlEditor->DeleteSelectionAsAction(nsIEditor::eNone,
                                          nsIEditor::eNoStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  WillInsert(*aSelection, aCancel);
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;

  // we need to get the doc
  nsCOMPtr<nsIDocument> doc = htmlEditor->GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }

  // for every property that is set, insert a new inline style node
  nsresult rv = CreateStyleForInsertText(*aSelection, *doc);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // get the (collapsed) selection location
  nsRange* firstRange = aSelection->GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }
  EditorDOMPoint pointToInsert(firstRange->StartRef());
  if (NS_WARN_IF(!pointToInsert.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(pointToInsert.IsSetAndValid());

  // dont put text in places that can't have it
  if (!EditorBase::IsTextNode(pointToInsert.GetContainer()) &&
      !htmlEditor->CanContainTag(*pointToInsert.GetContainer(),
                                 *nsGkAtoms::textTagName)) {
    return NS_ERROR_FAILURE;
  }

  if (aAction == EditAction::insertIMEText) {
    // Right now the WSRunObject code bails on empty strings, but IME needs
    // the InsertTextWithTransaction() call to still happen since empty strings
    // are meaningful there.
    // If there is one or more IME selections, its minimum offset should be
    // the insertion point.
    int32_t IMESelectionOffset =
      htmlEditor->GetIMESelectionStartOffsetIn(pointToInsert.GetContainer());
    if (IMESelectionOffset >= 0) {
      pointToInsert.Set(pointToInsert.GetContainer(), IMESelectionOffset);
    }

    if (inString->IsEmpty()) {
      rv = htmlEditor->InsertTextWithTransaction(
                         *doc, *inString, EditorRawDOMPoint(pointToInsert));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      return NS_OK;
    }

    WSRunObject wsObj(htmlEditor, pointToInsert);
    rv = wsObj.InsertText(*doc, *inString, pointToInsert);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // aAction == kInsertText

  // find where we are
  EditorDOMPoint currentPoint(pointToInsert);

  // is our text going to be PREformatted?
  // We remember this so that we know how to handle tabs.
  bool isPRE = EditorBase::IsPreformatted(pointToInsert.GetContainer());

  // turn off the edit listener: we know how to
  // build the "doc changed range" ourselves, and it's
  // must faster to do it once here than to track all
  // the changes one at a time.
  AutoLockListener lockit(&mListenerEnabled);

  // don't change my selection in subtransactions
  AutoTransactionsConserveSelection dontChangeMySelection(htmlEditor);
  nsAutoString tString(*inString);
  const char16_t *unicodeBuf = tString.get();
  int32_t pos = 0;
  NS_NAMED_LITERAL_STRING(newlineStr, LFSTR);

  {
    AutoTrackDOMPoint tracker(htmlEditor->mRangeUpdater, &pointToInsert);

    // for efficiency, break out the pre case separately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE || IsPlaintextEditor()) {
      while (unicodeBuf && pos != -1 &&
             pos < static_cast<int32_t>(inString->Length())) {
        int32_t oldPos = pos;
        int32_t subStrLen;
        pos = tString.FindChar(nsCRT::LF, oldPos);

        if (pos != -1) {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (!subStrLen) {
            subStrLen = 1;
          }
        } else {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);

        // is it a return?
        if (subStr.Equals(newlineStr)) {
          RefPtr<Element> brElement =
            htmlEditor->InsertBrElementWithTransaction(*aSelection,
                                                       currentPoint,
                                                       nsIEditor::eNone);
          if (NS_WARN_IF(!brElement)) {
            return NS_ERROR_FAILURE;
          }
          pos++;
          if (brElement->GetNextSibling()) {
            pointToInsert.Set(brElement->GetNextSibling());
          } else {
            pointToInsert.SetToEndOf(currentPoint.GetContainer());
          }
          // XXX In most cases, pointToInsert and currentPoint are same here.
          //     But if the <br> element has been moved to different point by
          //     mutation observer, those points become different.
          currentPoint.Set(brElement);
          DebugOnly<bool> advanced = currentPoint.AdvanceOffset();
          NS_WARNING_ASSERTION(advanced,
            "Failed to advance offset after the new <br> element");
          NS_WARNING_ASSERTION(currentPoint == pointToInsert,
            "Perhaps, <br> element position has been moved to different point "
            "by mutation observer");
        } else {
          EditorRawDOMPoint pointAfterInsertedString;
          rv = htmlEditor->InsertTextWithTransaction(
                             *doc, subStr,
                             EditorRawDOMPoint(currentPoint),
                             &pointAfterInsertedString);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          currentPoint = pointAfterInsertedString;
          pointToInsert = pointAfterInsertedString;
        }
      }
    } else {
      NS_NAMED_LITERAL_STRING(tabStr, "\t");
      NS_NAMED_LITERAL_STRING(spacesStr, "    ");
      char specialChars[] = {TAB, nsCRT::LF, 0};
      while (unicodeBuf && pos != -1 &&
             pos < static_cast<int32_t>(inString->Length())) {
        int32_t oldPos = pos;
        int32_t subStrLen;
        pos = tString.FindCharInSet(specialChars, oldPos);

        if (pos != -1) {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (!subStrLen) {
            subStrLen = 1;
          }
        } else {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        WSRunObject wsObj(htmlEditor, currentPoint);

        // is it a tab?
        if (subStr.Equals(tabStr)) {
          EditorRawDOMPoint pointAfterInsertedSpaces;
          rv = wsObj.InsertText(*doc, spacesStr, currentPoint,
                                &pointAfterInsertedSpaces);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          pos++;
          currentPoint = pointAfterInsertedSpaces;
          pointToInsert = pointAfterInsertedSpaces;
        }
        // is it a return?
        else if (subStr.Equals(newlineStr)) {
          RefPtr<Element> newBRElement =
            wsObj.InsertBreak(*aSelection, currentPoint, nsIEditor::eNone);
          if (NS_WARN_IF(!newBRElement)) {
            return NS_ERROR_FAILURE;
          }
          pos++;
          if (newBRElement->GetNextSibling()) {
            pointToInsert.Set(newBRElement->GetNextSibling());
          } else {
            pointToInsert.SetToEndOf(currentPoint.GetContainer());
          }
          currentPoint.Set(newBRElement);
          DebugOnly<bool> advanced = currentPoint.AdvanceOffset();
          NS_WARNING_ASSERTION(advanced,
            "Failed to advance offset to after the new <br> node");
          // XXX If the newBRElement has been moved or removed by mutation
          //     observer, we hit this assert.  We need to check if
          //     newBRElement is in expected point, though, we must have
          //     a lot of same bugs...
          NS_WARNING_ASSERTION(currentPoint == pointToInsert,
            "Perhaps, newBRElement has been moved or removed unexpectedly");
        } else {
          EditorRawDOMPoint pointAfterInsertedString;
          rv = wsObj.InsertText(*doc, subStr, currentPoint,
                                &pointAfterInsertedString);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          currentPoint = pointAfterInsertedString;
          pointToInsert = pointAfterInsertedString;
        }
      }
    }

    // After this block, pointToInsert is updated by AutoTrackDOMPoint.
  }

  aSelection->SetInterlinePosition(false, IgnoreErrors());

  if (currentPoint.IsSet()) {
    ErrorResult error;
    aSelection->Collapse(currentPoint, error);
    if (error.Failed()) {
      NS_WARNING("Failed to collapse at current point");
      error.SuppressException();
    }
  }

  // manually update the doc changed range so that AfterEdit will clean up
  // the correct portion of the document.
  if (!mDocChangeRange) {
    mDocChangeRange = new nsRange(pointToInsert.GetContainer());
  }

  if (currentPoint.IsSet()) {
    rv = mDocChangeRange->SetStartAndEnd(pointToInsert, currentPoint);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  rv = mDocChangeRange->CollapseTo(pointToInsert);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
HTMLEditRules::WillLoadHTML(Selection* aSelection,
                            bool* aCancel)
{
  NS_ENSURE_TRUE(aSelection && aCancel, NS_ERROR_NULL_POINTER);

  *aCancel = false;

  // Delete mBogusNode if it exists. If we really need one,
  // it will be added during post-processing in AfterEditInner().

  if (mBogusNode) {
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_UNEXPECTED;
    }
    RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
    DebugOnly<nsresult> rv = htmlEditor->DeleteNodeWithTransaction(*mBogusNode);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
      "Failed to remove the bogus node");
    mBogusNode = nullptr;
  }

  return NS_OK;
}

bool
HTMLEditRules::CanContainParagraph(Element& aElement) const
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return false;
  }

  if (mHTMLEditor->CanContainTag(aElement, *nsGkAtoms::p)) {
    return true;
  }

  // Even if the element cannot have a <p> element as a child, it can contain
  // <p> element as a descendant if it's one of the following elements.
  if (aElement.IsAnyOfHTMLElements(nsGkAtoms::ol,
                                   nsGkAtoms::ul,
                                   nsGkAtoms::dl,
                                   nsGkAtoms::table,
                                   nsGkAtoms::thead,
                                   nsGkAtoms::tbody,
                                   nsGkAtoms::tfoot,
                                   nsGkAtoms::tr)) {
    return true;
  }

  // XXX Otherwise, Chromium checks the CSS box is a block, but we don't do it
  //     for now.
  return false;
}

nsresult
HTMLEditRules::WillInsertBreak(Selection& aSelection,
                               bool* aCancel,
                               bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);
  *aCancel = false;
  *aHandled = false;

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // If the selection isn't collapsed, delete it.
  if (!aSelection.IsCollapsed()) {
    nsresult rv =
      htmlEditor->DeleteSelectionAsAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  WillInsert(aSelection, aCancel);

  // Initialize out param.  We want to ignore result of WillInsert().
  *aCancel = false;

  // Split any mailcites in the way.  Should we abort this if we encounter
  // table cell boundaries?
  if (IsMailEditor()) {
    nsresult rv = SplitMailCites(&aSelection, aHandled);
    NS_ENSURE_SUCCESS(rv, rv);
    if (*aHandled) {
      return NS_OK;
    }
  }

  // Smart splitting rules
  nsRange* firstRange = aSelection.GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return NS_ERROR_FAILURE;
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // Do nothing if the node is read-only
  if (!htmlEditor->IsModifiableNode(atStartOfSelection.GetContainer())) {
    *aCancel = true;
    return NS_OK;
  }

  // If the active editing host is an inline element, or if the active editing
  // host is the block parent itself and we're configured to use <br> as a
  // paragraph separator, just append a <br>.
  nsCOMPtr<Element> host = htmlEditor->GetActiveEditingHost();
  if (NS_WARN_IF(!host)) {
    return NS_ERROR_FAILURE;
  }

  // Look for the nearest parent block.  However, don't return error even if
  // there is no block parent here because in such case, i.e., editing host
  // is an inline element, we should insert <br> simply.
  RefPtr<Element> blockParent =
    HTMLEditor::GetBlock(*atStartOfSelection.GetContainer(), host);

  ParagraphSeparator separator = htmlEditor->GetDefaultParagraphSeparator();
  bool insertBRElement;
  // If there is no block parent in the editing host, i.e., the editing host
  // itself is also a non-block element, we should insert a <br> element.
  if (!blockParent) {
    // XXX Chromium checks if the CSS box of the editing host is block.
    insertBRElement = true;
  }
  // If only the editing host is block, and the default paragraph separator
  // is <br> or the editing host cannot contain a <p> element, we should
  // insert a <br> element.
  else if (host == blockParent) {
    insertBRElement =
      separator == ParagraphSeparator::br || !CanContainParagraph(*host);
  }
  // If the nearest block parent is a single-line container declared in
  // the execCommand spec and not the editing host, we should separate the
  // block even if the default paragraph separator is <br> element.
  else if (HTMLEditUtils::IsSingleLineContainer(*blockParent)) {
    insertBRElement = false;
  }
  // Otherwise, unless there is no block ancestor which can contain <p>
  // element, we shouldn't insert a <br> element here.
  else {
    insertBRElement = true;
    for (Element* blockAncestor = blockParent;
         blockAncestor && insertBRElement;
         blockAncestor = HTMLEditor::GetBlockNodeParent(blockAncestor, host)) {
      insertBRElement = !CanContainParagraph(*blockAncestor);
    }
  }

  // If we cannot insert a <p>/<div> element at the selection, we should insert
  // a <br> element instead.
  if (insertBRElement) {
    nsresult rv = InsertBRElement(aSelection, atStartOfSelection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    *aHandled = true;
    return NS_OK;
  }

  if (host == blockParent && separator != ParagraphSeparator::br) {
    // Insert a new block first
    MOZ_ASSERT(separator == ParagraphSeparator::div ||
               separator == ParagraphSeparator::p);
    nsresult rv = MakeBasicBlock(aSelection,
                                 ParagraphSeparatorElement(separator));
    // We warn on failure, but don't handle it, because it might be harmless.
    // Instead we just check that a new block was actually created.
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditRules::MakeBasicBlock() failed");

    firstRange = aSelection.GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    atStartOfSelection = firstRange->StartRef();
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

    blockParent =
      HTMLEditor::GetBlock(*atStartOfSelection.GetContainer(), host);
    if (NS_WARN_IF(!blockParent)) {
      return NS_ERROR_UNEXPECTED;
    }
    if (NS_WARN_IF(blockParent == host)) {
      // Didn't create a new block for some reason, fall back to <br>
      rv = InsertBRElement(aSelection, atStartOfSelection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      *aHandled = true;
      return NS_OK;
    }
    // Now, mNewBlock is last created block element for wrapping inline
    // elements around the caret position and AfterEditInner() will move
    // caret into it.  However, it may be different from block parent of
    // the caret position.  E.g., MakeBasicBlock() may wrap following
    // inline elements of a <br> element which is next sibling of container
    // of the caret.  So, we need to adjust mNewBlock here for avoiding
    // jumping caret to odd position.
    mNewBlock = blockParent;
  }

  // If block is empty, populate with br.  (For example, imagine a div that
  // contains the word "text".  The user selects "text" and types return.
  // "Text" is deleted leaving an empty block.  We want to put in one br to
  // make block have a line.  Then code further below will put in a second br.)
  if (IsEmptyBlockElement(*blockParent, IgnoreSingleBR::eNo)) {
    AutoEditorDOMPointChildInvalidator lockOffset(atStartOfSelection);
    EditorRawDOMPoint endOfBlockParent;
    endOfBlockParent.SetToEndOf(blockParent);
    RefPtr<Element> brElement =
      htmlEditor->InsertBrElementWithTransaction(aSelection, endOfBlockParent);
    if (NS_WARN_IF(!brElement)) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<Element> listItem = IsInListItem(blockParent);
  if (listItem && listItem != host) {
    ReturnInListItem(aSelection, *listItem, *atStartOfSelection.GetContainer(),
                     atStartOfSelection.Offset());
    *aHandled = true;
    return NS_OK;
  }

  if (HTMLEditUtils::IsHeader(*blockParent)) {
    // Headers: close (or split) header
    ReturnInHeader(aSelection, *blockParent, *atStartOfSelection.GetContainer(),
                   atStartOfSelection.Offset());
    *aHandled = true;
    return NS_OK;
  }

  // XXX Ideally, we should take same behavior with both <p> container and
  //     <div> container.  However, we are still using <br> as default
  //     paragraph separator (non-standard) and we've split only <p> container
  //     long time.  Therefore, some web apps may depend on this behavior like
  //     Gmail.  So, let's use traditional odd behavior only when the default
  //     paragraph separator is <br>.  Otherwise, take consistent behavior
  //     between <p> container and <div> container.
  if ((separator == ParagraphSeparator::br &&
       blockParent->IsHTMLElement(nsGkAtoms::p)) ||
      (separator != ParagraphSeparator::br &&
       blockParent->IsAnyOfHTMLElements(nsGkAtoms::p, nsGkAtoms::div))) {
    AutoEditorDOMPointChildInvalidator lockOffset(atStartOfSelection);
    // Paragraphs: special rules to look for <br>s
    EditActionResult result = ReturnInParagraph(aSelection, *blockParent);
    if (NS_WARN_IF(result.Failed())) {
      return result.Rv();
    }
    *aHandled = result.Handled();
    *aCancel = result.Canceled();
    if (result.Handled()) {
      // Now, atStartOfSelection may be invalid because the left paragraph
      // may have less children than its offset.  For avoiding warnings of
      // validation of EditorDOMPoint, we should not touch it anymore.
      lockOffset.Cancel();
      return NS_OK;
    }
    // Fall through, if ReturnInParagraph() didn't handle it.
    MOZ_ASSERT(!*aCancel, "ReturnInParagraph canceled this edit action, "
                          "WillInsertBreak() needs to handle such case");
  }

  // If nobody handles this edit action, let's insert new <br> at the selection.
  MOZ_ASSERT(!*aHandled, "Reached last resort of WillInsertBreak() "
                         "after the edit action is handled");
  nsresult rv = InsertBRElement(aSelection, atStartOfSelection);
  *aHandled = true;
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
HTMLEditRules::InsertBRElement(Selection& aSelection,
                               const EditorDOMPoint& aPointToBreak)
{
  if (NS_WARN_IF(!aPointToBreak.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  bool brElementIsAfterBlock = false;
  bool brElementIsBeforeBlock = false;

  // First, insert a <br> element.
  RefPtr<Element> brElement;
  if (IsPlaintextEditor()) {
    brElement =
      htmlEditor->InsertBrElementWithTransaction(aSelection, aPointToBreak);
    if (NS_WARN_IF(!brElement)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    EditorDOMPoint pointToBreak(aPointToBreak);
    WSRunObject wsObj(htmlEditor, pointToBreak);
    int32_t visOffset = 0;
    WSType wsType;
    nsCOMPtr<nsINode> visNode;
    wsObj.PriorVisibleNode(pointToBreak,
                           address_of(visNode), &visOffset, &wsType);
    if (wsType & WSType::block) {
      brElementIsAfterBlock = true;
    }
    wsObj.NextVisibleNode(pointToBreak,
                          address_of(visNode), &visOffset, &wsType);
    if (wsType & WSType::block) {
      brElementIsBeforeBlock = true;
    }
    // If the container of the break is a link, we need to split it and
    // insert new <br> between the split links.
    nsCOMPtr<nsINode> linkDOMNode;
    if (htmlEditor->IsInLink(pointToBreak.GetContainer(),
                             address_of(linkDOMNode))) {
      nsCOMPtr<Element> linkNode = do_QueryInterface(linkDOMNode);
      if (NS_WARN_IF(!linkNode)) {
        return NS_ERROR_FAILURE;
      }
      SplitNodeResult splitLinkNodeResult =
        htmlEditor->SplitNodeDeepWithTransaction(
                      *linkNode, pointToBreak,
                      SplitAtEdges::eDoNotCreateEmptyContainer);
      if (NS_WARN_IF(splitLinkNodeResult.Failed())) {
        return splitLinkNodeResult.Rv();
      }
      pointToBreak = splitLinkNodeResult.SplitPoint();
    }
    brElement = wsObj.InsertBreak(aSelection, pointToBreak, nsIEditor::eNone);
    if (NS_WARN_IF(!brElement)) {
      return NS_ERROR_FAILURE;
    }
  }

  // If the <br> element has already been removed from the DOM tree by a
  // mutation observer, don't continue handling this.
  if (NS_WARN_IF(!brElement->GetParentNode())) {
    return NS_ERROR_FAILURE;
  }

  if (brElementIsAfterBlock && brElementIsBeforeBlock) {
    // We just placed a <br> between block boundaries.  This is the one case
    // where we want the selection to be before the br we just placed, as the
    // br will be on a new line, rather than at end of prior line.
    // XXX brElementIsAfterBlock and brElementIsBeforeBlock were set before
    //     modifying the DOM tree.  So, now, the <br> element may not be
    //     between blocks.
    aSelection.SetInterlinePosition(true, IgnoreErrors());
    EditorRawDOMPoint point(brElement);
    ErrorResult error;
    aSelection.Collapse(point, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  EditorDOMPoint afterBRElement(brElement);
  DebugOnly<bool> advanced = afterBRElement.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced,
    "Failed to advance offset after the new <br> element");
  WSRunObject wsObj(htmlEditor, afterBRElement);
  nsCOMPtr<nsINode> maybeSecondBRNode;
  int32_t visOffset = 0;
  WSType wsType;
  wsObj.NextVisibleNode(afterBRElement,
                        address_of(maybeSecondBRNode), &visOffset, &wsType);
  if (wsType == WSType::br) {
    // The next thing after the break we inserted is another break.  Move the
    // second break to be the first break's sibling.  This will prevent them
    // from being in different inline nodes, which would break
    // SetInterlinePosition().  It will also assure that if the user clicks
    // away and then clicks back on their new blank line, they will still get
    // the style from the line above.
    EditorDOMPoint atSecondBRElement(maybeSecondBRNode);
    if (brElement->GetNextSibling() != maybeSecondBRNode) {
      nsresult rv =
        htmlEditor->MoveNodeWithTransaction(*maybeSecondBRNode->AsContent(),
                                            afterBRElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // SetInterlinePosition(true) means we want the caret to stick to the
  // content on the "right".  We want the caret to stick to whatever is past
  // the break.  This is because the break is on the same line we were on,
  // but the next content will be on the following line.

  // An exception to this is if the break has a next sibling that is a block
  // node.  Then we stick to the left to avoid an uber caret.
  nsIContent* nextSiblingOfBRElement = brElement->GetNextSibling();
  aSelection.SetInterlinePosition(!(nextSiblingOfBRElement &&
                                    IsBlockNode(*nextSiblingOfBRElement)),
                                  IgnoreErrors());
  ErrorResult error;
  aSelection.Collapse(afterBRElement, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

nsresult
HTMLEditRules::DidInsertBreak(Selection* aSelection,
                              nsresult aResult)
{
  return NS_OK;
}

nsresult
HTMLEditRules::SplitMailCites(Selection* aSelection,
                              bool* aHandled)
{
  if (NS_WARN_IF(!aSelection) || NS_WARN_IF(!aHandled)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  EditorRawDOMPoint pointToSplit(EditorBase::GetStartPoint(aSelection));
  if (NS_WARN_IF(!pointToSplit.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Element> citeNode =
    GetTopEnclosingMailCite(*pointToSplit.GetContainer());
  if (!citeNode) {
    return NS_OK;
  }

  // If our selection is just before a break, nudge it to be just after it.
  // This does two things for us.  It saves us the trouble of having to add
  // a break here ourselves to preserve the "blockness" of the inline span
  // mailquote (in the inline case), and :
  // it means the break won't end up making an empty line that happens to be
  // inside a mailquote (in either inline or block case).
  // The latter can confuse a user if they click there and start typing,
  // because being in the mailquote may affect wrapping behavior, or font
  // color, etc.
  WSRunObject wsObj(htmlEditor, pointToSplit);
  nsCOMPtr<nsINode> visNode;
  int32_t visOffset=0;
  WSType wsType;
  wsObj.NextVisibleNode(pointToSplit, address_of(visNode), &visOffset, &wsType);
  // If selection start point is before a break and it's inside the mailquote,
  // let's split it after the visible node.
  if (wsType == WSType::br &&
      visNode != citeNode && citeNode->Contains(visNode)) {
    pointToSplit.Set(visNode);
    DebugOnly<bool> advanced = pointToSplit.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
      "Failed to advance offset to after the visible node");
  }

  if (NS_WARN_IF(!pointToSplit.GetContainerAsContent())) {
    return NS_ERROR_FAILURE;
  }

  SplitNodeResult splitCiteNodeResult =
    htmlEditor->SplitNodeDeepWithTransaction(
                  *citeNode, pointToSplit,
                  SplitAtEdges::eDoNotCreateEmptyContainer);
  if (NS_WARN_IF(splitCiteNodeResult.Failed())) {
    return splitCiteNodeResult.Rv();
  }
  pointToSplit.Clear();

  // Add an invisible <br> to the end of current cite node (If new left cite
  // has not been created, we're at the end of it.  Otherwise, we're still at
  // the right node) if it was a <span> of style="display: block". This is
  // important, since when serializing the cite to plain text, the span which
  // caused the visual break is discarded.  So the added <br> will guarantee
  // that the serializer will insert a break where the user saw one.
  // FYI: splitCiteNodeResult grabs the previous node with nsCOMPtr.  So, it's
  //      safe to access previousNodeOfSplitPoint even after changing the DOM
  //      tree and/or selection even though it's raw pointer.
  nsIContent* previousNodeOfSplitPoint =
    splitCiteNodeResult.GetPreviousNode();
  if (previousNodeOfSplitPoint &&
      previousNodeOfSplitPoint->IsHTMLElement(nsGkAtoms::span) &&
      previousNodeOfSplitPoint->GetPrimaryFrame()->
                                  IsFrameOfType(nsIFrame::eBlockFrame)) {
    nsCOMPtr<nsINode> lastChild =
      previousNodeOfSplitPoint->GetLastChild();
    if (lastChild && !lastChild->IsHTMLElement(nsGkAtoms::br)) {
      // We ignore the result here.
      EditorRawDOMPoint endOfPreviousNodeOfSplitPoint;
      endOfPreviousNodeOfSplitPoint.SetToEndOf(previousNodeOfSplitPoint);
      RefPtr<Element> invisibleBrElement =
        htmlEditor->InsertBrElementWithTransaction(
                      *aSelection,
                      endOfPreviousNodeOfSplitPoint);
      NS_WARNING_ASSERTION(invisibleBrElement,
        "Failed to create an invisible <br> element");
    }
  }

  // In most cases, <br> should be inserted after current cite.  However, if
  // left cite hasn't been created because the split point was start of the
  // cite node, <br> should be inserted before the current cite.
  EditorRawDOMPoint pointToInsertBrNode(splitCiteNodeResult.SplitPoint());
  RefPtr<Element> brElement =
    htmlEditor->InsertBrElementWithTransaction(*aSelection,
                                               pointToInsertBrNode);
  if (NS_WARN_IF(!brElement)) {
    return NS_ERROR_FAILURE;
  }
  // Now, offset of pointToInsertBrNode is invalid.  Let's clear it.
  pointToInsertBrNode.Clear();

  // Want selection before the break, and on same line.
  EditorDOMPoint atBrNode(brElement);
  Unused << atBrNode.Offset(); // Needs offset after collapsing the selection.
  aSelection->SetInterlinePosition(true, IgnoreErrors());
  ErrorResult error;
  aSelection->Collapse(atBrNode, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  // if citeNode wasn't a block, we might also want another break before it.
  // We need to examine the content both before the br we just added and also
  // just after it.  If we don't have another br or block boundary adjacent,
  // then we will need a 2nd br added to achieve blank line that user expects.
  if (IsInlineNode(*citeNode)) {
    // Use DOM point which we tried to collapse to.
    EditorRawDOMPoint pointToCreateNewBrNode(atBrNode.GetContainer(),
                                             atBrNode.Offset());

    WSRunObject wsObj(htmlEditor, pointToCreateNewBrNode);
    nsCOMPtr<nsINode> visNode;
    int32_t visOffset=0;
    WSType wsType;
    wsObj.PriorVisibleNode(pointToCreateNewBrNode,
                           address_of(visNode), &visOffset, &wsType);
    if (wsType == WSType::normalWS || wsType == WSType::text ||
        wsType == WSType::special) {
      EditorRawDOMPoint pointAfterNewBrNode(pointToCreateNewBrNode);
      DebugOnly<bool> advanced = pointAfterNewBrNode.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
        "Failed to advance offset after the <br> node");
      WSRunObject wsObjAfterBR(htmlEditor, pointAfterNewBrNode);
      wsObjAfterBR.NextVisibleNode(pointAfterNewBrNode,
                                   address_of(visNode), &visOffset, &wsType);
      if (wsType == WSType::normalWS || wsType == WSType::text ||
          wsType == WSType::special ||
          // In case we're at the very end.
          wsType == WSType::thisBlock) {
        brElement =
          htmlEditor->InsertBrElementWithTransaction(*aSelection,
                                                     pointToCreateNewBrNode);
        if (NS_WARN_IF(!brElement)) {
          return NS_ERROR_FAILURE;
        }
        // Now, those points may be invalid.
        pointToCreateNewBrNode.Clear();
        pointAfterNewBrNode.Clear();
      }
    }
  }

  // delete any empty cites
  bool bEmptyCite = false;
  if (previousNodeOfSplitPoint) {
    nsresult rv =
      htmlEditor->IsEmptyNode(previousNodeOfSplitPoint, &bEmptyCite,
                              true, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (bEmptyCite) {
      rv = htmlEditor->DeleteNodeWithTransaction(*previousNodeOfSplitPoint);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  if (citeNode) {
    nsresult rv = htmlEditor->IsEmptyNode(citeNode, &bEmptyCite, true, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (bEmptyCite) {
      rv = htmlEditor->DeleteNodeWithTransaction(*citeNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  *aHandled = true;
  return NS_OK;
}


nsresult
HTMLEditRules::WillDeleteSelection(Selection* aSelection,
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
  RefPtr<Element> cell;
  NS_ENSURE_STATE(mHTMLEditor);
  nsresult rv =
    mHTMLEditor->GetFirstSelectedCell(nullptr, getter_AddRefs(cell));
  if (NS_SUCCEEDED(rv) && cell) {
    NS_ENSURE_STATE(mHTMLEditor);
    rv = mHTMLEditor->DeleteTableCellContents();
    *aHandled = true;
    return rv;
  }
  cell = nullptr;

  // origCollapsed is used later to determine whether we should join blocks. We
  // don't really care about bCollapsed because it will be modified by
  // ExtendSelectionForDelete later. TryToJoinBlocksWithTransaction() should
  // happen if the original selection is collapsed and the cursor is at the end
  // of a block element, in which case ExtendSelectionForDelete would always
  // make the selection not collapsed.
  bool bCollapsed = aSelection->IsCollapsed();
  bool join = false;
  bool origCollapsed = bCollapsed;

  nsCOMPtr<nsINode> selNode;
  int32_t selOffset;

  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  nsCOMPtr<nsINode> startNode = aSelection->GetRangeAt(0)->GetStartContainer();
  int32_t startOffset = aSelection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

  if (bCollapsed) {
    // If we are inside an empty block, delete it.
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<Element> host = mHTMLEditor->GetActiveEditingHost();
    NS_ENSURE_TRUE(host, NS_ERROR_FAILURE);
    rv = CheckForEmptyBlock(startNode, host, aSelection, aAction, aHandled);
    NS_ENSURE_SUCCESS(rv, rv);
    if (*aHandled) {
      return NS_OK;
    }

    // Test for distance between caret and text that will be deleted
    rv = CheckBidiLevelForDeletion(aSelection,
                                   EditorRawDOMPoint(startNode, startOffset),
                                   aAction, aCancel);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (*aCancel) {
      return NS_OK;
    }

    NS_ENSURE_STATE(mHTMLEditor);
    rv = mHTMLEditor->ExtendSelectionForDelete(aSelection, &aAction);
    NS_ENSURE_SUCCESS(rv, rv);

    // We should delete nothing.
    if (aAction == nsIEditor::eNone) {
      return NS_OK;
    }

    // ExtendSelectionForDelete() may have changed the selection, update it
    NS_ENSURE_STATE(aSelection->GetRangeAt(0));
    startNode = aSelection->GetRangeAt(0)->GetStartContainer();
    startOffset = aSelection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

    bCollapsed = aSelection->IsCollapsed();
  }

  if (bCollapsed) {
    // What's in the direction we are deleting?
    NS_ENSURE_STATE(mHTMLEditor);
    WSRunObject wsObj(mHTMLEditor, startNode, startOffset);
    nsCOMPtr<nsINode> visNode;
    int32_t visOffset;
    WSType wsType;

    // Find next visible node
    if (aAction == nsIEditor::eNext) {
      wsObj.NextVisibleNode(EditorRawDOMPoint(startNode, startOffset),
                            address_of(visNode), &visOffset, &wsType);
    } else {
      wsObj.PriorVisibleNode(EditorRawDOMPoint(startNode, startOffset),
                             address_of(visNode), &visOffset, &wsType);
    }

    if (!visNode) {
      // Can't find anything to delete!
      *aCancel = true;
      // XXX This is the result of mHTMLEditor->GetFirstSelectedCell().
      //     The value could be both an error and NS_OK.
      return rv;
    }

    if (wsType == WSType::normalWS) {
      // We found some visible ws to delete.  Let ws code handle it.
      *aHandled = true;
      if (aAction == nsIEditor::eNext) {
        rv = wsObj.DeleteWSForward();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        rv = wsObj.DeleteWSBackward();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      return InsertBRIfNeeded(aSelection);
    }

    if (wsType == WSType::text) {
      // Found normal text to delete.
      OwningNonNull<Text> nodeAsText = *visNode->GetAsText();
      int32_t so = visOffset;
      int32_t eo = visOffset + 1;
      if (aAction == nsIEditor::ePrevious) {
        if (!so) {
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

        NS_ASSERTION(range->GetStartContainer() == visNode,
                     "selection start not in visNode");
        NS_ASSERTION(range->GetEndContainer() == visNode,
                     "selection end not in visNode");

        so = range->StartOffset();
        eo = range->EndOffset();
      }
      if (NS_WARN_IF(!mHTMLEditor)) {
        return NS_ERROR_FAILURE;
      }
      RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
      rv = WSRunObject::PrepareToDeleteRange(htmlEditor, address_of(visNode),
                                             &so, address_of(visNode), &eo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      *aHandled = true;
      rv = htmlEditor->DeleteTextWithTransaction(nodeAsText, std::min(so, eo),
                                                 DeprecatedAbs(eo - so));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // XXX When Backspace key is pressed, Chromium removes following empty
      //     text nodes when removing the last character of the non-empty text
      //     node.  However, Edge never removes empty text nodes even if
      //     selection is in the following empty text node(s).  For now, we
      //     should keep our traditional behavior same as Edge for backward
      //     compatibility.
      // XXX When Delete key is pressed, Edge removes all preceding empty
      //     text nodes when removing the first character of the non-empty
      //     text node.  Chromium removes only selected empty text node and
      //     following empty text nodes and the first character of the
      //     non-empty text node.  For now, we should keep our traditional
      //     behavior same as Chromium for backward compatibility.

      DeleteNodeIfCollapsedText(nodeAsText);

      rv = InsertBRIfNeeded(aSelection);
      NS_ENSURE_SUCCESS(rv, rv);

      // Remember that we did a ranged delete for the benefit of
      // AfterEditInner().
      mDidRangedDelete = true;

      return NS_OK;
    }

    if (wsType == WSType::special || wsType == WSType::br ||
        visNode->IsHTMLElement(nsGkAtoms::hr)) {
      // Short circuit for invisible breaks.  delete them and recurse.
      if (visNode->IsHTMLElement(nsGkAtoms::br) &&
          (!mHTMLEditor || !mHTMLEditor->IsVisibleBRElement(visNode))) {
        if (NS_WARN_IF(!mHTMLEditor)) {
          return NS_ERROR_FAILURE;
        }
        RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
        rv = htmlEditor->DeleteNodeWithTransaction(*visNode);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
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
        selOffset = selNode ? selNode->ComputeIndexOf(visNode) : -1;

        ErrorResult err;
        bool interLineIsRight = aSelection->GetInterlinePosition(err);
        if (NS_WARN_IF(err.Failed())) {
          return err.StealNSResult();
        }

        if (startNode == selNode && startOffset - 1 == selOffset &&
            !interLineIsRight) {
          moveOnly = false;
        }

        if (moveOnly) {
          // Go to the position after the <hr>, but to the end of the <hr> line
          // by setting the interline position to left.
          ++selOffset;
          aSelection->Collapse(selNode, selOffset);
          aSelection->SetInterlinePosition(false, IgnoreErrors());
          mDidExplicitlySetInterline = true;
          *aHandled = true;

          // There is one exception to the move only case.  If the <hr> is
          // followed by a <br> we want to delete the <br>.

          WSType otherWSType;
          nsCOMPtr<nsINode> otherNode;
          int32_t otherOffset;

          wsObj.NextVisibleNode(EditorRawDOMPoint(startNode, startOffset),
                                address_of(otherNode),
                                &otherOffset, &otherWSType);

          if (otherWSType == WSType::br) {
            // Delete the <br>
            if (NS_WARN_IF(!mHTMLEditor) ||
                NS_WARN_IF(!otherNode->IsContent())) {
              return NS_ERROR_FAILURE;
            }
            nsIContent* otherContent = otherNode->AsContent();
            RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
            rv = WSRunObject::PrepareToDeleteNode(htmlEditor, otherContent);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
            rv = htmlEditor->DeleteNodeWithTransaction(*otherContent);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
          }

          return NS_OK;
        }
        // Else continue with normal delete code
      }

      if (NS_WARN_IF(!mHTMLEditor) ||
          NS_WARN_IF(!visNode->IsContent())) {
        return NS_ERROR_FAILURE;
      }
      RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
      // Found break or image, or hr.
      rv = WSRunObject::PrepareToDeleteNode(htmlEditor, visNode->AsContent());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // Remember sibling to visnode, if any
      nsCOMPtr<nsIContent> sibling = htmlEditor->GetPriorHTMLSibling(visNode);
      // Delete the node, and join like nodes if appropriate
      rv = htmlEditor->DeleteNodeWithTransaction(*visNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // We did something, so let's say so.
      *aHandled = true;
      // Is there a prior node and are they siblings?
      nsCOMPtr<nsINode> stepbrother;
      if (sibling) {
        stepbrother = htmlEditor->GetNextHTMLSibling(sibling);
      }
      // Are they both text nodes?  If so, join them!
      if (startNode == stepbrother && startNode->GetAsText() &&
          sibling->GetAsText()) {
        EditorDOMPoint pt =
          JoinNearestEditableNodesWithTransaction(*sibling,
                                                  *startNode->AsContent());
        if (NS_WARN_IF(!pt.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        // Fix up selection
        ErrorResult error;
        aSelection->Collapse(pt, error);
        if (NS_WARN_IF(error.Failed())) {
          return error.StealNSResult();
        }
      }
      rv = InsertBRIfNeeded(aSelection);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }

    if (wsType == WSType::otherBlock) {
      // Make sure it's not a table element.  If so, cancel the operation
      // (translation: users cannot backspace or delete across table cells)
      if (HTMLEditUtils::IsTableElement(visNode)) {
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
        wsObj.PriorVisibleNode(EditorRawDOMPoint(startNode, startOffset),
                               address_of(otherNode),
                               &otherOffset, &otherWSType);
      } else {
        wsObj.NextVisibleNode(EditorRawDOMPoint(startNode, startOffset),
                              address_of(otherNode),
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
        if (NS_WARN_IF(!mHTMLEditor)) {
          return NS_ERROR_FAILURE;
        }
        RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
        rv = htmlEditor->DeleteNodeWithTransaction(*otherNode);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        // XXX Only in this case, setting "handled" to true only when it
        //     succeeds?
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
        EditorDOMPoint newSel = GetGoodSelPointForNode(*leafNode, aAction);
        if (NS_WARN_IF(!newSel.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        aSelection->Collapse(newSel);
        return NS_OK;
      }

      // Else we are joining content to block

      nsCOMPtr<nsINode> selPointNode = startNode;
      int32_t selPointOffset = startOffset;
      {
        NS_ENSURE_STATE(mHTMLEditor);
        AutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                  address_of(selPointNode), &selPointOffset);
        NS_ENSURE_STATE(leftNode && leftNode->IsContent() &&
                        rightNode && rightNode->IsContent());
        EditActionResult ret =
          TryToJoinBlocksWithTransaction(*leftNode->AsContent(),
                                         *rightNode->AsContent());
        *aHandled |= ret.Handled();
        *aCancel |= ret.Canceled();
        if (NS_WARN_IF(ret.Failed())) {
          return ret.Rv();
        }
      }

      // If TryToJoinBlocksWithTransaction() didn't handle it  and it's not
      // canceled, user may want to modify the start leaf node or the last leaf
      // node of the block.
      if (!*aHandled && !*aCancel && leafNode != startNode) {
        int32_t offset =
          aAction == nsIEditor::ePrevious ?
            static_cast<int32_t>(leafNode->Length()) : 0;
        aSelection->Collapse(leafNode, offset);
        return WillDeleteSelection(aSelection, aAction, aStripWrappers,
                                   aCancel, aHandled);
      }

      // Otherwise, we must have deleted the selection as user expected.
      aSelection->Collapse(selPointNode, selPointOffset);
      return NS_OK;
    }

    if (wsType == WSType::thisBlock) {
      // At edge of our block.  Look beside it and see if we can join to an
      // adjacent block

      // Make sure it's not a table element.  If so, cancel the operation
      // (translation: users cannot backspace or delete across table cells)
      if (HTMLEditUtils::IsTableElement(visNode)) {
        *aCancel = true;
        return NS_OK;
      }

      // First find the relevant nodes
      nsCOMPtr<nsINode> leftNode, rightNode;
      if (aAction == nsIEditor::ePrevious) {
        NS_ENSURE_STATE(mHTMLEditor);
        leftNode = mHTMLEditor->GetPreviousEditableHTMLNode(*visNode);
        rightNode = startNode;
      } else {
        NS_ENSURE_STATE(mHTMLEditor);
        rightNode = mHTMLEditor->GetNextEditableHTMLNode(*visNode);
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
        AutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater,
                                  address_of(selPointNode), &selPointOffset);
        NS_ENSURE_STATE(leftNode->IsContent() && rightNode->IsContent());
        EditActionResult ret =
          TryToJoinBlocksWithTransaction(*leftNode->AsContent(),
                                         *rightNode->AsContent());
        // This should claim that trying to join the block means that
        // this handles the action because the caller shouldn't do anything
        // anymore in this case.
        *aHandled = true;
        *aCancel |= ret.Canceled();
        if (NS_WARN_IF(ret.Failed())) {
          return ret.Rv();
        }
      }
      aSelection->Collapse(selPointNode, selPointOffset);
      return NS_OK;
    }
  }


  // Else we have a non-collapsed selection.  First adjust the selection.
  rv = ExpandSelectionForDeletion(*aSelection);
  NS_ENSURE_SUCCESS(rv, rv);

  // Remember that we did a ranged delete for the benefit of AfterEditInner().
  mDidRangedDelete = true;

  // Refresh start and end points
  NS_ENSURE_STATE(aSelection->GetRangeAt(0));
  startNode = aSelection->GetRangeAt(0)->GetStartContainer();
  startOffset = aSelection->GetRangeAt(0)->StartOffset();
  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);
  nsCOMPtr<nsINode> endNode = aSelection->GetRangeAt(0)->GetEndContainer();
  int32_t endOffset = aSelection->GetRangeAt(0)->EndOffset();
  NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

  // Figure out if the endpoints are in nodes that can be merged.  Adjust
  // surrounding whitespace in preparation to delete selection.
  if (!IsPlaintextEditor()) {
    NS_ENSURE_STATE(mHTMLEditor);
    AutoTransactionsConserveSelection dontChangeMySelection(mHTMLEditor);
    rv = WSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                           address_of(startNode), &startOffset,
                                           address_of(endNode), &endOffset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  {
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_FAILURE;
    }
    RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

    // Track location of where we are deleting
    AutoTrackDOMPoint startTracker(htmlEditor->mRangeUpdater,
                                   address_of(startNode), &startOffset);
    AutoTrackDOMPoint endTracker(htmlEditor->mRangeUpdater,
                                 address_of(endNode), &endOffset);
    // We are handling all ranged deletions directly now.
    *aHandled = true;

    if (endNode == startNode) {
      rv = htmlEditor->DeleteSelectionWithTransaction(aAction, aStripWrappers);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
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
      nsCOMPtr<Element> leftParent = HTMLEditor::GetBlock(*startNode);
      nsCOMPtr<Element> rightParent = HTMLEditor::GetBlock(*endNode);

      // Are endpoint block parents the same?  Use default deletion
      if (leftParent && leftParent == rightParent) {
        NS_ENSURE_STATE(mHTMLEditor);
        htmlEditor->DeleteSelectionWithTransaction(aAction, aStripWrappers);
      } else {
        // Deleting across blocks.  Are the blocks of same type?
        NS_ENSURE_STATE(leftParent && rightParent);

        // Are the blocks siblings?
        nsCOMPtr<nsINode> leftBlockParent = leftParent->GetParentNode();
        nsCOMPtr<nsINode> rightBlockParent = rightParent->GetParentNode();

        // MOOSE: this could conceivably screw up a table.. fix me.
        if (leftBlockParent == rightBlockParent &&
            htmlEditor->AreNodesSameType(leftParent, rightParent) &&
            // XXX What's special about these three types of block?
            (leftParent->IsHTMLElement(nsGkAtoms::p) ||
             HTMLEditUtils::IsListItem(leftParent) ||
             HTMLEditUtils::IsHeader(*leftParent))) {
          // First delete the selection
          rv = htmlEditor->DeleteSelectionWithTransaction(aAction,
                                                          aStripWrappers);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          // Join blocks
          EditorDOMPoint pt =
            htmlEditor->JoinNodesDeepWithTransaction(*leftParent,
                                                     *rightParent);
          if (NS_WARN_IF(!pt.IsSet())) {
            return NS_ERROR_FAILURE;
          }
          // Fix up selection
          ErrorResult error;
          aSelection->Collapse(pt, error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          return NS_OK;
        }

        // Else blocks not same type, or not siblings.  Delete everything
        // except table elements.
        join = true;

        AutoRangeArray arrayOfRanges(aSelection);
        for (auto& range : arrayOfRanges.mRanges) {
          // Build a list of nodes in the range
          nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
          TrivialFunctor functor;
          DOMSubtreeIterator iter;
          nsresult rv = iter.Init(*range);
          NS_ENSURE_SUCCESS(rv, rv);
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
              if (Text* text = content->GetAsText()) {
                join = !htmlEditor->IsInVisibleTextFrames(*text);
              } else {
                join = content->IsHTMLElement(nsGkAtoms::br) &&
                       !htmlEditor->IsVisibleBRElement(somenode);
              }
            }
          }
        }

        // Check endpoints for possible text deletion.  We can assume that if
        // text node is found, we can delete to end or to begining as
        // appropriate, since the case where both sel endpoints in same text
        // node was already handled (we wouldn't be here)
        if (startNode->GetAsText() &&
            startNode->Length() > static_cast<uint32_t>(startOffset)) {
          // Delete to last character
          OwningNonNull<CharacterData> dataNode =
            *static_cast<CharacterData*>(startNode.get());
          rv = htmlEditor->DeleteTextWithTransaction(
                             dataNode, startOffset,
                             startNode->Length() - startOffset);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
        if (endNode->GetAsText() && endOffset) {
          // Delete to first character
          OwningNonNull<CharacterData> dataNode =
            *static_cast<CharacterData*>(endNode.get());
          rv = htmlEditor->DeleteTextWithTransaction(dataNode, 0, endOffset);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }

        if (join) {
          EditActionResult ret =
            TryToJoinBlocksWithTransaction(*leftParent, *rightParent);
          MOZ_ASSERT(*aHandled);
          *aCancel |= ret.Canceled();
          if (NS_WARN_IF(ret.Failed())) {
            return ret.Rv();
          }
        }
      }
    }
  }

  // We might have left only collapsed whitespace in the start/end nodes
  {
    AutoTrackDOMPoint startTracker(mHTMLEditor->mRangeUpdater,
                                   address_of(startNode), &startOffset);
    AutoTrackDOMPoint endTracker(mHTMLEditor->mRangeUpdater,
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
    rv = aSelection->Collapse(endNode, endOffset);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = aSelection->Collapse(startNode, startOffset);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
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
HTMLEditRules::DeleteNodeIfCollapsedText(nsINode& aNode)
{
  Text* text = aNode.GetAsText();
  if (!text) {
    return;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return;
  }

  if (!mHTMLEditor->IsVisibleTextNode(*text)) {
    RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
    DebugOnly<nsresult> rv = htmlEditor->DeleteNodeWithTransaction(aNode);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to remove aNode");
  }
}


/**
 * InsertBRIfNeeded() determines if a br is needed for current selection to not
 * be spastic.  If so, it inserts one.  Callers responsibility to only call
 * with collapsed selection.
 *
 * @param aSelection        The collapsed selection.
 */
nsresult
HTMLEditRules::InsertBRIfNeeded(Selection* aSelection)
{
  if (NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  EditorRawDOMPoint atStartOfSelection(EditorBase::GetStartPoint(aSelection));
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // inline elements don't need any br
  if (!IsBlockNode(*atStartOfSelection.GetContainer())) {
    return NS_OK;
  }

  // examine selection
  WSRunObject wsObj(htmlEditor, atStartOfSelection);
  if (((wsObj.mStartReason & WSType::block) ||
       (wsObj.mStartReason & WSType::br)) &&
      (wsObj.mEndReason & WSType::block)) {
    // if we are tucked between block boundaries then insert a br
    // first check that we are allowed to
    if (htmlEditor->CanContainTag(*atStartOfSelection.GetContainer(),
                                  *nsGkAtoms::br)) {
      RefPtr<Element> brElement =
        htmlEditor->InsertBrElementWithTransaction(*aSelection,
                                                   atStartOfSelection,
                                                   nsIEditor::ePrevious);
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }
      return NS_OK;
    }
  }
  return NS_OK;
}

/**
 * GetGoodSelPointForNode() finds where at a node you would want to set the
 * selection if you were trying to have a caret next to it.  Always returns a
 * valid value (unless mHTMLEditor has gone away).
 *
 * @param aNode         The node
 * @param aAction       Which edge to find:
 *                        eNext/eNextWord/eToEndOfLine indicates beginning,
 *                        ePrevious/PreviousWord/eToBeginningOfLine ending.
 */
EditorDOMPoint
HTMLEditRules::GetGoodSelPointForNode(nsINode& aNode,
                                      nsIEditor::EDirection aAction)
{
  MOZ_ASSERT(aAction == nsIEditor::eNext ||
             aAction == nsIEditor::eNextWord ||
             aAction == nsIEditor::ePrevious ||
             aAction == nsIEditor::ePreviousWord ||
             aAction == nsIEditor::eToBeginningOfLine ||
             aAction == nsIEditor::eToEndOfLine);

  bool isPreviousAction = (aAction == nsIEditor::ePrevious ||
                           aAction == nsIEditor::ePreviousWord ||
                           aAction == nsIEditor::eToBeginningOfLine);

  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditorDOMPoint();
  }
  if (aNode.GetAsText() || mHTMLEditor->IsContainer(&aNode) ||
      NS_WARN_IF(!aNode.GetParentNode())) {
    return EditorDOMPoint(&aNode, isPreviousAction ? aNode.Length() : 0);
  }

  if (NS_WARN_IF(!mHTMLEditor) ||
      NS_WARN_IF(!aNode.IsContent())) {
    return EditorDOMPoint();
  }

  EditorDOMPoint ret(&aNode);
  if ((!aNode.IsHTMLElement(nsGkAtoms::br) ||
       mHTMLEditor->IsVisibleBRElement(&aNode)) && isPreviousAction) {
    ret.AdvanceOffset();
  }
  return ret;
}

EditActionResult
HTMLEditRules::TryToJoinBlocksWithTransaction(nsIContent& aLeftNode,
                                              nsIContent& aRightNode)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditActionIgnored(NS_ERROR_UNEXPECTED);
  }

  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  nsCOMPtr<Element> leftBlock = htmlEditor->GetBlock(aLeftNode);
  nsCOMPtr<Element> rightBlock = htmlEditor->GetBlock(aRightNode);

  // Sanity checks
  if (NS_WARN_IF(!leftBlock) || NS_WARN_IF(!rightBlock)) {
    return EditActionIgnored(NS_ERROR_NULL_POINTER);
  }
  if (NS_WARN_IF(leftBlock == rightBlock)) {
    return EditActionIgnored(NS_ERROR_UNEXPECTED);
  }

  if (HTMLEditUtils::IsTableElement(leftBlock) ||
      HTMLEditUtils::IsTableElement(rightBlock)) {
    // Do not try to merge table elements
    return EditActionCanceled();
  }

  // Make sure we don't try to move things into HR's, which look like blocks
  // but aren't containers
  if (leftBlock->IsHTMLElement(nsGkAtoms::hr)) {
    leftBlock = htmlEditor->GetBlockNodeParent(leftBlock);
    if (NS_WARN_IF(!leftBlock)) {
      return EditActionIgnored(NS_ERROR_UNEXPECTED);
    }
  }
  if (rightBlock->IsHTMLElement(nsGkAtoms::hr)) {
    rightBlock = htmlEditor->GetBlockNodeParent(rightBlock);
    if (NS_WARN_IF(!rightBlock)) {
      return EditActionIgnored(NS_ERROR_UNEXPECTED);
    }
  }

  // Bail if both blocks the same
  if (leftBlock == rightBlock) {
    return EditActionIgnored();
  }

  // Joining a list item to its parent is a NOP.
  if (HTMLEditUtils::IsList(leftBlock) &&
      HTMLEditUtils::IsListItem(rightBlock) &&
      rightBlock->GetParentNode() == leftBlock) {
    return EditActionHandled();
  }

  // Special rule here: if we are trying to join list items, and they are in
  // different lists, join the lists instead.
  bool mergeLists = false;
  nsAtom* existingList = nsGkAtoms::_empty;
  EditorDOMPoint atChildInBlock;
  nsCOMPtr<Element> leftList, rightList;
  if (HTMLEditUtils::IsListItem(leftBlock) &&
      HTMLEditUtils::IsListItem(rightBlock)) {
    leftList = leftBlock->GetParentElement();
    rightList = rightBlock->GetParentElement();
    if (leftList && rightList && leftList != rightList &&
        !EditorUtils::IsDescendantOf(*leftList, *rightBlock, &atChildInBlock) &&
        !EditorUtils::IsDescendantOf(*rightList, *leftBlock, &atChildInBlock)) {
      // There are some special complications if the lists are descendants of
      // the other lists' items.  Note that it is okay for them to be
      // descendants of the other lists themselves, which is the usual case for
      // sublists in our implementation.
      MOZ_DIAGNOSTIC_ASSERT(!atChildInBlock.IsSet());
      leftBlock = leftList;
      rightBlock = rightList;
      mergeLists = true;
      existingList = leftList->NodeInfo()->NameAtom();
    }
  }

  AutoTransactionsConserveSelection dontChangeMySelection(htmlEditor);

  // offset below is where you find yourself in rightBlock when you traverse
  // upwards from leftBlock
  EditorDOMPoint atRightBlockChild;
  if (EditorUtils::IsDescendantOf(*leftBlock, *rightBlock,
                                  &atRightBlockChild)) {
    // Tricky case.  Left block is inside right block.  Do ws adjustment.  This
    // just destroys non-visible ws at boundaries we will be joining.
    DebugOnly<bool> advanced = atRightBlockChild.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
      "Failed to advance offset to after child of rightBlock, "
      "leftBlock is a descendant of the child");
    nsresult rv = WSRunObject::ScrubBlockBoundary(htmlEditor,
                                                  WSRunObject::kBlockEnd,
                                                  leftBlock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }

    {
      // We can't just track rightBlock because it's an Element.
      AutoTrackDOMPoint tracker(htmlEditor->mRangeUpdater, &atRightBlockChild);
      rv = WSRunObject::ScrubBlockBoundary(htmlEditor,
                                           WSRunObject::kAfterBlock,
                                           atRightBlockChild.GetContainer(),
                                           atRightBlockChild.Offset());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }

      // XXX AutoTrackDOMPoint instance, tracker, hasn't been destroyed here.
      //     Do we really need to do update rightBlock here??
      MOZ_ASSERT(rightBlock == atRightBlockChild.GetContainer());
      if (atRightBlockChild.GetContainerAsElement()) {
        rightBlock = atRightBlockChild.GetContainerAsElement();
      } else {
        if (NS_WARN_IF(!atRightBlockChild.GetContainer()->GetParentElement())) {
          return EditActionIgnored(NS_ERROR_UNEXPECTED);
        }
        rightBlock = atRightBlockChild.GetContainer()->GetParentElement();
      }
    }

    // Do br adjustment.
    RefPtr<Element> brNode =
      CheckForInvisibleBR(*leftBlock, BRLocation::blockEnd);
    EditActionResult ret(NS_OK);
    if (NS_WARN_IF(mergeLists)) {
      // Since 2002, here was the following comment:
      // > The idea here is to take all children in rightList that are past
      // > offset, and pull them into leftlist.
      // However, this has never been performed because we are here only when
      // neither left list nor right list is a descendant of the other but
      // in such case, getting a list item in the right list node almost
      // always failed since a variable for offset of rightList->GetChildAt()
      // was not initialized.  So, it might be a bug, but we should keep this
      // traditional behavior for now.  If you find when we get here, please
      // remove this comment if we don't need to do it.  Otherwise, please
      // move children of the right list node to the end of the left list node.
      MOZ_DIAGNOSTIC_ASSERT(!atChildInBlock.IsSet());

      // XXX Although, we don't do nothing here, but for keeping traditional
      //     behavior, we should mark as handled.
      ret.MarkAsHandled();
    } else {
      // XXX Why do we ignore the result of MoveBlock()?
      EditActionResult retMoveBlock =
        MoveBlock(*leftBlock, *rightBlock,
                  -1, atRightBlockChild.Offset());
      if (retMoveBlock.Handled()) {
        ret.MarkAsHandled();
      }
      // Now, all children of rightBlock were moved to leftBlock.  So,
      // atRightBlockChild is now invalid.
      atRightBlockChild.Clear();
    }
    if (brNode &&
        NS_SUCCEEDED(htmlEditor->DeleteNodeWithTransaction(*brNode))) {
      ret.MarkAsHandled();
    }
    return ret;
  }

  MOZ_DIAGNOSTIC_ASSERT(!atRightBlockChild.IsSet());

  // Offset below is where you find yourself in leftBlock when you traverse
  // upwards from rightBlock
  EditorDOMPoint leftBlockChild;
  if (EditorUtils::IsDescendantOf(*rightBlock, *leftBlock, &leftBlockChild)) {
    // Tricky case.  Right block is inside left block.  Do ws adjustment.  This
    // just destroys non-visible ws at boundaries we will be joining.
    nsresult rv = WSRunObject::ScrubBlockBoundary(htmlEditor,
                                                  WSRunObject::kBlockStart,
                                                  rightBlock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditActionIgnored(rv);
    }

    {
      // We can't just track leftBlock because it's an Element, so track
      // something else.
      AutoTrackDOMPoint tracker(htmlEditor->mRangeUpdater, &leftBlockChild);
      rv = WSRunObject::ScrubBlockBoundary(htmlEditor,
                                           WSRunObject::kBeforeBlock,
                                           leftBlock, leftBlockChild.Offset());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
      // XXX AutoTrackDOMPoint instance, tracker, hasn't been destroyed here.
      //     Do we really need to do update rightBlock here??
      MOZ_DIAGNOSTIC_ASSERT(leftBlock == leftBlockChild.GetContainer());
      if (leftBlockChild.GetContainerAsElement()) {
        leftBlock = leftBlockChild.GetContainerAsElement();
      } else {
        if (NS_WARN_IF(!leftBlockChild.GetContainer()->GetParentElement())) {
          return EditActionIgnored(NS_ERROR_UNEXPECTED);
        }
        leftBlock = leftBlockChild.GetContainer()->GetParentElement();
      }
    }
    // Do br adjustment.
    RefPtr<Element> brNode =
      CheckForInvisibleBR(*leftBlock, BRLocation::beforeBlock,
                          leftBlockChild.Offset());
    EditActionResult ret(NS_OK);
    if (mergeLists) {
      // XXX Why do we ignore the result of MoveContents()?
      int32_t offset = leftBlockChild.Offset();
      EditActionResult retMoveContents =
        MoveContents(*rightList, *leftList, &offset);
      if (retMoveContents.Handled()) {
        ret.MarkAsHandled();
      }
      // leftBlockChild was moved to rightList.  So, it's invalid now.
      leftBlockChild.Clear();
    } else {
      // Left block is a parent of right block, and the parent of the previous
      // visible content.  Right block is a child and contains the contents we
      // want to move.

      EditorDOMPoint previousContent;
      if (&aLeftNode == leftBlock) {
        // We are working with valid HTML, aLeftNode is a block node, and is
        // therefore allowed to contain rightBlock.  This is the simple case,
        // we will simply move the content in rightBlock out of its block.
        previousContent = leftBlockChild;
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
        previousContent.Set(&aLeftNode);

        // We want to move our content just after the previous visible node.
        previousContent.AdvanceOffset();
      }

      // Because we don't want the moving content to receive the style of the
      // previous content, we split the previous content's style.

      nsCOMPtr<Element> editorRoot = htmlEditor->GetEditorRoot();
      if (!editorRoot || &aLeftNode != editorRoot) {
        nsCOMPtr<nsIContent> splittedPreviousContent;
        nsCOMPtr<nsINode> previousContentParent =
          previousContent.GetContainer();
        int32_t previousContentOffset = previousContent.Offset();
        rv = htmlEditor->SplitStyleAbovePoint(
                           address_of(previousContentParent),
                           &previousContentOffset,
                           nullptr, nullptr, nullptr,
                           getter_AddRefs(splittedPreviousContent));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return EditActionIgnored(rv);
        }

        if (splittedPreviousContent) {
          previousContent.Set(splittedPreviousContent);
        } else {
          previousContent.Set(previousContentParent, previousContentOffset);
        }
      }

      if (NS_WARN_IF(!previousContent.IsSet())) {
        return EditActionIgnored(NS_ERROR_NULL_POINTER);
      }

      ret |= MoveBlock(*previousContent.GetContainerAsElement(), *rightBlock,
                       previousContent.Offset(), 0);
      if (NS_WARN_IF(ret.Failed())) {
        return ret;
      }
    }
    if (brNode &&
        NS_SUCCEEDED(htmlEditor->DeleteNodeWithTransaction(*brNode))) {
      ret.MarkAsHandled();
    }
    return ret;
  }

  MOZ_DIAGNOSTIC_ASSERT(!atRightBlockChild.IsSet());
  MOZ_DIAGNOSTIC_ASSERT(!leftBlockChild.IsSet());

  // Normal case.  Blocks are siblings, or at least close enough.  An example
  // of the latter is <p>paragraph</p><ul><li>one<li>two<li>three</ul>.  The
  // first li and the p are not true siblings, but we still want to join them
  // if you backspace from li into p.

  // Adjust whitespace at block boundaries
  nsresult rv =
    WSRunObject::PrepareToJoinBlocks(htmlEditor, leftBlock, rightBlock);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }
  // Do br adjustment.
  nsCOMPtr<Element> brNode =
    CheckForInvisibleBR(*leftBlock, BRLocation::blockEnd);
  EditActionResult ret(NS_OK);
  if (mergeLists || leftBlock->NodeInfo()->NameAtom() ==
                    rightBlock->NodeInfo()->NameAtom()) {
    // Nodes are same type.  merge them.
    EditorDOMPoint pt =
      JoinNearestEditableNodesWithTransaction(*leftBlock, *rightBlock);
    if (pt.IsSet() && mergeLists) {
      RefPtr<Element> newBlock =
        ConvertListType(rightBlock, existingList, nsGkAtoms::li);
    }
    ret.MarkAsHandled();
  } else {
    // Nodes are dissimilar types.
    ret |= MoveBlock(*leftBlock, *rightBlock, -1, 0);
    if (NS_WARN_IF(ret.Failed())) {
      return ret;
    }
  }
  if (brNode) {
    rv = htmlEditor->DeleteNodeWithTransaction(*brNode);
    // XXX In other top level if blocks, the result of
    //     DeleteNodeWithTransaction() is ignored.  Why does only this result
    //     is respected?
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return ret.SetResult(rv);
    }
    ret.MarkAsHandled();
  }
  return ret;
}

EditActionResult
HTMLEditRules::MoveBlock(Element& aLeftBlock,
                         Element& aRightBlock,
                         int32_t aLeftOffset,
                         int32_t aRightOffset)
{
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  // GetNodesFromPoint is the workhorse that figures out what we wnat to move.
  nsresult rv = GetNodesFromPoint(EditorDOMPoint(&aRightBlock, aRightOffset),
                                  EditAction::makeList, arrayOfNodes,
                                  TouchContent::yes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditActionIgnored(rv);
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditActionIgnored(NS_ERROR_NOT_AVAILABLE);
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
  EditActionResult ret(NS_OK);
  for (uint32_t i = 0; i < arrayOfNodes.Length(); i++) {
    // get the node to act on
    if (IsBlockNode(arrayOfNodes[i])) {
      // For block nodes, move their contents only, then delete block.
      ret |=
        MoveContents(*arrayOfNodes[i]->AsElement(), aLeftBlock, &aLeftOffset);
      if (NS_WARN_IF(ret.Failed())) {
        return ret;
      }
      if (NS_WARN_IF(!mHTMLEditor)) {
        return ret.SetResult(NS_ERROR_UNEXPECTED);
      }
      rv = htmlEditor->DeleteNodeWithTransaction(*arrayOfNodes[i]);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
        "Failed to remove a block node");
      ret.MarkAsHandled();
    } else {
      // Otherwise move the content as is, checking against the DTD.
      ret |=
        MoveNodeSmart(*arrayOfNodes[i]->AsContent(), aLeftBlock, &aLeftOffset);
    }
  }

  // XXX We're only checking return value of the last iteration
  if (NS_WARN_IF(ret.Failed())) {
    return ret;
  }

  return ret;
}

EditActionResult
HTMLEditRules::MoveNodeSmart(nsIContent& aNode,
                             Element& aDestElement,
                             int32_t* aInOutDestOffset)
{
  MOZ_ASSERT(aInOutDestOffset);

  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditActionIgnored(NS_ERROR_UNEXPECTED);
  }

  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // Check if this node can go into the destination node
  if (htmlEditor->CanContain(aDestElement, aNode)) {
    // If it can, move it there.
    if (*aInOutDestOffset == -1) {
      nsresult rv =
        htmlEditor->MoveNodeToEndWithTransaction(aNode, aDestElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
    } else {
      EditorRawDOMPoint pointToInsert(&aDestElement, *aInOutDestOffset);
      nsresult rv =
        htmlEditor->MoveNodeWithTransaction(aNode, pointToInsert);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditActionIgnored(rv);
      }
    }
    if (*aInOutDestOffset != -1) {
      (*aInOutDestOffset)++;
    }
    // XXX Should we check if the node is actually moved in this case?
    return EditActionHandled();
  }

  // If it can't, move its children (if any), and then delete it.
  EditActionResult ret(NS_OK);
  if (aNode.IsElement()) {
    ret = MoveContents(*aNode.AsElement(), aDestElement, aInOutDestOffset);
    if (NS_WARN_IF(ret.Failed())) {
      return ret;
    }
  }

  nsresult rv = htmlEditor->DeleteNodeWithTransaction(aNode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ret.SetResult(rv);
  }
  return ret.MarkAsHandled();
}

EditActionResult
HTMLEditRules::MoveContents(Element& aElement,
                            Element& aDestElement,
                            int32_t* aInOutDestOffset)
{
  MOZ_ASSERT(aInOutDestOffset);

  if (NS_WARN_IF(&aElement == &aDestElement)) {
    return EditActionIgnored(NS_ERROR_ILLEGAL_VALUE);
  }

  EditActionResult ret(NS_OK);
  while (aElement.GetFirstChild()) {
    ret |=
      MoveNodeSmart(*aElement.GetFirstChild(), aDestElement, aInOutDestOffset);
    if (NS_WARN_IF(ret.Failed())) {
      return ret;
    }
  }
  return ret;
}


nsresult
HTMLEditRules::DeleteNonTableElements(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  if (!HTMLEditUtils::IsTableElementButNotTable(aNode)) {
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
    nsresult rv = htmlEditor->DeleteNodeWithTransaction(*aNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  AutoTArray<nsCOMPtr<nsIContent>, 10> childList;
  for (nsIContent* child = aNode->GetFirstChild();
       child; child = child->GetNextSibling()) {
    childList.AppendElement(child);
  }

  for (const auto& child: childList) {
    nsresult rv = DeleteNonTableElements(child);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
HTMLEditRules::DidDeleteSelection(Selection* aSelection,
                                  nsIEditor::EDirection aDir,
                                  nsresult aResult)
{
  if (!aSelection) {
    return NS_ERROR_NULL_POINTER;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // find where we are
  EditorDOMPoint atStartOfSelection(EditorBase::GetStartPoint(aSelection));
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // find any enclosing mailcite
  RefPtr<Element> citeNode =
    GetTopEnclosingMailCite(*atStartOfSelection.GetContainer());
  if (citeNode) {
    bool isEmpty = true, seenBR = false;
    htmlEditor->IsEmptyNodeImpl(citeNode, &isEmpty, true, true, false,
                                &seenBR);
    if (isEmpty) {
      EditorDOMPoint atCiteNode(citeNode);
      {
        AutoEditorDOMPointChildInvalidator lockOffset(atCiteNode);
        nsresult rv = htmlEditor->DeleteNodeWithTransaction(*citeNode);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      if (atCiteNode.IsSet() && seenBR) {
        RefPtr<Element> brElement =
          htmlEditor->InsertBrElementWithTransaction(*aSelection, atCiteNode);
        if (NS_WARN_IF(!brElement)) {
          return NS_ERROR_FAILURE;
        }
        IgnoredErrorResult error;
        aSelection->Collapse(EditorRawDOMPoint(brElement), error);
        NS_WARNING_ASSERTION(!error.Failed(),
          "Failed to collapse selection at the new <br> element");
      }
    }
  }

  // call through to base class
  return TextEditRules::DidDeleteSelection(aSelection, aDir, aResult);
}

nsresult
HTMLEditRules::WillMakeList(Selection* aSelection,
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
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  OwningNonNull<nsAtom> listType = NS_Atomize(*aListType);

  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = false;

  // deduce what tag to use for list items
  RefPtr<nsAtom> itemType;
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

  nsresult rv = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoSelectionRestorer selectionRestorer(aSelection, htmlEditor);

  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  rv = GetListActionNodes(arrayOfNodes,
                          aEntireList ? EntireList::yes : EntireList::no);
  NS_ENSURE_SUCCESS(rv, rv);

  // check if all our nodes are <br>s, or empty inlines
  bool bOnlyBreaks = true;
  for (auto& curNode : arrayOfNodes) {
    // if curNode is not a Break or empty inline, we're done
    if (!TextEditUtils::IsBreak(curNode) &&
        !IsEmptyInline(curNode)) {
      bOnlyBreaks = false;
      break;
    }
  }

  // if no nodes, we make empty list.  Ditto if the user tried to make a list
  // of some # of breaks.
  if (arrayOfNodes.IsEmpty() || bOnlyBreaks) {
    // if only breaks, delete them
    if (bOnlyBreaks) {
      for (auto& node : arrayOfNodes) {
        rv = htmlEditor->DeleteNodeWithTransaction(*node);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    nsRange* firstRange = aSelection->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Make sure we can put a list here.
    if (!htmlEditor->CanContainTag(*atStartOfSelection.GetContainer(),
                                   listType)) {
      *aCancel = true;
      return NS_OK;
    }

    SplitNodeResult splitAtSelectionStartResult =
      MaybeSplitAncestorsForInsertWithTransaction(listType, atStartOfSelection);
    if (NS_WARN_IF(splitAtSelectionStartResult.Failed())) {
      return splitAtSelectionStartResult.Rv();
    }
    RefPtr<Element> theList =
      htmlEditor->CreateNodeWithTransaction(
                    *listType,
                    splitAtSelectionStartResult.SplitPoint());
    if (NS_WARN_IF(!theList)) {
      return NS_ERROR_FAILURE;
    }

    EditorRawDOMPoint atFirstListItemToInsertBefore(theList, 0);
    RefPtr<Element> theListItem =
      htmlEditor->CreateNodeWithTransaction(*itemType,
                                            atFirstListItemToInsertBefore);
    if (NS_WARN_IF(!theListItem)) {
      return NS_ERROR_FAILURE;
    }

    // remember our new block for postprocessing
    mNewBlock = theListItem;
    // put selection in new list item
    *aHandled = true;
    ErrorResult error;
    aSelection->Collapse(EditorRawDOMPoint(theListItem, 0), error);
    // Don't restore the selection
    selectionRestorer.Abort();
    if (NS_WARN_IF(!error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // if there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.

  LookInsideDivBQandList(arrayOfNodes);

  // Ok, now go through all the nodes and put then in the list,
  // or whatever is approriate.  Wohoo!

  uint32_t listCount = arrayOfNodes.Length();
  nsCOMPtr<Element> curList, prevListItem;

  for (uint32_t i = 0; i < listCount; i++) {
    // here's where we actually figure out what to do
    RefPtr<Element> newBlock;
    NS_ENSURE_STATE(arrayOfNodes[i]->IsContent());
    OwningNonNull<nsIContent> curNode = *arrayOfNodes[i]->AsContent();

    // make sure we don't assemble content that is in different table cells
    // into the same list.  respect table cell boundaries when listifying.
    if (curList && InDifferentTableElements(curList, curNode)) {
      curList = nullptr;
    }

    // If curNode is a break, delete it, and quit remembering prev list item.
    // If an empty inline container, delete it, but still remember the previous
    // item.
    if (htmlEditor->IsEditable(curNode) && (TextEditUtils::IsBreak(curNode) ||
                                            IsEmptyInline(curNode))) {
      rv = htmlEditor->DeleteNodeWithTransaction(*curNode);
      NS_ENSURE_SUCCESS(rv, rv);
      if (TextEditUtils::IsBreak(curNode)) {
        prevListItem = nullptr;
      }
      continue;
    }

    if (HTMLEditUtils::IsList(curNode)) {
      // do we have a curList already?
      if (curList && !EditorUtils::IsDescendantOf(*curNode, *curList)) {
        // move all of our children into curList.  cheezy way to do it: move
        // whole list and then RemoveContainerWithTransaction() on the list.
        // ConvertListType first: that routine handles converting the list
        // item types, if needed.
        rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode, *curList);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        newBlock = ConvertListType(curNode->AsElement(), listType, itemType);
        if (NS_WARN_IF(!newBlock)) {
          return NS_ERROR_FAILURE;
        }
        rv = htmlEditor->RemoveBlockContainerWithTransaction(*newBlock);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // replace list with new list type
        curList = ConvertListType(curNode->AsElement(), listType, itemType);
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
      }
      prevListItem = nullptr;
      continue;
    }

    EditorRawDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(atCurNode.IsSetAndValid());
    if (HTMLEditUtils::IsListItem(curNode)) {
      if (!atCurNode.IsContainerHTMLElement(listType)) {
        // list item is in wrong type of list. if we don't have a curList,
        // split the old list and make a new list of correct type.
        if (!curList || EditorUtils::IsDescendantOf(*curNode, *curList)) {
          if (NS_WARN_IF(!atCurNode.GetContainerAsContent())) {
            return NS_ERROR_FAILURE;
          }
          ErrorResult error;
          nsCOMPtr<nsIContent> newLeftNode =
            htmlEditor->SplitNodeWithTransaction(atCurNode, error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          newBlock = newLeftNode ? newLeftNode->AsElement() : nullptr;
          EditorRawDOMPoint atParentOfCurNode(atCurNode.GetContainer());
          curList = htmlEditor->CreateNodeWithTransaction(*listType,
                                                          atParentOfCurNode);
          if (NS_WARN_IF(!curList)) {
            return NS_ERROR_FAILURE;
          }
        }
        // move list item to new list
        rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode, *curList);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        // convert list item type if needed
        if (!curNode->IsHTMLElement(itemType)) {
          newBlock =
            htmlEditor->ReplaceContainerWithTransaction(*curNode->AsElement(),
                                                        *itemType);
          if (NS_WARN_IF(!newBlock)) {
            return NS_ERROR_FAILURE;
          }
        }
      } else {
        // item is in right type of list.  But we might still have to move it.
        // and we might need to convert list item types.
        if (!curList) {
          curList = atCurNode.GetContainerAsElement();
        } else if (atCurNode.GetContainer() != curList) {
          // move list item to new list
          rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode, *curList);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
        if (!curNode->IsHTMLElement(itemType)) {
          newBlock =
            htmlEditor->ReplaceContainerWithTransaction(*curNode->AsElement(),
                                                        *itemType);
          if (NS_WARN_IF(!newBlock)) {
            return NS_ERROR_FAILURE;
          }
        }
      }
      nsCOMPtr<Element> curElement = do_QueryInterface(curNode);
      if (NS_WARN_IF(!curElement)) {
        return NS_ERROR_FAILURE;
      }
      if (aBulletType && !aBulletType->IsEmpty()) {
        rv = htmlEditor->SetAttributeWithTransaction(*curElement,
                                                     *nsGkAtoms::type,
                                                     *aBulletType);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        rv = htmlEditor->RemoveAttributeWithTransaction(*curElement,
                                                        *nsGkAtoms::type);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      continue;
    }

    // if we hit a div clear our prevListItem, insert divs contents
    // into our node array, and remove the div
    if (curNode->IsHTMLElement(nsGkAtoms::div)) {
      prevListItem = nullptr;
      int32_t j = i + 1;
      GetInnerContent(*curNode, arrayOfNodes, &j);
      rv = htmlEditor->RemoveContainerWithTransaction(*curNode->AsElement());
      NS_ENSURE_SUCCESS(rv, rv);
      listCount = arrayOfNodes.Length();
      continue;
    }

    // need to make a list to put things in if we haven't already,
    if (!curList) {
      SplitNodeResult splitCurNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(listType, atCurNode);
      if (NS_WARN_IF(splitCurNodeResult.Failed())) {
        return splitCurNodeResult.Rv();
      }
      curList =
        htmlEditor->CreateNodeWithTransaction(*listType,
                                              splitCurNodeResult.SplitPoint());
      if (NS_WARN_IF(!curList)) {
        return NS_ERROR_FAILURE;
      }
      // remember our new block for postprocessing
      mNewBlock = curList;
      // curList is now the correct thing to put curNode in
      prevListItem = nullptr;

      // atCurNode is now referring the right node with mOffset but
      // referring the left node with mRef.  So, invalidate it now.
      atCurNode.Clear();
    }

    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<Element> listItem;
    if (!HTMLEditUtils::IsListItem(curNode)) {
      if (IsInlineNode(curNode) && prevListItem) {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode, *prevListItem);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (curNode->IsHTMLElement(nsGkAtoms::p)) {
          listItem =
            htmlEditor->ReplaceContainerWithTransaction(*curNode->AsElement(),
                                                        *itemType);
          if (NS_WARN_IF(!listItem)) {
            return NS_ERROR_FAILURE;
          }
        } else {
          listItem =
            htmlEditor->InsertContainerWithTransaction(*curNode, *itemType);
          if (NS_WARN_IF(!listItem)) {
            return NS_ERROR_FAILURE;
          }
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
      rv = htmlEditor->MoveNodeToEndWithTransaction(*listItem, *curList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}


nsresult
HTMLEditRules::WillRemoveList(Selection* aSelection,
                              bool aOrdered,
                              bool* aCancel,
                              bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }
  // initialize out param
  *aCancel = false;
  *aHandled = true;

  nsresult rv = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(mHTMLEditor);
  AutoSelectionRestorer selectionRestorer(aSelection, mHTMLEditor);

  nsTArray<RefPtr<nsRange>> arrayOfRanges;
  GetPromotedRanges(*aSelection, arrayOfRanges, EditAction::makeList);

  // use these ranges to contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  rv = GetListActionNodes(arrayOfNodes, EntireList::no);
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove all non-editable nodes.  Leave them be.
  for (int32_t i = arrayOfNodes.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsINode> testNode = arrayOfNodes[i];
    NS_ENSURE_STATE(mHTMLEditor);
    if (!mHTMLEditor->IsEditable(testNode)) {
      arrayOfNodes.RemoveElementAt(i);
    }
  }

  // Only act on lists or list items in the array
  for (auto& curNode : arrayOfNodes) {
    // here's where we actually figure out what to do
    if (HTMLEditUtils::IsListItem(curNode)) {
      // unlist this listitem
      bool bOutOfList;
      do {
        rv = PopListItem(*curNode->AsContent(), &bOutOfList);
        NS_ENSURE_SUCCESS(rv, rv);
      } while (!bOutOfList); // keep popping it out until it's not in a list anymore
    } else if (HTMLEditUtils::IsList(curNode)) {
      // node is a list, move list items out
      rv = RemoveListStructure(*curNode->AsElement());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

nsresult
HTMLEditRules::WillMakeDefListItem(Selection* aSelection,
                                   const nsAString *aItemType,
                                   bool aEntireList,
                                   bool* aCancel,
                                   bool* aHandled)
{
  // for now we let WillMakeList handle this
  NS_NAMED_LITERAL_STRING(listType, "dl");
  return WillMakeList(aSelection, &listType.AsString(), aEntireList, nullptr,
                      aCancel, aHandled, aItemType);
}

nsresult
HTMLEditRules::WillMakeBasicBlock(Selection& aSelection,
                                  const nsAString& aBlockType,
                                  bool* aCancel,
                                  bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);

  OwningNonNull<nsAtom> blockType = NS_Atomize(aBlockType);

  WillInsert(aSelection, aCancel);
  // We want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsresult rv = MakeBasicBlock(aSelection, blockType);
  Unused << NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

nsresult
HTMLEditRules::MakeBasicBlock(Selection& aSelection, nsAtom& blockType)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  nsresult rv = NormalizeSelection(&aSelection);
  NS_ENSURE_SUCCESS(rv, rv);
  AutoSelectionRestorer selectionRestorer(&aSelection, htmlEditor);
  AutoTransactionsConserveSelection dontChangeMySelection(htmlEditor);

  // Contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  rv = GetNodesFromSelection(aSelection, EditAction::makeBasicBlock,
                             arrayOfNodes);
  NS_ENSURE_SUCCESS(rv, rv);

  // If nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    nsRange* firstRange = aSelection.GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint pointToInsertBlock(firstRange->StartRef());
    if (&blockType == nsGkAtoms::normal ||
        &blockType == nsGkAtoms::_empty) {
      // We are removing blocks (going to "body text")
      RefPtr<Element> curBlock =
        htmlEditor->GetBlock(*pointToInsertBlock.GetContainer());
      if (NS_WARN_IF(!curBlock)) {
        return NS_ERROR_FAILURE;
      }
      if (!HTMLEditUtils::IsFormatNode(curBlock)) {
        return NS_OK;
      }

      // If the first editable node after selection is a br, consume it.
      // Otherwise it gets pushed into a following block after the split,
      // which is visually bad.
      nsCOMPtr<nsIContent> brContent =
        htmlEditor->GetNextEditableHTMLNode(pointToInsertBlock);
      if (brContent && brContent->IsHTMLElement(nsGkAtoms::br)) {
        AutoEditorDOMPointChildInvalidator lockOffset(pointToInsertBlock);
        rv = htmlEditor->DeleteNodeWithTransaction(*brContent);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      // Do the splits!
      SplitNodeResult splitNodeResult =
        htmlEditor->SplitNodeDeepWithTransaction(
                      *curBlock, pointToInsertBlock,
                      SplitAtEdges::eDoNotCreateEmptyContainer);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      EditorRawDOMPoint pointToInsertBrNode(splitNodeResult.SplitPoint());
      // Put a <br> element at the split point
      brContent =
        htmlEditor->InsertBrElementWithTransaction(aSelection,
                                                   pointToInsertBrNode);
      if (NS_WARN_IF(!brContent)) {
        return NS_ERROR_FAILURE;
      }
      // Put selection at the split point
      EditorRawDOMPoint atBrNode(brContent);
      ErrorResult error;
      aSelection.Collapse(atBrNode, error);
      // Don't restore the selection
      selectionRestorer.Abort();
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
      return NS_OK;
    }

    // We are making a block.  Consume a br, if needed.
    nsCOMPtr<nsIContent> brNode =
      htmlEditor->GetNextEditableHTMLNodeInBlock(pointToInsertBlock);
    if (brNode && brNode->IsHTMLElement(nsGkAtoms::br)) {
      AutoEditorDOMPointChildInvalidator lockOffset(pointToInsertBlock);
      rv = htmlEditor->DeleteNodeWithTransaction(*brNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // We don't need to act on this node any more
      arrayOfNodes.RemoveElement(brNode);
    }
    // Make sure we can put a block here.
    SplitNodeResult splitNodeResult =
      MaybeSplitAncestorsForInsertWithTransaction(blockType,
                                                  pointToInsertBlock);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }
    RefPtr<Element> block =
      htmlEditor->CreateNodeWithTransaction(blockType,
                                            splitNodeResult.SplitPoint());
    if (NS_WARN_IF(!block)) {
      return NS_ERROR_FAILURE;
    }
    // Remember our new block for postprocessing
    mNewBlock = block;
    // Delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      rv = htmlEditor->DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      arrayOfNodes.RemoveElementAt(0);
    }
    // Put selection in new block
    rv = aSelection.Collapse(block, 0);
    // Don't restore the selection
    selectionRestorer.Abort();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }
  // Okay, now go through all the nodes and make the right kind of blocks, or
  // whatever is approriate.  Woohoo!  Note: blockquote is handled a little
  // differently.
  if (&blockType == nsGkAtoms::blockquote) {
    rv = MakeBlockquote(arrayOfNodes);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (&blockType == nsGkAtoms::normal ||
             &blockType == nsGkAtoms::_empty) {
    rv = RemoveBlockStyle(arrayOfNodes);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = ApplyBlockStyle(arrayOfNodes, blockType);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
HTMLEditRules::DidMakeBasicBlock(Selection* aSelection,
                                 RulesInfo* aInfo,
                                 nsresult aResult)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  // check for empty block.  if so, put a moz br in it.
  if (!aSelection->IsCollapsed()) {
    return NS_OK;
  }

  NS_ENSURE_STATE(aSelection->GetRangeAt(0) &&
                  aSelection->GetRangeAt(0)->GetStartContainer());
  nsresult rv =
    InsertMozBRIfNeeded(*aSelection->GetRangeAt(0)->GetStartContainer());
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
HTMLEditRules::WillIndent(Selection* aSelection,
                          bool* aCancel,
                          bool* aHandled)
{
  NS_ENSURE_STATE(mHTMLEditor);
  if (mHTMLEditor->IsCSSEnabled()) {
    nsresult rv = WillCSSIndent(aSelection, aCancel, aHandled);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    nsresult rv = WillHTMLIndent(aSelection, aCancel, aHandled);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
HTMLEditRules::WillCSSIndent(Selection* aSelection,
                             bool* aCancel,
                             bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsresult rv = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(rv, rv);
  AutoSelectionRestorer selectionRestorer(aSelection, htmlEditor);
  nsTArray<OwningNonNull<nsRange>> arrayOfRanges;
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;

  // short circuit: detect case of collapsed selection inside an <li>.
  // just sublist that <li>.  This prevents bug 97797.

  nsCOMPtr<Element> liNode;
  if (aSelection->IsCollapsed()) {
    EditorRawDOMPoint selectionStartPoint(
                        EditorBase::GetStartPoint(aSelection));
    if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    Element* block =
      htmlEditor->GetBlock(*selectionStartPoint.GetContainer());
    if (block && HTMLEditUtils::IsListItem(block)) {
      liNode = block;
    }
  }

  if (liNode) {
    arrayOfNodes.AppendElement(*liNode);
  } else {
    // convert the selection ranges into "promoted" selection ranges:
    // this basically just expands the range to include the immediate
    // block parent, and then further expands to include any ancestors
    // whose children are all in the range
    rv = GetNodesFromSelection(*aSelection, EditAction::indent, arrayOfNodes);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    // get selection location
    nsRange* firstRange = aSelection->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // make sure we can put a block here
    SplitNodeResult splitNodeResult =
      MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                  atStartOfSelection);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }
    RefPtr<Element> theBlock =
      htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div,
                                            splitNodeResult.SplitPoint());
    if (NS_WARN_IF(!theBlock)) {
      return NS_ERROR_FAILURE;
    }
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    ChangeIndentation(*theBlock, Change::plus);
    // delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      rv = htmlEditor->DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      arrayOfNodes.RemoveElementAt(0);
    }
    // put selection in new block
    *aHandled = true;
    EditorRawDOMPoint atStartOfTheBlock(theBlock, 0);
    ErrorResult error;
    aSelection->Collapse(atStartOfTheBlock, error);
    // Don't restore the selection
    selectionRestorer.Abort();
    if (NS_WARN_IF(!error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // Ok, now go through all the nodes and put them in a blockquote,
  // or whatever is appropriate.  Wohoo!
  nsCOMPtr<Element> curList, curQuote;
  nsCOMPtr<nsIContent> sibling;
  for (OwningNonNull<nsINode>& curNode : arrayOfNodes) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      continue;
    }

    // Ignore all non-editable nodes.  Leave them be.
    if (!htmlEditor->IsEditable(curNode)) {
      continue;
    }

    // some logic for putting list items into nested lists...
    if (HTMLEditUtils::IsList(atCurNode.GetContainer())) {
      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      sibling = htmlEditor->GetNextHTMLSibling(curNode);
      if (sibling && HTMLEditUtils::IsList(sibling) &&
          atCurNode.GetContainer()->NodeInfo()->NameAtom() ==
            sibling->NodeInfo()->NameAtom() &&
          atCurNode.GetContainer()->NodeInfo()->NamespaceID() ==
            sibling->NodeInfo()->NamespaceID()) {
        rv = htmlEditor->MoveNodeWithTransaction(*curNode->AsContent(),
                                                 EditorRawDOMPoint(sibling, 0));
        NS_ENSURE_SUCCESS(rv, rv);
        continue;
      }

      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      sibling = htmlEditor->GetPriorHTMLSibling(curNode);
      if (sibling && HTMLEditUtils::IsList(sibling) &&
          atCurNode.GetContainer()->NodeInfo()->NameAtom() ==
            sibling->NodeInfo()->NameAtom() &&
          atCurNode.GetContainer()->NodeInfo()->NamespaceID() ==
            sibling->NodeInfo()->NamespaceID()) {
        rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                      *sibling);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      sibling = nullptr;
      if (curList) {
        sibling = htmlEditor->GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        nsAtom* containerName =
          atCurNode.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*containerName,
                                                      atCurNode);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        curList =
          htmlEditor->CreateNodeWithTransaction(*containerName,
                                                splitNodeResult.SplitPoint());
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                    *curList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    // Not a list item.

    if (IsBlockNode(*curNode)) {
      ChangeIndentation(*curNode->AsElement(), Change::plus);
      curQuote = nullptr;
      continue;
    }

    if (!curQuote) {
      // First, check that our element can contain a div.
      if (!htmlEditor->CanContainTag(*atCurNode.GetContainer(),
                                     *nsGkAtoms::div)) {
        return NS_OK; // cancelled
      }

      SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div, atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curQuote =
        htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div,
                                              splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!curQuote)) {
        return NS_ERROR_FAILURE;
      }
      ChangeIndentation(*curQuote, Change::plus);
      // remember our new block for postprocessing
      mNewBlock = curQuote;
      // curQuote is now the correct thing to put curNode in
    }

    // tuck the node into the end of the active blockquote
    rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                  *curQuote);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
HTMLEditRules::WillHTMLIndent(Selection* aSelection,
                              bool* aCancel,
                              bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsresult rv = NormalizeSelection(aSelection);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoSelectionRestorer selectionRestorer(aSelection, htmlEditor);

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range

  nsTArray<RefPtr<nsRange>> arrayOfRanges;
  GetPromotedRanges(*aSelection, arrayOfRanges, EditAction::indent);

  // use these ranges to contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  rv = GetNodesForOperation(arrayOfRanges, arrayOfNodes, EditAction::indent);
  NS_ENSURE_SUCCESS(rv, rv);

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    nsRange* firstRange = aSelection->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Make sure we can put a block here.
    SplitNodeResult splitNodeResult =
      MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::blockquote,
                                                  atStartOfSelection);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }
    RefPtr<Element> theBlock =
      htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::blockquote,
                                            splitNodeResult.SplitPoint());
    if (NS_WARN_IF(!theBlock)) {
      return NS_ERROR_FAILURE;
    }
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    // delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      rv = htmlEditor->DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      arrayOfNodes.RemoveElementAt(0);
    }
    // put selection in new block
    *aHandled = true;
    EditorRawDOMPoint atStartOfTheBlock(theBlock, 0);
    ErrorResult error;
    aSelection->Collapse(atStartOfTheBlock, error);
    // Don't restore the selection
    selectionRestorer.Abort();
    if (NS_WARN_IF(!error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // Ok, now go through all the nodes and put them in a blockquote,
  // or whatever is appropriate.  Wohoo!
  nsCOMPtr<nsIContent> sibling;
  nsCOMPtr<Element> curList, curQuote, indentedLI;
  for (OwningNonNull<nsINode>& curNode: arrayOfNodes) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      continue;
    }

    // Ignore all non-editable nodes.  Leave them be.
    if (!htmlEditor->IsEditable(curNode)) {
      continue;
    }

    // some logic for putting list items into nested lists...
    if (HTMLEditUtils::IsList(atCurNode.GetContainer())) {
      // Check for whether we should join a list that follows curNode.
      // We do this if the next element is a list, and the list is of the
      // same type (li/ol) as curNode was a part it.
      sibling = htmlEditor->GetNextHTMLSibling(curNode);
      if (sibling && HTMLEditUtils::IsList(sibling) &&
          atCurNode.GetContainer()->NodeInfo()->NameAtom() ==
            sibling->NodeInfo()->NameAtom() &&
          atCurNode.GetContainer()->NodeInfo()->NamespaceID() ==
            sibling->NodeInfo()->NamespaceID()) {
        rv = htmlEditor->MoveNodeWithTransaction(*curNode->AsContent(),
                                                 EditorRawDOMPoint(sibling, 0));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // Check for whether we should join a list that preceeds curNode.
      // We do this if the previous element is a list, and the list is of
      // the same type (li/ol) as curNode was a part of.
      sibling = htmlEditor->GetPriorHTMLSibling(curNode);
      if (sibling && HTMLEditUtils::IsList(sibling) &&
          atCurNode.GetContainer()->NodeInfo()->NameAtom() ==
            sibling->NodeInfo()->NameAtom() &&
          atCurNode.GetContainer()->NodeInfo()->NamespaceID() ==
            sibling->NodeInfo()->NamespaceID()) {
        rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                      *sibling);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      sibling = nullptr;
      if (curList) {
        sibling = htmlEditor->GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        nsAtom* containerName =
          atCurNode.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*containerName,
                                                      atCurNode);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        curList =
          htmlEditor->CreateNodeWithTransaction(*containerName,
                                                splitNodeResult.SplitPoint());
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                    *curList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // forget curQuote, if any
      curQuote = nullptr;

      continue;
    }

    // Not a list item, use blockquote?

    // if we are inside a list item, we don't want to blockquote, we want
    // to sublist the list item.  We may have several nodes listed in the
    // array of nodes to act on, that are in the same list item.  Since
    // we only want to indent that li once, we must keep track of the most
    // recent indented list item, and not indent it if we find another node
    // to act on that is still inside the same li.
    RefPtr<Element> listItem = IsInListItem(curNode);
    if (listItem) {
      if (indentedLI == listItem) {
        // already indented this list item
        continue;
      }
      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList) {
        sibling = htmlEditor->GetPriorHTMLSibling(listItem);
      }

      if (!curList || (sibling && sibling != curList)) {
        EditorDOMPoint atListItem(listItem);
        if (NS_WARN_IF(!listItem)) {
          return NS_ERROR_FAILURE;
        }
        nsAtom* containerName =
          atListItem.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*containerName,
                                                      atListItem);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        curList =
          htmlEditor->CreateNodeWithTransaction(*containerName,
                                                splitNodeResult.SplitPoint());
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
      }

      rv = htmlEditor->MoveNodeToEndWithTransaction(*listItem, *curList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // remember we indented this li
      indentedLI = listItem;

      continue;
    }

    // need to make a blockquote to put things in if we haven't already,
    // or if this node doesn't go in blockquote we used earlier.
    // One reason it might not go in prio blockquote is if we are now
    // in a different table cell.
    if (curQuote && InDifferentTableElements(curQuote, curNode)) {
      curQuote = nullptr;
    }

    if (!curQuote) {
      // First, check that our element can contain a blockquote.
      if (!htmlEditor->CanContainTag(*atCurNode.GetContainer(),
                                     *nsGkAtoms::blockquote)) {
        return NS_OK; // cancelled
      }

      SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::blockquote,
                                                    atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curQuote =
        htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::blockquote,
                                              splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!curQuote)) {
        return NS_ERROR_FAILURE;
      }
      // remember our new block for postprocessing
      mNewBlock = curQuote;
      // curQuote is now the correct thing to put curNode in
    }

    // tuck the node into the end of the active blockquote
    rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                  *curQuote);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // forget curList, if any
    curList = nullptr;
  }
  return NS_OK;
}


nsresult
HTMLEditRules::WillOutdent(Selection& aSelection,
                           bool* aCancel,
                           bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);
  *aCancel = false;
  *aHandled = true;
  nsCOMPtr<nsIContent> rememberedLeftBQ, rememberedRightBQ;
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
  bool useCSS = htmlEditor->IsCSSEnabled();

  nsresult rv = NormalizeSelection(&aSelection);
  NS_ENSURE_SUCCESS(rv, rv);

  // Some scoping for selection resetting - we may need to tweak it
  {
    AutoSelectionRestorer selectionRestorer(&aSelection, htmlEditor);

    // Convert the selection ranges into "promoted" selection ranges: this
    // basically just expands the range to include the immediate block parent,
    // and then further expands to include any ancestors whose children are all
    // in the range
    nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
    rv = GetNodesFromSelection(aSelection, EditAction::outdent, arrayOfNodes);
    NS_ENSURE_SUCCESS(rv, rv);

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
      int32_t offset;
      nsCOMPtr<nsINode> curParent =
        EditorBase::GetNodeLocation(curNode, &offset);
      if (!curParent) {
        continue;
      }

      // Is it a blockquote?
      if (curNode->IsHTMLElement(nsGkAtoms::blockquote)) {
        // If it is a blockquote, remove it.  So we need to finish up dealng
        // with any curBlockQuote first.
        if (curBlockQuote) {
          rv = OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                                  curBlockQuoteIsIndentedWithCSS,
                                  getter_AddRefs(rememberedLeftBQ),
                                  getter_AddRefs(rememberedRightBQ));
          NS_ENSURE_SUCCESS(rv, rv);
          curBlockQuote = nullptr;
          firstBQChild = nullptr;
          lastBQChild = nullptr;
          curBlockQuoteIsIndentedWithCSS = false;
        }
        rv = htmlEditor->RemoveBlockContainerWithTransaction(
                           *curNode->AsElement());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }
      // Is it a block with a 'margin' property?
      if (useCSS && IsBlockNode(curNode)) {
        nsAtom& marginProperty = MarginPropertyAtomForIndent(curNode);
        nsAutoString value;
        CSSEditUtils::GetSpecifiedProperty(curNode, marginProperty, value);
        float f;
        RefPtr<nsAtom> unit;
        CSSEditUtils::ParseLength(value, &f, getter_AddRefs(unit));
        if (f > 0) {
          ChangeIndentation(*curNode->AsElement(), Change::minus);
          continue;
        }
      }
      // Is it a list item?
      if (HTMLEditUtils::IsListItem(curNode)) {
        // If it is a list item, that means we are not outdenting whole list.
        // So we need to finish up dealing with any curBlockQuote, and then pop
        // this list item.
        if (curBlockQuote) {
          rv = OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                                  curBlockQuoteIsIndentedWithCSS,
                                  getter_AddRefs(rememberedLeftBQ),
                                  getter_AddRefs(rememberedRightBQ));
          NS_ENSURE_SUCCESS(rv, rv);
          curBlockQuote = nullptr;
          firstBQChild = nullptr;
          lastBQChild = nullptr;
          curBlockQuoteIsIndentedWithCSS = false;
        }
        rv = PopListItem(*curNode->AsContent());
        NS_ENSURE_SUCCESS(rv, rv);
        continue;
      }
      // Do we have a blockquote that we are already committed to removing?
      if (curBlockQuote) {
        // If so, is this node a descendant?
        if (EditorUtils::IsDescendantOf(*curNode, *curBlockQuote)) {
          lastBQChild = curNode;
          // Then we don't need to do anything different for this node
          continue;
        }
        // Otherwise, we have progressed beyond end of curBlockQuote, so
        // let's handle it now.  We need to remove the portion of
        // curBlockQuote that contains [firstBQChild - lastBQChild].
        rv = OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                                curBlockQuoteIsIndentedWithCSS,
                                getter_AddRefs(rememberedLeftBQ),
                                getter_AddRefs(rememberedRightBQ));
        NS_ENSURE_SUCCESS(rv, rv);
        curBlockQuote = nullptr;
        firstBQChild = nullptr;
        lastBQChild = nullptr;
        curBlockQuoteIsIndentedWithCSS = false;
        // Fall out and handle curNode
      }

      // Are we inside a blockquote?
      OwningNonNull<nsINode> n = curNode;
      curBlockQuoteIsIndentedWithCSS = false;
      // Keep looking up the hierarchy as long as we don't hit the body or the
      // active editing host or a table element (other than an entire table)
      while (!n->IsHTMLElement(nsGkAtoms::body) && 
             htmlEditor->IsDescendantOfEditorRoot(n) &&
             (n->IsHTMLElement(nsGkAtoms::table) ||
              !HTMLEditUtils::IsTableElement(n))) {
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
          nsAtom& marginProperty = MarginPropertyAtomForIndent(curNode);
          nsAutoString value;
          CSSEditUtils::GetSpecifiedProperty(*n, marginProperty, value);
          float f;
          RefPtr<nsAtom> unit;
          CSSEditUtils::ParseLength(value, &f, getter_AddRefs(unit));
          if (f > 0 && !(HTMLEditUtils::IsList(curParent) &&
                         HTMLEditUtils::IsList(curNode))) {
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
        if (HTMLEditUtils::IsList(curParent)) {
          // Move node out of list
          if (HTMLEditUtils::IsList(curNode)) {
            // Just unwrap this sublist
            rv = htmlEditor->RemoveBlockContainerWithTransaction(
                               *curNode->AsElement());
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
          }
          // handled list item case above
        } else if (HTMLEditUtils::IsList(curNode)) {
          // node is a list, but parent is non-list: move list items out
          nsCOMPtr<nsIContent> child = curNode->GetLastChild();
          while (child) {
            if (HTMLEditUtils::IsListItem(child)) {
              rv = PopListItem(*child);
              NS_ENSURE_SUCCESS(rv, rv);
            } else if (HTMLEditUtils::IsList(child)) {
              // We have an embedded list, so move it out from under the parent
              // list. Be sure to put it after the parent list because this
              // loop iterates backwards through the parent's list of children.
              EditorRawDOMPoint afterCurrentList(curParent, offset + 1);
              rv = htmlEditor->MoveNodeWithTransaction(*child,
                                                       afterCurrentList);
              if (NS_WARN_IF(NS_FAILED(rv))) {
                return rv;
              }
            } else {
              // Delete any non-list items for now
              rv = htmlEditor->DeleteNodeWithTransaction(*child);
              if (NS_WARN_IF(NS_FAILED(rv))) {
                return rv;
              }
            }
            child = curNode->GetLastChild();
          }
          // Delete the now-empty list
          rv = htmlEditor->RemoveBlockContainerWithTransaction(
                             *curNode->AsElement());
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        } else if (useCSS) {
          nsCOMPtr<Element> element;
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
      rv = OutdentPartOfBlock(*curBlockQuote, *firstBQChild, *lastBQChild,
                              curBlockQuoteIsIndentedWithCSS,
                              getter_AddRefs(rememberedLeftBQ),
                              getter_AddRefs(rememberedRightBQ));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  // Make sure selection didn't stick to last piece of content in old bq (only
  // a problem for collapsed selections)
  if (rememberedLeftBQ || rememberedRightBQ) {
    if (aSelection.IsCollapsed()) {
      // Push selection past end of rememberedLeftBQ
      NS_ENSURE_TRUE(aSelection.GetRangeAt(0), NS_OK);
      nsCOMPtr<nsINode> startNode =
        aSelection.GetRangeAt(0)->GetStartContainer();
      if (rememberedLeftBQ &&
          (startNode == rememberedLeftBQ ||
           EditorUtils::IsDescendantOf(*startNode, *rememberedLeftBQ))) {
        // Selection is inside rememberedLeftBQ - push it past it.
        EditorRawDOMPoint afterRememberedLeftBQ(rememberedLeftBQ);
        afterRememberedLeftBQ.AdvanceOffset();
        aSelection.Collapse(afterRememberedLeftBQ);
      }
      // And pull selection before beginning of rememberedRightBQ
      startNode = aSelection.GetRangeAt(0)->GetStartContainer();
      if (rememberedRightBQ &&
          (startNode == rememberedRightBQ ||
           EditorUtils::IsDescendantOf(*startNode, *rememberedRightBQ))) {
        // Selection is inside rememberedRightBQ - push it before it.
        EditorRawDOMPoint atRememberedRightBQ(rememberedRightBQ);
        aSelection.Collapse(atRememberedRightBQ);
      }
    }
    return NS_OK;
  }
  return NS_OK;
}


/**
 * RemovePartOfBlock() splits aBlock and move aStartChild to aEndChild out of
 * aBlock.
 */
nsresult
HTMLEditRules::RemovePartOfBlock(Element& aBlock,
                                 nsIContent& aStartChild,
                                 nsIContent& aEndChild)
{
  SplitBlock(aBlock, aStartChild, aEndChild);
  // Get rid of part of blockquote we are outdenting

  NS_ENSURE_STATE(mHTMLEditor);
  nsresult rv = mHTMLEditor->RemoveBlockContainerWithTransaction(aBlock);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
HTMLEditRules::SplitBlock(Element& aBlock,
                          nsIContent& aStartChild,
                          nsIContent& aEndChild,
                          nsIContent** aOutLeftNode,
                          nsIContent** aOutRightNode,
                          nsIContent** aOutMiddleNode)
{
  // aStartChild and aEndChild must be exclusive descendants of aBlock
  MOZ_ASSERT(EditorUtils::IsDescendantOf(aStartChild, aBlock) &&
             EditorUtils::IsDescendantOf(aEndChild, aBlock));
  NS_ENSURE_TRUE_VOID(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // Split at the start.
  SplitNodeResult splitAtStartResult =
    htmlEditor->SplitNodeDeepWithTransaction(
                  aBlock, EditorRawDOMPoint(&aStartChild),
                  SplitAtEdges::eDoNotCreateEmptyContainer);
  NS_WARNING_ASSERTION(splitAtStartResult.Succeeded(),
    "Failed to split aBlock at start");

  // Split at after the end
  EditorRawDOMPoint atAfterEnd(&aEndChild);
  DebugOnly<bool> advanced = atAfterEnd.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced,
    "Failed to advance offset after the end node");
  SplitNodeResult splitAtEndResult =
    htmlEditor->SplitNodeDeepWithTransaction(
                  aBlock, atAfterEnd,
                  SplitAtEdges::eDoNotCreateEmptyContainer);
  NS_WARNING_ASSERTION(splitAtEndResult.Succeeded(),
    "Failed to split aBlock at after end");

  if (aOutLeftNode) {
    NS_IF_ADDREF(*aOutLeftNode = splitAtStartResult.GetPreviousNode());
  }

  if (aOutRightNode) {
    NS_IF_ADDREF(*aOutRightNode = splitAtEndResult.GetNextNode());
  }

  if (aOutMiddleNode) {
    if (splitAtEndResult.GetPreviousNode()) {
      NS_IF_ADDREF(*aOutMiddleNode = splitAtEndResult.GetPreviousNode());
    } else {
      NS_IF_ADDREF(*aOutMiddleNode = splitAtStartResult.GetNextNode());
    }
  }
}

nsresult
HTMLEditRules::OutdentPartOfBlock(Element& aBlock,
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

  if (NS_WARN_IF(!middleNode) || NS_WARN_IF(!middleNode->IsElement())) {
    return NS_ERROR_FAILURE;
  }

  if (!aIsBlockIndentedWithCSS) {
    NS_ENSURE_STATE(mHTMLEditor);
    nsresult rv =
      mHTMLEditor->RemoveBlockContainerWithTransaction(
                     *middleNode->AsElement());
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (middleNode->IsElement()) {
    // We do nothing if middleNode isn't an element
    nsresult rv = ChangeIndentation(*middleNode->AsElement(), Change::minus);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/**
 * ConvertListType() converts list type and list item type.
 */
already_AddRefed<Element>
HTMLEditRules::ConvertListType(Element* aList,
                               nsAtom* aListType,
                               nsAtom* aItemType)
{
  MOZ_ASSERT(aList);
  MOZ_ASSERT(aListType);
  MOZ_ASSERT(aItemType);

  nsCOMPtr<nsINode> child = aList->GetFirstChild();
  while (child) {
    if (child->IsElement()) {
      dom::Element* element = child->AsElement();
      if (HTMLEditUtils::IsListItem(element) &&
          !element->IsHTMLElement(aItemType)) {
        child =
          mHTMLEditor->ReplaceContainerWithTransaction(*element, *aItemType);
        if (NS_WARN_IF(!child)) {
          return nullptr;
        }
      } else if (HTMLEditUtils::IsList(element) &&
                 !element->IsHTMLElement(aListType)) {
        child = ConvertListType(child->AsElement(), aListType, aItemType);
        if (NS_WARN_IF(!child)) {
          return nullptr;
        }
      }
    }
    child = child->GetNextSibling();
  }

  if (aList->IsHTMLElement(aListType)) {
    RefPtr<dom::Element> list = aList->AsElement();
    return list.forget();
  }

  return mHTMLEditor->ReplaceContainerWithTransaction(*aList, *aListType);
}


/**
 * CreateStyleForInsertText() takes care of clearing and setting appropriate
 * style nodes for text insertion.
 */
nsresult
HTMLEditRules::CreateStyleForInsertText(Selection& aSelection,
                                        nsIDocument& aDoc)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  MOZ_ASSERT(htmlEditor->mTypeInState);

  bool weDidSomething = false;
  NS_ENSURE_STATE(aSelection.GetRangeAt(0));
  nsCOMPtr<nsINode> node = aSelection.GetRangeAt(0)->GetStartContainer();
  int32_t offset = aSelection.GetRangeAt(0)->StartOffset();

  nsCOMPtr<Element> rootElement = aDoc.GetRootElement();
  NS_ENSURE_STATE(rootElement);

  // process clearing any styles first
  UniquePtr<PropItem> item =
    Move(htmlEditor->mTypeInState->TakeClearProperty());

  {
    // Transactions may set selection, but we will set selection if necessary.
    AutoTransactionsConserveSelection dontChangeMySelection(htmlEditor);

    while (item && node != rootElement) {
      // XXX If we redesign ClearStyle(), we can use EditorDOMPoint in this
      //     method.
      nsresult rv =
        htmlEditor->ClearStyle(address_of(node), &offset,
                               item->tag, item->attr);
      NS_ENSURE_SUCCESS(rv, rv);
      item = Move(htmlEditor->mTypeInState->TakeClearProperty());
      weDidSomething = true;
    }
  }

  // then process setting any styles
  int32_t relFontSize = htmlEditor->mTypeInState->TakeRelativeFontSize();
  item = Move(htmlEditor->mTypeInState->TakeSetProperty());

  if (item || relFontSize) {
    // we have at least one style to add; make a new text node to insert style
    // nodes above.
    if (RefPtr<Text> text = node->GetAsText()) {
      // if we are in a text node, split it
      SplitNodeResult splitTextNodeResult =
        htmlEditor->SplitNodeDeepWithTransaction(
                       *text, EditorRawDOMPoint(text, offset),
                       SplitAtEdges::eAllowToCreateEmptyContainer);
      if (NS_WARN_IF(splitTextNodeResult.Failed())) {
        return splitTextNodeResult.Rv();
      }
      EditorRawDOMPoint splitPoint(splitTextNodeResult.SplitPoint());
      node = splitPoint.GetContainer();
      offset = splitPoint.Offset();
    }
    if (!htmlEditor->IsContainer(node)) {
      return NS_OK;
    }
    OwningNonNull<Text> newNode =
      EditorBase::CreateTextNode(aDoc, EmptyString());
    nsresult rv =
      htmlEditor->InsertNodeWithTransaction(*newNode,
                                            EditorRawDOMPoint(node, offset));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    node = newNode;
    offset = 0;
    weDidSomething = true;

    if (relFontSize) {
      // dir indicated bigger versus smaller.  1 = bigger, -1 = smaller
      HTMLEditor::FontSize dir = relFontSize > 0 ?
        HTMLEditor::FontSize::incr : HTMLEditor::FontSize::decr;
      for (int32_t j = 0; j < DeprecatedAbs(relFontSize); j++) {
        rv = htmlEditor->RelativeFontChangeOnTextNode(dir, newNode, 0, -1);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    while (item) {
      rv = htmlEditor->SetInlinePropertyOnNode(*node->AsContent(),
                                               *item->tag, item->attr,
                                               item->value);
      NS_ENSURE_SUCCESS(rv, rv);
      item = htmlEditor->mTypeInState->TakeSetProperty();
    }
  }
  if (weDidSomething) {
    return aSelection.Collapse(node, offset);
  }

  return NS_OK;
}

bool
HTMLEditRules::IsEmptyBlockElement(Element& aElement,
                                   IgnoreSingleBR aIgnoreSingleBR)
{
  if (NS_WARN_IF(!IsBlockNode(aElement))) {
    return false;
  }
  bool isEmpty = true;
  nsresult rv =
    mHTMLEditor->IsEmptyNode(&aElement, &isEmpty,
                             aIgnoreSingleBR == IgnoreSingleBR::eYes);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return isEmpty;
}

nsresult
HTMLEditRules::WillAlign(Selection& aSelection,
                         const nsAString& aAlignType,
                         bool* aCancel,
                         bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  WillInsert(aSelection, aCancel);

  // Initialize out param.  We want to ignore result of WillInsert().
  *aCancel = false;
  *aHandled = false;

  nsresult rv = NormalizeSelection(&aSelection);
  NS_ENSURE_SUCCESS(rv, rv);
  AutoSelectionRestorer selectionRestorer(&aSelection, htmlEditor);

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

    if (HTMLEditUtils::SupportsAlignAttr(*node)) {
      // The node is a table element, an hr, a paragraph, a div or a section
      // header; in HTML 4, it can directly carry the ALIGN attribute and we
      // don't need to make a div! If we are in CSS mode, all the work is done
      // in AlignBlock
      rv = AlignBlock(*node->AsElement(), aAlignType, ContentsOnly::yes);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }

    if (TextEditUtils::IsBreak(node)) {
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
                      aSelection.GetRangeAt(0)->GetStartContainer());
      OwningNonNull<nsINode> parent =
        *aSelection.GetRangeAt(0)->GetStartContainer();

      emptyDiv = !HTMLEditUtils::IsTableElement(parent) ||
                 HTMLEditUtils::IsTableCellOrCaption(parent);
    }
  }
  if (emptyDiv) {
    nsRange* firstRange = aSelection.GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    SplitNodeResult splitNodeResult =
      MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                  atStartOfSelection);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }

    // Consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    nsCOMPtr<nsIContent> brContent =
      htmlEditor->GetNextEditableHTMLNodeInBlock(splitNodeResult.SplitPoint());
    EditorDOMPoint pointToInsertDiv(splitNodeResult.SplitPoint());
    if (brContent && TextEditUtils::IsBreak(brContent)) {
      // Making use of html structure... if next node after where we are
      // putting our div is not a block, then the br we found is in same block
      // we are, so it's safe to consume it.
      nsCOMPtr<nsIContent> sibling;
      if (pointToInsertDiv.GetChild()) {
        sibling = htmlEditor->GetNextHTMLSibling(pointToInsertDiv.GetChild());
      }
      if (sibling && !IsBlockNode(*sibling)) {
        AutoEditorDOMPointChildInvalidator lockOffset(pointToInsertDiv);
        rv = htmlEditor->DeleteNodeWithTransaction(*brContent);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
    RefPtr<Element> div =
      htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div, pointToInsertDiv);
    if (NS_WARN_IF(!div)) {
      return NS_ERROR_FAILURE;
    }
    // Remember our new block for postprocessing
    mNewBlock = div;
    // Set up the alignment on the div, using HTML or CSS
    rv = AlignBlock(*div, aAlignType, ContentsOnly::yes);
    NS_ENSURE_SUCCESS(rv, rv);
    *aHandled = true;
    // Put in a moz-br so that it won't get deleted
    RefPtr<Element> brElement = CreateMozBR(EditorRawDOMPoint(div, 0));
    if (NS_WARN_IF(!brElement)) {
      return NS_ERROR_FAILURE;
    }
    EditorRawDOMPoint atStartOfDiv(div, 0);
    ErrorResult error;
    aSelection.Collapse(atStartOfDiv, error);
    // Don't restore the selection
    selectionRestorer.Abort();
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.

  nsTArray<bool> transitionList;
  MakeTransitionList(nodeArray, transitionList);

  // Okay, now go through all the nodes and give them an align attrib or put
  // them in a div, or whatever is appropriate.  Woohoo!

  nsCOMPtr<Element> curDiv;
  bool useCSS = htmlEditor->IsCSSEnabled();
  int32_t indexOfTransitionList = -1;
  for (OwningNonNull<nsINode>& curNode : nodeArray) {
    ++indexOfTransitionList;

    // Ignore all non-editable nodes.  Leave them be.
    if (!htmlEditor->IsEditable(curNode)) {
      continue;
    }

    // The node is a table element, an hr, a paragraph, a div or a section
    // header; in HTML 4, it can directly carry the ALIGN attribute and we
    // don't need to nest it, just set the alignment.  In CSS, assign the
    // corresponding CSS styles in AlignBlock
    if (HTMLEditUtils::SupportsAlignAttr(*curNode)) {
      rv = AlignBlock(*curNode->AsElement(), aAlignType, ContentsOnly::no);
      NS_ENSURE_SUCCESS(rv, rv);
      // Clear out curDiv so that we don't put nodes after this one into it
      curDiv = nullptr;
      continue;
    }

    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      continue;
    }

    // Skip insignificant formatting text nodes to prevent unnecessary
    // structure splitting!
    bool isEmptyTextNode = false;
    if (curNode->GetAsText() &&
        ((HTMLEditUtils::IsTableElement(atCurNode.GetContainer()) &&
          !HTMLEditUtils::IsTableCellOrCaption(*atCurNode.GetContainer())) ||
         HTMLEditUtils::IsList(atCurNode.GetContainer()) ||
         (NS_SUCCEEDED(htmlEditor->IsEmptyNode(curNode, &isEmptyTextNode)) &&
          isEmptyTextNode))) {
      continue;
    }

    // If it's a list item, or a list inside a list, forget any "current" div,
    // and instead put divs inside the appropriate block (td, li, etc.)
    if (HTMLEditUtils::IsListItem(curNode) ||
        HTMLEditUtils::IsList(curNode)) {
      AutoEditorDOMPointOffsetInvalidator lockChild(atCurNode);
      rv = RemoveAlignment(*curNode, aAlignType, true);
      NS_ENSURE_SUCCESS(rv, rv);
      if (useCSS) {
        htmlEditor->mCSSEditUtils->SetCSSEquivalentToHTMLStyle(
            curNode->AsElement(), nullptr,
            nsGkAtoms::align, &aAlignType, false);
        curDiv = nullptr;
        continue;
      }
      if (HTMLEditUtils::IsList(atCurNode.GetContainer())) {
        // If we don't use CSS, add a contraint to list element: they have to
        // be inside another list, i.e., >= second level of nesting
        rv = AlignInnerBlocks(*curNode, aAlignType);
        NS_ENSURE_SUCCESS(rv, rv);
        curDiv = nullptr;
        continue;
      }
      // Clear out curDiv so that we don't put nodes after this one into it
    }

    // Need to make a div to put things in if we haven't already, or if this
    // node doesn't go in div we used earlier.
    if (!curDiv || transitionList[indexOfTransitionList]) {
      // First, check that our element can contain a div.
      if (!htmlEditor->CanContainTag(*atCurNode.GetContainer(),
                                     *nsGkAtoms::div)) {
        // Cancelled
        return NS_OK;
      }

      SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div, atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curDiv =
        htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div,
                                              splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!curDiv)) {
        return NS_ERROR_FAILURE;
      }
      // Remember our new block for postprocessing
      mNewBlock = curDiv;
      // Set up the alignment on the div
      rv = AlignBlock(*curDiv, aAlignType, ContentsOnly::yes);
    }

    // Tuck the node into the end of the active div
    rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                  *curDiv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}


/**
 * AlignInnerBlocks() aligns inside table cells or list items.
 */
nsresult
HTMLEditRules::AlignInnerBlocks(nsINode& aNode,
                                const nsAString& aAlignType)
{
  // Gather list of table cells or list items
  nsTArray<OwningNonNull<nsINode>> nodeArray;
  TableCellAndListItemFunctor functor;
  DOMIterator iter(aNode);
  iter.AppendList(functor, nodeArray);

  // Now that we have the list, align their contents as requested
  for (auto& node : nodeArray) {
    nsresult rv = AlignBlockContents(*node, aAlignType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}


/**
 * AlignBlockContents() aligns contents of a block element.
 */
nsresult
HTMLEditRules::AlignBlockContents(nsINode& aNode,
                                  const nsAString& aAlignType)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  nsCOMPtr<nsIContent> firstChild = htmlEditor->GetFirstEditableChild(aNode);
  if (!firstChild) {
    // this cell has no content, nothing to align
    return NS_OK;
  }

  nsCOMPtr<nsIContent> lastChild = htmlEditor->GetLastEditableChild(aNode);
  if (firstChild == lastChild && firstChild->IsHTMLElement(nsGkAtoms::div)) {
    // the cell already has a div containing all of its content: just
    // act on this div.
    return htmlEditor->SetAttributeOrEquivalent(firstChild->AsElement(),
                                                nsGkAtoms::align,
                                                aAlignType, false);
  }

  // else we need to put in a div, set the alignment, and toss in all the
  // children
  EditorRawDOMPoint atStartOfNode(&aNode, 0);
  RefPtr<Element> divElem =
    htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div, atStartOfNode);
  if (NS_WARN_IF(!divElem)) {
    return NS_ERROR_FAILURE;
  }
  // set up the alignment on the div
  nsresult rv =
    htmlEditor->SetAttributeOrEquivalent(divElem, nsGkAtoms::align,
                                         aAlignType, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // tuck the children into the end of the active div
  while (lastChild && (lastChild != divElem)) {
    nsresult rv =
      htmlEditor->MoveNodeWithTransaction(*lastChild,
                                          EditorRawDOMPoint(divElem, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    lastChild = htmlEditor->GetLastEditableChild(aNode);
  }
  return NS_OK;
}

/**
 * CheckForEmptyBlock() is called by WillDeleteSelection() to detect and handle
 * case of deleting from inside an empty block.
 */
nsresult
HTMLEditRules::CheckForEmptyBlock(nsINode* aStartNode,
                                  Element* aBodyNode,
                                  Selection* aSelection,
                                  nsIEditor::EDirection aAction,
                                  bool* aHandled)
{
  // If the editing host is an inline element, bail out early.
  if (aBodyNode && IsInlineNode(*aBodyNode)) {
    return NS_OK;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // If we are inside an empty block, delete it.  Note: do NOT delete table
  // elements this way.
  RefPtr<Element> block = htmlEditor->GetBlock(*aStartNode);
  bool bIsEmptyNode;
  RefPtr<Element> emptyBlock;
  if (block && block != aBodyNode) {
    // Efficiency hack, avoiding IsEmptyNode() call when in body
    nsresult rv = htmlEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
    NS_ENSURE_SUCCESS(rv, rv);
    while (block && bIsEmptyNode && !HTMLEditUtils::IsTableElement(block) &&
           block != aBodyNode) {
      emptyBlock = block;
      block = htmlEditor->GetBlockNodeParent(emptyBlock);
      if (block) {
        rv = htmlEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  if (emptyBlock && emptyBlock->IsEditable()) {
    nsCOMPtr<nsINode> blockParent = emptyBlock->GetParentNode();
    NS_ENSURE_TRUE(blockParent, NS_ERROR_FAILURE);

    if (HTMLEditUtils::IsListItem(emptyBlock)) {
      // Are we the first list item in the list?
      if (htmlEditor->IsFirstEditableChild(emptyBlock)) {
        EditorDOMPoint atBlockParent(blockParent);
        if (NS_WARN_IF(!atBlockParent.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        // If we are a sublist, skip the br creation
        if (!HTMLEditUtils::IsList(atBlockParent.GetContainer())) {
          // Create a br before list
          RefPtr<Element> brElement =
            htmlEditor->InsertBrElementWithTransaction(*aSelection,
                                                       atBlockParent);
          if (NS_WARN_IF(!brElement)) {
            return NS_ERROR_FAILURE;
          }
          // Adjust selection to be right before it
          ErrorResult error;
          aSelection->Collapse(EditorRawDOMPoint(brElement), error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
        }
        // Else just let selection percolate up.  We'll adjust it in
        // AfterEdit()
      }
    } else {
      if (aAction == nsIEditor::eNext || aAction == nsIEditor::eNextWord ||
          aAction == nsIEditor::eToEndOfLine) {
        // Move to the start of the next node, if any
        EditorRawDOMPoint afterEmptyBlock(emptyBlock);
        DebugOnly<bool> advanced = afterEmptyBlock.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
          "Failed to set selection to the after the empty block");
        nsCOMPtr<nsIContent> nextNode =
          htmlEditor->GetNextNode(afterEmptyBlock);
        if (nextNode) {
          EditorDOMPoint pt = GetGoodSelPointForNode(*nextNode, aAction);
          ErrorResult error;
          aSelection->Collapse(pt, error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
        } else {
          // Adjust selection to be right after it.
          EditorRawDOMPoint afterEmptyBlock(emptyBlock);
          if (NS_WARN_IF(!afterEmptyBlock.AdvanceOffset())) {
            return NS_ERROR_FAILURE;
          }
          ErrorResult error;
          aSelection->Collapse(afterEmptyBlock, error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
        }
      } else if (aAction == nsIEditor::ePrevious ||
                 aAction == nsIEditor::ePreviousWord ||
                 aAction == nsIEditor::eToBeginningOfLine) {
        // Move to the end of the previous node
        EditorRawDOMPoint atEmptyBlock(emptyBlock);
        nsCOMPtr<nsIContent> priorNode =
          htmlEditor->GetPreviousEditableNode(atEmptyBlock);
        if (priorNode) {
          EditorDOMPoint pt = GetGoodSelPointForNode(*priorNode, aAction);
          nsresult rv = aSelection->Collapse(pt);
          NS_ENSURE_SUCCESS(rv, rv);
        } else {
          EditorRawDOMPoint afterEmptyBlock(emptyBlock);
          if (NS_WARN_IF(!afterEmptyBlock.AdvanceOffset())) {
            return NS_ERROR_FAILURE;
          }
          nsresult rv = aSelection->Collapse(afterEmptyBlock);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      } else if (aAction != nsIEditor::eNone) {
        MOZ_CRASH("CheckForEmptyBlock doesn't support this action yet");
      }
    }
    *aHandled = true;
    nsresult rv = htmlEditor->DeleteNodeWithTransaction(*emptyBlock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

Element*
HTMLEditRules::CheckForInvisibleBR(Element& aBlock,
                                   BRLocation aWhere,
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
    // Since rightmostNode is always the last child, its index is equal to the
    // child count, so instead of ComputeIndexOf() we use the faster
    // GetChildCount(), and assert the equivalence below.
    testOffset = testNode->GetChildCount();

    // Use offset + 1, so last node is included in our evaluation
    MOZ_ASSERT(testNode->ComputeIndexOf(rightmostNode) + 1 == testOffset);
  } else if (aOffset) {
    testNode = &aBlock;
    // We'll check everything to the left of the input position
    testOffset = aOffset;
  } else {
    return nullptr;
  }

  WSRunObject wsTester(mHTMLEditor, testNode, testOffset);
  if (WSType::br == wsTester.mStartReason) {
    return wsTester.mStartReasonNode->AsElement();
  }

  return nullptr;
}

/**
 * aLists and aTables allow the caller to specify what kind of content to
 * "look inside".  If aTables is Tables::yes, look inside any table content,
 * and insert the inner content into the supplied nsTArray at offset
 * aIndex.  Similarly with aLists and list content.  aIndex is updated to
 * point past inserted elements.
 */
void
HTMLEditRules::GetInnerContent(
                 nsINode& aNode,
                 nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                 int32_t* aIndex,
                 Lists aLists,
                 Tables aTables)
{
  MOZ_ASSERT(aIndex);

  for (nsCOMPtr<nsIContent> node = mHTMLEditor->GetFirstEditableChild(aNode);
       node; node = node->GetNextSibling()) {
    if ((aLists == Lists::yes && (HTMLEditUtils::IsList(node) ||
                                  HTMLEditUtils::IsListItem(node))) ||
        (aTables == Tables::yes && HTMLEditUtils::IsTableElement(node))) {
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
HTMLEditRules::ExpandSelectionForDeletion(Selection& aSelection)
{
  // Don't need to touch collapsed selections
  if (aSelection.IsCollapsed()) {
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

  nsCOMPtr<nsINode> selStartNode = range->GetStartContainer();
  int32_t selStartOffset = range->StartOffset();
  nsCOMPtr<nsINode> selEndNode = range->GetEndContainer();
  int32_t selEndOffset = range->EndOffset();

  // Find current selection common block parent
  nsCOMPtr<Element> selCommon =
    HTMLEditor::GetBlock(*range->GetCommonAncestor());
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
      WSRunObject wsObj(mHTMLEditor, selStartNode, selStartOffset);
      wsObj.PriorVisibleNode(EditorRawDOMPoint(selStartNode, selStartOffset),
                             address_of(unused), &visOffset, &wsType);
      if (wsType != WSType::thisBlock) {
        break;
      }
      // We want to keep looking up.  But stop if we are crossing table
      // element boundaries, or if we hit the root.
      if (HTMLEditUtils::IsTableElement(wsObj.mStartReasonNode) ||
          selCommon == wsObj.mStartReasonNode ||
          root == wsObj.mStartReasonNode) {
        break;
      }
      selStartNode = wsObj.mStartReasonNode->GetParentNode();
      selStartOffset = selStartNode ?
        selStartNode->ComputeIndexOf(wsObj.mStartReasonNode) : -1;
    }
  }

  // Find next visible thingy after end of selection
  if (selEndNode != selCommon && selEndNode != root) {
    for (;;) {
      WSRunObject wsObj(mHTMLEditor, selEndNode, selEndOffset);
      wsObj.NextVisibleNode(EditorRawDOMPoint(selEndNode, selEndOffset),
                            address_of(unused), &visOffset, &wsType);
      if (wsType == WSType::br) {
        if (mHTMLEditor->IsVisibleBRElement(wsObj.mEndReasonNode)) {
          break;
        }
        if (!firstBRParent) {
          firstBRParent = selEndNode;
          firstBROffset = selEndOffset;
        }
        selEndNode = wsObj.mEndReasonNode->GetParentNode();
        selEndOffset = selEndNode
          ? selEndNode->ComputeIndexOf(wsObj.mEndReasonNode) + 1 : 0;
      } else if (wsType == WSType::thisBlock) {
        // We want to keep looking up.  But stop if we are crossing table
        // element boundaries, or if we hit the root.
        if (HTMLEditUtils::IsTableElement(wsObj.mEndReasonNode) ||
            selCommon == wsObj.mEndReasonNode ||
            root == wsObj.mEndReasonNode) {
          break;
        }
        selEndNode = wsObj.mEndReasonNode->GetParentNode();
        selEndOffset = 1 + selEndNode->ComputeIndexOf(wsObj.mEndReasonNode);
      } else {
        break;
      }
    }
  }
  // Now set the selection to the new range
  aSelection.Collapse(selStartNode, selStartOffset);

  // Expand selection endpoint only if we didn't pass a br, or if we really
  // needed to pass that br (i.e., its block is now totally selected)
  bool doEndExpansion = true;
  if (firstBRParent) {
    // Find block node containing br
    nsCOMPtr<Element> brBlock = HTMLEditor::GetBlock(*firstBRParent);
    bool nodeBefore = false, nodeAfter = false;

    // Create a range that represents expanded selection
    RefPtr<nsRange> range = new nsRange(selStartNode);
    nsresult rv = range->SetStartAndEnd(selStartNode, selStartOffset,
                                        selEndNode, selEndOffset);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

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
    nsresult rv = aSelection.Extend(selEndNode, selEndOffset);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Only expand to just before br
    nsresult rv = aSelection.Extend(firstBRParent, firstBROffset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/**
 * NormalizeSelection() tweaks non-collapsed selections to be more "natural".
 * Idea here is to adjust selection endpoint so that they do not cross breaks
 * or block boundaries unless something editable beyond that boundary is also
 * selected.  This adjustment makes it much easier for the various block
 * operations to determine what nodes to act on.
 */
nsresult
HTMLEditRules::NormalizeSelection(Selection* inSelection)
{
  NS_ENSURE_TRUE(inSelection, NS_ERROR_NULL_POINTER);

  // don't need to touch collapsed selections
  if (inSelection->IsCollapsed()) {
    return NS_OK;
  }

  // we don't need to mess with cell selections, and we assume multirange selections are those.
  if (inSelection->RangeCount() != 1) {
    return NS_OK;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_UNEXPECTED;
  }
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditor;

  RefPtr<nsRange> range = inSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> startNode = range->GetStartContainer();
  if (NS_WARN_IF(!startNode)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsINode> endNode = range->GetEndContainer();
  if (NS_WARN_IF(!endNode)) {
    return NS_ERROR_FAILURE;
  }
  nsIContent* startChild = range->GetChildAtStartOffset();
  nsIContent* endChild = range->GetChildAtEndOffset();
  uint32_t startOffset = range->StartOffset();
  uint32_t endOffset = range->EndOffset();

  // adjusted values default to original values
  nsCOMPtr<nsINode> newStartNode = startNode;
  uint32_t newStartOffset = startOffset;
  nsCOMPtr<nsINode> newEndNode = endNode;
  uint32_t newEndOffset = endOffset;

  // some locals we need for whitespace code
  nsCOMPtr<nsINode> unused;
  int32_t offset = -1;
  WSType wsType;

  // let the whitespace code do the heavy lifting
  WSRunObject wsEndObj(htmlEditor, endNode, static_cast<int32_t>(endOffset));
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  wsEndObj.PriorVisibleNode(EditorRawDOMPoint(endNode, endOffset),
                            address_of(unused), &offset, &wsType);
  if (wsType != WSType::text && wsType != WSType::normalWS) {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsEndObj.mStartReason == WSType::otherBlock) {
      // endpoint is just after the close of a block.
      nsINode* child =
        htmlEditor->GetRightmostChild(wsEndObj.mStartReasonNode, true);
      if (child) {
        int32_t offset = -1;
        newEndNode = EditorBase::GetNodeLocation(child, &offset);
        // offset *after* child
        newEndOffset = static_cast<uint32_t>(offset + 1);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsEndObj.mStartReason == WSType::thisBlock) {
      // endpoint is just after start of this block
      EditorRawDOMPoint atEnd(endNode, endChild, endOffset);
      nsINode* child = htmlEditor->GetPreviousEditableHTMLNode(atEnd);
      if (child) {
        int32_t offset = -1;
        newEndNode = EditorBase::GetNodeLocation(child, &offset);
        // offset *after* child
        newEndOffset = static_cast<uint32_t>(offset + 1);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsEndObj.mStartReason == WSType::br) {
      // endpoint is just after break.  lets adjust it to before it.
      int32_t offset = -1;
      newEndNode =
        EditorBase::GetNodeLocation(wsEndObj.mStartReasonNode, &offset);
      newEndOffset = static_cast<uint32_t>(offset);;
    }
  }


  // similar dealio for start of range
  WSRunObject wsStartObj(htmlEditor, startNode,
                         static_cast<int32_t>(startOffset));
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  wsStartObj.NextVisibleNode(EditorRawDOMPoint(startNode, startOffset),
                             address_of(unused), &offset, &wsType);
  if (wsType != WSType::text && wsType != WSType::normalWS) {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsStartObj.mEndReason == WSType::otherBlock) {
      // startpoint is just before the start of a block.
      nsINode* child =
        htmlEditor->GetLeftmostChild(wsStartObj.mEndReasonNode, true);
      if (child) {
        int32_t offset = -1;
        newStartNode = EditorBase::GetNodeLocation(child, &offset);
        newStartOffset = static_cast<uint32_t>(offset);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsStartObj.mEndReason == WSType::thisBlock) {
      // startpoint is just before end of this block
      nsINode* child =
        htmlEditor->GetNextEditableHTMLNode(
                      EditorRawDOMPoint(startNode, startChild, startOffset));
      if (child) {
        int32_t offset = -1;
        newStartNode = EditorBase::GetNodeLocation(child, &offset);
        newStartOffset = static_cast<uint32_t>(offset);
      }
      // else block is empty - we can leave selection alone here, i think.
    } else if (wsStartObj.mEndReason == WSType::br) {
      // startpoint is just before a break.  lets adjust it to after it.
      int32_t offset = -1;
      newStartNode =
        EditorBase::GetNodeLocation(wsStartObj.mEndReasonNode, &offset);
      // offset *after* break
      newStartOffset = static_cast<uint32_t>(offset + 1);
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
  if (comp == 1) {
    return NS_OK;  // New end before old start.
  }
  comp = nsContentUtils::ComparePoints(newStartNode, newStartOffset,
                                       endNode, endOffset);
  if (comp == 1) {
    return NS_OK;  // New start after old end.
  }

  // otherwise set selection to new values.
  inSelection->Collapse(newStartNode, newStartOffset);
  inSelection->Extend(newEndNode, newEndOffset);
  return NS_OK;
}

/**
 * GetPromotedPoint() figures out where a start or end point for a block
 * operation really is.
 */
EditorDOMPoint
HTMLEditRules::GetPromotedPoint(RulesEndpoint aWhere,
                                nsINode& aNode,
                                int32_t aOffset,
                                EditAction actionID)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditorDOMPoint(&aNode, aOffset);
  }
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditor;

  // we do one thing for text actions, something else entirely for other
  // actions
  if (actionID == EditAction::insertText ||
      actionID == EditAction::insertIMEText ||
      actionID == EditAction::insertBreak ||
      actionID == EditAction::deleteText) {
    bool isSpace, isNBSP;
    nsCOMPtr<nsIContent> content =
      aNode.IsContent() ? aNode.AsContent() : nullptr;
    nsCOMPtr<nsIContent> temp;
    int32_t newOffset = aOffset;
    // for text actions, we want to look backwards (or forwards, as
    // appropriate) for additional whitespace or nbsp's.  We may have to act on
    // these later even though they are outside of the initial selection.  Even
    // if they are in another node!
    while (content) {
      int32_t offset;
      if (aWhere == kStart) {
        htmlEditor->IsPrevCharInNodeWhitespace(content, newOffset,
                                               &isSpace, &isNBSP,
                                               getter_AddRefs(temp), &offset);
      } else {
        htmlEditor->IsNextCharInNodeWhitespace(content, newOffset,
                                               &isSpace, &isNBSP,
                                               getter_AddRefs(temp), &offset);
      }
      if (isSpace || isNBSP) {
        content = temp;
        newOffset = offset;
      } else {
        break;
      }
    }

    return EditorDOMPoint(content, newOffset);
  }

  EditorDOMPoint point(&aNode, aOffset);

  // else not a text section.  In this case we want to see if we should grab
  // any adjacent inline nodes and/or parents and other ancestors
  if (aWhere == kStart) {
    // some special casing for text nodes
    if (point.IsInTextNode()) {
      if (!point.GetContainer()->GetParentNode()) {
        // Okay, can't promote any further
        return point;
      }
      point.Set(point.GetContainer());
    }

    // look back through any further inline nodes that aren't across a <br>
    // from us, and that are enclosed in the same block.
    nsCOMPtr<nsINode> priorNode =
      htmlEditor->GetPreviousEditableHTMLNodeInBlock(point);

    while (priorNode && priorNode->GetParentNode() &&
           !htmlEditor->IsVisibleBRElement(priorNode) &&
           !IsBlockNode(*priorNode)) {
      point.Set(priorNode);
      priorNode = htmlEditor->GetPreviousEditableHTMLNodeInBlock(point);
    }

    // finding the real start for this point.  look up the tree for as long as
    // we are the first node in the container, and as long as we haven't hit
    // the body node.
    nsCOMPtr<nsIContent> nearNode =
      htmlEditor->GetPreviousEditableHTMLNodeInBlock(point);
    while (!nearNode &&
           !point.IsContainerHTMLElement(nsGkAtoms::body) &&
           point.GetContainer()->GetParentNode()) {
      // some cutoffs are here: we don't need to also include them in the
      // aWhere == kEnd case.  as long as they are in one or the other it will
      // work.  special case for outdent: don't keep looking up if we have
      // found a blockquote element to act on
      if (actionID == EditAction::outdent &&
          point.IsContainerHTMLElement(nsGkAtoms::blockquote)) {
        break;
      }

      // Don't walk past the editable section. Note that we need to check
      // before walking up to a parent because we need to return the parent
      // object, so the parent itself might not be in the editable area, but
      // it's OK if we're not performing a block-level action.
      bool blockLevelAction = actionID == EditAction::indent ||
                              actionID == EditAction::outdent ||
                              actionID == EditAction::align ||
                              actionID == EditAction::makeBasicBlock;
      if (!htmlEditor->IsDescendantOfEditorRoot(
                         point.GetContainer()->GetParentNode()) &&
          (blockLevelAction ||
           !htmlEditor->IsDescendantOfEditorRoot(point.GetContainer()))) {
        break;
      }

      point.Set(point.GetContainer());
      nearNode = htmlEditor->GetPreviousEditableHTMLNodeInBlock(point);
    }
    return point;
  }

  // aWhere == kEnd
  // some special casing for text nodes
  if (point.IsInTextNode()) {
    if (!point.GetContainer()->GetParentNode()) {
      // Okay, can't promote any further
      return point;
    }
    // want to be after the text node
    point.Set(point.GetContainer());
    DebugOnly<bool> advanced = point.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
      "Failed to advance offset to after the text node");
  }

  // look ahead through any further inline nodes that aren't across a <br> from
  // us, and that are enclosed in the same block.
  // XXX Currently, we stop block-extending when finding visible <br> element.
  //     This might be different from "block-extend" of execCommand spec.
  //     However, the spec is really unclear.
  // XXX Probably, scanning only editable nodes is wrong for
  //     EditAction::makeBasicBlock because it might be better to wrap existing
  //     inline elements even if it's non-editable.  For example, following
  //     examples with insertParagraph causes different result:
  //     * <div contenteditable>foo[]<b contenteditable="false">bar</b></div>
  //     * <div contenteditable>foo[]<b>bar</b></div>
  //     * <div contenteditable>foo[]<b contenteditable="false">bar</b>baz</div>
  //     Only in the first case, after the caret position isn't wrapped with
  //     new <div> element.
  nsCOMPtr<nsIContent> nextNode =
    htmlEditor->GetNextEditableHTMLNodeInBlock(point);

  while (nextNode && !IsBlockNode(*nextNode) && nextNode->GetParentNode()) {
    point.Set(nextNode);
    if (NS_WARN_IF(!point.AdvanceOffset())) {
      break;
    }
    if (htmlEditor->IsVisibleBRElement(nextNode)) {
      break;
    }

    // Check for newlines in pre-formatted text nodes.
    if (EditorBase::IsPreformatted(nextNode) &&
        EditorBase::IsTextNode(nextNode)) {
      nsAutoString tempString;
      nextNode->GetAsText()->GetData(tempString);
      int32_t newlinePos = tempString.FindChar(nsCRT::LF);
      if (newlinePos >= 0) {
        if (static_cast<uint32_t>(newlinePos) + 1 == tempString.Length()) {
          // No need for special processing if the newline is at the end.
          break;
        }
        return EditorDOMPoint(nextNode, newlinePos + 1);
      }
    }
    nextNode = htmlEditor->GetNextEditableHTMLNodeInBlock(point);
  }

  // finding the real end for this point.  look up the tree for as long as we
  // are the last node in the container, and as long as we haven't hit the body
  // node.
  nsCOMPtr<nsIContent> nearNode =
    htmlEditor->GetNextEditableHTMLNodeInBlock(point);
  while (!nearNode &&
         !point.IsContainerHTMLElement(nsGkAtoms::body) &&
         point.GetContainer()->GetParentNode()) {
    // Don't walk past the editable section. Note that we need to check before
    // walking up to a parent because we need to return the parent object, so
    // the parent itself might not be in the editable area, but it's OK.
    if (!htmlEditor->IsDescendantOfEditorRoot(point.GetContainer()) &&
        !htmlEditor->IsDescendantOfEditorRoot(
                       point.GetContainer()->GetParentNode())) {
      break;
    }

    point.Set(point.GetContainer());
    if (NS_WARN_IF(!point.AdvanceOffset())) {
      break;
    }
    nearNode = htmlEditor->GetNextEditableHTMLNodeInBlock(point);
  }
  return point;
}

/**
 * GetPromotedRanges() runs all the selection range endpoint through
 * GetPromotedPoint().
 */
void
HTMLEditRules::GetPromotedRanges(Selection& aSelection,
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

/**
 * PromoteRange() expands a range to include any parents for which all editable
 * children are already in range.
 */
void
HTMLEditRules::PromoteRange(nsRange& aRange,
                            EditAction aOperationType)
{
  NS_ENSURE_TRUE(mHTMLEditor, );
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  if (!aRange.IsPositioned()) {
    return;
  }

  nsCOMPtr<nsINode> startNode = aRange.GetStartContainer();
  nsCOMPtr<nsINode> endNode = aRange.GetEndContainer();
  int32_t startOffset = aRange.StartOffset();
  int32_t endOffset = aRange.EndOffset();

  // MOOSE major hack:
  // GetPromotedPoint doesn't really do the right thing for collapsed ranges
  // inside block elements that contain nothing but a solo <br>.  It's easier
  // to put a workaround here than to revamp GetPromotedPoint.  :-(
  if (startNode == endNode && startOffset == endOffset) {
    nsCOMPtr<Element> block = htmlEditor->GetBlock(*startNode);
    if (block) {
      bool bIsEmptyNode = false;
      nsCOMPtr<nsIContent> root = htmlEditor->GetActiveEditingHost();
      // Make sure we don't go higher than our root element in the content tree
      NS_ENSURE_TRUE(root, );
      if (!nsContentUtils::ContentIsDescendantOf(root, block)) {
        htmlEditor->IsEmptyNode(block, &bIsEmptyNode, true, false);
      }
      if (bIsEmptyNode) {
        startNode = block;
        endNode = block;
        startOffset = 0;
        endOffset = block->Length();
      }
    }
  }

  if (aOperationType == EditAction::insertText ||
      aOperationType == EditAction::insertIMEText ||
      aOperationType == EditAction::insertBreak ||
      aOperationType == EditAction::deleteText) {
     if (!startNode->IsContent() ||
         !endNode->IsContent()) {
       // GetPromotedPoint cannot promote node when action type is text
       // operation and selected node isn't content node.
       return;
     }
  }

  // Make a new adjusted range to represent the appropriate block content.
  // This is tricky.  The basic idea is to push out the range endpoints to
  // truly enclose the blocks that we will affect.

  // Make sure that the new range ends up to be in the editable section.
  // XXX Looks like that this check wastes the time.  Perhaps, we should
  //     implement a method which checks both two DOM points in the editor
  //     root.
  EditorDOMPoint startPoint =
    GetPromotedPoint(kStart, *startNode, startOffset, aOperationType);
  if (!htmlEditor->IsDescendantOfEditorRoot(
         EditorBase::GetNodeAtRangeOffsetPoint(startPoint))) {
    return;
  }
  EditorDOMPoint endPoint =
    GetPromotedPoint(kEnd, *endNode, endOffset, aOperationType);
  EditorRawDOMPoint lastRawPoint(endPoint);
  lastRawPoint.RewindOffset();
  if (!htmlEditor->IsDescendantOfEditorRoot(
         EditorBase::GetNodeAtRangeOffsetPoint(lastRawPoint))) {
    return;
  }

  DebugOnly<nsresult> rv = aRange.SetStartAndEnd(startPoint, endPoint);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

class UniqueFunctor final : public BoolDomIterFunctor
{
public:
  explicit UniqueFunctor(nsTArray<OwningNonNull<nsINode>>& aArray)
    : mArray(aArray)
  {
  }

  // Used to build list of all nodes iterator covers.
  virtual bool operator()(nsINode* aNode) const override
  {
    return !mArray.Contains(aNode);
  }

private:
  nsTArray<OwningNonNull<nsINode>>& mArray;
};

/**
 * GetNodesForOperation() runs through the ranges in the array and construct a
 * new array of nodes to be acted on.
 *
 * XXX This name stats with "Get" but actually this modifies the DOM tree with
 *     transaction.  We should rename this to making clearer what this does.
 */
nsresult
HTMLEditRules::GetNodesForOperation(
                 nsTArray<RefPtr<nsRange>>& aArrayOfRanges,
                 nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                 EditAction aOperationType,
                 TouchContent aTouchContent)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  if (aTouchContent == TouchContent::yes) {
    // Split text nodes. This is necessary, since GetPromotedPoint() may return a
    // range ending in a text node in case where part of a pre-formatted
    // elements needs to be moved.
    for (RefPtr<nsRange>& range : aArrayOfRanges) {
      EditorDOMPoint atEnd(range->EndRef());
      if (NS_WARN_IF(!atEnd.IsSet()) || !atEnd.IsInTextNode()) {
        continue;
      }

      if (!atEnd.IsStartOfContainer() && !atEnd.IsEndOfContainer()) {
        // Split the text node.
        ErrorResult error;
        nsCOMPtr<nsIContent> newLeftNode =
          htmlEditor->SplitNodeWithTransaction(atEnd, error);
        if (NS_WARN_IF(error.Failed())) {
          return error.StealNSResult();
        }

        // Correct the range.
        // The new end parent becomes the parent node of the text.
        // XXX We want nsRange::SetEnd(const RawRangeBoundary&)
        EditorRawDOMPoint atContainerOfSplitNode(atEnd.GetContainer());
        range->SetEnd(atContainerOfSplitNode, error);
        if (NS_WARN_IF(error.Failed())) {
          error.SuppressException();
        }
      }
    }
  }

  // Bust up any inlines that cross our range endpoints, but only if we are
  // allowed to touch content.

  if (aTouchContent == TouchContent::yes) {
    nsTArray<OwningNonNull<RangeItem>> rangeItemArray;
    rangeItemArray.AppendElements(aArrayOfRanges.Length());

    // First register ranges for special editor gravity
    for (auto& rangeItem : rangeItemArray) {
      rangeItem = new RangeItem();
      rangeItem->StoreRange(aArrayOfRanges[0]);
      htmlEditor->mRangeUpdater.RegisterRangeItem(rangeItem);
      aArrayOfRanges.RemoveElementAt(0);
    }
    // Now bust up inlines.
    nsresult rv = NS_OK;
    for (auto& item : Reversed(rangeItemArray)) {
      rv = BustUpInlinesAtRangeEndpoints(*item);
      if (NS_FAILED(rv)) {
        break;
      }
    }
    // Then unregister the ranges
    for (auto& item : rangeItemArray) {
      htmlEditor->mRangeUpdater.DropRangeItem(item);
      RefPtr<nsRange> range = item->GetRange();
      if (range) {
        aArrayOfRanges.AppendElement(range);
      }
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // Gather up a list of all the nodes
  for (auto& range : aArrayOfRanges) {
    DOMSubtreeIterator iter;
    nsresult rv = iter.Init(*range);
    NS_ENSURE_SUCCESS(rv, rv);
    if (aOutArrayOfNodes.IsEmpty()) {
      iter.AppendList(TrivialFunctor(), aOutArrayOfNodes);
    } else {
      // We don't want duplicates in aOutArrayOfNodes, so we use an
      // iterator/functor that only return nodes that are not already in
      // aOutArrayOfNodes.
      nsTArray<OwningNonNull<nsINode>> nodes;
      iter.AppendList(UniqueFunctor(aOutArrayOfNodes), nodes);
      aOutArrayOfNodes.AppendElements(nodes);
    }
  }

  // Certain operations should not act on li's and td's, but rather inside
  // them.  Alter the list as needed.
  if (aOperationType == EditAction::makeBasicBlock) {
    for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
      OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
      if (HTMLEditUtils::IsListItem(node)) {
        int32_t j = i;
        aOutArrayOfNodes.RemoveElementAt(i);
        GetInnerContent(*node, aOutArrayOfNodes, &j);
      }
    }
    // Empty text node shouldn't be selected if unnecessary
    for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
      if (Text* text = aOutArrayOfNodes[i]->GetAsText()) {
        // Don't select empty text except to empty block
        if (!htmlEditor->IsVisibleTextNode(*text)) {
          aOutArrayOfNodes.RemoveElementAt(i);
        }
      }
    }
  }
  // Indent/outdent already do something special for list items, but we still
  // need to make sure we don't act on table elements
  else if (aOperationType == EditAction::outdent ||
           aOperationType == EditAction::indent ||
           aOperationType == EditAction::setAbsolutePosition) {
    for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
      OwningNonNull<nsINode> node = aOutArrayOfNodes[i];
      if (HTMLEditUtils::IsTableElementButNotTable(node)) {
        int32_t j = i;
        aOutArrayOfNodes.RemoveElementAt(i);
        GetInnerContent(*node, aOutArrayOfNodes, &j);
      }
    }
  }
  // Outdent should look inside of divs.
  if (aOperationType == EditAction::outdent &&
      !htmlEditor->IsCSSEnabled()) {
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
          htmlEditor->IsContainer(node) && !EditorBase::IsTextNode(node)) {
        nsTArray<OwningNonNull<nsINode>> arrayOfInlines;
        nsresult rv = BustUpInlinesAtBRs(*node->AsContent(), arrayOfInlines);
        NS_ENSURE_SUCCESS(rv, rv);

        // Put these nodes in aOutArrayOfNodes, replacing the current node
        aOutArrayOfNodes.RemoveElementAt(i);
        aOutArrayOfNodes.InsertElementsAt(i, arrayOfInlines);
      }
    }
  }
  return NS_OK;
}

void
HTMLEditRules::GetChildNodesForOperation(
                 nsINode& aNode,
                 nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes)
{
  for (nsCOMPtr<nsIContent> child = aNode.GetFirstChild();
       child; child = child->GetNextSibling()) {
    outArrayOfNodes.AppendElement(*child);
  }
}

nsresult
HTMLEditRules::GetListActionNodes(
                 nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes,
                 EntireList aEntireList,
                 TouchContent aTouchContent)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  RefPtr<Selection> selection = htmlEditor->GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  // Added this in so that ui code can ask to change an entire list, even if
  // selection is only in part of it.  used by list item dialog.
  if (aEntireList == EntireList::yes) {
    uint32_t rangeCount = selection->RangeCount();
    for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
      RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
      for (nsCOMPtr<nsINode> parent = range->GetCommonAncestor();
           parent; parent = parent->GetParentNode()) {
        if (HTMLEditUtils::IsList(parent)) {
          aOutArrayOfNodes.AppendElement(*parent);
          break;
        }
      }
    }
    // If we didn't find any nodes this way, then try the normal way.  Perhaps
    // the selection spans multiple lists but with no common list parent.
    if (!aOutArrayOfNodes.IsEmpty()) {
      return NS_OK;
    }
  }

  {
    // We don't like other people messing with our selection!
    AutoTransactionsConserveSelection dontChangeMySelection(htmlEditor);

    // contruct a list of nodes to act on.
    nsresult rv = GetNodesFromSelection(*selection, EditAction::makeList,
                                        aOutArrayOfNodes, aTouchContent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Pre-process our list of nodes
  for (int32_t i = aOutArrayOfNodes.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsINode> testNode = aOutArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!htmlEditor->IsEditable(testNode)) {
      aOutArrayOfNodes.RemoveElementAt(i);
      continue;
    }

    // Scan for table elements and divs.  If we find table elements other than
    // table, replace it with a list of any editable non-table content.
    if (HTMLEditUtils::IsTableElementButNotTable(testNode)) {
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
HTMLEditRules::LookInsideDivBQandList(
                 nsTArray<OwningNonNull<nsINode>>& aNodeArray)
{
  NS_ENSURE_TRUE(mHTMLEditor, );
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // If there is only one node in the array, and it is a list, div, or
  // blockquote, then look inside of it until we find inner list or content.
  if (aNodeArray.Length() != 1) {
    return;
  }

  OwningNonNull<nsINode> curNode = aNodeArray[0];

  while (curNode->IsHTMLElement(nsGkAtoms::div) ||
         HTMLEditUtils::IsList(curNode) ||
         curNode->IsHTMLElement(nsGkAtoms::blockquote)) {
    // Dive as long as there's only one child, and it's a list, div, blockquote
    uint32_t numChildren = htmlEditor->CountEditableChildren(curNode);
    if (numChildren != 1) {
      break;
    }

    // Keep diving!  XXX One would expect to dive into the one editable node.
    nsCOMPtr<nsIContent> child = curNode->GetFirstChild();
    if (!child->IsHTMLElement(nsGkAtoms::div) &&
        !HTMLEditUtils::IsList(child) &&
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

void
HTMLEditRules::GetDefinitionListItemTypes(dom::Element* aElement,
                                          bool* aDT,
                                          bool* aDD)
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
HTMLEditRules::GetParagraphFormatNodes(
                 nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                 TouchContent aTouchContent)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  RefPtr<Selection> selection = htmlEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  // Contruct a list of nodes to act on.
  nsresult rv = GetNodesFromSelection(*selection, EditAction::makeBasicBlock,
                                      outArrayOfNodes, aTouchContent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Pre-process our list of nodes
  for (int32_t i = outArrayOfNodes.Length() - 1; i >= 0; i--) {
    OwningNonNull<nsINode> testNode = outArrayOfNodes[i];

    // Remove all non-editable nodes.  Leave them be.
    if (!htmlEditor->IsEditable(testNode)) {
      outArrayOfNodes.RemoveElementAt(i);
      continue;
    }

    // Scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.  Ditto for
    // list elements.
    if (HTMLEditUtils::IsTableElement(testNode) ||
        HTMLEditUtils::IsList(testNode) ||
        HTMLEditUtils::IsListItem(testNode)) {
      int32_t j = i;
      outArrayOfNodes.RemoveElementAt(i);
      GetInnerContent(testNode, outArrayOfNodes, &j);
    }
  }
  return NS_OK;
}

nsresult
HTMLEditRules::BustUpInlinesAtRangeEndpoints(RangeItem& item)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool isCollapsed = item.mStartContainer == item.mEndContainer &&
                     item.mStartOffset == item.mEndOffset;

  nsCOMPtr<nsIContent> endInline = GetHighestInlineParent(*item.mEndContainer);
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_FAILURE;
  }

  // XXX Oh, then, if the range is collapsed, we don't need to call
  //     GetHighestInlineParent(), isn't it?
  if (endInline && !isCollapsed) {
    RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
    SplitNodeResult splitEndInlineResult =
      htmlEditor->SplitNodeDeepWithTransaction(
                    *endInline,
                    EditorRawDOMPoint(item.mEndContainer, item.mEndOffset),
                    SplitAtEdges::eDoNotCreateEmptyContainer);
    if (NS_WARN_IF(splitEndInlineResult.Failed())) {
      return splitEndInlineResult.Rv();
    }
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_FAILURE;
    }
    EditorRawDOMPoint splitPointAtEnd(splitEndInlineResult.SplitPoint());
    item.mEndContainer = splitPointAtEnd.GetContainer();
    item.mEndOffset = splitPointAtEnd.Offset();
  }

  nsCOMPtr<nsIContent> startInline =
    GetHighestInlineParent(*item.mStartContainer);
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_FAILURE;
  }

  if (startInline) {
    RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
    SplitNodeResult splitStartInlineResult =
      htmlEditor->SplitNodeDeepWithTransaction(
                    *startInline,
                    EditorRawDOMPoint(item.mStartContainer, item.mStartOffset),
                    SplitAtEdges::eDoNotCreateEmptyContainer);
    if (NS_WARN_IF(splitStartInlineResult.Failed())) {
      return splitStartInlineResult.Rv();
    }
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_FAILURE;
    }
    EditorRawDOMPoint splitPointAtStart(splitStartInlineResult.SplitPoint());
    item.mStartContainer = splitPointAtStart.GetContainer();
    item.mStartOffset = splitPointAtStart.Offset();
  }

  return NS_OK;
}

nsresult
HTMLEditRules::BustUpInlinesAtBRs(
                 nsIContent& aNode,
                 nsTArray<OwningNonNull<nsINode>>& aOutArrayOfNodes)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // First build up a list of all the break nodes inside the inline container.
  nsTArray<OwningNonNull<nsINode>> arrayOfBreaks;
  BRNodeFunctor functor;
  DOMIterator iter(aNode);
  iter.AppendList(functor, arrayOfBreaks);

  // If there aren't any breaks, just put inNode itself in the array
  if (arrayOfBreaks.IsEmpty()) {
    aOutArrayOfNodes.AppendElement(aNode);
    return NS_OK;
  }

  // Else we need to bust up aNode along all the breaks
  nsCOMPtr<nsIContent> nextNode = &aNode;
  for (OwningNonNull<nsINode>& brNode : arrayOfBreaks) {
    EditorRawDOMPoint atBrNode(brNode);
    if (NS_WARN_IF(!atBrNode.IsSet())) {
      return NS_ERROR_FAILURE;
    }
    SplitNodeResult splitNodeResult =
      htmlEditor->SplitNodeDeepWithTransaction(
                    *nextNode, atBrNode,
                    SplitAtEdges::eAllowToCreateEmptyContainer);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }

    // Put previous node at the split point.
    if (splitNodeResult.GetPreviousNode()) {
      // Might not be a left node.  A break might have been at the very
      // beginning of inline container, in which case
      // SplitNodeDeepWithTransaction() would not actually split anything.
      aOutArrayOfNodes.AppendElement(*splitNodeResult.GetPreviousNode());
    }

    // Move break outside of container and also put in node list
    EditorRawDOMPoint atNextNode(splitNodeResult.GetNextNode());
    nsresult rv =
      htmlEditor->MoveNodeWithTransaction(*brNode->AsContent(), atNextNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    aOutArrayOfNodes.AppendElement(*brNode);

    nextNode = splitNodeResult.GetNextNode();
  }

  // Now tack on remaining next node.
  aOutArrayOfNodes.AppendElement(*nextNode);

  return NS_OK;
}

nsIContent*
HTMLEditRules::GetHighestInlineParent(nsINode& aNode)
{
  if (!aNode.IsContent() || IsBlockNode(aNode)) {
    return nullptr;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return nullptr;
  }

  Element* host = mHTMLEditor->GetActiveEditingHost();
  if (NS_WARN_IF(!host)) {
    return nullptr;
  }

  // If aNode is the editing host itself, there is no modifiable inline parent.
  if (&aNode == host) {
    return nullptr;
  }

  // If aNode is outside of the <body> element, we don't support to edit
  // such elements for now.
  // XXX This should be MOZ_ASSERT after fixing bug 1413131 for avoiding
  //     calling this expensive method.
  if (NS_WARN_IF(!EditorUtils::IsDescendantOf(aNode, *host))) {
    return nullptr;
  }

  // Looks for the highest inline parent in the editing host.
  nsIContent* content = aNode.AsContent();
  for (nsIContent* parent = content->GetParent();
       parent && parent != host && IsInlineNode(*parent);
       parent = parent->GetParent()) {
    content = parent;
  }
  return content;
}

/**
 * GetNodesFromPoint() constructs a list of nodes from a point that will be
 * operated on.
 */
nsresult
HTMLEditRules::GetNodesFromPoint(
                 const EditorDOMPoint& aPoint,
                 EditAction aOperation,
                 nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                 TouchContent aTouchContent)
{
  if (NS_WARN_IF(!aPoint.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }
  RefPtr<nsRange> range = new nsRange(aPoint.GetContainer());
  ErrorResult error;
  range->SetStart(aPoint, error);
  // error will assert on failure, because we are not cleaning it up,
  // but we're asserting in that case anyway.
  MOZ_ASSERT(!error.Failed());

  // Expand the range to include adjacent inlines
  PromoteRange(*range, aOperation);

  // Make array of ranges
  nsTArray<RefPtr<nsRange>> arrayOfRanges;

  // Stuff new opRange into array
  arrayOfRanges.AppendElement(range);

  // Use these ranges to contruct a list of nodes to act on
  nsresult rv2 =
    GetNodesForOperation(arrayOfRanges, outArrayOfNodes, aOperation,
                         aTouchContent);
  if (NS_WARN_IF(NS_FAILED(rv2))) {
    return rv2;
  }

  return NS_OK;
}

/**
 * GetNodesFromSelection() constructs a list of nodes from the selection that
 * will be operated on.
 */
nsresult
HTMLEditRules::GetNodesFromSelection(
                 Selection& aSelection,
                 EditAction aOperation,
                 nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes,
                 TouchContent aTouchContent)
{
  // Promote selection ranges
  nsTArray<RefPtr<nsRange>> arrayOfRanges;
  GetPromotedRanges(aSelection, arrayOfRanges, aOperation);

  // Use these ranges to contruct a list of nodes to act on.
  nsresult rv = GetNodesForOperation(arrayOfRanges, outArrayOfNodes,
                                     aOperation, aTouchContent);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/**
 * MakeTransitionList() detects all the transitions in the array, where a
 * transition means that adjacent nodes in the array don't have the same parent.
 */
void
HTMLEditRules::MakeTransitionList(nsTArray<OwningNonNull<nsINode>>& aNodeArray,
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

/**
 * If aNode is the descendant of a listitem, return that li.  But table element
 * boundaries are stoppers on the search.  Also stops on the active editor host
 * (contenteditable).  Also test if aNode is an li itself.
 */
Element*
HTMLEditRules::IsInListItem(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, nullptr);
  if (HTMLEditUtils::IsListItem(aNode)) {
    return aNode->AsElement();
  }

  Element* parent = aNode->GetParentElement();
  while (parent &&
         mHTMLEditor && mHTMLEditor->IsDescendantOfEditorRoot(parent) &&
         !HTMLEditUtils::IsTableElement(parent)) {
    if (HTMLEditUtils::IsListItem(parent)) {
      return parent;
    }
    parent = parent->GetParentElement();
  }
  return nullptr;
}

nsAtom&
HTMLEditRules::DefaultParagraphSeparator()
{
  MOZ_ASSERT(mHTMLEditor);
  if (!mHTMLEditor) {
    return *nsGkAtoms::div;
  }
  return ParagraphSeparatorElement(mHTMLEditor->GetDefaultParagraphSeparator());
}

/**
 * ReturnInHeader: do the right thing for returns pressed in headers
 */
nsresult
HTMLEditRules::ReturnInHeader(Selection& aSelection,
                              Element& aHeader,
                              nsINode& aNode,
                              int32_t aOffset)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // Remember where the header is
  nsCOMPtr<nsINode> headerParent = aHeader.GetParentNode();
  int32_t offset = headerParent ? headerParent->ComputeIndexOf(&aHeader) : -1;

  // Get ws code to adjust any ws
  nsCOMPtr<nsINode> node = &aNode;
  nsresult rv = WSRunObject::PrepareToSplitAcrossBlocks(htmlEditor,
                                                        address_of(node),
                                                        &aOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_WARN_IF(!node->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  // Split the header
  ErrorResult error;
  SplitNodeResult splitHeaderResult =
    htmlEditor->SplitNodeDeepWithTransaction(
                  aHeader, EditorRawDOMPoint(node, aOffset),
                  SplitAtEdges::eAllowToCreateEmptyContainer);
  NS_WARNING_ASSERTION(splitHeaderResult.Succeeded(),
    "Failed to split aHeader");

  // If the previous heading of split point is empty, put a mozbr into it.
  nsCOMPtr<nsIContent> prevItem = htmlEditor->GetPriorHTMLSibling(&aHeader);
  if (prevItem) {
    MOZ_DIAGNOSTIC_ASSERT(
      HTMLEditUtils::IsHeader(*prevItem));
    bool isEmptyNode;
    rv = htmlEditor->IsEmptyNode(prevItem, &isEmptyNode);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEmptyNode) {
      RefPtr<Element> brElement = CreateMozBR(EditorRawDOMPoint(prevItem, 0));
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  // If the new (righthand) header node is empty, delete it
  if (IsEmptyBlockElement(aHeader, IgnoreSingleBR::eYes)) {
    rv = htmlEditor->DeleteNodeWithTransaction(aHeader);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Layout tells the caret to blink in a weird place if we don't place a
    // break after the header.
    nsCOMPtr<nsIContent> sibling;
    if (aHeader.GetNextSibling()) {
      sibling = htmlEditor->GetNextHTMLSibling(aHeader.GetNextSibling());
    }
    if (!sibling || !sibling->IsHTMLElement(nsGkAtoms::br)) {
      ClearCachedStyles();
      htmlEditor->mTypeInState->ClearAllProps();

      // Create a paragraph
      nsAtom& paraAtom = DefaultParagraphSeparator();
      // We want a wrapper element even if we separate with <br>
      EditorRawDOMPoint nextToHeader(headerParent, offset + 1);
      RefPtr<Element> pNode =
        htmlEditor->CreateNodeWithTransaction(&paraAtom == nsGkAtoms::br ?
                                                *nsGkAtoms::p : paraAtom,
                                              nextToHeader);
      if (NS_WARN_IF(!pNode)) {
        return NS_ERROR_FAILURE;
      }

      // Append a <br> to it
      RefPtr<Element> brElement =
        htmlEditor->InsertBrElementWithTransaction(aSelection,
                                                   EditorRawDOMPoint(pNode, 0));
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }

      // Set selection to before the break
      ErrorResult error;
      aSelection.Collapse(EditorRawDOMPoint(pNode, 0), error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    } else {
      EditorRawDOMPoint afterSibling(sibling);
      if (NS_WARN_IF(!afterSibling.AdvanceOffset())) {
        return NS_ERROR_FAILURE;
      }
      // Put selection after break
      rv = aSelection.Collapse(afterSibling);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    // Put selection at front of righthand heading
    rv = aSelection.Collapse(&aHeader, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

EditActionResult
HTMLEditRules::ReturnInParagraph(Selection& aSelection,
                                 Element& aParentDivOrP)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditActionResult(NS_ERROR_NOT_AVAILABLE);
  }

  RefPtr<HTMLEditor> htmlEditor = mHTMLEditor;

  nsRange* firstRange = aSelection.GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return EditActionResult(NS_ERROR_FAILURE);
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return EditActionResult(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // We shouldn't create new anchor element which has non-empty href unless
  // splitting middle of it because we assume that users don't want to create
  // *same* anchor element across two or more paragraphs in most cases.
  // So, adjust selection start if it's edge of anchor element(s).
  // XXX We don't support whitespace collapsing in these cases since it needs
  //     some additional work with WSRunObject but it's not usual case.
  //     E.g., |<a href="foo"><b>foo []</b> </a>|
  if (atStartOfSelection.IsStartOfContainer()) {
    for (nsIContent* container = atStartOfSelection.GetContainerAsContent();
         container && container != &aParentDivOrP;
         container = container->GetParent()) {
      if (HTMLEditUtils::IsLink(container)) {
        // Found link should be only in right node.  So, we shouldn't split it.
        atStartOfSelection.Set(container);
        // Even if we found an anchor element, don't break because DOM API
        // allows to nest anchor elements.
      }
      // If the container is middle of its parent, stop adjusting split point.
      if (container->GetPreviousSibling()) {
        // XXX Should we check if previous sibling is visible content?
        //     E.g., should we ignore comment node, invisible <br> element?
        break;
      }
    }
  }
  // We also need to check if selection is at invisible <br> element at end
  // of an <a href="foo"> element because editor inserts a <br> element when
  // user types Enter key after a whitespace which is at middle of
  // <a href="foo"> element and when setting selection at end of the element,
  // selection becomes referring the <br> element.  We may need to change this
  // behavior later if it'd be standardized.
  else if (atStartOfSelection.IsEndOfContainer() ||
           atStartOfSelection.IsBRElementAtEndOfContainer()) {
    // If there are 2 <br> elements, the first <br> element is visible.  E.g.,
    // |<a href="foo"><b>boo[]<br></b><br></a>|, we should split the <a>
    // element.  Otherwise, E.g., |<a href="foo"><b>boo[]<br></b></a>|,
    // we should not split the <a> element and ignore inline elements in it.
    bool foundBRElement = atStartOfSelection.IsBRElementAtEndOfContainer();
    for (nsIContent* container = atStartOfSelection.GetContainerAsContent();
         container && container != &aParentDivOrP;
         container = container->GetParent()) {
      if (HTMLEditUtils::IsLink(container)) {
        // Found link should be only in left node.  So, we shouldn't split it.
        atStartOfSelection.SetAfter(container);
        // Even if we found an anchor element, don't break because DOM API
        // allows to nest anchor elements.
      }
      // If the container is middle of its parent, stop adjusting split point.
      if (nsIContent* nextSibling = container->GetNextSibling()) {
        if (foundBRElement) {
          // If we've already found a <br> element, we assume found node is
          // visible <br> or something other node.
          // XXX Should we check if non-text data node like comment?
          break;
        }

        // XXX Should we check if non-text data node like comment?
        if (!nextSibling->IsHTMLElement(nsGkAtoms::br)) {
          break;
        }
        foundBRElement = true;
      }
    }
  }

  bool doesCRCreateNewP = htmlEditor->GetReturnInParagraphCreatesNewParagraph();

  bool splitAfterNewBR = false;
  nsCOMPtr<nsIContent> brContent;

  EditorDOMPoint pointToSplitParentDivOrP(atStartOfSelection);

  EditorRawDOMPoint pointToInsertBR;
  if (doesCRCreateNewP &&
      atStartOfSelection.GetContainer() == &aParentDivOrP) {
    // We are at the edges of the block, so, we don't need to create new <br>.
    brContent = nullptr;
  } else if (atStartOfSelection.IsInTextNode()) {
    // at beginning of text node?
    if (atStartOfSelection.IsStartOfContainer()) {
      // is there a BR prior to it?
      brContent =
        htmlEditor->GetPriorHTMLSibling(atStartOfSelection.GetContainer());
      if (!brContent ||
          !htmlEditor->IsVisibleBRElement(brContent) ||
          TextEditUtils::HasMozAttr(brContent)) {
        pointToInsertBR.Set(atStartOfSelection.GetContainer());
        brContent = nullptr;
      }
    } else if (atStartOfSelection.IsEndOfContainer()) {
      // we're at the end of text node...
      // is there a BR after to it?
      brContent =
        htmlEditor->GetNextHTMLSibling(atStartOfSelection.GetContainer());
      if (!brContent ||
          !htmlEditor->IsVisibleBRElement(brContent) ||
          TextEditUtils::HasMozAttr(brContent)) {
        pointToInsertBR.Set(atStartOfSelection.GetContainer());
        DebugOnly<bool> advanced = pointToInsertBR.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
          "Failed to advance offset to after the container of selection start");
        brContent = nullptr;
      }
    } else {
      if (doesCRCreateNewP) {
        ErrorResult error;
        nsCOMPtr<nsIContent> newLeftDivOrP =
          htmlEditor->SplitNodeWithTransaction(pointToSplitParentDivOrP, error);
        if (NS_WARN_IF(error.Failed())) {
          return EditActionResult(error.StealNSResult());
        }
        pointToSplitParentDivOrP.SetToEndOf(newLeftDivOrP);
      }

      // We need to put new <br> after the left node if given node was split
      // above.
      pointToInsertBR.Set(pointToSplitParentDivOrP.GetContainer());
      DebugOnly<bool> advanced = pointToInsertBR.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
        "Failed to advance offset to after the container of selection start");
    }
  } else {
    // not in a text node.
    // is there a BR prior to it?
    nsCOMPtr<nsIContent> nearNode;
    nearNode =
      htmlEditor->GetPreviousEditableHTMLNode(atStartOfSelection);
    if (!nearNode || !htmlEditor->IsVisibleBRElement(nearNode) ||
        TextEditUtils::HasMozAttr(nearNode)) {
      // is there a BR after it?
      nearNode =
        htmlEditor->GetNextEditableHTMLNode(atStartOfSelection);
      if (!nearNode || !htmlEditor->IsVisibleBRElement(nearNode) ||
          TextEditUtils::HasMozAttr(nearNode)) {
        pointToInsertBR = atStartOfSelection;
        splitAfterNewBR = true;
      }
    }
    if (!pointToInsertBR.IsSet() && TextEditUtils::IsBreak(nearNode)) {
      brContent = nearNode;
    }
  }
  if (pointToInsertBR.IsSet()) {
    // Don't modify the DOM tree if mHTMLEditor disappeared.
    if (NS_WARN_IF(!mHTMLEditor)) {
      return EditActionResult(NS_ERROR_NOT_AVAILABLE);
    }

    // if CR does not create a new P, default to BR creation
    if (NS_WARN_IF(!doesCRCreateNewP)) {
      return EditActionResult(NS_OK);
    }

    brContent =
      htmlEditor->InsertBrElementWithTransaction(aSelection, pointToInsertBR);
    NS_WARNING_ASSERTION(brContent, "Failed to create a <br> element");
    if (splitAfterNewBR) {
      // We split the parent after the br we've just inserted.
      pointToSplitParentDivOrP.Set(brContent);
      DebugOnly<bool> advanced = pointToSplitParentDivOrP.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
        "Failed to advance offset after the new <br>");
    }
  }
  EditActionResult result(
    SplitParagraph(aSelection, aParentDivOrP, pointToSplitParentDivOrP,
                   brContent));
  result.MarkAsHandled();
  if (NS_WARN_IF(result.Failed())) {
    return result;
  }
  return result;
}

template<typename PT, typename CT>
nsresult
HTMLEditRules::SplitParagraph(
                 Selection& aSelection,
                 Element& aParentDivOrP,
                 const EditorDOMPointBase<PT, CT>& aStartOfRightNode,
                 nsIContent* aNextBRNode)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<HTMLEditor> htmlEditor = mHTMLEditor;

  // split para
  // get ws code to adjust any ws
  nsCOMPtr<nsINode> selNode = aStartOfRightNode.GetContainer();
  int32_t selOffset = aStartOfRightNode.Offset();
  nsresult rv =
    WSRunObject::PrepareToSplitAcrossBlocks(htmlEditor,
                                            address_of(selNode), &selOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_WARN_IF(!selNode->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  // Split the paragraph.
  SplitNodeResult splitDivOrPResult =
    htmlEditor->SplitNodeDeepWithTransaction(
                  aParentDivOrP,
                  EditorRawDOMPoint(selNode, selOffset),
                  SplitAtEdges::eAllowToCreateEmptyContainer);
  if (NS_WARN_IF(splitDivOrPResult.Failed())) {
    return splitDivOrPResult.Rv();
  }
  if (NS_WARN_IF(!splitDivOrPResult.DidSplit())) {
    return NS_ERROR_FAILURE;
  }

  // Get rid of the break, if it is visible (otherwise it may be needed to
  // prevent an empty p).
  if (aNextBRNode && htmlEditor->IsVisibleBRElement(aNextBRNode)) {
    rv = htmlEditor->DeleteNodeWithTransaction(*aNextBRNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Remove ID attribute on the paragraph from the existing right node.
  rv = htmlEditor->RemoveAttributeWithTransaction(*aParentDivOrP.AsElement(),
                                                  *nsGkAtoms::id);
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to ensure to both paragraphs visible even if they are empty.
  // However, moz-<br> element isn't useful in this case because moz-<br>
  // elements will be ignored by PlaintextSerializer.  Additionally,
  // moz-<br> will be exposed as <br> with Element.innerHTML.  Therefore,
  // we can use normal <br> elements for placeholder in this case.
  // Note that Chromium also behaves so.
  rv = InsertBRIfNeeded(*splitDivOrPResult.GetPreviousNode());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InsertBRIfNeeded(*splitDivOrPResult.GetNextNode());
  NS_ENSURE_SUCCESS(rv, rv);

  // selection to beginning of right hand para;
  // look inside any containers that are up front.
  nsIContent* child = htmlEditor->GetLeftmostChild(&aParentDivOrP, true);
  if (EditorBase::IsTextNode(child) || htmlEditor->IsContainer(child)) {
    EditorRawDOMPoint atStartOfChild(child, 0);
    ErrorResult error;
    aSelection.Collapse(atStartOfChild, error);
    if (NS_WARN_IF(error.Failed())) {
      error.SuppressException();
    }
  } else {
    EditorRawDOMPoint atChild(child);
    ErrorResult error;
    aSelection.Collapse(atChild, error);
    if (NS_WARN_IF(error.Failed())) {
      error.SuppressException();
    }
  }
  return NS_OK;
}

/**
 * ReturnInListItem: do the right thing for returns pressed in list items
 */
nsresult
HTMLEditRules::ReturnInListItem(Selection& aSelection,
                                Element& aListItem,
                                nsINode& aNode,
                                int32_t aOffset)
{
  MOZ_ASSERT(HTMLEditUtils::IsListItem(&aListItem));

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // Get the item parent and the active editing host.
  nsCOMPtr<Element> root = htmlEditor->GetActiveEditingHost();

  // If we are in an empty item, then we want to pop up out of the list, but
  // only if prefs say it's okay and if the parent isn't the active editing
  // host.
  if (mReturnInEmptyLIKillsList &&
      root != aListItem.GetParentElement() &&
      IsEmptyBlockElement(aListItem, IgnoreSingleBR::eYes)) {
    nsCOMPtr<nsIContent> leftListNode = aListItem.GetParent();
    // Are we the last list item in the list?
    if (!htmlEditor->IsLastEditableChild(&aListItem)) {
      // We need to split the list!
      EditorRawDOMPoint atListItem(&aListItem);
      ErrorResult error;
      leftListNode = htmlEditor->SplitNodeWithTransaction(atListItem, error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    }

    // Are we in a sublist?
    EditorRawDOMPoint atNextSiblingOfLeftList(leftListNode);
    DebugOnly<bool> advanced = atNextSiblingOfLeftList.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
      "Failed to advance offset after the right list node");
    if (HTMLEditUtils::IsList(atNextSiblingOfLeftList.GetContainer())) {
      // If so, move item out of this list and into the grandparent list
      nsresult rv =
        htmlEditor->MoveNodeWithTransaction(aListItem,
                                            atNextSiblingOfLeftList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      ErrorResult error;
      aSelection.Collapse(RawRangeBoundary(&aListItem, 0), error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    } else {
      // Otherwise kill this item
      nsresult rv = htmlEditor->DeleteNodeWithTransaction(aListItem);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Time to insert a paragraph
      nsAtom& paraAtom = DefaultParagraphSeparator();
      // We want a wrapper even if we separate with <br>
      RefPtr<Element> pNode =
        htmlEditor->CreateNodeWithTransaction(&paraAtom == nsGkAtoms::br ?
                                                *nsGkAtoms::p : paraAtom,
                                              atNextSiblingOfLeftList);
      if (NS_WARN_IF(!pNode)) {
        return NS_ERROR_FAILURE;
      }

      RefPtr<Selection> selection = htmlEditor->GetSelection();
      if (NS_WARN_IF(!selection)) {
        return NS_ERROR_FAILURE;
      }

      // Append a <br> to it
      RefPtr<Element> brElement =
        htmlEditor->InsertBrElementWithTransaction(*selection,
                                                   EditorRawDOMPoint(pNode, 0));
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }

      // Set selection to before the break
      ErrorResult error;
      aSelection.Collapse(EditorRawDOMPoint(pNode, 0), error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    }
    return NS_OK;
  }

  // Else we want a new list item at the same list level.  Get ws code to
  // adjust any ws.
  nsCOMPtr<nsINode> selNode = &aNode;
  nsresult rv =
    WSRunObject::PrepareToSplitAcrossBlocks(htmlEditor,
                                            address_of(selNode), &aOffset);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_WARN_IF(!selNode->IsContent())) {
    return NS_ERROR_FAILURE;
  }

  // Now split the list item.
  SplitNodeResult splitListItemResult =
    htmlEditor->SplitNodeDeepWithTransaction(
                  aListItem, EditorRawDOMPoint(selNode, aOffset),
                  SplitAtEdges::eAllowToCreateEmptyContainer);
  NS_WARNING_ASSERTION(splitListItemResult.Succeeded(),
    "Failed to split the list item");

  // Hack: until I can change the damaged doc range code back to being
  // extra-inclusive, I have to manually detect certain list items that may be
  // left empty.
  nsCOMPtr<nsIContent> prevItem = htmlEditor->GetPriorHTMLSibling(&aListItem);
  if (prevItem && HTMLEditUtils::IsListItem(prevItem)) {
    bool isEmptyNode;
    rv = htmlEditor->IsEmptyNode(prevItem, &isEmptyNode);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isEmptyNode) {
      RefPtr<Element> brElement = CreateMozBR(EditorRawDOMPoint(prevItem, 0));
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }
    } else {
      rv = htmlEditor->IsEmptyNode(&aListItem, &isEmptyNode, true);
      NS_ENSURE_SUCCESS(rv, rv);
      if (isEmptyNode) {
        RefPtr<nsAtom> nodeAtom = aListItem.NodeInfo()->NameAtom();
        if (nodeAtom == nsGkAtoms::dd || nodeAtom == nsGkAtoms::dt) {
          nsCOMPtr<nsINode> list = aListItem.GetParentNode();
          int32_t itemOffset = list ? list->ComputeIndexOf(&aListItem) : -1;

          nsAtom* listAtom = nodeAtom == nsGkAtoms::dt ? nsGkAtoms::dd
                                                        : nsGkAtoms::dt;
          MOZ_DIAGNOSTIC_ASSERT(itemOffset != -1);
          EditorRawDOMPoint atNextListItem(list, aListItem.GetNextSibling(),
                                           itemOffset + 1);
          RefPtr<Element> newListItem =
            htmlEditor->CreateNodeWithTransaction(*listAtom, atNextListItem);
          if (NS_WARN_IF(!newListItem)) {
            return NS_ERROR_FAILURE;
          }
          rv = htmlEditor->DeleteNodeWithTransaction(aListItem);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          ErrorResult error;
          aSelection.Collapse(EditorRawDOMPoint(newListItem, 0), error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          return NS_OK;
        }

        RefPtr<Element> brElement;
        nsresult rv =
          htmlEditor->CopyLastEditableChildStylesWithTransaction(
                        *prevItem->AsElement(), aListItem,
                        address_of(brElement));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return NS_ERROR_FAILURE;
        }
        if (brElement) {
          EditorRawDOMPoint atBrNode(brElement);
          if (NS_WARN_IF(!atBrNode.IsSetAndValid())) {
            return NS_ERROR_FAILURE;
          }
          ErrorResult error;
          aSelection.Collapse(atBrNode, error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          return NS_OK;
        }
      } else {
        WSRunObject wsObj(htmlEditor, &aListItem, 0);
        nsCOMPtr<nsINode> visNode;
        int32_t visOffset = 0;
        WSType wsType;
        wsObj.NextVisibleNode(EditorRawDOMPoint(&aListItem, 0),
                              address_of(visNode), &visOffset, &wsType);
        if (wsType == WSType::special || wsType == WSType::br ||
            visNode->IsHTMLElement(nsGkAtoms::hr)) {
          EditorRawDOMPoint atVisNode(visNode);
          if (NS_WARN_IF(!atVisNode.IsSetAndValid())) {
            return NS_ERROR_FAILURE;
          }
          ErrorResult error;
          aSelection.Collapse(atVisNode, error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
          return NS_OK;
        }

        rv = aSelection.Collapse(visNode, visOffset);
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
  }

  ErrorResult error;
  aSelection.Collapse(EditorRawDOMPoint(&aListItem, 0), error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

/**
 * MakeBlockquote() puts the list of nodes into one or more blockquotes.
 */
nsresult
HTMLEditRules::MakeBlockquote(nsTArray<OwningNonNull<nsINode>>& aNodeArray)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // The idea here is to put the nodes into a minimal number of blockquotes.
  // When the user blockquotes something, they expect one blockquote.  That may
  // not be possible (for instance, if they have two table cells selected, you
  // need two blockquotes inside the cells).
  nsCOMPtr<Element> curBlock;
  nsCOMPtr<nsINode> prevParent;

  for (auto& curNode : aNodeArray) {
    // Get the node to act on, and its location
    NS_ENSURE_STATE(curNode->IsContent());

    // If the node is a table element or list item, dive inside
    if (HTMLEditUtils::IsTableElementButNotTable(curNode) ||
        HTMLEditUtils::IsListItem(curNode)) {
      // Forget any previous block
      curBlock = nullptr;
      // Recursion time
      nsTArray<OwningNonNull<nsINode>> childArray;
      GetChildNodesForOperation(*curNode, childArray);
      nsresult rv = MakeBlockquote(childArray);
      NS_ENSURE_SUCCESS(rv, rv);
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
      EditorDOMPoint atCurNode(curNode);
      SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::blockquote,
                                                    atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curBlock =
        htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::blockquote,
                                              splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!curBlock)) {
        return NS_ERROR_FAILURE;
      }
      // remember our new block for postprocessing
      mNewBlock = curBlock;
      // note: doesn't matter if we set mNewBlock multiple times.
    }

    nsresult rv =
      htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                               *curBlock);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

/**
 * RemoveBlockStyle() makes the nodes have no special block type.
 */
nsresult
HTMLEditRules::RemoveBlockStyle(nsTArray<OwningNonNull<nsINode>>& aNodeArray)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // Intent of this routine is to be used for converting to/from headers,
  // paragraphs, pre, and address.  Those blocks that pretty much just contain
  // inline things...
  nsCOMPtr<Element> curBlock;
  nsCOMPtr<nsIContent> firstNode, lastNode;
  for (auto& curNode : aNodeArray) {
    // If curNode is a address, p, header, address, or pre, remove it
    if (HTMLEditUtils::IsFormatNode(curNode)) {
      // Process any partial progress saved
      if (curBlock) {
        nsresult rv = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
        NS_ENSURE_SUCCESS(rv, rv);
        firstNode = lastNode = curBlock = nullptr;
      }
      if (!mHTMLEditor->IsEditable(curNode)) {
        continue;
      }
      // Remove current block
      nsresult rv =
        htmlEditor->RemoveBlockContainerWithTransaction(*curNode->AsElement());
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (curNode->IsAnyOfHTMLElements(nsGkAtoms::table,
                                            nsGkAtoms::tr,
                                            nsGkAtoms::tbody,
                                            nsGkAtoms::td,
                                            nsGkAtoms::li,
                                            nsGkAtoms::blockquote,
                                            nsGkAtoms::div) ||
                HTMLEditUtils::IsList(curNode)) {
      // Process any partial progress saved
      if (curBlock) {
        nsresult rv = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
        NS_ENSURE_SUCCESS(rv, rv);
        firstNode = lastNode = curBlock = nullptr;
      }
      if (!mHTMLEditor->IsEditable(curNode)) {
        continue;
      }
      // Recursion time
      nsTArray<OwningNonNull<nsINode>> childArray;
      GetChildNodesForOperation(*curNode, childArray);
      nsresult rv = RemoveBlockStyle(childArray);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (IsInlineNode(curNode)) {
      if (curBlock) {
        // If so, is this node a descendant?
        if (EditorUtils::IsDescendantOf(*curNode, *curBlock)) {
          // Then we don't need to do anything different for this node
          lastNode = curNode->AsContent();
          continue;
        }
        // Otherwise, we have progressed beyond end of curBlock, so let's
        // handle it now.  We need to remove the portion of curBlock that
        // contains [firstNode - lastNode].
        nsresult rv = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
        NS_ENSURE_SUCCESS(rv, rv);
        firstNode = lastNode = curBlock = nullptr;
        // Fall out and handle curNode
      }
      curBlock = htmlEditor->GetBlockNodeParent(curNode);
      if (!curBlock || !HTMLEditUtils::IsFormatNode(curBlock) ||
          !mHTMLEditor->IsEditable(curBlock)) {
        // Not a block kind that we care about.
        curBlock = nullptr;
      } else {
        firstNode = lastNode = curNode->AsContent();
      }
    } else if (curBlock) {
      // Some node that is already sans block style.  Skip over it and process
      // any partial progress saved.
      nsresult rv = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
      NS_ENSURE_SUCCESS(rv, rv);
      firstNode = lastNode = curBlock = nullptr;
    }
  }
  // Process any partial progress saved
  if (curBlock) {
    nsresult rv = RemovePartOfBlock(*curBlock, *firstNode, *lastNode);
    NS_ENSURE_SUCCESS(rv, rv);
    firstNode = lastNode = curBlock = nullptr;
  }
  return NS_OK;
}

/**
 * ApplyBlockStyle() does whatever it takes to make the list of nodes into one
 * or more blocks of type aBlockTag.
 */
nsresult
HTMLEditRules::ApplyBlockStyle(nsTArray<OwningNonNull<nsINode>>& aNodeArray,
                               nsAtom& aBlockTag)
{
  // Intent of this routine is to be used for converting to/from headers,
  // paragraphs, pre, and address.  Those blocks that pretty much just contain
  // inline things...
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  nsCOMPtr<Element> newBlock;

  nsCOMPtr<Element> curBlock;
  for (auto& curNode : aNodeArray) {
    EditorDOMPoint atCurNode(curNode);

    // Is it already the right kind of block, or an uneditable block?
    if (curNode->IsHTMLElement(&aBlockTag) ||
        (!mHTMLEditor->IsEditable(curNode) && IsBlockNode(curNode))) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      // Do nothing to this block
      continue;
    }

    // If curNode is a address, p, header, address, or pre, replace it with a
    // new block of correct type.
    // XXX: pre can't hold everything the others can
    if (HTMLEditUtils::IsMozDiv(curNode) ||
        HTMLEditUtils::IsFormatNode(curNode)) {
      // Forget any previous block used for previous inline nodes
      curBlock = nullptr;
      newBlock =
        htmlEditor->ReplaceContainerAndCloneAttributesWithTransaction(
                      *curNode->AsElement(), aBlockTag);
      NS_ENSURE_STATE(newBlock);
      continue;
    }

    if (HTMLEditUtils::IsTable(curNode) ||
        HTMLEditUtils::IsList(curNode) ||
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
      if (!childArray.IsEmpty()) {
        nsresult rv = ApplyBlockStyle(childArray, aBlockTag);
        NS_ENSURE_SUCCESS(rv, rv);
        continue;
      }

      // Make sure we can put a block here
      SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(aBlockTag, atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      RefPtr<Element> theBlock =
        htmlEditor->CreateNodeWithTransaction(aBlockTag,
                                              splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!theBlock)) {
        return NS_ERROR_FAILURE;
      }
      // Remember our new block for postprocessing
      mNewBlock = theBlock;
      continue;
    }

    if (curNode->IsHTMLElement(nsGkAtoms::br)) {
      // If the node is a break, we honor it by putting further nodes in a new
      // parent
      if (curBlock) {
        // Forget any previous block used for previous inline nodes
        curBlock = nullptr;
        nsresult rv = htmlEditor->DeleteNodeWithTransaction(*curNode);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // The break is the first (or even only) node we encountered.  Create a
      // block for it.
      SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(aBlockTag, atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curBlock =
        htmlEditor->CreateNodeWithTransaction(aBlockTag,
                                              splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!curBlock)) {
        return NS_ERROR_FAILURE;
      }
      // Remember our new block for postprocessing
      mNewBlock = curBlock;
      // Note: doesn't matter if we set mNewBlock multiple times.
      nsresult rv =
        htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                 *curBlock);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    if (IsInlineNode(curNode)) {
      // If curNode is inline, pull it into curBlock.  Note: it's assumed that
      // consecutive inline nodes in aNodeArray are actually members of the
      // same block parent.  This happens to be true now as a side effect of
      // how aNodeArray is contructed, but some additional logic should be
      // added here if that should change
      //
      // If curNode is a non editable, drop it if we are going to <pre>.
      if (&aBlockTag == nsGkAtoms::pre && !htmlEditor->IsEditable(curNode)) {
        // Do nothing to this block
        continue;
      }

      // If no curBlock, make one
      if (!curBlock) {
        AutoEditorDOMPointOffsetInvalidator lockChild(atCurNode);

        SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(aBlockTag, atCurNode);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        curBlock =
          htmlEditor->CreateNodeWithTransaction(aBlockTag,
                                                splitNodeResult.SplitPoint());
        if (NS_WARN_IF(!curBlock)) {
          return NS_ERROR_FAILURE;
        }
        // Remember our new block for postprocessing
        mNewBlock = curBlock;
        // Note: doesn't matter if we set mNewBlock multiple times.
      }

      if (NS_WARN_IF(!atCurNode.IsSet())) {
        // This is possible due to mutation events, let's not assert
        return NS_ERROR_UNEXPECTED;
      }

      // XXX If curNode is a br, replace it with a return if going to <pre>

      // This is a continuation of some inline nodes that belong together in
      // the same block item.  Use curBlock.
      nsresult rv =
        htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                 *curBlock);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }
  return NS_OK;
}

template<typename PT, typename CT>
SplitNodeResult
HTMLEditRules::MaybeSplitAncestorsForInsertWithTransaction(
                 nsAtom& aTag,
                 const EditorDOMPointBase<PT, CT>& aStartOfDeepestRightNode)
{
  if (NS_WARN_IF(!aStartOfDeepestRightNode.IsSet())) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }
  MOZ_ASSERT(aStartOfDeepestRightNode.IsSetAndValid());

  if (NS_WARN_IF(!mHTMLEditor)) {
    return SplitNodeResult(NS_ERROR_NOT_AVAILABLE);
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  RefPtr<Element> host = htmlEditor->GetActiveEditingHost();
  if (NS_WARN_IF(!host)) {
    return SplitNodeResult(NS_ERROR_FAILURE);
  }

  // The point must be descendant of editing host.
  if (NS_WARN_IF(aStartOfDeepestRightNode.GetContainer() != host &&
                 !EditorUtils::IsDescendantOf(
                   *aStartOfDeepestRightNode.GetContainer(), *host))) {
    return SplitNodeResult(NS_ERROR_INVALID_ARG);
  }

  // Look for a node that can legally contain the tag.
  EditorRawDOMPoint pointToInsert(aStartOfDeepestRightNode);
  for (; pointToInsert.IsSet();
       pointToInsert.Set(pointToInsert.GetContainer())) {
    // We cannot split active editing host and its ancestor.  So, there is
    // no element to contain the specified element.
    if (NS_WARN_IF(pointToInsert.GetChild() == host)) {
      return SplitNodeResult(NS_ERROR_FAILURE);
    }

    if (htmlEditor->CanContainTag(*pointToInsert.GetContainer(), aTag)) {
      // Found an ancestor node which can contain the element.
      break;
    }
  }

  MOZ_DIAGNOSTIC_ASSERT(pointToInsert.IsSet());

  // If the point itself can contain the tag, we don't need to split any
  // ancestor nodes.  In this case, we should return the given split point
  // as is.
  if (pointToInsert.GetContainer() == aStartOfDeepestRightNode.GetContainer()) {
    return SplitNodeResult(aStartOfDeepestRightNode);
  }

  SplitNodeResult splitNodeResult =
    htmlEditor->SplitNodeDeepWithTransaction(
                  *pointToInsert.GetChild(),
                  aStartOfDeepestRightNode,
                  SplitAtEdges::eAllowToCreateEmptyContainer);
  NS_WARNING_ASSERTION(splitNodeResult.Succeeded(),
    "Failed to split the node for insert the element");
  return splitNodeResult;
}

EditorDOMPoint
HTMLEditRules::JoinNearestEditableNodesWithTransaction(nsIContent& aNodeLeft,
                                                       nsIContent& aNodeRight)
{
  // Caller responsible for left and right node being the same type
  nsCOMPtr<nsINode> parent = aNodeLeft.GetParentNode();
  if (NS_WARN_IF(!parent)) {
    return EditorDOMPoint();
  }
  int32_t parOffset = parent->ComputeIndexOf(&aNodeLeft);
  nsCOMPtr<nsINode> rightParent = aNodeRight.GetParentNode();

  // If they don't have the same parent, first move the right node to after the
  // left one
  if (parent != rightParent) {
    if (NS_WARN_IF(!mHTMLEditor)) {
      return EditorDOMPoint();
    }
    nsresult rv =
      mHTMLEditor->MoveNodeWithTransaction(aNodeRight,
                                           EditorRawDOMPoint(parent,
                                                             parOffset));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorDOMPoint();
    }
  }

  EditorDOMPoint ret(&aNodeRight, aNodeLeft.Length());

  // Separate join rules for differing blocks
  if (HTMLEditUtils::IsList(&aNodeLeft) || aNodeLeft.GetAsText()) {
    // For lists, merge shallow (wouldn't want to combine list items)
    nsresult rv = mHTMLEditor->JoinNodesWithTransaction(aNodeLeft, aNodeRight);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorDOMPoint();
    }
    return ret;
  }

  // Remember the last left child, and first right child
  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditorDOMPoint();
  }
  nsCOMPtr<nsIContent> lastLeft = mHTMLEditor->GetLastEditableChild(aNodeLeft);
  if (NS_WARN_IF(!lastLeft)) {
    return EditorDOMPoint();
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditorDOMPoint();
  }
  nsCOMPtr<nsIContent> firstRight = mHTMLEditor->GetFirstEditableChild(aNodeRight);
  if (NS_WARN_IF(!firstRight)) {
    return EditorDOMPoint();
  }

  // For list items, divs, etc., merge smart
  if (NS_WARN_IF(!mHTMLEditor)) {
    return EditorDOMPoint();
  }
  nsresult rv = mHTMLEditor->JoinNodesWithTransaction(aNodeLeft, aNodeRight);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorDOMPoint();
  }

  if (lastLeft && firstRight && mHTMLEditor &&
      mHTMLEditor->AreNodesSameType(lastLeft, firstRight) &&
      (lastLeft->GetAsText() || !mHTMLEditor ||
       (lastLeft->IsElement() && firstRight->IsElement() &&
        CSSEditUtils::ElementsSameStyle(lastLeft->AsElement(),
                                        firstRight->AsElement())))) {
    if (NS_WARN_IF(!mHTMLEditor)) {
      return EditorDOMPoint();
    }
    return JoinNearestEditableNodesWithTransaction(*lastLeft, *firstRight);
  }
  return ret;
}

Element*
HTMLEditRules::GetTopEnclosingMailCite(nsINode& aNode)
{
  nsCOMPtr<Element> ret;

  for (nsCOMPtr<nsINode> node = &aNode; node; node = node->GetParentNode()) {
    if ((IsPlaintextEditor() && node->IsHTMLElement(nsGkAtoms::pre)) ||
        HTMLEditUtils::IsMailCite(node)) {
      ret = node->AsElement();
    }
    if (node->IsHTMLElement(nsGkAtoms::body)) {
      break;
    }
  }

  return ret;
}

nsresult
HTMLEditRules::CacheInlineStyles(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  NS_ENSURE_STATE(mHTMLEditor);

  nsresult rv = GetInlineStyles(aNode, mCachedStyles);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
HTMLEditRules::GetInlineStyles(nsINode* aNode,
                               StyleCache aStyleCache[SIZE_STYLE_TABLE])
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(mHTMLEditor);

  bool useCSS = mHTMLEditor->IsCSSEnabled();

  for (size_t j = 0; j < SIZE_STYLE_TABLE; ++j) {
    // If type-in state is set, don't intervene
    bool typeInSet, unused;
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_UNEXPECTED;
    }
    mHTMLEditor->mTypeInState->GetTypingState(typeInSet, unused,
      aStyleCache[j].tag, aStyleCache[j].attr, nullptr);
    if (typeInSet) {
      continue;
    }

    bool isSet = false;
    nsAutoString outValue;
    // Don't use CSS for <font size>, we don't support it usefully (bug 780035)
    if (!useCSS || (aStyleCache[j].tag == nsGkAtoms::font &&
                    aStyleCache[j].attr == nsGkAtoms::size)) {
      NS_ENSURE_STATE(mHTMLEditor);
      isSet = mHTMLEditor->IsTextPropertySetByContent(aNode, aStyleCache[j].tag,
                                                      aStyleCache[j].attr,
                                                      nullptr,
                                                      &outValue);
    } else {
      isSet = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
                aNode, aStyleCache[j].tag, aStyleCache[j].attr, outValue,
                CSSEditUtils::eComputed);
    }
    if (isSet) {
      aStyleCache[j].mPresent = true;
      aStyleCache[j].value.Assign(outValue);
    }
  }
  return NS_OK;
}

nsresult
HTMLEditRules::ReapplyCachedStyles()
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
    do_QueryInterface(selection->GetRangeAt(0)->GetStartContainer());
  if (!selNode) {
    // Nothing to do
    return NS_OK;
  }

  StyleCache styleAtInsertionPoint[SIZE_STYLE_TABLE];
  InitStyleCacheArray(styleAtInsertionPoint);
  nsresult rv = GetInlineStyles(selNode, styleAtInsertionPoint);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;
  }

  for (size_t i = 0; i < SIZE_STYLE_TABLE; ++i) {
    if (mCachedStyles[i].mPresent) {
      bool bFirst, bAny, bAll;
      bFirst = bAny = bAll = false;

      nsAutoString curValue;
      if (useCSS) {
        // check computed style first in css case
        bAny = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
                 selNode, mCachedStyles[i].tag, mCachedStyles[i].attr, curValue,
                 CSSEditUtils::eComputed);
      }
      if (!bAny) {
        // then check typeinstate and html style
        NS_ENSURE_STATE(mHTMLEditor);
        nsresult rv =
          mHTMLEditor->GetInlinePropertyBase(*mCachedStyles[i].tag,
                                             mCachedStyles[i].attr,
                                             &(mCachedStyles[i].value),
                                             &bFirst, &bAny, &bAll,
                                             &curValue);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // This style has disappeared through deletion.  Let's add the styles to
      // mTypeInState when same style isn't applied to the node already.
      if ((!bAny || IsStyleCachePreservingAction(mTheAction)) &&
           (!styleAtInsertionPoint[i].mPresent ||
            styleAtInsertionPoint[i].value != mCachedStyles[i].value)) {
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
HTMLEditRules::ClearCachedStyles()
{
  // clear the mPresent bits in mCachedStyles array
  for (size_t j = 0; j < SIZE_STYLE_TABLE; j++) {
    mCachedStyles[j].mPresent = false;
    mCachedStyles[j].value.Truncate();
  }
}

void
HTMLEditRules::AdjustSpecialBreaks()
{
  NS_ENSURE_TRUE_VOID(mHTMLEditor);

  // Gather list of empty nodes
  nsTArray<OwningNonNull<nsINode>> nodeArray;
  EmptyEditableFunctor functor(mHTMLEditor);
  DOMIterator iter;
  if (NS_WARN_IF(NS_FAILED(iter.Init(*mDocChangeRange)))) {
    return;
  }
  iter.AppendList(functor, nodeArray);

  // Put moz-br's into these empty li's and td's
  for (auto& node : nodeArray) {
    // Need to put br at END of node.  It may have empty containers in it and
    // still pass the "IsEmptyNode" test, and we want the br's to be after
    // them.  Also, we want the br to be after the selection if the selection
    // is in this node.
    EditorRawDOMPoint endOfNode;
    endOfNode.SetToEndOf(node);
    RefPtr<Element> brElement = CreateMozBR(endOfNode);
    if (NS_WARN_IF(!brElement)) {
      return;
    }
  }
}

nsresult
HTMLEditRules::AdjustWhitespace(Selection* aSelection)
{
  EditorRawDOMPoint selectionStartPoint(EditorBase::GetStartPoint(aSelection));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Ask whitespace object to tweak nbsp's
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return WSRunObject(mHTMLEditor, selectionStartPoint).AdjustWhitespace();
}

nsresult
HTMLEditRules::PinSelectionToNewBlock(Selection* aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  if (!aSelection->IsCollapsed()) {
    return NS_OK;
  }

  if (NS_WARN_IF(!mNewBlock)) {
    return NS_ERROR_NULL_POINTER;
  }

  EditorRawDOMPoint selectionStartPoint(EditorBase::GetStartPoint(aSelection));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Use ranges and nsRange::CompareNodeToRange() to compare selection start
  // to new block.
  // XXX It's too expensive to use nsRange and set it only for comparing a
  //     DOM point with a node.
  RefPtr<nsRange> range = new nsRange(selectionStartPoint.GetContainer());
  nsresult rv = range->CollapseTo(selectionStartPoint);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool nodeBefore, nodeAfter;
  rv = nsRange::CompareNodeToRange(mNewBlock, range, &nodeBefore, &nodeAfter);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nodeBefore && nodeAfter) {
    return NS_OK;  // selection is inside block
  }

  if (nodeBefore) {
    // selection is after block.  put at end of block.
    NS_ENSURE_STATE(mHTMLEditor);
    nsCOMPtr<nsINode> tmp = mHTMLEditor->GetLastEditableChild(*mNewBlock);
    if (!tmp) {
      tmp = mNewBlock;
    }
    EditorRawDOMPoint endPoint;
    if (EditorBase::IsTextNode(tmp) ||
        mHTMLEditor->IsContainer(tmp)) {
      endPoint.SetToEndOf(tmp);
    } else {
      endPoint.Set(tmp);
      if (NS_WARN_IF(!endPoint.AdvanceOffset())) {
        return NS_ERROR_FAILURE;
      }
    }
    return aSelection->Collapse(endPoint);
  }

  // selection is before block.  put at start of block.
  NS_ENSURE_STATE(mHTMLEditor);
  nsCOMPtr<nsINode> tmp = mHTMLEditor->GetFirstEditableChild(*mNewBlock);
  if (!tmp) {
    tmp = mNewBlock;
  }
  EditorRawDOMPoint atStartOfBlock;
  if (EditorBase::IsTextNode(tmp) ||
      mHTMLEditor->IsContainer(tmp)) {
    atStartOfBlock.Set(tmp);
  } else {
    atStartOfBlock.Set(tmp, 0);
  }
  return aSelection->Collapse(atStartOfBlock);
}

void
HTMLEditRules::CheckInterlinePosition(Selection& aSelection)
{
  // If the selection isn't collapsed, do nothing.
  if (!aSelection.IsCollapsed()) {
    return;
  }

  NS_ENSURE_TRUE_VOID(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // Get the (collapsed) selection location
  nsRange* firstRange = aSelection.GetRangeAt(0);
  if (NS_WARN_IF(!firstRange)) {
    return;
  }

  EditorDOMPoint atStartOfSelection(firstRange->StartRef());
  if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
    return;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValid());

  // First, let's check to see if we are after a <br>.  We take care of this
  // special-case first so that we don't accidentally fall through into one of
  // the other conditionals.
  nsCOMPtr<nsIContent> node =
    htmlEditor->GetPreviousEditableHTMLNodeInBlock(atStartOfSelection);
  if (node && node->IsHTMLElement(nsGkAtoms::br)) {
    aSelection.SetInterlinePosition(true, IgnoreErrors());
    return;
  }

  // Are we after a block?  If so try set caret to following content
  if (atStartOfSelection.GetChild()) {
    node = htmlEditor->GetPriorHTMLSibling(atStartOfSelection.GetChild());
  } else {
    node = nullptr;
  }
  if (node && IsBlockNode(*node)) {
    aSelection.SetInterlinePosition(true, IgnoreErrors());
    return;
  }

  // Are we before a block?  If so try set caret to prior content
  if (atStartOfSelection.GetChild()) {
    node = htmlEditor->GetNextHTMLSibling(atStartOfSelection.GetChild());
  } else {
    node = nullptr;
  }
  if (node && IsBlockNode(*node)) {
    aSelection.SetInterlinePosition(false, IgnoreErrors());
  }
}

nsresult
HTMLEditRules::AdjustSelection(Selection* aSelection,
                               nsIEditor::EDirection aAction)
{
  if (NS_WARN_IF(!aSelection)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  // if the selection isn't collapsed, do nothing.
  // moose: one thing to do instead is check for the case of
  // only a single break selected, and collapse it.  Good thing?  Beats me.
  if (!aSelection->IsCollapsed()) {
    return NS_OK;
  }

  // get the (collapsed) selection location
  EditorDOMPoint point(EditorBase::GetStartPoint(aSelection));
  if (NS_WARN_IF(!point.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // are we in an editable node?
  while (!htmlEditor->IsEditable(point.GetContainer())) {
    // scan up the tree until we find an editable place to be
    point.Set(point.GetContainer());
    if (NS_WARN_IF(!point.IsSet())) {
      return NS_ERROR_FAILURE;
    }
  }

  // make sure we aren't in an empty block - user will see no cursor.  If this
  // is happening, put a <br> in the block if allowed.
  RefPtr<Element> theblock = htmlEditor->GetBlock(*point.GetContainer());

  if (theblock && htmlEditor->IsEditable(theblock)) {
    bool bIsEmptyNode;
    nsresult rv =
      htmlEditor->IsEmptyNode(theblock, &bIsEmptyNode, false, false);
    NS_ENSURE_SUCCESS(rv, rv);
    // check if br can go into the destination node
    if (bIsEmptyNode &&
        htmlEditor->CanContainTag(*point.GetContainer(), *nsGkAtoms::br)) {
      Element* rootElement = htmlEditor->GetRoot();
      if (NS_WARN_IF(!rootElement)) {
        return NS_ERROR_FAILURE;
      }
      if (point.GetContainer() == rootElement) {
        // Our root node is completely empty. Don't add a <br> here.
        // AfterEditInner() will add one for us when it calls
        // CreateBogusNodeIfNeeded()!
        return NS_OK;
      }

      // we know we can skip the rest of this routine given the cirumstance
      RefPtr<Element> brElement = CreateMozBR(point);
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }
      return NS_OK;
    }
  }

  // are we in a text node?
  if (point.IsInTextNode()) {
    return NS_OK; // we LIKE it when we are in a text node.  that RULZ
  }

  // do we need to insert a special mozBR?  We do if we are:
  // 1) prior node is in same block where selection is AND
  // 2) prior node is a br AND
  // 3) that br is not visible

  nsCOMPtr<nsIContent> nearNode =
    htmlEditor->GetPreviousEditableHTMLNode(point);
  if (nearNode) {
    // is nearNode also a descendant of same block?
    RefPtr<Element> block = htmlEditor->GetBlock(*point.GetContainer());
    RefPtr<Element> nearBlock = htmlEditor->GetBlockNodeParent(nearNode);
    if (block && block == nearBlock) {
      if (nearNode && TextEditUtils::IsBreak(nearNode)) {
        if (!htmlEditor->IsVisibleBRElement(nearNode)) {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  Also, things
          // like table cells won't grow in height.
          RefPtr<Element> brElement = CreateMozBR(point);
          if (NS_WARN_IF(!brElement)) {
            return NS_ERROR_FAILURE;
          }
          point.Set(brElement);
          // selection stays *before* moz-br, sticking to it
          aSelection->SetInterlinePosition(true, IgnoreErrors());
          ErrorResult error;
          aSelection->Collapse(point, error);
          if (NS_WARN_IF(error.Failed())) {
            return error.StealNSResult();
          }
        } else {
          nsCOMPtr<nsIContent> nextNode =
            htmlEditor->GetNextEditableHTMLNodeInBlock(*nearNode);
          if (nextNode && TextEditUtils::IsMozBR(nextNode)) {
            // selection between br and mozbr.  make it stick to mozbr
            // so that it will be on blank line.
            aSelection->SetInterlinePosition(true, IgnoreErrors());
          }
        }
      }
    }
  }

  // we aren't in a textnode: are we adjacent to text or a break or an image?
  nearNode = htmlEditor->GetPreviousEditableHTMLNodeInBlock(point);
  if (nearNode && (TextEditUtils::IsBreak(nearNode) ||
                   EditorBase::IsTextNode(nearNode) ||
                   HTMLEditUtils::IsImage(nearNode) ||
                   nearNode->IsHTMLElement(nsGkAtoms::hr))) {
    // this is a good place for the caret to be
    return NS_OK;
  }
  nearNode = htmlEditor->GetNextEditableHTMLNodeInBlock(point);
  if (nearNode && (TextEditUtils::IsBreak(nearNode) ||
                   EditorBase::IsTextNode(nearNode) ||
                   nearNode->IsAnyOfHTMLElements(nsGkAtoms::img,
                                                 nsGkAtoms::hr))) {
    return NS_OK; // this is a good place for the caret to be
  }

  // look for a nearby text node.
  // prefer the correct direction.
  nearNode = FindNearEditableNode(point, aAction);
  if (!nearNode) {
    return NS_OK;
  }

  EditorDOMPoint pt = GetGoodSelPointForNode(*nearNode, aAction);
  ErrorResult error;
  aSelection->Collapse(pt, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

template<typename PT, typename CT>
nsIContent*
HTMLEditRules::FindNearEditableNode(const EditorDOMPointBase<PT, CT>& aPoint,
                                    nsIEditor::EDirection aDirection)
{
  if (NS_WARN_IF(!aPoint.IsSet()) ||
      NS_WARN_IF(!mHTMLEditor)) {
    return nullptr;
  }
  MOZ_ASSERT(aPoint.IsSetAndValid());

  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  nsIContent* nearNode = nullptr;
  if (aDirection == nsIEditor::ePrevious) {
    nearNode = htmlEditor->GetPreviousEditableHTMLNode(aPoint);
    if (!nearNode) {
      return nullptr; // Not illegal.
    }
  } else {
    nearNode = htmlEditor->GetNextEditableHTMLNode(aPoint);
    if (NS_WARN_IF(!nearNode)) {
      // Perhaps, illegal because the node pointed by aPoint isn't editable
      // and nobody of previous nodes is editable.
      return nullptr;
    }
  }

  // scan in the right direction until we find an eligible text node,
  // but don't cross any breaks, images, or table elements.
  // XXX This comment sounds odd.  |nearNode| may have already crossed breaks
  //     and/or images.
  while (nearNode && !(EditorBase::IsTextNode(nearNode) ||
                       TextEditUtils::IsBreak(nearNode) ||
                       HTMLEditUtils::IsImage(nearNode))) {
    if (aDirection == nsIEditor::ePrevious) {
      nearNode = htmlEditor->GetPreviousEditableHTMLNode(*nearNode);
      if (NS_WARN_IF(!nearNode)) {
        return nullptr;
      }
    } else {
      nearNode = htmlEditor->GetNextEditableHTMLNode(*nearNode);
      if (NS_WARN_IF(!nearNode)) {
        return nullptr;
      }
    }
  }

  // don't cross any table elements
  if (InDifferentTableElements(nearNode, aPoint.GetContainer())) {
    return nullptr;
  }

  // otherwise, ok, we have found a good spot to put the selection
  return nearNode;
}

bool
HTMLEditRules::InDifferentTableElements(nsINode* aNode1,
                                        nsINode* aNode2)
{
  MOZ_ASSERT(aNode1 && aNode2);

  while (aNode1 && !HTMLEditUtils::IsTableElement(aNode1)) {
    aNode1 = aNode1->GetParentNode();
  }

  while (aNode2 && !HTMLEditUtils::IsTableElement(aNode2)) {
    aNode2 = aNode2->GetParentNode();
  }

  return aNode1 != aNode2;
}


nsresult
HTMLEditRules::RemoveEmptyNodes()
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

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

  nsresult rv = iter->Init(mDocChangeRange);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<OwningNonNull<nsINode>> arrayOfEmptyNodes, arrayOfEmptyCites, skipList;

  // Check for empty nodes
  while (!iter->IsDone()) {
    OwningNonNull<nsINode> node = *iter->GetCurrentNode();

    nsCOMPtr<nsINode> parent = node->GetParentNode();

    size_t idx = skipList.IndexOf(node);
    if (idx != skipList.NoIndex) {
      // This node is on our skip list.  Skip processing for this node, and
      // replace its value in the skip list with the value of its parent
      if (parent) {
        skipList[idx] = parent;
      }
    } else {
      bool bIsCandidate = false;
      bool bIsEmptyNode = false;
      bool bIsMailCite = false;

      if (node->IsElement()) {
        if (node->IsHTMLElement(nsGkAtoms::body)) {
          // Don't delete the body
        } else if ((bIsMailCite = HTMLEditUtils::IsMailCite(node)) ||
                   node->IsHTMLElement(nsGkAtoms::a) ||
                   HTMLEditUtils::IsInlineStyle(node) ||
                   HTMLEditUtils::IsList(node) ||
                   node->IsHTMLElement(nsGkAtoms::div)) {
          // Only consider certain nodes to be empty for purposes of removal
          bIsCandidate = true;
        } else if (HTMLEditUtils::IsFormatNode(node) ||
                   HTMLEditUtils::IsListItem(node) ||
                   node->IsHTMLElement(nsGkAtoms::blockquote)) {
          // These node types are candidates if selection is not in them.  If
          // it is one of these, don't delete if selection inside.  This is so
          // we can create empty headings, etc., for the user to type into.
          bool bIsSelInNode;
          rv = SelectionEndpointInNode(node, &bIsSelInNode);
          NS_ENSURE_SUCCESS(rv, rv);
          if (!bIsSelInNode) {
            bIsCandidate = true;
          }
        }
      }

      if (bIsCandidate) {
        // We delete mailcites even if they have a solo br in them.  Other
        // nodes we require to be empty.
        rv = htmlEditor->IsEmptyNode(node, &bIsEmptyNode, bIsMailCite, true);
        NS_ENSURE_SUCCESS(rv, rv);
        if (bIsEmptyNode) {
          if (bIsMailCite) {
            // mailcites go on a separate list from other empty nodes
            arrayOfEmptyCites.AppendElement(*node);
          } else {
            arrayOfEmptyNodes.AppendElement(*node);
          }
        }
      }

      if (!bIsEmptyNode && parent) {
        // put parent on skip list
        skipList.AppendElement(*parent);
      }
    }

    iter->Next();
  }

  // now delete the empty nodes
  for (OwningNonNull<nsINode>& delNode : arrayOfEmptyNodes) {
    if (htmlEditor->IsModifiableNode(delNode)) {
      rv = htmlEditor->DeleteNodeWithTransaction(*delNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // Now delete the empty mailcites.  This is a separate step because we want
  // to pull out any br's and preserve them.
  for (OwningNonNull<nsINode>& delNode : arrayOfEmptyCites) {
    bool bIsEmptyNode;
    rv = htmlEditor->IsEmptyNode(delNode, &bIsEmptyNode, false, true);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!bIsEmptyNode) {
      RefPtr<Selection> selection = htmlEditor->GetSelection();
      if (NS_WARN_IF(!selection)) {
        return NS_ERROR_FAILURE;
      }
      // We are deleting a cite that has just a br.  We want to delete cite,
      // but preserve br.
      RefPtr<Element> brElement =
        htmlEditor->InsertBrElementWithTransaction(*selection,
                                                   EditorRawDOMPoint(delNode));
      if (NS_WARN_IF(!brElement)) {
        return NS_ERROR_FAILURE;
      }
    }
    rv = htmlEditor->DeleteNodeWithTransaction(*delNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
HTMLEditRules::SelectionEndpointInNode(nsINode* aNode,
                                       bool* aResult)
{
  NS_ENSURE_TRUE(aNode && aResult, NS_ERROR_NULL_POINTER);

  *aResult = false;

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<Selection> selection = mHTMLEditor->GetSelection();
  NS_ENSURE_STATE(selection);

  uint32_t rangeCount = selection->RangeCount();
  for (uint32_t rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx) {
    RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
    nsINode* startContainer = range->GetStartContainer();
    if (startContainer) {
      if (aNode == startContainer) {
        *aResult = true;
        return NS_OK;
      }
      if (EditorUtils::IsDescendantOf(*startContainer, *aNode)) {
        *aResult = true;
        return NS_OK;
      }
    }
    nsINode* endContainer = range->GetEndContainer();
    if (startContainer == endContainer) {
      continue;
    }
    if (endContainer) {
      if (aNode == endContainer) {
        *aResult = true;
        return NS_OK;
      }
      if (EditorUtils::IsDescendantOf(*endContainer, *aNode)) {
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
HTMLEditRules::IsEmptyInline(nsINode& aNode)
{
  NS_ENSURE_TRUE(mHTMLEditor, false);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  if (IsInlineNode(aNode) && htmlEditor->IsContainer(&aNode)) {
    bool isEmpty = true;
    htmlEditor->IsEmptyNode(&aNode, &isEmpty);
    return isEmpty;
  }
  return false;
}


bool
HTMLEditRules::ListIsEmptyLine(nsTArray<OwningNonNull<nsINode>>& aArrayOfNodes)
{
  // We have a list of nodes which we are candidates for being moved into a new
  // block.  Determine if it's anything more than a blank line.  Look for
  // editable content above and beyond one single BR.
  NS_ENSURE_TRUE(aArrayOfNodes.Length(), true);

  NS_ENSURE_TRUE(mHTMLEditor, false);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  int32_t brCount = 0;

  for (auto& node : aArrayOfNodes) {
    if (!htmlEditor->IsEditable(node)) {
      continue;
    }
    if (TextEditUtils::IsBreak(node)) {
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
HTMLEditRules::PopListItem(nsIContent& aListItem,
                           bool* aOutOfList)
{
  // init out params
  if (aOutOfList) {
    *aOutOfList = false;
  }

  nsCOMPtr<nsIContent> kungFuDeathGrip(&aListItem);
  Unused << kungFuDeathGrip;


  if (NS_WARN_IF(!mHTMLEditor) ||
      NS_WARN_IF(!aListItem.GetParent()) ||
      NS_WARN_IF(!aListItem.GetParent()->GetParentNode()) ||
      !HTMLEditUtils::IsListItem(&aListItem)) {
    return NS_ERROR_FAILURE;
  }

  // if it's first or last list item, don't need to split the list
  // otherwise we do.
  bool bIsFirstListItem = mHTMLEditor->IsFirstEditableChild(&aListItem);
  MOZ_ASSERT(mHTMLEditor);
  bool bIsLastListItem = mHTMLEditor->IsLastEditableChild(&aListItem);
  MOZ_ASSERT(mHTMLEditor);

  nsCOMPtr<nsIContent> leftListNode = aListItem.GetParent();
  if (!bIsFirstListItem && !bIsLastListItem) {
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atListItem(&aListItem);
    if (NS_WARN_IF(!atListItem.IsSet())) {
      return NS_ERROR_INVALID_ARG;
    }
    MOZ_ASSERT(atListItem.IsSetAndValid());

    // split the list
    ErrorResult error;
    leftListNode = mHTMLEditor->SplitNodeWithTransaction(atListItem, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    if (NS_WARN_IF(!mHTMLEditor)) {
      return NS_ERROR_FAILURE;
    }
  }

  // In most cases, insert the list item into the new left list node..
  EditorDOMPoint pointToInsertListItem(leftListNode);
  if (NS_WARN_IF(!pointToInsertListItem.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(pointToInsertListItem.IsSetAndValid());

  // But when the list item was the first child of the right list, it should
  // be inserted into the next sibling of the list.  This allows user to hit
  // Enter twice at a list item breaks the parent list node.
  if (!bIsFirstListItem) {
    DebugOnly<bool> advanced = pointToInsertListItem.AdvanceOffset();
    NS_WARNING_ASSERTION(advanced,
      "Failed to advance offset to right list node");
  }

  nsresult rv =
    mHTMLEditor->MoveNodeWithTransaction(aListItem, pointToInsertListItem);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // unwrap list item contents if they are no longer in a list
  if (!HTMLEditUtils::IsList(pointToInsertListItem.GetContainer()) &&
      HTMLEditUtils::IsListItem(&aListItem)) {
    NS_ENSURE_STATE(mHTMLEditor);
    rv = mHTMLEditor->RemoveBlockContainerWithTransaction(
                        *aListItem.AsElement());
    NS_ENSURE_SUCCESS(rv, rv);
    if (aOutOfList) {
      *aOutOfList = true;
    }
  }
  return NS_OK;
}

nsresult
HTMLEditRules::RemoveListStructure(Element& aList)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
  while (aList.GetFirstChild()) {
    OwningNonNull<nsIContent> child = *aList.GetFirstChild();

    if (HTMLEditUtils::IsListItem(child)) {
      bool isOutOfList;
      // Keep popping it out until it's not in a list anymore
      do {
        nsresult rv = PopListItem(child, &isOutOfList);
        NS_ENSURE_SUCCESS(rv, rv);
      } while (!isOutOfList);
    } else if (HTMLEditUtils::IsList(child)) {
      nsresult rv = RemoveListStructure(*child->AsElement());
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // Delete any non-list items for now
      nsresult rv = htmlEditor->DeleteNodeWithTransaction(*child);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // Delete the now-empty list
  nsresult rv = htmlEditor->RemoveBlockContainerWithTransaction(aList);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// XXX This method is not necessary because even if selection is outside the
//     <body> element, the element can be editable.
nsresult
HTMLEditRules::ConfirmSelectionInBody()
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Element* rootElement = mHTMLEditor->GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_UNEXPECTED;
  }

  Selection* selection = mHTMLEditor->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_UNEXPECTED;
  }

  EditorRawDOMPoint selectionStartPoint(EditorBase::GetStartPoint(selection));
  if (NS_WARN_IF(!selectionStartPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Check that selection start container is inside the <body> element.
  //XXXsmaug this code is insane.
  nsINode* temp = selectionStartPoint.GetContainer();
  while (temp && !temp->IsHTMLElement(nsGkAtoms::body)) {
    temp = temp->GetParentOrHostNode();
  }

  // If we aren't in the <body> element, force the issue.
  if (!temp) {
    selection->Collapse(rootElement, 0);
    return NS_OK;
  }

  EditorRawDOMPoint selectionEndPoint(EditorBase::GetEndPoint(selection));
  if (NS_WARN_IF(!selectionEndPoint.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // check that selNode is inside body
  //XXXsmaug this code is insane.
  temp = selectionEndPoint.GetContainer();
  while (temp && !temp->IsHTMLElement(nsGkAtoms::body)) {
    temp = temp->GetParentOrHostNode();
  }

  // If we aren't in the <body> element, force the issue.
  if (!temp) {
    selection->Collapse(rootElement, 0);
  }

  return NS_OK;
}

nsresult
HTMLEditRules::UpdateDocChangeRange(nsRange* aRange)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // first make sure aRange is in the document.  It might not be if
  // portions of our editting action involved manipulating nodes
  // prior to placing them in the document (e.g., populating a list item
  // before placing it in its list)
  const RangeBoundary& atStart = aRange->StartRef();
  if (NS_WARN_IF(!atStart.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  if (!mHTMLEditor->IsDescendantOfRoot(atStart.Container())) {
    // just return - we don't need to adjust mDocChangeRange in this case
    return NS_OK;
  }

  if (!mDocChangeRange) {
    // clone aRange.
    mDocChangeRange = aRange->CloneRange();
  } else {
    // compare starts of ranges
    ErrorResult error;
    int16_t result =
      mDocChangeRange->CompareBoundaryPoints(RangeBinding::START_TO_START,
                                             *aRange, error);
    if (error.ErrorCodeIs(NS_ERROR_NOT_INITIALIZED)) {
      // This will happen is mDocChangeRange is non-null, but the range is
      // uninitialized. In this case we'll set the start to aRange start.
      // The same test won't be needed further down since after we've set
      // the start the range will be collapsed to that point.
      result = 1;
      error.SuppressException();
    }
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    // Positive result means mDocChangeRange start is after aRange start.
    if (result > 0) {
      mDocChangeRange->SetStart(atStart.AsRaw(), error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    }

    // compare ends of ranges
    result =
      mDocChangeRange->CompareBoundaryPoints(RangeBinding::END_TO_END,
                                             *aRange, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    // Negative result means mDocChangeRange end is before aRange end.
    if (result < 0) {
      const RangeBoundary& atEnd = aRange->EndRef();
      if (NS_WARN_IF(!atEnd.IsSet())) {
        return NS_ERROR_FAILURE;
      }
      mDocChangeRange->SetEnd(atEnd.AsRaw(), error);
      if (NS_WARN_IF(error.Failed())) {
        return error.StealNSResult();
      }
    }
  }
  return NS_OK;
}

nsresult
HTMLEditRules::InsertBRIfNeededInternal(nsINode& aNode,
                                        bool aInsertMozBR)
{
  if (!IsBlockNode(aNode)) {
    return NS_OK;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_UNEXPECTED;
  }
  bool isEmpty;
  nsresult rv = mHTMLEditor->IsEmptyNode(&aNode, &isEmpty);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!isEmpty) {
    return NS_OK;
  }

  RefPtr<Element> brElement =
    CreateBRInternal(EditorRawDOMPoint(&aNode, 0), aInsertMozBR);
  if (NS_WARN_IF(!brElement)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
HTMLEditRules::DidCreateNode(Element* aNewElement)
{
  if (!mListenerEnabled) {
    return;
  }
  if (NS_WARN_IF(!aNewElement)) {
    return;
  }
  // assumption that Join keeps the righthand node
  IgnoredErrorResult error;
  mUtilRange->SelectNode(*aNewElement, error);
  if (NS_WARN_IF(error.Failed())) {
    return;
  }
  UpdateDocChangeRange(mUtilRange);
}

void
HTMLEditRules::DidInsertNode(nsIContent& aContent)
{
  if (!mListenerEnabled) {
    return;
  }
  IgnoredErrorResult error;
  mUtilRange->SelectNode(aContent, error);
  if (NS_WARN_IF(error.Failed())) {
    return;
  }
  UpdateDocChangeRange(mUtilRange);
}

void
HTMLEditRules::WillDeleteNode(nsINode* aChild)
{
  if (!mListenerEnabled) {
    return;
  }
  if (NS_WARN_IF(!aChild)) {
    return;
  }
  IgnoredErrorResult error;
  mUtilRange->SelectNode(*aChild, error);
  if (NS_WARN_IF(error.Failed())) {
    return;
  }
  UpdateDocChangeRange(mUtilRange);
}

void
HTMLEditRules::DidSplitNode(nsINode* aExistingRightNode,
                            nsINode* aNewLeftNode)
{
  if (!mListenerEnabled) {
    return;
  }
  nsresult rv = mUtilRange->SetStartAndEnd(aNewLeftNode, 0,
                                           aExistingRightNode, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  UpdateDocChangeRange(mUtilRange);
}

void
HTMLEditRules::WillJoinNodes(nsINode& aLeftNode,
                             nsINode& aRightNode)
{
  if (!mListenerEnabled) {
    return;
  }
  // remember split point
  mJoinOffset = aLeftNode.Length();
}

void
HTMLEditRules::DidJoinNodes(nsINode& aLeftNode,
                            nsINode& aRightNode)
{
  if (!mListenerEnabled) {
    return;
  }
  // assumption that Join keeps the righthand node
  nsresult rv = mUtilRange->CollapseTo(&aRightNode, mJoinOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  UpdateDocChangeRange(mUtilRange);
}

void
HTMLEditRules::DidInsertText(nsINode* aTextNode,
                             int32_t aOffset,
                             const nsAString& aString)
{
  if (!mListenerEnabled) {
    return;
  }
  int32_t length = aString.Length();
  nsresult rv = mUtilRange->SetStartAndEnd(aTextNode, aOffset,
                                           aTextNode, aOffset + length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  UpdateDocChangeRange(mUtilRange);
}

void
HTMLEditRules::DidDeleteText(nsINode* aTextNode,
                             int32_t aOffset,
                             int32_t aLength)
{
  if (!mListenerEnabled) {
    return;
  }
  nsresult rv = mUtilRange->CollapseTo(aTextNode, aOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  UpdateDocChangeRange(mUtilRange);
}

void
HTMLEditRules::WillDeleteSelection(Selection* aSelection)
{
  if (!mListenerEnabled) {
    return;
  }
  if (NS_WARN_IF(!aSelection)) {
    return;
  }
  EditorRawDOMPoint startPoint = EditorBase::GetStartPoint(aSelection);
  if (NS_WARN_IF(!startPoint.IsSet())) {
    return;
  }
  EditorRawDOMPoint endPoint = EditorBase::GetEndPoint(aSelection);
  if (NS_WARN_IF(!endPoint.IsSet())) {
    return;
  }
  nsresult rv = mUtilRange->SetStartAndEnd(startPoint, endPoint);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  UpdateDocChangeRange(mUtilRange);
}

// Let's remove all alignment hints in the children of aNode; it can
// be an ALIGN attribute (in case we just remove it) or a CENTER
// element (here we have to remove the container and keep its
// children). We break on tables and don't look at their children.
nsresult
HTMLEditRules::RemoveAlignment(nsINode& aNode,
                               const nsAString& aAlignType,
                               bool aChildrenOnly)
{
  if (EditorBase::IsTextNode(&aNode) || HTMLEditUtils::IsTable(&aNode)) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> child, tmp;
  if (aChildrenOnly) {
    child = aNode.GetFirstChild();
  } else {
    child = &aNode;
  }
  NS_ENSURE_STATE(mHTMLEditor);
  bool useCSS = mHTMLEditor->IsCSSEnabled();

  while (child) {
    if (aChildrenOnly) {
      // get the next sibling right now because we could have to remove child
      tmp = child->GetNextSibling();
    } else {
      tmp = nullptr;
    }

    if (child->IsHTMLElement(nsGkAtoms::center)) {
      // the current node is a CENTER element
      // first remove children's alignment
      nsresult rv = RemoveAlignment(*child, aAlignType, true);
      NS_ENSURE_SUCCESS(rv, rv);

      // we may have to insert BRs in first and last position of element's children
      // if the nodes before/after are not blocks and not BRs
      rv = MakeSureElemStartsOrEndsOnCR(*child);
      NS_ENSURE_SUCCESS(rv, rv);

      // now remove the CENTER container
      NS_ENSURE_STATE(mHTMLEditor);
      rv = mHTMLEditor->RemoveContainerWithTransaction(*child->AsElement());
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (IsBlockNode(*child) || child->IsHTMLElement(nsGkAtoms::hr)) {
      // the current node is a block element
      if (HTMLEditUtils::SupportsAlignAttr(*child)) {
        // remove the ALIGN attribute if this element can have it
        NS_ENSURE_STATE(mHTMLEditor);
        nsresult rv =
          mHTMLEditor->RemoveAttributeWithTransaction(*child->AsElement(),
                                                      *nsGkAtoms::align);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      if (useCSS) {
        if (child->IsAnyOfHTMLElements(nsGkAtoms::table, nsGkAtoms::hr)) {
          NS_ENSURE_STATE(mHTMLEditor);
          nsresult rv =
            mHTMLEditor->SetAttributeOrEquivalent(child->AsElement(),
                                                  nsGkAtoms::align,
                                                  aAlignType, false);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        } else {
          nsAutoString dummyCssValue;
          NS_ENSURE_STATE(mHTMLEditor);
          nsresult rv = mHTMLEditor->mCSSEditUtils->RemoveCSSInlineStyle(
                                                      *child,
                                                      nsGkAtoms::textAlign,
                                                      dummyCssValue);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
      }
      if (!child->IsHTMLElement(nsGkAtoms::table)) {
        // unless this is a table, look at children
        nsresult rv = RemoveAlignment(*child, aAlignType, true);
        NS_ENSURE_SUCCESS(rv, rv);
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
HTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsINode& aNode,
                                            bool aStarts)
{
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  nsINode* child = aStarts ? htmlEditor->GetFirstEditableChild(aNode) :
                             htmlEditor->GetLastEditableChild(aNode);
  if (NS_WARN_IF(!child)) {
    return NS_OK;
  }

  bool foundCR = false;
  if (IsBlockNode(*child) || child->IsHTMLElement(nsGkAtoms::br)) {
    foundCR = true;
  } else {
    nsINode* sibling =
      aStarts ? htmlEditor->GetPriorHTMLSibling(&aNode) :
                htmlEditor->GetNextHTMLSibling(&aNode);
    if (sibling) {
      if (IsBlockNode(*sibling) || sibling->IsHTMLElement(nsGkAtoms::br)) {
        foundCR = true;
      }
    } else {
      foundCR = true;
    }
  }
  if (!foundCR) {
    RefPtr<Selection> selection = htmlEditor->GetSelection();
    if (NS_WARN_IF(!selection)) {
      return NS_ERROR_FAILURE;
    }
    EditorRawDOMPoint pointToInsert;
    if (!aStarts) {
      pointToInsert.SetToEndOf(&aNode);
    } else {
      pointToInsert.Set(&aNode, 0);
    }
    RefPtr<Element> brElement =
      htmlEditor->InsertBrElementWithTransaction(*selection, pointToInsert);
    if (NS_WARN_IF(!brElement)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsresult
HTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsINode& aNode)
{
  nsresult rv = MakeSureElemStartsOrEndsOnCR(aNode, false);
  NS_ENSURE_SUCCESS(rv, rv);
  return MakeSureElemStartsOrEndsOnCR(aNode, true);
}

nsresult
HTMLEditRules::AlignBlock(Element& aElement,
                          const nsAString& aAlignType,
                          ContentsOnly aContentsOnly)
{
  if (!IsBlockNode(aElement) && !aElement.IsHTMLElement(nsGkAtoms::hr)) {
    // We deal only with blocks; early way out
    return NS_OK;
  }

  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  nsresult rv = RemoveAlignment(aElement, aAlignType,
                                aContentsOnly == ContentsOnly::yes);
  NS_ENSURE_SUCCESS(rv, rv);
  if (htmlEditor->IsCSSEnabled()) {
    // Let's use CSS alignment; we use margin-left and margin-right for tables
    // and text-align for other block-level elements
    return htmlEditor->SetAttributeOrEquivalent(
                         &aElement, nsGkAtoms::align, aAlignType, false);
  }

  // HTML case; this code is supposed to be called ONLY if the element
  // supports the align attribute but we'll never know...
  if (NS_WARN_IF(!HTMLEditUtils::SupportsAlignAttr(aElement))) {
    // XXX error?
    return NS_OK;
  }

  return htmlEditor->SetAttributeOrEquivalent(&aElement, nsGkAtoms::align,
                                              aAlignType, false);
}

nsresult
HTMLEditRules::ChangeIndentation(Element& aElement,
                                 Change aChange)
{
  NS_ENSURE_STATE(mHTMLEditor);
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  nsAtom& marginProperty = MarginPropertyAtomForIndent(aElement);
  nsAutoString value;
  CSSEditUtils::GetSpecifiedProperty(aElement, marginProperty, value);
  float f;
  RefPtr<nsAtom> unit;
  CSSEditUtils::ParseLength(value, &f, getter_AddRefs(unit));
  if (!f) {
    nsAutoString defaultLengthUnit;
    CSSEditUtils::GetDefaultLengthUnit(defaultLengthUnit);
    unit = NS_Atomize(defaultLengthUnit);
  }
  int8_t multiplier = aChange == Change::plus ? +1 : -1;
  if (nsGkAtoms::in == unit) {
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
    htmlEditor->mCSSEditUtils->SetCSSProperty(aElement, marginProperty,
                                              newValue);
    return NS_OK;
  }

  htmlEditor->mCSSEditUtils->RemoveCSSProperty(aElement, marginProperty,
                                               value);

  // Remove unnecessary divs
  if (!aElement.IsHTMLElement(nsGkAtoms::div) ||
      &aElement == htmlEditor->GetActiveEditingHost() ||
      !htmlEditor->IsDescendantOfEditorRoot(&aElement) ||
      HTMLEditor::HasAttributes(&aElement)) {
    return NS_OK;
  }

  nsresult rv = htmlEditor->RemoveContainerWithTransaction(aElement);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
HTMLEditRules::WillAbsolutePosition(Selection& aSelection,
                                    bool* aCancel,
                                    bool* aHandled)
{
  MOZ_ASSERT(aCancel && aHandled);

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);

  WillInsert(aSelection, aCancel);

  // We want to ignore result of WillInsert()
  *aCancel = false;
  *aHandled = true;

  nsCOMPtr<Element> focusElement = htmlEditor->GetSelectionContainer();
  if (focusElement && HTMLEditUtils::IsImage(focusElement)) {
    mNewBlock = focusElement;
    return NS_OK;
  }

  nsresult rv = NormalizeSelection(&aSelection);
  NS_ENSURE_SUCCESS(rv, rv);
  AutoSelectionRestorer selectionRestorer(&aSelection, htmlEditor);

  // Convert the selection ranges into "promoted" selection ranges: this
  // basically just expands the range to include the immediate block parent,
  // and then further expands to include any ancestors whose children are all
  // in the range.

  nsTArray<RefPtr<nsRange>> arrayOfRanges;
  GetPromotedRanges(aSelection, arrayOfRanges,
                    EditAction::setAbsolutePosition);

  // Use these ranges to contruct a list of nodes to act on.
  nsTArray<OwningNonNull<nsINode>> arrayOfNodes;
  rv = GetNodesForOperation(arrayOfRanges, arrayOfNodes,
                            EditAction::setAbsolutePosition);
  NS_ENSURE_SUCCESS(rv, rv);

  // If nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes)) {
    nsRange* firstRange = aSelection.GetRangeAt(0);
    if (NS_WARN_IF(!firstRange)) {
      return NS_ERROR_FAILURE;
    }

    EditorDOMPoint atStartOfSelection(firstRange->StartRef());
    if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Make sure we can put a block here.
    SplitNodeResult splitNodeResult =
      MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                  atStartOfSelection);
    if (NS_WARN_IF(splitNodeResult.Failed())) {
      return splitNodeResult.Rv();
    }
    RefPtr<Element> positionedDiv =
      htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div,
                                            splitNodeResult.SplitPoint());
    if (NS_WARN_IF(!positionedDiv)) {
      return NS_ERROR_FAILURE;
    }
    // Remember our new block for postprocessing
    mNewBlock = positionedDiv;
    // Delete anything that was in the list of nodes
    while (!arrayOfNodes.IsEmpty()) {
      OwningNonNull<nsINode> curNode = arrayOfNodes[0];
      rv = htmlEditor->DeleteNodeWithTransaction(*curNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      arrayOfNodes.RemoveElementAt(0);
    }
    // Put selection in new block
    *aHandled = true;
    rv = aSelection.Collapse(positionedDiv, 0);
    // Don't restore the selection
    selectionRestorer.Abort();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // Okay, now go through all the nodes and put them in a blockquote, or
  // whatever is appropriate.  Woohoo!
  nsCOMPtr<Element> curList, curPositionedDiv, indentedLI;
  for (OwningNonNull<nsINode>& curNode : arrayOfNodes) {
    // Here's where we actually figure out what to do.
    EditorDOMPoint atCurNode(curNode);
    if (NS_WARN_IF(!atCurNode.IsSet())) {
      return NS_ERROR_FAILURE; // XXX not continue??
    }

    // Ignore all non-editable nodes.  Leave them be.
    if (!htmlEditor->IsEditable(curNode)) {
      continue;
    }

    nsCOMPtr<nsIContent> sibling;

    // Some logic for putting list items into nested lists...
    if (HTMLEditUtils::IsList(atCurNode.GetContainer())) {
      // Check to see if curList is still appropriate.  Which it is if curNode
      // is still right after it in the same list.
      if (curList) {
        sibling = htmlEditor->GetPriorHTMLSibling(curNode);
      }

      if (!curList || (sibling && sibling != curList)) {
        nsAtom* containerName =
          atCurNode.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type.
        SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*containerName,
                                                      atCurNode);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        if (!curPositionedDiv) {
          curPositionedDiv =
            htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div,
                                                  splitNodeResult.SplitPoint());
          NS_WARNING_ASSERTION(curPositionedDiv,
            "Failed to create current positioned div element");
          mNewBlock = curPositionedDiv;
        }
        EditorRawDOMPoint atEndOfCurPositionedDiv;
        atEndOfCurPositionedDiv.SetToEndOf(curPositionedDiv);
        curList =
          htmlEditor->CreateNodeWithTransaction(*containerName,
                                                atEndOfCurPositionedDiv);
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
        // curList is now the correct thing to put curNode in.  Remember our
        // new block for postprocessing.
      }
      // Tuck the node into the end of the active list
      rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                    *curList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    // Not a list item, use blockquote?  If we are inside a list item, we
    // don't want to blockquote, we want to sublist the list item.  We may
    // have several nodes listed in the array of nodes to act on, that are in
    // the same list item.  Since we only want to indent that li once, we
    // must keep track of the most recent indented list item, and not indent
    // it if we find another node to act on that is still inside the same li.
    RefPtr<Element> listItem = IsInListItem(curNode);
    if (listItem) {
      if (indentedLI == listItem) {
        // Already indented this list item
        continue;
      }
      // Check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList) {
        sibling = htmlEditor->GetPriorHTMLSibling(listItem);
      }

      if (!curList || (sibling && sibling != curList)) {
        EditorDOMPoint atListItem(listItem);
        if (NS_WARN_IF(!atListItem.IsSet())) {
          return NS_ERROR_FAILURE;
        }
        nsAtom* containerName =
          atListItem.GetContainer()->NodeInfo()->NameAtom();
        // Create a new nested list of correct type
        SplitNodeResult splitNodeResult =
          MaybeSplitAncestorsForInsertWithTransaction(*containerName,
                                                      atListItem);
        if (NS_WARN_IF(splitNodeResult.Failed())) {
          return splitNodeResult.Rv();
        }
        if (!curPositionedDiv) {
          EditorRawDOMPoint atListItemParent(atListItem.GetContainer());
          curPositionedDiv =
            htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div,
                                                  atListItemParent);
          NS_WARNING_ASSERTION(curPositionedDiv,
            "Failed to create current positioned div element");
          mNewBlock = curPositionedDiv;
        }
        EditorRawDOMPoint atEndOfCurPositionedDiv;
        atEndOfCurPositionedDiv.SetToEndOf(curPositionedDiv);
        curList =
          htmlEditor->CreateNodeWithTransaction(*containerName,
                                                atEndOfCurPositionedDiv);
        if (NS_WARN_IF(!curList)) {
          return NS_ERROR_FAILURE;
        }
      }
      rv = htmlEditor->MoveNodeToEndWithTransaction(*listItem, *curList);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // Remember we indented this li
      indentedLI = listItem;
      continue;
    }

    // Need to make a div to put things in if we haven't already
    if (!curPositionedDiv) {
      if (curNode->IsHTMLElement(nsGkAtoms::div)) {
        curPositionedDiv = curNode->AsElement();
        mNewBlock = curPositionedDiv;
        curList = nullptr;
        continue;
      }
      SplitNodeResult splitNodeResult =
        MaybeSplitAncestorsForInsertWithTransaction(*nsGkAtoms::div,
                                                    atCurNode);
      if (NS_WARN_IF(splitNodeResult.Failed())) {
        return splitNodeResult.Rv();
      }
      curPositionedDiv =
        htmlEditor->CreateNodeWithTransaction(*nsGkAtoms::div,
                                              splitNodeResult.SplitPoint());
      if (NS_WARN_IF(!curPositionedDiv)) {
        return NS_ERROR_FAILURE;
      }
      // Remember our new block for postprocessing
      mNewBlock = curPositionedDiv;
      // curPositionedDiv is now the correct thing to put curNode in
    }

    // Tuck the node into the end of the active blockquote
    rv = htmlEditor->MoveNodeToEndWithTransaction(*curNode->AsContent(),
                                                  *curPositionedDiv);
    NS_ENSURE_SUCCESS(rv, rv);
    // Forget curList, if any
    curList = nullptr;
  }
  return NS_OK;
}

nsresult
HTMLEditRules::DidAbsolutePosition()
{
  if (!mNewBlock) {
    return NS_OK;
  }
  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditor;
  return htmlEditor->SetPositionToAbsoluteOrStatic(*mNewBlock, true);
}

nsresult
HTMLEditRules::WillRemoveAbsolutePosition(Selection* aSelection,
                                          bool* aCancel,
                                          bool* aHandled) {
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditor;

  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore aCancel from WillInsert()
  *aCancel = false;
  *aHandled = true;

  RefPtr<Element> element =
    htmlEditor->GetAbsolutelyPositionedSelectionContainer();
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }

  AutoSelectionRestorer selectionRestorer(aSelection, htmlEditor);

  return htmlEditor->SetPositionToAbsoluteOrStatic(*element, false);
}

nsresult
HTMLEditRules::WillRelativeChangeZIndex(Selection* aSelection,
                                        int32_t aChange,
                                        bool* aCancel,
                                        bool* aHandled)
{
  if (!aSelection || !aCancel || !aHandled) {
    return NS_ERROR_NULL_POINTER;
  }

  if (NS_WARN_IF(!mHTMLEditor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<HTMLEditor> htmlEditor = mHTMLEditor;

  WillInsert(*aSelection, aCancel);

  // initialize out param
  // we want to ignore aCancel from WillInsert()
  *aCancel = false;
  *aHandled = true;

  RefPtr<Element> element =
    htmlEditor->GetAbsolutelyPositionedSelectionContainer();
  if (NS_WARN_IF(!element)) {
    return NS_ERROR_FAILURE;
  }

  AutoSelectionRestorer selectionRestorer(aSelection, htmlEditor);

  int32_t zIndex;
  return htmlEditor->RelativeChangeElementZIndex(*element, aChange, &zIndex);
}

nsresult
HTMLEditRules::DocumentModified()
{
  nsContentUtils::AddScriptRunner(
    NewRunnableMethod("HTMLEditRules::DocumentModifiedWorker",
                      this,
                      &HTMLEditRules::DocumentModifiedWorker));
  return NS_OK;
}

void
HTMLEditRules::DocumentModifiedWorker()
{
  if (!mHTMLEditor) {
    return;
  }

  // DeleteNodeWithTransaction() below may cause a flush, which could destroy
  // the editor
  nsAutoScriptBlockerSuppressNodeRemoved scriptBlocker;

  RefPtr<HTMLEditor> htmlEditor(mHTMLEditor);
  RefPtr<Selection> selection = htmlEditor->GetSelection();
  if (!selection) {
    return;
  }

  // Delete our bogus node, if we have one, since the document might not be
  // empty any more.
  if (mBogusNode) {
    DebugOnly<nsresult> rv = htmlEditor->DeleteNodeWithTransaction(*mBogusNode);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
      "Failed to remove the bogus node");
    mBogusNode = nullptr;
  }

  // Try to recreate the bogus node if needed.
  CreateBogusNodeIfNeeded(selection);
}

} // namespace mozilla
