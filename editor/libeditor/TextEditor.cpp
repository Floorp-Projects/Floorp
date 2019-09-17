/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditor.h"

#include "EditAggregateTransaction.h"
#include "HTMLEditRules.h"
#include "InternetCiter.h"
#include "TextEditUtils.h"
#include "gfxFontUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_editor.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TextServicesDocument.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsCRT.h"
#include "nsCaret.h"
#include "nsCharTraits.h"
#include "nsComponentManagerUtils.h"
#include "nsContentCID.h"
#include "nsContentList.h"
#include "nsCopySupport.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIAbsorbingTransaction.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "nsIDocumentEncoder.h"
#include "nsINode.h"
#include "nsIPrincipal.h"
#include "nsISelectionController.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "nsIWeakReferenceUtils.h"
#include "nsNameSpaceManager.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTextFragment.h"
#include "nsTextNode.h"
#include "nsUnicharUtils.h"
#include "nsXPCOM.h"

class nsIOutputStream;
class nsISupports;

namespace mozilla {

using namespace dom;

TextEditor::TextEditor()
    : mWrapColumn(0),
      mMaxTextLength(-1),
      mInitTriggerCounter(0),
      mNewlineHandling(nsIPlaintextEditor::eNewlinesPasteToFirst),
#ifdef XP_WIN
      mCaretStyle(1),
#else
      mCaretStyle(0),
#endif
      mUnmaskedStart(UINT32_MAX),
      mUnmaskedLength(0),
      mIsMaskingPassword(true) {
  // printf("Size of TextEditor: %zu\n", sizeof(TextEditor));
  static_assert(
      sizeof(TextEditor) <= 512,
      "TextEditor instance should be allocatable in the quantum class bins");

  // check the "single line editor newline handling"
  // and "caret behaviour in selection" prefs
  GetDefaultEditorPrefs(mNewlineHandling, mCaretStyle);
}

TextEditor::~TextEditor() {
  // Remove event listeners. Note that if we had an HTML editor,
  //  it installed its own instead of these
  RemoveEventListeners();

  if (mRules) mRules->DetachEditor();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TextEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TextEditor, EditorBase)
  if (tmp->mRules) {
    tmp->mRules->DetachEditor();
  }
  if (tmp->mMaskTimer) {
    tmp->mMaskTimer->Cancel();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedDocumentEncoder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMaskTimer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TextEditor, EditorBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCachedDocumentEncoder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMaskTimer)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(TextEditor, EditorBase)
NS_IMPL_RELEASE_INHERITED(TextEditor, EditorBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextEditor)
  NS_INTERFACE_MAP_ENTRY(nsIPlaintextEditor)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END_INHERITING(EditorBase)

nsresult TextEditor::Init(Document& aDoc, Element* aRoot,
                          nsISelectionController* aSelCon, uint32_t aFlags,
                          const nsAString& aInitialValue) {
  if (mRules) {
    mRules->DetachEditor();
  }

  nsresult rulesRv = NS_OK;
  {
    // block to scope AutoEditInitRulesTrigger
    AutoEditInitRulesTrigger rulesTrigger(this, rulesRv);

    // Init the base editor
    nsresult rv = EditorBase::Init(aDoc, aRoot, aSelCon, aFlags, aInitialValue);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rulesRv), "Failed to initialize us");
  return EditorBase::ToGenericNSResult(rulesRv);
}

static int32_t sNewlineHandlingPref = -1, sCaretStylePref = -1;

static void EditorPrefsChangedCallback(const char* aPrefName, void*) {
  if (!nsCRT::strcmp(aPrefName, "editor.singleLine.pasteNewlines")) {
    sNewlineHandlingPref =
        Preferences::GetInt("editor.singleLine.pasteNewlines",
                            nsIPlaintextEditor::eNewlinesPasteToFirst);
  } else if (!nsCRT::strcmp(aPrefName, "layout.selection.caret_style")) {
    sCaretStylePref = Preferences::GetInt("layout.selection.caret_style",
#ifdef XP_WIN
                                          1);
    if (!sCaretStylePref) {
      sCaretStylePref = 1;
    }
#else
                                          0);
#endif
  }
}

// static
void TextEditor::GetDefaultEditorPrefs(int32_t& aNewlineHandling,
                                       int32_t& aCaretStyle) {
  if (sNewlineHandlingPref == -1) {
    Preferences::RegisterCallbackAndCall(EditorPrefsChangedCallback,
                                         "editor.singleLine.pasteNewlines");
    Preferences::RegisterCallbackAndCall(EditorPrefsChangedCallback,
                                         "layout.selection.caret_style");
  }

  aNewlineHandling = sNewlineHandlingPref;
  aCaretStyle = sCaretStylePref;
}

void TextEditor::BeginEditorInit() { mInitTriggerCounter++; }

nsresult TextEditor::EndEditorInit() {
  MOZ_ASSERT(mInitTriggerCounter > 0, "ended editor init before we began?");
  mInitTriggerCounter--;
  if (mInitTriggerCounter) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = InitRules();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  // Throw away the old transaction manager if this is not the first time that
  // we're initializing the editor.
  ClearUndoRedo();
  EnableUndoRedo();
  mInitSucceeded = true;
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::SetDocumentCharacterSet(const nsACString& characterSet) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eSetCharacterSet);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = EditorBase::SetDocumentCharacterSet(characterSet);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }

  // Update META charset element.
  RefPtr<Document> doc = GetDocument();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (UpdateMetaCharset(*doc, characterSet)) {
    return NS_OK;
  }

  RefPtr<nsContentList> headList =
      doc->GetElementsByTagName(NS_LITERAL_STRING("head"));
  if (NS_WARN_IF(!headList)) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> headNode = headList->Item(0);
  if (NS_WARN_IF(!headNode)) {
    return NS_OK;
  }

  // Create a new meta charset tag
  RefPtr<Element> metaElement =
      CreateNodeWithTransaction(*nsGkAtoms::meta, EditorDOMPoint(headNode, 0));
  if (NS_WARN_IF(!metaElement)) {
    return NS_OK;
  }

  // Set attributes to the created element
  if (characterSet.IsEmpty()) {
    return NS_OK;
  }

  // not undoable, undo should undo CreateNodeWithTransaction().
  metaElement->SetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv,
                       NS_LITERAL_STRING("Content-Type"), true);
  metaElement->SetAttr(kNameSpaceID_None, nsGkAtoms::content,
                       NS_LITERAL_STRING("text/html;charset=") +
                           NS_ConvertASCIItoUTF16(characterSet),
                       true);
  return NS_OK;
}

bool TextEditor::UpdateMetaCharset(Document& aDocument,
                                   const nsACString& aCharacterSet) {
  // get a list of META tags
  RefPtr<nsContentList> metaList =
      aDocument.GetElementsByTagName(NS_LITERAL_STRING("meta"));
  if (NS_WARN_IF(!metaList)) {
    return false;
  }

  for (uint32_t i = 0; i < metaList->Length(true); ++i) {
    nsCOMPtr<nsIContent> metaNode = metaList->Item(i);
    MOZ_ASSERT(metaNode);

    if (!metaNode->IsElement()) {
      continue;
    }

    nsAutoString currentValue;
    metaNode->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv,
                                   currentValue);

    if (!FindInReadable(NS_LITERAL_STRING("content-type"), currentValue,
                        nsCaseInsensitiveStringComparator())) {
      continue;
    }

    metaNode->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::content,
                                   currentValue);

    NS_NAMED_LITERAL_STRING(charsetEquals, "charset=");
    nsAString::const_iterator originalStart, start, end;
    originalStart = currentValue.BeginReading(start);
    currentValue.EndReading(end);
    if (!FindInReadable(charsetEquals, start, end,
                        nsCaseInsensitiveStringComparator())) {
      continue;
    }

    // set attribute to <original prefix> charset=text/html
    RefPtr<Element> metaElement = metaNode->AsElement();
    MOZ_ASSERT(metaElement);
    nsresult rv = SetAttributeWithTransaction(
        *metaElement, *nsGkAtoms::content,
        Substring(originalStart, start) + charsetEquals +
            NS_ConvertASCIItoUTF16(aCharacterSet));
    return NS_SUCCEEDED(rv);
  }
  return false;
}

nsresult TextEditor::InitRules() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mRules) {
    // instantiate the rules for this text editor
    mRules = new TextEditRules();
  }
  RefPtr<TextEditRules> textEditRules(mRules);
  return textEditRules->Init(this);
}

