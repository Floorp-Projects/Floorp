/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorUtils_h
#define mozilla_EditorUtils_h

#include "mozilla/EditorBase.h"      // for EditorBase
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint, EditorDOMRange, etc
#include "mozilla/EditorForwards.h"
#include "mozilla/IntegerRange.h"       // for IntegerRange
#include "mozilla/Result.h"             // for Result<>
#include "mozilla/dom/Element.h"        // for dom::Element
#include "mozilla/dom/HTMLBRElement.h"  // for dom::HTMLBRElement
#include "mozilla/dom/Selection.h"      // for dom::Selection
#include "mozilla/dom/Text.h"           // for dom::Text

#include "nsAtom.h"          // for nsStaticAtom
#include "nsCOMPtr.h"        // for nsCOMPtr
#include "nsContentUtils.h"  // for nsContentUtils
#include "nsDebug.h"         // for NS_WARNING, etc
#include "nsError.h"         // for NS_SUCCESS_* and NS_ERROR_*
#include "nsRange.h"         // for nsRange
#include "nsString.h"        // for nsAString, nsString, etc

class nsITransferable;

namespace mozilla {

/***************************************************************************
 * EditActionResult is useful to return multiple results of an editor
 * action handler without out params.
 * Note that when you return an anonymous instance from a method, you should
 * use EditActionIgnored(), EditActionHandled() or EditActionCanceled() for
 * easier to read.  In other words, EditActionResult should be used when
 * declaring return type of a method, being an argument or defined as a local
 * variable.
 */
class MOZ_STACK_CLASS EditActionResult final {
 public:
  bool Succeeded() const { return NS_SUCCEEDED(mRv); }
  bool Failed() const { return NS_FAILED(mRv); }
  nsresult Rv() const { return mRv; }
  bool Canceled() const { return mCanceled; }
  bool Handled() const { return mHandled; }
  bool Ignored() const { return !mCanceled && !mHandled; }
  bool EditorDestroyed() const { return mRv == NS_ERROR_EDITOR_DESTROYED; }

  EditActionResult SetResult(nsresult aRv) {
    mRv = aRv;
    return *this;
  }
  EditActionResult MarkAsCanceled() {
    mCanceled = true;
    return *this;
  }
  EditActionResult MarkAsHandled() {
    mHandled = true;
    return *this;
  }

  explicit EditActionResult(nsresult aRv)
      : mRv(aRv), mCanceled(false), mHandled(false) {}

  EditActionResult& operator|=(const EditActionResult& aOther) {
    mCanceled |= aOther.mCanceled;
    mHandled |= aOther.mHandled;
    // When both result are same, keep the result.
    if (mRv == aOther.mRv) {
      return *this;
    }
    // If one of the result is NS_ERROR_EDITOR_DESTROYED, use it since it's
    // the most important error code for editor.
    if (EditorDestroyed() || aOther.EditorDestroyed()) {
      mRv = NS_ERROR_EDITOR_DESTROYED;
    }
    // If one of the results is error, use NS_ERROR_FAILURE.
    else if (Failed() || aOther.Failed()) {
      mRv = NS_ERROR_FAILURE;
    } else {
      // Otherwise, use generic success code, NS_OK.
      mRv = NS_OK;
    }
    return *this;
  }

  EditActionResult& operator|=(const MoveNodeResult& aMoveNodeResult);

 private:
  nsresult mRv;
  bool mCanceled;
  bool mHandled;

  EditActionResult(nsresult aRv, bool aCanceled, bool aHandled)
      : mRv(aRv), mCanceled(aCanceled), mHandled(aHandled) {}

  EditActionResult()
      : mRv(NS_ERROR_NOT_INITIALIZED), mCanceled(false), mHandled(false) {}

  friend EditActionResult EditActionIgnored(nsresult aRv);
  friend EditActionResult EditActionHandled(nsresult aRv);
  friend EditActionResult EditActionCanceled(nsresult aRv);
};

/***************************************************************************
 * When an edit action handler (or its helper) does nothing,
 * EditActionIgnored should be returned.
 */
inline EditActionResult EditActionIgnored(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, false, false);
}

/***************************************************************************
 * When an edit action handler (or its helper) handled and not canceled,
 * EditActionHandled should be returned.
 */
inline EditActionResult EditActionHandled(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, false, true);
}

/***************************************************************************
 * When an edit action handler (or its helper) handled and canceled,
 * EditActionHandled should be returned.
 */
inline EditActionResult EditActionCanceled(nsresult aRv = NS_OK) {
  return EditActionResult(aRv, true, true);
}

/***************************************************************************
 * CreateNodeResultBase is a simple class for CreateSomething() methods
 * which want to return new node.
 */