nsresult TextEditor::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent) {
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_texteditor_keyevent_handling.html
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html
  //
  // And also when you add new key handling, you need to change the subclass's
  // HandleKeyPressEvent()'s switch statement.

  if (IsReadonly() || IsDisabled()) {
    // When we're not editable, the events handled on EditorBase.
    return EditorBase::HandleKeyPressEvent(aKeyboardEvent);
  }

  if (NS_WARN_IF(!aKeyboardEvent)) {
    return NS_ERROR_UNEXPECTED;
  }
  MOZ_ASSERT(aKeyboardEvent->mMessage == eKeyPress,
             "HandleKeyPressEvent gets non-keypress event");

  switch (aKeyboardEvent->mKeyCode) {
    case NS_VK_META:
    case NS_VK_WIN:
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
      // These keys are handled on EditorBase
      return EditorBase::HandleKeyPressEvent(aKeyboardEvent);
    case NS_VK_BACK: {
      if (aKeyboardEvent->IsControl() || aKeyboardEvent->IsAlt() ||
          aKeyboardEvent->IsMeta() || aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelectionAsAction(nsIEditor::ePrevious, nsIEditor::eStrip);
      aKeyboardEvent->PreventDefault();  // consumed
      return NS_OK;
    }
    case NS_VK_DELETE: {
      // on certain platforms (such as windows) the shift key
      // modifies what delete does (cmd_cut in this case).
      // bailing here to allow the keybindings to do the cut.
      if (aKeyboardEvent->IsShift() || aKeyboardEvent->IsControl() ||
          aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
          aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelectionAsAction(nsIEditor::eNext, nsIEditor::eStrip);
      aKeyboardEvent->PreventDefault();  // consumed
      return NS_OK;
    }
    case NS_VK_TAB: {
      if (IsTabbable()) {
        return NS_OK;  // let it be used for focus switching
      }

      if (aKeyboardEvent->IsShift() || aKeyboardEvent->IsControl() ||
          aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
          aKeyboardEvent->IsOS()) {
        return NS_OK;
      }

      // else we insert the tab straight through
      aKeyboardEvent->PreventDefault();
      return OnInputText(NS_LITERAL_STRING("\t"));
    }
    case NS_VK_RETURN:
      if (IsSingleLineEditor() || !aKeyboardEvent->IsInputtingLineBreak()) {
        return NS_OK;
      }
      aKeyboardEvent->PreventDefault();
      return InsertLineBreakAsAction();
  }

  if (!aKeyboardEvent->IsInputtingText()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  aKeyboardEvent->PreventDefault();
  nsAutoString str(aKeyboardEvent->mCharCode);
  return OnInputText(str);
}

nsresult TextEditor::OnInputText(const nsAString& aStringToInsert) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  MOZ_ASSERT(!aStringToInsert.IsVoid());
  editActionData.SetData(aStringToInsert);

  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName);
  nsresult rv = InsertTextAsSubAction(aStringToInsert);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::InsertLineBreakAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertLineBreak,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // XXX This may be called by execCommand() with "insertParagraph".
  //     In such case, naming the transaction "TypingTxnName" is odd.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName);
  nsresult rv = InsertLineBreakAsSubAction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::ExtendSelectionForDelete(nsIEditor::EDirection* aAction) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  bool bCollapsed = SelectionRefPtr()->IsCollapsed();

  if (*aAction == eNextWord || *aAction == ePreviousWord ||
      (*aAction == eNext && bCollapsed) ||
      (*aAction == ePrevious && bCollapsed) || *aAction == eToBeginningOfLine ||
      *aAction == eToEndOfLine) {
    nsCOMPtr<nsISelectionController> selCont;
    GetSelectionController(getter_AddRefs(selCont));
    NS_ENSURE_TRUE(selCont, NS_ERROR_NO_INTERFACE);

    switch (*aAction) {
      case eNextWord: {
        nsresult rv = selCont->WordExtendForDelete(true);
        // DeleteSelectionWithTransaction() doesn't handle these actions
        // because it's inside batching, so don't confuse it:
        *aAction = eNone;
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
      case ePreviousWord: {
        nsresult rv = selCont->WordExtendForDelete(false);
        *aAction = eNone;
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
      case eNext: {
        nsresult rv = selCont->CharacterExtendForDelete();
        // Don't set aAction to eNone (see Bug 502259)
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
      case ePrevious: {
        // Only extend the selection where the selection is after a UTF-16
        // surrogate pair or a variation selector.
        // For other cases we don't want to do that, in order
        // to make sure that pressing backspace will only delete the last
        // typed character.
        EditorRawDOMPoint atStartOfSelection =
            EditorBase::GetStartPoint(*SelectionRefPtr());
        if (NS_WARN_IF(!atStartOfSelection.IsSet())) {
          return NS_ERROR_FAILURE;
        }

        // node might be anonymous DIV, so we find better text node
        EditorRawDOMPoint insertionPoint =
            FindBetterInsertionPoint(atStartOfSelection);

        if (insertionPoint.IsInTextNode()) {
          const nsTextFragment* data =
              &insertionPoint.GetContainerAsText()->TextFragment();
          uint32_t offset = insertionPoint.Offset();
          if ((offset > 1 &&
               data->IsLowSurrogateFollowingHighSurrogateAt(offset - 1)) ||
              (offset > 0 &&
               gfxFontUtils::IsVarSelector(data->CharAt(offset - 1)))) {
            nsresult rv = selCont->CharacterExtendForBackspace();
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
          }
        }
        return NS_OK;
      }
      case eToBeginningOfLine: {
        // Select to beginning
        nsresult rv = selCont->IntraLineMove(false, true);
        *aAction = eNone;
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
      case eToEndOfLine: {
        nsresult rv = selCont->IntraLineMove(true, true);
        *aAction = eNext;
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        return NS_OK;
      }
      // For avoiding several compiler warnings
      default:
        return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::DeleteSelection(EDirection aAction, EStripWrappers aStripWrappers) {
  nsresult rv = DeleteSelectionAsAction(aAction, aStripWrappers);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to do delete selection");
  return rv;
}

nsresult TextEditor::DeleteSelectionAsAction(EDirection aDirection,
                                             EStripWrappers aStripWrappers,
                                             nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);
  // Showing this assertion is fine if this method is called by outside via
  // mutation event listener or something.  Otherwise, this is called by
  // wrong method.
  NS_ASSERTION(!mPlaceholderBatch,
               "Should be called only when this is the only edit action of the "
               "operation "
               "unless mutation event listener nests some operations");

  EditAction editAction = EditAction::eDeleteSelection;
  switch (aDirection) {
    case nsIEditor::ePrevious:
      editAction = EditAction::eDeleteBackward;
      break;
    case nsIEditor::eNext:
      editAction = EditAction::eDeleteForward;
      break;
    case nsIEditor::ePreviousWord:
      editAction = EditAction::eDeleteWordBackward;
      break;
    case nsIEditor::eNextWord:
      editAction = EditAction::eDeleteWordForward;
      break;
    case nsIEditor::eToBeginningOfLine:
      editAction = EditAction::eDeleteToBeginningOfSoftLine;
      break;
    case nsIEditor::eToEndOfLine:
      editAction = EditAction::eDeleteToEndOfSoftLine;
      break;
  }

  AutoEditActionDataSetter editActionData(*this, editAction, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // If there is an existing selection when an extended delete is requested,
  // platforms that use "caret-style" caret positioning collapse the
  // selection to the  start and then create a new selection.
  // Platforms that use "selection-style" caret positioning just delete the
  // existing selection without extending it.
  if (!SelectionRefPtr()->IsCollapsed()) {
    switch (aDirection) {
      case eNextWord:
      case ePreviousWord:
      case eToBeginningOfLine:
      case eToEndOfLine: {
        if (mCaretStyle != 1) {
          aDirection = eNone;
          break;
        }
        ErrorResult error;
        SelectionRefPtr()->CollapseToStart(error);
        if (NS_WARN_IF(error.Failed())) {
          return EditorBase::ToGenericNSResult(error.StealNSResult());
        }
        break;
      }
      default:
        break;
    }
  }

  // If Selection is still NOT collapsed, it does not important removing
  // range of the operation since we'll remove the selected content.  However,
  // information of direction (backward or forward) may be important for
  // web apps.  E.g., web apps may want to mark selected range as "deleted"
  // and move caret before or after the range.  Therefore, we should forget
  // only the range information but keep range information.  See discussion
  // of the spec issue for the detail:
  // https://github.com/w3c/input-events/issues/82
  if (!SelectionRefPtr()->IsCollapsed()) {
    switch (editAction) {
      case EditAction::eDeleteWordBackward:
      case EditAction::eDeleteToBeginningOfSoftLine:
        editActionData.UpdateEditAction(EditAction::eDeleteBackward);
        break;
      case EditAction::eDeleteWordForward:
      case EditAction::eDeleteToEndOfSoftLine:
        editActionData.UpdateEditAction(EditAction::eDeleteForward);
        break;
      default:
        break;
    }
  }

  // delete placeholder txns merge.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::DeleteTxnName);
  nsresult rv = DeleteSelectionAsSubAction(aDirection, aStripWrappers);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "DeleteSelectionAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::DeleteSelectionAsSubAction(EDirection aDirectionAndAmount,
                                                EStripWrappers aStripWrappers) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mPlaceholderBatch);

  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteSelectedContent, aDirectionAndAmount);

  EditActionResult result =
      HandleDeleteSelection(aDirectionAndAmount, aStripWrappers);
  if (NS_WARN_IF(result.Failed()) || result.Canceled()) {
    return result.Rv();
  }

  // XXX This is odd.  We just tries to remove empty text node here but we
  //     refer `Selection`.  It may be modified by mutation event listeners
  //     so that we should remove the empty text node when we make it empty.
  EditorDOMPoint atNewStartOfSelection(
      EditorBase::GetStartPoint(*SelectionRefPtr()));
  if (NS_WARN_IF(!atNewStartOfSelection.IsSet())) {
    // XXX And also it seems that we don't need to return error here.
    //     Why don't we just ignore?  `Selection::RemoveAllRanges()` may
    //     have been called by mutation event listeners.
    return NS_ERROR_FAILURE;
  }
  if (atNewStartOfSelection.IsInTextNode() &&
      !atNewStartOfSelection.GetContainer()->Length()) {
    nsresult rv = DeleteNodeWithTransaction(
        MOZ_KnownLive(*atNewStartOfSelection.GetContainer()));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // XXX I don't think that this is necessary in anonymous `<div>` element of
  //     TextEditor since there should be at most one text node and at most
  //     one padding `<br>` element so that `<br>` element won't be before
  //     caret.
  if (!TopLevelEditSubActionDataRef().mDidExplicitlySetInterLine) {
    // We prevent the caret from sticking on the left of previous `<br>`
    // element (i.e. the end of previous line) after this deletion. Bug 92124.
    ErrorResult error;
    SelectionRefPtr()->SetInterlinePosition(true, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  return NS_OK;
}

nsresult TextEditor::DeleteSelectionWithTransaction(
    EDirection aDirection, EStripWrappers aStripWrappers) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  RefPtr<EditAggregateTransaction> deleteSelectionTransaction;
  nsCOMPtr<nsINode> deleteNode;
  int32_t deleteCharOffset = 0, deleteCharLength = 0;
  if (!SelectionRefPtr()->IsCollapsed() || aDirection != eNone) {
    deleteSelectionTransaction =
        CreateTxnForDeleteSelection(aDirection, getter_AddRefs(deleteNode),
                                    &deleteCharOffset, &deleteCharLength);
    if (NS_WARN_IF(!deleteSelectionTransaction)) {
      return NS_ERROR_FAILURE;
    }
  }

  RefPtr<CharacterData> deleteCharData =
      CharacterData::FromNodeOrNull(deleteNode);
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eDeleteSelectedContent, aDirection);

  if (AsHTMLEditor()) {
    if (!deleteNode) {
      // XXX We may remove multiple ranges in the following.  Therefore,
      //     this must have a bug since we only add the first range into
      //     the changed range.
      TopLevelEditSubActionDataRef().WillDeleteRange(
          *this, EditorBase::GetStartPoint(*SelectionRefPtr()),
          EditorBase::GetEndPoint(*SelectionRefPtr()));
    } else if (!deleteCharData) {
      MOZ_ASSERT(deleteNode->IsContent());
      TopLevelEditSubActionDataRef().WillDeleteContent(
          *this, *deleteNode->AsContent());
    }
  }

  // Notify nsIEditActionListener::WillDelete[Selection|Text]
  if (!mActionListeners.IsEmpty()) {
    if (!deleteNode) {
      AutoActionListenerArray listeners(mActionListeners);
      for (auto& listener : listeners) {
        listener->WillDeleteSelection(SelectionRefPtr());
      }
    } else if (deleteCharData) {
      AutoActionListenerArray listeners(mActionListeners);
      for (auto& listener : listeners) {
        listener->WillDeleteText(deleteCharData, deleteCharOffset, 1);
      }
    }
  }

  // Delete the specified amount
  nsresult rv = DoTransactionInternal(deleteSelectionTransaction);

  if (AsHTMLEditor() && deleteCharData) {
    MOZ_ASSERT(deleteNode);
    TopLevelEditSubActionDataRef().DidDeleteText(*this,
                                                 EditorRawDOMPoint(deleteNode));
  }

  if (mTextServicesDocument && NS_SUCCEEDED(rv) && deleteNode &&
      !deleteCharData) {
    RefPtr<TextServicesDocument> textServicesDocument = mTextServicesDocument;
    textServicesDocument->DidDeleteNode(deleteNode);
  }

  // Notify nsIEditActionListener::DidDelete[Selection|Text|Node]
  {
    AutoActionListenerArray listeners(mActionListeners);
    if (!deleteNode) {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteSelection(SelectionRefPtr());
      }
    } else if (deleteCharData) {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteText(deleteCharData, deleteCharOffset, 1, rv);
      }
    } else {
      for (auto& listener : mActionListeners) {
        listener->DidDeleteNode(deleteNode, rv);
      }
    }
  }

  return rv;
}

already_AddRefed<Element> TextEditor::DeleteSelectionAndCreateElement(
    nsAtom& aTag) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsresult rv = DeleteSelectionAndPrepareToCreateNode();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  EditorDOMPoint pointToInsert(SelectionRefPtr()->AnchorRef());
  if (!pointToInsert.IsSet()) {
    return nullptr;
  }
  RefPtr<Element> newElement = CreateNodeWithTransaction(aTag, pointToInsert);

  // We want the selection to be just after the new node
  EditorRawDOMPoint afterNewElement(newElement);
  MOZ_ASSERT(afterNewElement.IsSetAndValid());
  DebugOnly<bool> advanced = afterNewElement.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced,
                       "Failed to move offset next to the new element");
  ErrorResult error;
  SelectionRefPtr()->Collapse(afterNewElement, error);
  if (NS_WARN_IF(error.Failed())) {
    // XXX Even if it succeeded to create new element, this returns error
    //     when Selection.Collapse() fails something.  This could occur with
    //     mutation observer or mutation event listener.
    error.SuppressException();
    return nullptr;
  }
  return newElement.forget();
}

nsresult TextEditor::DeleteSelectionAndPrepareToCreateNode() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!SelectionRefPtr()->GetAnchorFocusRange())) {
    return NS_OK;
  }

  if (!SelectionRefPtr()->GetAnchorFocusRange()->Collapsed()) {
    nsresult rv = DeleteSelectionAsSubAction(eNone, eStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(SelectionRefPtr()->GetAnchorFocusRange() &&
                   SelectionRefPtr()->GetAnchorFocusRange()->Collapsed(),
               "Selection not collapsed after delete");
  }

  // If the selection is a chardata node, split it if necessary and compute
  // where to put the new node
  EditorDOMPoint atAnchor(SelectionRefPtr()->AnchorRef());
  if (NS_WARN_IF(!atAnchor.IsSet()) || !atAnchor.IsInDataNode()) {
    return NS_OK;
  }

  if (NS_WARN_IF(!atAnchor.GetContainer()->GetParentNode())) {
    return NS_ERROR_FAILURE;
  }

  if (atAnchor.IsStartOfContainer()) {
    EditorRawDOMPoint atAnchorContainer(atAnchor.GetContainer());
    if (NS_WARN_IF(!atAnchorContainer.IsSetAndValid())) {
      return NS_ERROR_FAILURE;
    }
    ErrorResult error;
    SelectionRefPtr()->Collapse(atAnchorContainer, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  if (atAnchor.IsEndOfContainer()) {
    EditorRawDOMPoint afterAnchorContainer(atAnchor.GetContainer());
    if (NS_WARN_IF(!afterAnchorContainer.AdvanceOffset())) {
      return NS_ERROR_FAILURE;
    }
    ErrorResult error;
    SelectionRefPtr()->Collapse(afterAnchorContainer, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    return NS_OK;
  }

  ErrorResult error;
  nsCOMPtr<nsIContent> newLeftNode = SplitNodeWithTransaction(atAnchor, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  EditorRawDOMPoint atRightNode(atAnchor.GetContainer());
  if (NS_WARN_IF(!atRightNode.IsSet())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atRightNode.IsSetAndValid());
  SelectionRefPtr()->Collapse(atRightNode, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::InsertText(const nsAString& aStringToInsert) {
  nsresult rv = InsertTextAsAction(aStringToInsert);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert text");
  return rv;
}

nsresult TextEditor::InsertTextAsAction(const nsAString& aStringToInsert,
                                        nsIPrincipal* aPrincipal) {
  // Showing this assertion is fine if this method is called by outside via
  // mutation event listener or something.  Otherwise, this is called by
  // wrong method.
  NS_ASSERTION(!mPlaceholderBatch,
               "Should be called only when this is the only edit action of the "
               "operation "
               "unless mutation event listener nests some operations");

  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  // Note that we don't need to replace native line breaks with XP line breaks
  // here because Chrome does not do it.
  MOZ_ASSERT(!aStringToInsert.IsVoid());
  editActionData.SetData(aStringToInsert);

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsresult rv = InsertTextAsSubAction(aStringToInsert);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::InsertTextAsSubAction(const nsAString& aStringToInsert) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mPlaceholderBatch);

  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  EditSubAction editSubAction = ShouldHandleIMEComposition()
                                    ? EditSubAction::eInsertTextComingFromIME
                                    : EditSubAction::eInsertText;

  AutoEditSubActionNotifier startToHandleEditSubAction(*this, editSubAction,
                                                       nsIEditor::eNext);

  EditActionResult result = HandleInsertText(editSubAction, aStringToInsert);
  NS_WARNING_ASSERTION(result.Succeeded(), "HandleInsertText() failed");
  return result.Rv();
}

NS_IMETHODIMP
TextEditor::InsertLineBreak() {
  if (NS_WARN_IF(IsSingleLineEditor())) {
    return NS_ERROR_FAILURE;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertLineBreak);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsresult rv = InsertLineBreakAsSubAction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::InsertLineBreakAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertLineBreak, nsIEditor::eNext);

  EditActionResult result = InsertLineFeedCharacterAtSelection();
  if (result.EditorDestroyed()) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(
      result.Succeeded(),
      "InsertLineFeedCharacterAtSelection() failed, but ignored");
  return result.Rv();
}

nsresult TextEditor::SetTextAsAction(const nsAString& aString,
                                     nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aString.FindChar(static_cast<char16_t>('\r')) == kNotFound);

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetText,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsresult rv = SetTextAsSubAction(aString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetTextAsSubAction() failed");
  return EditorBase::ToGenericNSResult(rv);
}

nsresult TextEditor::ReplaceTextAsAction(const nsAString& aString,
                                         nsRange* aReplaceRange,
                                         nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eReplaceText,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (!AsHTMLEditor()) {
    editActionData.SetData(aString);
  } else {
    editActionData.InitializeDataTransfer(aString);
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);

  // This should emulates inserting text for better undo/redo behavior.
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext);

  if (!aReplaceRange) {
    nsresult rv = SetTextAsSubAction(aString);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetTextAsSubAction() failed");
    return EditorBase::ToGenericNSResult(rv);
  }

  if (NS_WARN_IF(aString.IsEmpty() && aReplaceRange->Collapsed())) {
    return NS_OK;
  }

  // Note that do not notify selectionchange caused by selecting all text
  // because it's preparation of our delete implementation so web apps
  // shouldn't receive such selectionchange before the first mutation.
  AutoUpdateViewBatch preventSelectionChangeEvent(*this);

  // Select the range but as far as possible, we should not create new range
  // even if it's part of special Selection.
  nsresult rv = SelectionRefPtr()->RemoveAllRangesTemporarily();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  ErrorResult error;
  SelectionRefPtr()->AddRangeAndSelectFramesAndNotifyListeners(*aReplaceRange,
                                                               error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  rv = ReplaceSelectionAsSubAction(aString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::SetTextAsSubAction(const nsAString& aString) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mPlaceholderBatch);

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eSetText, nsIEditor::eNext);

  if (IsPlaintextEditor() && !IsIMEComposing() && !IsUndoRedoEnabled() &&
      GetEditAction() != EditAction::eReplaceText && mMaxTextLength < 0) {
    EditActionResult result = SetTextWithoutTransaction(aString);
    if (NS_WARN_IF(result.Failed()) || result.Canceled() || result.Handled()) {
      return result.Rv();
    }
  }

  {
    // Note that do not notify selectionchange caused by selecting all text
    // because it's preparation of our delete implementation so web apps
    // shouldn't receive such selectionchange before the first mutation.
    AutoUpdateViewBatch preventSelectionChangeEvent(*this);

    Element* rootElement = GetRoot();
    if (NS_WARN_IF(!rootElement)) {
      return NS_ERROR_FAILURE;
    }

    // We want to select trailing `<br>` element to remove all nodes to replace
    // all, but TextEditor::SelectEntireDocument() doesn't select such `<br>`
    // elements.
    // XXX We should make ReplaceSelectionAsSubAction() take range.  Then,
    //     we can saving the expensive cost of modifying `Selection` here.
    nsresult rv;
    if (IsEmpty()) {
      rv = SelectionRefPtr()->Collapse(rootElement, 0);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Selection::Collapse() failed, but ignored");
    } else {
      // XXX Oh, we shouldn't select padding `<br>` element for empty last
      //     line here since we will need to recreate it in multiline
      //     text editor.
      ErrorResult error;
      SelectionRefPtr()->SelectAllChildren(*rootElement, error);
      NS_WARNING_ASSERTION(
          !error.Failed(),
          "Selection::SelectAllChildren() failed, but ignored");
      rv = error.StealNSResult();
    }
    if (NS_SUCCEEDED(rv)) {
      DebugOnly<nsresult> rvIgnored = ReplaceSelectionAsSubAction(aString);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                           "ReplaceSelectionAsSubAction() failed, but ignored");
    }
  }

  // Destroying AutoUpdateViewBatch may cause destroying us.
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  return NS_OK;
}

nsresult TextEditor::ReplaceSelectionAsSubAction(const nsAString& aString) {
  if (aString.IsEmpty()) {
    nsresult rv = DeleteSelectionAsSubAction(eNone, eStrip);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  nsresult rv = InsertTextAsSubAction(aString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

bool TextEditor::EnsureComposition(WidgetCompositionEvent& aCompositionEvent) {
  if (mComposition) {
    return true;
  }
  // The compositionstart event must cause creating new TextComposition
  // instance at being dispatched by IMEStateManager.
  mComposition = IMEStateManager::GetTextCompositionFor(&aCompositionEvent);
  if (!mComposition) {
    // However, TextComposition may be committed before the composition
    // event comes here.
    return false;
  }
  mComposition->StartHandlingComposition(this);
  return true;
}

nsresult TextEditor::OnCompositionStart(
    WidgetCompositionEvent& aCompositionStartEvent) {
  if (NS_WARN_IF(mComposition)) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eStartComposition);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  EnsureComposition(aCompositionStartEvent);
  NS_WARNING_ASSERTION(mComposition, "Failed to get TextComposition instance?");
  return NS_OK;
}

nsresult TextEditor::OnCompositionChange(
    WidgetCompositionEvent& aCompositionChangeEvent) {
  MOZ_ASSERT(aCompositionChangeEvent.mMessage == eCompositionChange,
             "The event should be eCompositionChange");

  if (NS_WARN_IF(!mComposition)) {
    return NS_ERROR_FAILURE;
  }

  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eUpdateComposition);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // If:
  // - new composition string is not empty,
  // - there is no composition string in the DOM tree,
  // - and there is non-collapsed Selection,
  // the selected content will be removed by this composition.
  if (aCompositionChangeEvent.mData.IsEmpty() &&
      mComposition->String().IsEmpty() && !SelectionRefPtr()->IsCollapsed()) {
    editActionData.UpdateEditAction(EditAction::eDeleteByComposition);
  }

  // If Input Events Level 2 is enabled, EditAction::eDeleteByComposition is
  // mapped to EditorInputType::eDeleteByComposition and it requires null
  // for InputEvent.data.  Therefore, only otherwise, we should set data.
  if (ToInputType(editActionData.GetEditAction()) !=
      EditorInputType::eDeleteByComposition) {
    MOZ_ASSERT(ToInputType(editActionData.GetEditAction()) ==
               EditorInputType::eInsertCompositionText);
    MOZ_ASSERT(!aCompositionChangeEvent.mData.IsVoid());
    editActionData.SetData(aCompositionChangeEvent.mData);
  }

  if (!EnsureComposition(aCompositionChangeEvent)) {
    return NS_OK;
  }

  if (NS_WARN_IF(!GetPresShell())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // NOTE: TextComposition should receive selection change notification before
  //       CompositionChangeEventHandlingMarker notifies TextComposition of the
  //       end of handling compositionchange event because TextComposition may
  //       need to ignore selection changes caused by composition.  Therefore,
  //       CompositionChangeEventHandlingMarker must be destroyed after a call
  //       of NotifiyEditorObservers(eNotifyEditorObserversOfEnd) or
  //       NotifiyEditorObservers(eNotifyEditorObserversOfCancel) which notifies
  //       TextComposition of a selection change.
  MOZ_ASSERT(
      !mPlaceholderBatch,
      "UpdateIMEComposition() must be called without place holder batch");
  TextComposition::CompositionChangeEventHandlingMarker
      compositionChangeEventHandlingMarker(mComposition,
                                           &aCompositionChangeEvent);

  RefPtr<nsCaret> caret = GetCaret();

  nsresult rv;
  {
    AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::IMETxnName);

    MOZ_ASSERT(
        mIsInEditSubAction,
        "AutoPlaceholderBatch should've notified the observes of before-edit");
    rv = InsertTextAsSubAction(aCompositionChangeEvent.mData);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Failed to insert new composition string");

    if (caret) {
      caret->SetSelection(SelectionRefPtr());
    }
  }

  // If still composing, we should fire input event via observer.
  // Note that if the composition will be committed by the following
  // compositionend event, we don't need to notify editor observes of this
  // change.
  // NOTE: We must notify after the auto batch will be gone.
  if (!aCompositionChangeEvent.IsFollowedByCompositionEnd()) {
    NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

void TextEditor::OnCompositionEnd(
    WidgetCompositionEvent& aCompositionEndEvent) {
  if (NS_WARN_IF(!mComposition)) {
    return;
  }

  EditAction editAction = aCompositionEndEvent.mData.IsEmpty()
                              ? EditAction::eCancelComposition
                              : EditAction::eCommitComposition;
  AutoEditActionDataSetter editActionData(*this, editAction);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return;
  }
  // If Input Events Level 2 is enabled, EditAction::eCancelComposition is
  // mapped to EditorInputType::eDeleteCompositionText and it requires null
  // for InputEvent.data.  Therefore, only otherwise, we should set data.
  if (ToInputType(editAction) != EditorInputType::eDeleteCompositionText) {
    MOZ_ASSERT(
        ToInputType(editAction) == EditorInputType::eInsertCompositionText ||
        ToInputType(editAction) == EditorInputType::eInsertFromComposition);
    MOZ_ASSERT(!aCompositionEndEvent.mData.IsVoid());
    editActionData.SetData(aCompositionEndEvent.mData);
  }

  // commit the IME transaction..we can get at it via the transaction mgr.
  // Note that this means IME won't work without an undo stack!
  if (mTransactionManager) {
    nsCOMPtr<nsITransaction> txn = mTransactionManager->PeekUndoStack();
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryInterface(txn);
    if (plcTxn) {
      DebugOnly<nsresult> rv = plcTxn->Commit();
      NS_ASSERTION(NS_SUCCEEDED(rv),
                   "nsIAbsorbingTransaction::Commit() failed");
    }
  }

  // Composition string may have hidden the caret.  Therefore, we need to
  // cancel it here.
  HideCaret(false);

  // FYI: mComposition still keeps storing container text node of committed
  //      string, its offset and length.  However, they will be invalidated
  //      soon since its Destroy() will be called by IMEStateManager.
  mComposition->EndHandlingComposition(this);
  mComposition = nullptr;

  // notify editor observers of action
  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
}

already_AddRefed<Element> TextEditor::GetInputEventTargetElement() {
  nsCOMPtr<Element> target = do_QueryInterface(mEventTarget);
  return target.forget();
}

bool TextEditor::IsEmpty() const {
  if (mPaddingBRElementForEmptyEditor) {
    return true;
  }

  // Even if there is no padding <br> element for empty editor, we should be
  // detected as empty editor if all the children are text nodes and these
  // have no content.
  Element* anonymousDivElement = GetRoot();
  if (!anonymousDivElement) {
    return true;  // Don't warn it, this is possible, e.g., 997805.html
  }

  // Only when there is non-empty text node, we are not empty.
  return !anonymousDivElement->GetFirstChild() ||
         !anonymousDivElement->GetFirstChild()->IsText() ||
         !anonymousDivElement->GetFirstChild()->Length();
}

NS_IMETHODIMP
TextEditor::GetDocumentIsEmpty(bool* aDocumentIsEmpty) {
  MOZ_ASSERT(aDocumentIsEmpty);
  *aDocumentIsEmpty = IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::GetTextLength(int32_t* aCount) {
  MOZ_ASSERT(aCount);

  // initialize out params
  *aCount = 0;

  // special-case for empty document, to account for the padding <br> element
  // for empty editor.
  // XXX This should be overridden by `HTMLEditor` and we should return the
  //     first text node's length from `TextEditor` instead.  The following
  //     code is too expensive.
  if (IsEmpty()) {
    return NS_OK;
  }

  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t totalLength = 0;
  PostContentIterator postOrderIter;
  postOrderIter.Init(rootElement);
  for (; !postOrderIter.IsDone(); postOrderIter.Next()) {
    nsCOMPtr<nsINode> currentNode = postOrderIter.GetCurrentNode();
    if (IsTextNode(currentNode) && IsEditable(currentNode)) {
      totalLength += currentNode->Length();
    }
  }

  *aCount = totalLength;
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::GetWrapWidth(int32_t* aWrapColumn) {
  if (NS_WARN_IF(!aWrapColumn)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aWrapColumn = WrapWidth();
  return NS_OK;
}

//
// See if the style value includes this attribute, and if it does,
// cut out everything from the attribute to the next semicolon.
//
static void CutStyle(const char* stylename, nsString& styleValue) {
  // Find the current wrapping type:
  int32_t styleStart = styleValue.Find(stylename, true);
  if (styleStart >= 0) {
    int32_t styleEnd = styleValue.Find(";", false, styleStart);
    if (styleEnd > styleStart) {
      styleValue.Cut(styleStart, styleEnd - styleStart + 1);
    } else {
      styleValue.Cut(styleStart, styleValue.Length() - styleStart);
    }
  }
}

NS_IMETHODIMP
TextEditor::SetWrapWidth(int32_t aWrapColumn) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eSetWrapWidth);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  SetWrapColumn(aWrapColumn);

  // Make sure we're a plaintext editor, otherwise we shouldn't
  // do the rest of this.
  if (!IsPlaintextEditor()) {
    return NS_OK;
  }

  // Ought to set a style sheet here ...
  // Probably should keep around an mPlaintextStyleSheet for this purpose.
  dom::Element* rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  // Get the current style for this root element:
  nsAutoString styleValue;
  rootElement->GetAttr(kNameSpaceID_None, nsGkAtoms::style, styleValue);

  // We'll replace styles for these values:
  CutStyle("white-space", styleValue);
  CutStyle("width", styleValue);
  CutStyle("font-family", styleValue);

  // If we have other style left, trim off any existing semicolons
  // or whitespace, then add a known semicolon-space:
  if (!styleValue.IsEmpty()) {
    styleValue.Trim("; \t", false, true);
    styleValue.AppendLiteral("; ");
  }

  // Make sure we have fixed-width font.  This should be done for us,
  // but it isn't, see bug 22502, so we have to add "font: -moz-fixed;".
  // Only do this if we're wrapping.
  if (IsWrapHackEnabled() && aWrapColumn >= 0) {
    styleValue.AppendLiteral("font-family: -moz-fixed; ");
  }

  // and now we're ready to set the new whitespace/wrapping style.
  if (aWrapColumn > 0) {
    // Wrap to a fixed column.
    styleValue.AppendLiteral("white-space: pre-wrap; width: ");
    styleValue.AppendInt(aWrapColumn);
    styleValue.AppendLiteral("ch;");
  } else if (!aWrapColumn) {
    styleValue.AppendLiteral("white-space: pre-wrap;");
  } else {
    styleValue.AppendLiteral("white-space: pre;");
  }

  return rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleValue,
                              true);
}

NS_IMETHODIMP
TextEditor::GetNewlineHandling(int32_t* aNewlineHandling) {
  NS_ENSURE_ARG_POINTER(aNewlineHandling);

  *aNewlineHandling = mNewlineHandling;
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::SetNewlineHandling(int32_t aNewlineHandling) {
  mNewlineHandling = aNewlineHandling;

  return NS_OK;
}

nsresult TextEditor::UndoAsAction(uint32_t aCount, nsIPrincipal* aPrincipal) {
  if (aCount == 0 || IsReadonly() || IsDisabled()) {
    return NS_OK;
  }

  // If we don't have transaction in the undo stack, we shouldn't notify
  // anybody of trying to undo since it's not useful notification but we
  // need to pay some runtime cost.
  if (!CanUndo()) {
    return NS_OK;
  }

  // If there is composition, we shouldn't allow to undo with committing
  // composition since Chrome doesn't allow it and it doesn't make sense
  // because committing composition causes one transaction and Undo(1)
  // undoes the committing composition.
  if (GetComposition()) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eUndo, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoUpdateViewBatch preventSelectionChangeEvent(*this);

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);
  if (NS_WARN_IF(!CanUndo()) || NS_WARN_IF(Destroyed())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;
  {
    AutoEditSubActionNotifier startToHandleEditSubAction(
        *this, EditSubAction::eUndo, nsIEditor::eNone);

    RefPtr<TransactionManager> transactionManager(mTransactionManager);
    for (uint32_t i = 0; i < aCount; ++i) {
      if (NS_WARN_IF(NS_FAILED(transactionManager->Undo()))) {
        break;
      }
      DoAfterUndoTransaction();
    }

    if (NS_WARN_IF(!mRootElement)) {
      rv = NS_ERROR_FAILURE;
    } else {
      // The idea here is to see if the magic empty node has suddenly
      // reappeared as the result of the undo.  If it has, set our state
      // so we remember it.  There is a tradeoff between doing here and
      // at redo, or doing it everywhere else that might care.  Since undo
      // and redo are relatively rare, it makes sense to take the (small)
      // performance hit here.
      nsIContent* leftMostChild = GetLeftmostChild(mRootElement);
      if (leftMostChild &&
          EditorBase::IsPaddingBRElementForEmptyEditor(*leftMostChild)) {
        mPaddingBRElementForEmptyEditor =
            static_cast<HTMLBRElement*>(leftMostChild);
      } else {
        mPaddingBRElementForEmptyEditor = nullptr;
      }
    }
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::RedoAsAction(uint32_t aCount, nsIPrincipal* aPrincipal) {
  if (aCount == 0 || IsReadonly() || IsDisabled()) {
    return NS_OK;
  }

  // If we don't have transaction in the redo stack, we shouldn't notify
  // anybody of trying to redo since it's not useful notification but we
  // need to pay some runtime cost.
  if (!CanRedo()) {
    return NS_OK;
  }

  // If there is composition, we shouldn't allow to redo with committing
  // composition since Chrome doesn't allow it and it doesn't make sense
  // because committing composition causes removing all transactions from
  // the redo queue.  So, it becomes impossible to redo anything.
  if (GetComposition()) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRedo, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoUpdateViewBatch preventSelectionChangeEvent(*this);

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);
  if (NS_WARN_IF(!CanRedo()) || NS_WARN_IF(Destroyed())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;
  {
    AutoEditSubActionNotifier startToHandleEditSubAction(
        *this, EditSubAction::eRedo, nsIEditor::eNone);

    RefPtr<TransactionManager> transactionManager(mTransactionManager);
    for (uint32_t i = 0; i < aCount; ++i) {
      if (NS_WARN_IF(NS_FAILED(transactionManager->Redo()))) {
        break;
      }
      DoAfterRedoTransaction();
    }

    if (NS_WARN_IF(!mRootElement)) {
      rv = NS_ERROR_FAILURE;
    } else {
      // We may take empty <br> element for empty editor back with this redo.
      // We need to store it again.
      // XXX Looks like that this is too slow if there are a lot of nodes.
      //     Shouldn't we just scan children in the root?
      nsCOMPtr<nsIHTMLCollection> nodeList =
          mRootElement->GetElementsByTagName(NS_LITERAL_STRING("br"));
      MOZ_ASSERT(nodeList);
      Element* brElement =
          nodeList->Length() == 1 ? nodeList->Item(0) : nullptr;
      if (brElement &&
          EditorBase::IsPaddingBRElementForEmptyEditor(*brElement)) {
        mPaddingBRElementForEmptyEditor =
            static_cast<HTMLBRElement*>(brElement);
      } else {
        mPaddingBRElementForEmptyEditor = nullptr;
      }
    }
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

bool TextEditor::IsCopyToClipboardAllowedInternal() const {
  MOZ_ASSERT(IsEditActionDataAvailable());
  if (SelectionRefPtr()->IsCollapsed()) {
    return false;
  }

  if (!IsSingleLineEditor() || !IsPasswordEditor()) {
    return true;
  }

  // If we're a password editor, we should allow selected text to be copied
  // to the clipboard only when selection range is in unmasked range.
  if (IsAllMasked() || IsMaskingPassword() || mUnmaskedLength == 0) {
    return false;
  }

  // If there are 2 or more ranges, we don't allow to copy/cut for now since
  // we need to check whether all ranges are in unmasked range or not.
  // Anyway, such operation in password field does not make sense.
  if (SelectionRefPtr()->RangeCount() > 1) {
    return false;
  }

  uint32_t selectionStart = 0, selectionEnd = 0;
  nsContentUtils::GetSelectionInTextControl(SelectionRefPtr(), mRootElement,
                                            selectionStart, selectionEnd);
  return mUnmaskedStart <= selectionStart && UnmaskedEnd() >= selectionEnd;
}

bool TextEditor::FireClipboardEvent(EventMessage aEventMessage,
                                    int32_t aSelectionType,
                                    bool* aActionTaken) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (aEventMessage == ePaste) {
    CommitComposition();
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (NS_WARN_IF(!presShell)) {
    return false;
  }

  if (!nsCopySupport::FireClipboardEvent(
          aEventMessage, aSelectionType, presShell,
          MOZ_KnownLive(SelectionRefPtr()), aActionTaken)) {
    return false;
  }

  // If the event handler caused the editor to be destroyed, return false.
  // Otherwise return true to indicate that the event was not cancelled.
  return !mDidPreDestroy;
}

nsresult TextEditor::CutAsAction(nsIPrincipal* aPrincipal) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eCut, aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool actionTaken = false;
  if (FireClipboardEvent(eCut, nsIClipboard::kGlobalClipboard, &actionTaken)) {
    // XXX This transaction name is referred by PlaceholderTransaction::Merge()
    //     so that we need to keep using it here.
    AutoPlaceholderBatch treatAsOneTransaction(*this,
                                               *nsGkAtoms::DeleteTxnName);
    nsresult rv = DeleteSelectionAsSubAction(eNone, eStrip);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return EditorBase::ToGenericNSResult(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "DeleteSelectionAsSubAction() failed, but ignored");
  }
  return EditorBase::ToGenericNSResult(
      actionTaken ? NS_OK : NS_ERROR_EDITOR_ACTION_CANCELED);
}

bool TextEditor::IsCutCommandEnabled() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  // Cut is always enabled in HTML documents, but if the document is chrome,
  // let it control it.
  Document* document = GetDocument();
  if (document && document->IsHTMLOrXHTML() &&
      !nsContentUtils::IsChromeDoc(document)) {
    return true;
  }

  return IsModifiable() && IsCopyToClipboardAllowedInternal();
}

NS_IMETHODIMP
TextEditor::Copy() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eCopy);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool actionTaken = false;
  FireClipboardEvent(eCopy, nsIClipboard::kGlobalClipboard, &actionTaken);

  return EditorBase::ToGenericNSResult(
      actionTaken ? NS_OK : NS_ERROR_EDITOR_ACTION_CANCELED);
}

bool TextEditor::IsCopyCommandEnabled() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  // Copy is always enabled in HTML documents, but if the document is chrome,
  // let it control it.
  Document* document = GetDocument();
  if (document && document->IsHTMLOrXHTML() &&
      !nsContentUtils::IsChromeDoc(document)) {
    return true;
  }

  return IsCopyToClipboardAllowedInternal();
}

bool TextEditor::CanDeleteSelection() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  return IsModifiable() && !SelectionRefPtr()->IsCollapsed();
}

already_AddRefed<nsIDocumentEncoder> TextEditor::GetAndInitDocEncoder(
    const nsAString& aFormatType, uint32_t aDocumentEncoderFlags,
    const nsACString& aCharset) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  if (!mCachedDocumentEncoder ||
      !mCachedDocumentEncoderType.Equals(aFormatType)) {
    nsAutoCString formatType;
    LossyAppendUTF16toASCII(aFormatType, formatType);
    docEncoder = do_createDocumentEncoder(PromiseFlatCString(formatType).get());
    if (NS_WARN_IF(!docEncoder)) {
      return nullptr;
    }
    mCachedDocumentEncoder = docEncoder;
    mCachedDocumentEncoderType = aFormatType;
  } else {
    docEncoder = mCachedDocumentEncoder;
  }

  RefPtr<Document> doc = GetDocument();
  NS_ASSERTION(doc, "Need a document");

  nsresult rv = docEncoder->NativeInit(
      doc, aFormatType,
      aDocumentEncoderFlags | nsIDocumentEncoder::RequiresReinitAfterOutput);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  if (!aCharset.IsEmpty() && !aCharset.EqualsLiteral("null")) {
    docEncoder->SetCharset(aCharset);
  }

  int32_t wrapWidth = WrapWidth();
  if (wrapWidth >= 0) {
    Unused << docEncoder->SetWrapColumn(wrapWidth);
  }

  // Set the selection, if appropriate.
  // We do this either if the OutputSelectionOnly flag is set,
  // in which case we use our existing selection ...
  if (aDocumentEncoderFlags & nsIDocumentEncoder::OutputSelectionOnly) {
    rv = docEncoder->SetSelection(SelectionRefPtr());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }
  // ... or if the root element is not a body,
  // in which case we set the selection to encompass the root.
  else {
    dom::Element* rootElement = GetRoot();
    if (NS_WARN_IF(!rootElement)) {
      return nullptr;
    }
    if (!rootElement->IsHTMLElement(nsGkAtoms::body)) {
      rv = docEncoder->SetContainerNode(rootElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
    }
  }

  return docEncoder.forget();
}

NS_IMETHODIMP
TextEditor::OutputToString(const nsAString& aFormatType,
                           uint32_t aDocumentEncoderFlags,
                           nsAString& aOutputString) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv =
      ComputeValueInternal(aFormatType, aDocumentEncoderFlags, aOutputString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "ComputeValueInternal() failed");
  // This is low level API for XUL application.  So, we should return raw
  // error code here.
  return rv;
}

nsresult TextEditor::ComputeValueInternal(const nsAString& aFormatType,
                                          uint32_t aDocumentEncoderFlags,
                                          nsAString& aOutputString) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // First, let's try to get the value simply only from text node if the
  // caller wants plaintext value.
  if (aFormatType.LowerCaseEqualsLiteral("text/plain")) {
    // If it's necessary to check selection range or the editor wraps hard,
    // we need some complicated handling.  In such case, we need to use the
    // expensive path.
    // XXX Anything else what we cannot return the text node data simply?
    if (!(aDocumentEncoderFlags & (nsIDocumentEncoder::OutputSelectionOnly |
                                   nsIDocumentEncoder::OutputWrap))) {
      EditActionResult result =
          ComputeValueFromTextNodeAndPaddingBRElement(aOutputString);
      if (NS_WARN_IF(result.Failed()) || result.Canceled() ||
          result.Handled()) {
        return result.Rv();
      }
    }
  }

  nsAutoCString charset;
  nsresult rv = GetDocumentCharsetInternal(charset);
  if (NS_FAILED(rv) || charset.IsEmpty()) {
    charset.AssignLiteral("windows-1252");
  }

  nsCOMPtr<nsIDocumentEncoder> encoder =
      GetAndInitDocEncoder(aFormatType, aDocumentEncoderFlags, charset);
  if (NS_WARN_IF(!encoder)) {
    return NS_ERROR_FAILURE;
  }

  rv = encoder->EncodeToString(aOutputString);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "nsIDocumentEncoder::EncodeToString() failed");
  return rv;
}

nsresult TextEditor::PasteAsQuotationAsAction(int32_t aClipboardType,
                                              bool aDispatchPasteEvent,
                                              nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aClipboardType == nsIClipboard::kGlobalClipboard ||
             aClipboardType == nsIClipboard::kSelectionClipboard);

  AutoEditActionDataSetter editActionData(*this, EditAction::ePasteAsQuotation,
                                          aPrincipal);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // XXX Why don't we dispatch ePaste event here?

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareTransferable(getter_AddRefs(trans));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  if (!trans) {
    return NS_OK;
  }

  // Get the Data from the clipboard
  clipboard->GetData(trans, aClipboardType);

  // Now we ask the transferable for the data
  // it still owns the data, we just have a pointer to it.
  // If it can't support a "text" output of the data the call will fail
  nsCOMPtr<nsISupports> genericDataObj;
  nsAutoCString flav;
  rv = trans->GetAnyTransferData(flav, getter_AddRefs(genericDataObj));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }

  if (!flav.EqualsLiteral(kUnicodeMime) &&
      !flav.EqualsLiteral(kMozTextInternal)) {
    return NS_OK;
  }

  if (nsCOMPtr<nsISupportsString> text = do_QueryInterface(genericDataObj)) {
    nsAutoString stuffToPaste;
    text->GetData(stuffToPaste);
    editActionData.SetData(stuffToPaste);
    if (!stuffToPaste.IsEmpty()) {
      AutoPlaceholderBatch treatAsOneTransaction(*this);
      nsresult rv = InsertWithQuotationsAsSubAction(stuffToPaste);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "InsertWithQuotationsAsSubAction() failed");
      return EditorBase::ToGenericNSResult(rv);
    }
  }
  return NS_OK;
}