#define NS_INSTANTIATE_CREATE_NODE_RESULT_METHOD(aResultType, aMethodName, \
                                                 ...)                      \
  template aResultType CreateContentResult::aMethodName(__VA_ARGS__);      \
  template aResultType CreateElementResult::aMethodName(__VA_ARGS__);      \
  template aResultType CreateTextResult::aMethodName(__VA_ARGS__);

#define NS_INSTANTIATE_CREATE_NODE_RESULT_CONST_METHOD(aResultType,         \
                                                       aMethodName, ...)    \
  template aResultType CreateContentResult::aMethodName(__VA_ARGS__) const; \
  template aResultType CreateElementResult::aMethodName(__VA_ARGS__) const; \
  template aResultType CreateTextResult::aMethodName(__VA_ARGS__) const;

enum class SuggestCaret {
  // If specified, the method returns NS_OK when there is no recommended caret
  // position.
  OnlyIfHasSuggestion,
  // If specified and if EditorBase::AllowsTransactionsToChangeSelection
  // returns false, the method does nothing and returns NS_OK.
  OnlyIfTransactionsAllowedToDoIt,
  // If specified, the method returns
  // NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR even if
  // EditorBase::CollapseSelectionTo returns an error except when
  // NS_ERROR_EDITOR_DESTROYED.
  AndIgnoreTrivialError,
};

// TODO: Perhaps, we can make this inherits mozilla::Result for guaranteeing
//       same API.  Then, changing to/from Result<*, nsresult> can be easier.
//       For now, we should give same API name rather than same as
//       mozilla::ErrorResult.
template <typename NodeType>
class MOZ_STACK_CLASS CreateNodeResultBase final {
  typedef CreateNodeResultBase<NodeType> SelfType;

 public:
  // FYI: NS_SUCCEEDED and NS_FAILED contain MOZ_(UN)LIKELY so that isOk() and
  // isErr() must not required to wrap with them.
  bool isOk() const { return NS_SUCCEEDED(mRv); }
  bool isErr() const { return NS_FAILED(mRv); }
  bool Handled() const {
    MOZ_ASSERT_IF(mRv == NS_SUCCESS_DOM_NO_OPERATION, !mNode);
    return isOk() && mRv == NS_SUCCESS_DOM_NO_OPERATION;
  }
  constexpr nsresult inspectErr() const { return mRv; }
  constexpr nsresult unwrapErr() const { return inspectErr(); }
  constexpr bool EditorDestroyed() const {
    return MOZ_UNLIKELY(mRv == NS_ERROR_EDITOR_DESTROYED);
  }
  constexpr bool GotUnexpectedDOMTree() const {
    return MOZ_UNLIKELY(mRv == NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  NodeType* GetNewNode() const { return mNode; }
  RefPtr<NodeType> UnwrapNewNode() { return std::move(mNode); }

  /**
   * Suggest caret position to aEditorBase.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult SuggestCaretPointTo(
      const EditorBase& aEditorBase, const SuggestCaretOptions& aOptions) const;

  /**
   * IgnoreCaretPointSuggestion() should be called if the method does not want
   * to use caret position recommended by this instance.
   */
  void IgnoreCaretPointSuggestion() const { mHandledCaretPoint = true; }

  bool HasCaretPointSuggestion() const { return mCaretPoint.IsSet(); }
  EditorDOMPoint&& UnwrapCaretPoint() {
    mHandledCaretPoint = true;
    return std::move(mCaretPoint);
  }
  bool MoveCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                        const SuggestCaretOptions& aOptions) {
    MOZ_ASSERT(!aOptions.contains(SuggestCaret::AndIgnoreTrivialError));
    MOZ_ASSERT(
        !aOptions.contains(SuggestCaret::OnlyIfTransactionsAllowedToDoIt));
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion) &&
        !mCaretPoint.IsSet()) {
      return false;
    }
    aPointToPutCaret = UnwrapCaretPoint();
    return true;
  }
  bool MoveCaretPointTo(EditorDOMPoint& aPointToPutCaret,
                        const EditorBase& aEditorBase,
                        const SuggestCaretOptions& aOptions);

  explicit CreateNodeResultBase(nsresult aRv) : mRv(aRv) {
    MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mRv));
  }

  explicit CreateNodeResultBase(NodeType* aNode)
      : mNode(aNode), mRv(aNode ? NS_OK : NS_ERROR_FAILURE) {}
  explicit CreateNodeResultBase(NodeType* aNode,
                                const EditorDOMPoint& aCandidateCaretPoint)
      : mNode(aNode),
        mCaretPoint(aCandidateCaretPoint),
        mRv(aNode ? NS_OK : NS_ERROR_FAILURE) {}
  explicit CreateNodeResultBase(NodeType* aNode,
                                EditorDOMPoint&& aCandidateCaretPoint)
      : mNode(aNode),
        mCaretPoint(std::move(aCandidateCaretPoint)),
        mRv(aNode ? NS_OK : NS_ERROR_FAILURE) {}

  explicit CreateNodeResultBase(RefPtr<NodeType>&& aNode)
      : mNode(std::move(aNode)), mRv(mNode.get() ? NS_OK : NS_ERROR_FAILURE) {}
  explicit CreateNodeResultBase(RefPtr<NodeType>&& aNode,
                                const EditorDOMPoint& aCandidateCaretPoint)
      : mNode(std::move(aNode)),
        mCaretPoint(aCandidateCaretPoint),
        mRv(mNode.get() ? NS_OK : NS_ERROR_FAILURE) {}
  explicit CreateNodeResultBase(RefPtr<NodeType>&& aNode,
                                EditorDOMPoint&& aCandidateCaretPoint)
      : mNode(std::move(aNode)),
        mCaretPoint(std::move(aCandidateCaretPoint)),
        mRv(mNode.get() ? NS_OK : NS_ERROR_FAILURE) {}

  [[nodiscard]] static SelfType NotHandled() {
    SelfType result;
    result.mRv = NS_SUCCESS_DOM_NO_OPERATION;
    return result;
  }
  [[nodiscard]] static SelfType NotHandled(
      const EditorDOMPoint& aPointToPutCaret) {
    SelfType result;
    result.mRv = NS_SUCCESS_DOM_NO_OPERATION;
    result.mCaretPoint = aPointToPutCaret;
    return result;
  }
  [[nodiscard]] static SelfType NotHandled(EditorDOMPoint&& aPointToPutCaret) {
    SelfType result;
    result.mRv = NS_SUCCESS_DOM_NO_OPERATION;
    result.mCaretPoint = std::move(aPointToPutCaret);
    return result;
  }