nsresult TextEditor::InsertWithQuotationsAsSubAction(
    const nsAString& aQuotedText) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (IsReadonly() || IsDisabled()) {
    return NS_OK;
  }

  // Let the citer quote it for us:
  nsString quotedStuff;
  nsresult rv = InternetCiter::GetCiteString(aQuotedText, quotedStuff);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // It's best to put a blank line after the quoted text so that mails
  // written without thinking won't be so ugly.
  if (!aQuotedText.IsEmpty() && (aQuotedText.Last() != char16_t('\n'))) {
    quotedStuff.Append(char16_t('\n'));
  }

  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext);

  // XXX Do we need to support paste-as-quotation in password editor (and
  //     also in single line editor)?
  MaybeDoAutoPasswordMasking();

  rv = EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InsertTextAsSubAction(quotedStuff);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "InsertTextAsSubAction() failed");
  return rv;
}

nsresult TextEditor::SharedOutputString(uint32_t aFlags, bool* aIsCollapsed,
                                        nsAString& aResult) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  *aIsCollapsed = SelectionRefPtr()->IsCollapsed();

  if (!*aIsCollapsed) {
    aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  }
  // If the selection isn't collapsed, we'll use the whole document.
  nsresult rv =
      ComputeValueInternal(NS_LITERAL_STRING("text/plain"), aFlags, aResult);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "ComputeValueInternal() failed");
  return rv;
}

void TextEditor::OnStartToHandleTopLevelEditSubAction(
    EditSubAction aEditSubAction, nsIEditor::EDirection aDirection) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  EditorBase::OnStartToHandleTopLevelEditSubAction(aEditSubAction, aDirection);
  if (!rules) {
    return;
  }

  MOZ_ASSERT(GetTopLevelEditSubAction() == aEditSubAction);
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() == aDirection);

  if (aEditSubAction == EditSubAction::eSetText) {
    // SetText replaces all text, so spell checker handles starting from the
    // start of new value.
    SetSpellCheckRestartPoint(EditorDOMPoint(mRootElement, 0));
  } else {
    bool useSelectionAnchor = true;
    if (aEditSubAction == EditSubAction::eInsertText ||
        aEditSubAction == EditSubAction::eInsertTextComingFromIME) {
      // For spell checker, previous selected node should be text node if
      // possible. If anchor is root of editor, it may become invalid offset
      // after inserting text.
      EditorRawDOMPoint point = FindBetterInsertionPoint(
          EditorRawDOMPoint(SelectionRefPtr()->AnchorRef()));
      if (point.IsSet()) {
        SetSpellCheckRestartPoint(point);
        useSelectionAnchor = false;
      }
    }
    if (useSelectionAnchor && SelectionRefPtr()->AnchorRef().IsSet()) {
      SetSpellCheckRestartPoint(
          EditorRawDOMPoint(SelectionRefPtr()->AnchorRef()));
    }
  }

  DebugOnly<nsresult> rv = rules->BeforeEdit();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "TextEditRules::BeforeEdit() failed to handle something");
}