#ifdef DEBUG
  ~CreateNodeResultBase() {
    MOZ_ASSERT_IF(isOk(), !mCaretPoint.IsSet() || mHandledCaretPoint);
  }
#endif

  CreateNodeResultBase(const SelfType& aOther) = delete;
  SelfType& operator=(const SelfType& aOther) = delete;
  CreateNodeResultBase(SelfType&& aOther) = default;
  SelfType& operator=(SelfType&& aOther) = default;

 private:
  CreateNodeResultBase() = default;

  RefPtr<NodeType> mNode;
  EditorDOMPoint mCaretPoint;
  nsresult mRv = NS_OK;
  bool mutable mHandledCaretPoint = false;
};

/***************************************************************************
 * stack based helper class for calling EditorBase::EndTransaction() after
 * EditorBase::BeginTransaction().  This shouldn't be used in editor classes
 * or helper classes while an edit action is being handled.  Use
 * AutoTransactionBatch in such cases since it uses non-virtual internal
 * methods.
 ***************************************************************************/
class MOZ_RAII AutoTransactionBatchExternal final {
 public:
  MOZ_CAN_RUN_SCRIPT explicit AutoTransactionBatchExternal(
      EditorBase& aEditorBase)
      : mEditorBase(aEditorBase) {
    MOZ_KnownLive(mEditorBase).BeginTransaction();
  }

  MOZ_CAN_RUN_SCRIPT ~AutoTransactionBatchExternal() {
    MOZ_KnownLive(mEditorBase).EndTransaction();
  }

 private:
  EditorBase& mEditorBase;
};

/******************************************************************************
 * AutoSelectionRangeArray stores all ranges in `aSelection`.
 * Note that modifying the ranges means modifing the selection ranges.
 *****************************************************************************/
class MOZ_STACK_CLASS AutoSelectionRangeArray final {
 public:
  explicit AutoSelectionRangeArray(dom::Selection& aSelection) {
    for (const uint32_t i : IntegerRange(aSelection.RangeCount())) {
      MOZ_ASSERT(aSelection.GetRangeAt(i));
      mRanges.AppendElement(*aSelection.GetRangeAt(i));
    }
  }

  AutoTArray<mozilla::OwningNonNull<nsRange>, 8> mRanges;
};

class EditorUtils final {
 public:
  using EditorType = EditorBase::EditorType;
  using Selection = dom::Selection;