void TextEditor::OnEndHandlingTopLevelEditSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (mRules) {
    // Protect the edit rules object from dying
    RefPtr<TextEditRules> rules(mRules);

    // post processing
    DebugOnly<nsresult> rv = rules->AfterEdit();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "TextEditRules::AfterEdit() failed to handle something");
  }
  EditorBase::OnEndHandlingTopLevelEditSubAction();
  MOZ_ASSERT(!GetTopLevelEditSubAction());
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() == eNone);
}

nsresult TextEditor::SelectEntireDocument() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());

  if (!mInitSucceeded) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  Element* anonymousDivElement = GetRoot();
  if (NS_WARN_IF(!anonymousDivElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // If we're empty, don't select all children because that would select the
  // padding <br> element for empty editor.
  if (IsEmpty()) {
    nsresult rv = SelectionRefPtr()->Collapse(anonymousDivElement, 0);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to move caret to start of the editor root element");
    return rv;
  }

  // XXX We just need to select all of first text node (if there is).
  //     Why do we do this kind of complicated things?

  // Don't select the trailing BR node if we have one
  nsCOMPtr<nsIContent> childNode;
  nsresult rv = EditorBase::GetEndChildNode(*SelectionRefPtr(),
                                            getter_AddRefs(childNode));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (childNode) {
    childNode = childNode->GetPreviousSibling();
  }

  if (childNode && EditorBase::IsPaddingBRElementForEmptyLastLine(*childNode)) {
    ErrorResult error;
    MOZ_KnownLive(SelectionRefPtr())
        ->SetStartAndEndInLimiter(RawRangeBoundary(anonymousDivElement, 0),
                                  EditorRawDOMPoint(childNode), error);
    NS_WARNING_ASSERTION(!error.Failed(),
                         "Failed to select all children of the editor root "
                         "element except the padding <br> element");
    return error.StealNSResult();
  }

  ErrorResult error;
  SelectionRefPtr()->SelectAllChildren(*anonymousDivElement, error);
  NS_WARNING_ASSERTION(
      !error.Failed(),
      "Failed to select all children of the editor root element");
  return error.StealNSResult();
}

EventTarget* TextEditor::GetDOMEventTarget() { return mEventTarget; }

nsresult TextEditor::SetAttributeOrEquivalent(Element* aElement,
                                              nsAtom* aAttribute,
                                              const nsAString& aValue,
                                              bool aSuppressTransaction) {
  if (NS_WARN_IF(!aElement) || NS_WARN_IF(!aAttribute)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetAttribute);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = SetAttributeWithTransaction(*aElement, *aAttribute, aValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::RemoveAttributeOrEquivalent(Element* aElement,
                                                 nsAtom* aAttribute,
                                                 bool aSuppressTransaction) {
  if (NS_WARN_IF(!aElement) || NS_WARN_IF(!aAttribute)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eRemoveAttribute);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RemoveAttributeWithTransaction(*aElement, *aAttribute);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::EnsurePaddingBRElementForEmptyEditor() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!AsHTMLEditor());

  // If there is padding <br> element for empty editor, we have no work to do.
  if (mPaddingBRElementForEmptyEditor) {
    return NS_OK;
  }

  // Likewise, nothing to be done if we could never have inserted a trailing
  // <br> element.
  // XXX Why don't we use same path for <textarea> and <input>?
  if (IsSingleLineEditor()) {
    nsresult rv = MaybeCreatePaddingBRElementForEmptyEditor();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to create padding <br> element for empty editor");
    return rv;
  }

  if (NS_WARN_IF(!mRootElement)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t childCount = mRootElement->GetChildCount();
  if (childCount == 0) {
    nsresult rv = MaybeCreatePaddingBRElementForEmptyEditor();
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to create padding <br> element for empty editor");
    return rv;
  }

  if (childCount > 1) {
    return NS_OK;
  }

  RefPtr<HTMLBRElement> brElement =
      HTMLBRElement::FromNodeOrNull(mRootElement->GetFirstChild());
  if (!brElement ||
      !EditorBase::IsPaddingBRElementForEmptyLastLine(*brElement)) {
    return NS_OK;
  }

  // Rather than deleting this node from the DOM tree we should instead
  // morph this <br> element into the padding <br> element for editor.
  mPaddingBRElementForEmptyEditor = std::move(brElement);
  mPaddingBRElementForEmptyEditor->UnsetFlags(NS_PADDING_FOR_EMPTY_LAST_LINE);
  mPaddingBRElementForEmptyEditor->SetFlags(NS_PADDING_FOR_EMPTY_EDITOR);

  return NS_OK;
}

nsresult TextEditor::SetUnmaskRangeInternal(uint32_t aStart, uint32_t aLength,
                                            uint32_t aTimeout, bool aNotify,
                                            bool aForceStartMasking) {
  mIsMaskingPassword = aForceStartMasking || aTimeout != 0;

  // We cannot manage multiple unmasked ranges so that shrink the previous
  // range first.
  if (!IsAllMasked()) {
    mUnmaskedLength = 0;
    if (mMaskTimer) {
      mMaskTimer->Cancel();
    }
  }

  // If we're not a password editor, return error since this call does not
  // make sense.
  if (!IsPasswordEditor()) {
    if (mMaskTimer) {
      mMaskTimer = nullptr;
    }
    return NS_ERROR_NOT_AVAILABLE;
  }

  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  Text* text = Text::FromNodeOrNull(rootElement->GetFirstChild());
  if (!text) {
    // There is no anonymous text node in the editor.
    return aStart > 0 && aStart != UINT32_MAX ? NS_ERROR_INVALID_ARG : NS_OK;
  }

  if (aStart < UINT32_MAX) {
    uint32_t valueLength = text->Length();
    if (aStart >= valueLength) {
      return NS_ERROR_INVALID_ARG;  // There is no character can be masked.
    }
    // If aStart is middle of a surrogate pair, expand it to include the
    // preceding high surrogate because the caller may want to show a
    // character before the character at `aStart + 1`.
    const nsTextFragment* textFragment = text->GetText();
    if (textFragment->IsLowSurrogateFollowingHighSurrogateAt(aStart)) {
      mUnmaskedStart = aStart - 1;
      // If caller collapses the range, keep it.  Otherwise, expand the length.
      if (aLength > 0) {
        ++aLength;
      }
    } else {
      mUnmaskedStart = aStart;
    }
    mUnmaskedLength = std::min(valueLength - mUnmaskedStart, aLength);
    // If unmasked end is middle of a surrogate pair, expand it to include
    // the following low surrogate because the caller may want to show a
    // character after the character at `aStart + aLength`.
    if (UnmaskedEnd() < valueLength &&
        textFragment->IsLowSurrogateFollowingHighSurrogateAt(UnmaskedEnd())) {
      ++mUnmaskedLength;
    }
    // If it's first time to mask the unmasking characters with timer, create
    // the timer now.  Then, we'll keep using it for saving the creation cost.
    if (!mMaskTimer && aLength && aTimeout && mUnmaskedLength) {
      mMaskTimer = NS_NewTimer();
    }
  } else {
    if (NS_WARN_IF(aLength != 0)) {
      return NS_ERROR_INVALID_ARG;
    }
    mUnmaskedStart = UINT32_MAX;
    mUnmaskedLength = 0;
  }

  // Notify nsTextFrame of this update if the caller wants this to do it.
  // Only in this case, script may run.
  if (aNotify) {
    MOZ_ASSERT(IsEditActionDataAvailable());

    RefPtr<Document> document = GetDocument();
    if (NS_WARN_IF(!document)) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    // Notify nsTextFrame of masking range change.
    if (PresShell* presShell = document->GetObservingPresShell()) {
      uint32_t valueLength = text->Length();
      CharacterDataChangeInfo changeInfo = {false, 0, valueLength, valueLength,
                                            nullptr};
      presShell->CharacterDataChanged(text, changeInfo);
    }

    // Scroll caret into the view since masking or unmasking character may
    // move caret to outside of the view.
    ScrollSelectionIntoView(false);

    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
  }

  if (!IsAllMasked() && aTimeout != 0) {
    // Initialize the timer to mask the range automatically.
    MOZ_ASSERT(mMaskTimer);
    mMaskTimer->InitWithCallback(this, aTimeout, nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
}

// static
char16_t TextEditor::PasswordMask() {
  char16_t ret = LookAndFeel::GetPasswordCharacter();
  if (!ret) {
    ret = '*';
  }
  return ret;
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY
NS_IMETHODIMP
TextEditor::Notify(nsITimer* aTimer) {
  // Check whether our text editor's password flag was changed before this
  // "hide password character" timer actually fires.
  if (!IsPasswordEditor()) {
    return NS_OK;
  }

  if (IsAllMasked()) {
    return NS_OK;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eHidePassword);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Mask all characters.
  nsresult rv = MaskAllCharactersAndNotify();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to mask all characters");

  if (StaticPrefs::editor_password_testing_mask_delay()) {
    if (RefPtr<Element> target = GetInputEventTargetElement()) {
      RefPtr<Document> document = target->OwnerDoc();
      nsContentUtils::DispatchTrustedEvent(
          document, target, NS_LITERAL_STRING("MozLastInputMasked"),
          CanBubble::eYes, Cancelable::eNo);
    }
  }

  return EditorBase::ToGenericNSResult(rv);
}

NS_IMETHODIMP
TextEditor::GetName(nsACString& aName) {
  aName.AssignLiteral("TextEditor");
  return NS_OK;
}

void TextEditor::WillDeleteText(uint32_t aCurrentLength,
                                uint32_t aRemoveStartOffset,
                                uint32_t aRemoveLength) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!IsPasswordEditor() || IsAllMasked()) {
    return;
  }

  // Adjust unmasked range before deletion since DOM mutation may cause
  // layout referring the range in old text.

  // If we need to mask automatically, mask all now.
  if (mIsMaskingPassword) {
    DebugOnly<nsresult> rvIgnored = MaskAllCharacters();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored), "MaskAllCharacters() failed");
    return;
  }

  if (aRemoveStartOffset < mUnmaskedStart) {
    // If removing range is before the unmasked range, move it.
    if (aRemoveStartOffset + aRemoveLength <= mUnmaskedStart) {
      DebugOnly<nsresult> rvIgnored =
          SetUnmaskRange(mUnmaskedStart - aRemoveLength, mUnmaskedLength);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored), "SetUnmaskRange() failed");
      return;
    }

    // If removing range starts before unmasked range, and ends in unmasked
    // range, move and shrink the range.
    if (aRemoveStartOffset + aRemoveLength < UnmaskedEnd()) {
      uint32_t unmaskedLengthInRemovingRange =
          aRemoveStartOffset + aRemoveLength - mUnmaskedStart;
      DebugOnly<nsresult> rvIgnored = SetUnmaskRange(
          aRemoveStartOffset, mUnmaskedLength - unmaskedLengthInRemovingRange);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored), "SetUnmaskRange() failed");
      return;
    }

    // If removing range includes all unmasked range, collapse it to the
    // remove offset.
    DebugOnly<nsresult> rvIgnored = SetUnmaskRange(aRemoveStartOffset, 0);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored), "SetUnmaskRange() failed");
    return;
  }

  if (aRemoveStartOffset < UnmaskedEnd()) {
    // If removing range is in unmasked range, shrink the range.
    if (aRemoveStartOffset + aRemoveLength <= UnmaskedEnd()) {
      DebugOnly<nsresult> rvIgnored =
          SetUnmaskRange(mUnmaskedStart, mUnmaskedLength - aRemoveLength);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored), "SetUnmaskRange() failed");
      return;
    }

    // If removing range starts from unmasked range, and ends after it,
    // shrink it.
    DebugOnly<nsresult> rvIgnored =
        SetUnmaskRange(mUnmaskedStart, aRemoveStartOffset - mUnmaskedStart);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored), "SetUnmaskRange() failed");
    return;
  }

  // If removing range is after the unmasked range, keep it.
}