  /**
   * IsDescendantOf() checks if aNode is a child or a descendant of aParent.
   * aOutPoint is set to the child of aParent.
   *
   * @return            true if aNode is a child or a descendant of aParent.
   */
  static bool IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                             EditorRawDOMPoint* aOutPoint = nullptr);
  static bool IsDescendantOf(const nsINode& aNode, const nsINode& aParent,
                             EditorDOMPoint* aOutPoint);

  /**
   * Returns true if aContent is a <br> element and it's marked as padding for
   * empty editor.
   */
  static bool IsPaddingBRElementForEmptyEditor(const nsIContent& aContent) {
    const dom::HTMLBRElement* brElement =
        dom::HTMLBRElement::FromNode(&aContent);
    return brElement && brElement->IsPaddingForEmptyEditor();
  }

  /**
   * Returns true if aContent is a <br> element and it's marked as padding for
   * empty last line.
   */
  static bool IsPaddingBRElementForEmptyLastLine(const nsIContent& aContent) {
    const dom::HTMLBRElement* brElement =
        dom::HTMLBRElement::FromNode(&aContent);
    return brElement && brElement->IsPaddingForEmptyLastLine();
  }

  /**
   * IsEditableContent() returns true if aContent's data or children is ediable
   * for the given editor type.  Be aware, returning true does NOT mean the
   * node can be removed from its parent node, and returning false does NOT
   * mean the node cannot be removed from the parent node.
   * XXX May be the anonymous nodes in TextEditor not editable?  If it's not
   *     so, we can get rid of aEditorType.
   */
  static bool IsEditableContent(const nsIContent& aContent,
                                EditorType aEditorType) {
    if (aEditorType == EditorType::HTML &&
        (!aContent.IsEditable() || !aContent.IsInComposedDoc())) {
      // FIXME(emilio): Why only for HTML editors? All content from the root
      // content in text editors is also editable, so afaict we can remove the
      // special-case.
      return false;
    }
    return IsElementOrText(aContent);
  }

  /**
   * Returns true if aContent is a usual element node (not padding <br> element
   * for empty editor) or a text node.  In other words, returns true if
   * aContent is a usual element node or visible data node.
   */
  static bool IsElementOrText(const nsIContent& aContent) {
    if (aContent.IsText()) {
      return true;
    }
    return aContent.IsElement() && !IsPaddingBRElementForEmptyEditor(aContent);
  }

  /**
   * IsWhiteSpacePreformatted() checks the style info for the node for the
   * preformatted text style.  This does NOT flush layout.
   */
  static bool IsWhiteSpacePreformatted(const nsIContent& aContent);

  /**
   * IsNewLinePreformatted() checks whether the linefeed characters are
   * preformatted or collapsible white-spaces.  This does NOT flush layout.
   */
  static bool IsNewLinePreformatted(const nsIContent& aContent);

  /**
   * IsOnlyNewLinePreformatted() checks whether the linefeed characters are
   * preformated but white-spaces are collapsed, or otherwise.  I.e., this
   * returns true only when `white-space:pre-line`.
   */
  static bool IsOnlyNewLinePreformatted(const nsIContent& aContent);

  /**
   * Helper method for `AppendString()` and `AppendSubString()`.  This should
   * be called only when `aText` is in a password field.  This method masks
   * A part of or all of `aText` (`aStartOffsetInText` and later) should've
   * been copied (apppended) to `aString`.  `aStartOffsetInString` is where
   * the password was appended into `aString`.
   */
  static void MaskString(nsString& aString, const dom::Text& aTextNode,
                         uint32_t aStartOffsetInString,
                         uint32_t aStartOffsetInText);

  static nsStaticAtom* GetTagNameAtom(const nsAString& aTagName) {
    if (aTagName.IsEmpty()) {
      return nullptr;
    }
    nsAutoString lowerTagName;
    nsContentUtils::ASCIIToLower(aTagName, lowerTagName);
    return NS_GetStaticAtom(lowerTagName);
  }

  static nsStaticAtom* GetAttributeAtom(const nsAString& aAttribute) {
    if (aAttribute.IsEmpty()) {
      return nullptr;  // Don't use nsGkAtoms::_empty for attribute.
    }
    return NS_GetStaticAtom(aAttribute);
  }

  /**
   * Helper method for deletion.  When this returns true, Selection will be
   * computed with nsFrameSelection that also requires flushed layout
   * information.
   */
  template <typename SelectionOrAutoRangeArray>
  static bool IsFrameSelectionRequiredToExtendSelection(
      nsIEditor::EDirection aDirectionAndAmount,
      SelectionOrAutoRangeArray& aSelectionOrAutoRangeArray) {
    switch (aDirectionAndAmount) {
      case nsIEditor::eNextWord:
      case nsIEditor::ePreviousWord:
      case nsIEditor::eToBeginningOfLine:
      case nsIEditor::eToEndOfLine:
        return true;
      case nsIEditor::ePrevious:
      case nsIEditor::eNext:
        return aSelectionOrAutoRangeArray.IsCollapsed();
      default:
        return false;
    }
  }

  /**
   * Returns true if aSelection includes the point in aParentContent.
   */
  static bool IsPointInSelection(const Selection& aSelection,
                                 const nsINode& aParentNode, uint32_t aOffset);

  /**
   * Create an nsITransferable instance which has kUnicodeMime and
   * kMozTextInternal flavors.
   */
  static Result<nsCOMPtr<nsITransferable>, nsresult>
  CreateTransferableForPlainText(const dom::Document& aDocument);
};

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorUtils_h