nsresult TextEditor::DidInsertText(uint32_t aNewLength,
                                   uint32_t aInsertedOffset,
                                   uint32_t aInsertedLength) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!IsPasswordEditor() || IsAllMasked()) {
    return NS_OK;
  }

  if (mIsMaskingPassword) {
    // If we need to mask password, mask all right now.
    nsresult rv = MaskAllCharactersAndNotify();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "MaskAllCharacters() failed");
    return rv;
  }

  if (aInsertedOffset < mUnmaskedStart) {
    // If insertion point is before unmasked range, expand the unmasked range
    // to include the new text.
    nsresult rv = SetUnmaskRangeAndNotify(
        aInsertedOffset, UnmaskedEnd() + aInsertedLength - aInsertedOffset);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetUnmaskRange() failed");
    return rv;
  }

  if (aInsertedOffset <= UnmaskedEnd()) {
    // If insertion point is in unmasked range, unmask new text.
    nsresult rv = SetUnmaskRangeAndNotify(mUnmaskedStart,
                                          mUnmaskedLength + aInsertedLength);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetUnmaskRange() failed");
    return rv;
  }

  // If insertion point is after unmasked range, extend the unmask range to
  // include the new text.
  nsresult rv = SetUnmaskRangeAndNotify(
      mUnmaskedStart, aInsertedOffset + aInsertedLength - mUnmaskedStart);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "SetUnmaskRange() failed");
  return rv;
}

}  // namespace mozilla
