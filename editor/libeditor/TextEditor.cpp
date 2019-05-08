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
#include "mozilla/mozalloc.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TextServicesDocument.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Element.h"
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
      mNewlineHandling(nsIPlaintextEditor::eNewlinesPasteToFirst)
#ifdef XP_WIN
      ,
      mCaretStyle(1)
#else
      ,
      mCaretStyle(0)
#endif
{
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
  if (tmp->mRules) tmp->mRules->DetachEditor();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRules)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedDocumentEncoder)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TextEditor, EditorBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRules)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCachedDocumentEncoder)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(TextEditor, EditorBase)
NS_IMPL_RELEASE_INHERITED(TextEditor, EditorBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextEditor)
  NS_INTERFACE_MAP_ENTRY(nsIPlaintextEditor)
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
  NS_ENSURE_SUCCESS(rulesRv, rulesRv);

  // mRules may not have been initialized yet, when this is called via
  // HTMLEditor::Init.
  if (mRules) {
    mRules->SetInitialValue(aInitialValue);
  }

  return NS_OK;
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
    return EditorBase::ToGenericNSResult(rv);
  }
  // Throw away the old transaction manager if this is not the first time that
  // we're initializing the editor.
  ClearUndoRedo();
  EnableUndoRedo();
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
    case NS_VK_BACK:
      if (aKeyboardEvent->IsControl() || aKeyboardEvent->IsAlt() ||
          aKeyboardEvent->IsMeta() || aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelectionAsAction(nsIEditor::ePrevious, nsIEditor::eStrip);
      aKeyboardEvent->PreventDefault();  // consumed
      return NS_OK;
    case NS_VK_DELETE:
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

nsresult TextEditor::InsertLineBreakAsAction() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertLineBreak);
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

already_AddRefed<Element> TextEditor::InsertBrElementWithTransaction(
    const EditorDOMPoint& aPointToInsert, EDirection aSelect /* = eNone */) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aPointToInsert.IsSet())) {
    return nullptr;
  }

  // We need to insert a <br> node.
  RefPtr<Element> newBRElement;
  if (aPointToInsert.IsInTextNode()) {
    EditorDOMPoint pointInContainer;
    if (aPointToInsert.IsStartOfContainer()) {
      // Insert before the text node.
      pointInContainer.Set(aPointToInsert.GetContainer());
      if (NS_WARN_IF(!pointInContainer.IsSet())) {
        return nullptr;
      }
    } else if (aPointToInsert.IsEndOfContainer()) {
      // Insert after the text node.
      pointInContainer.Set(aPointToInsert.GetContainer());
      if (NS_WARN_IF(!pointInContainer.IsSet())) {
        return nullptr;
      }
      DebugOnly<bool> advanced = pointInContainer.AdvanceOffset();
      NS_WARNING_ASSERTION(advanced,
                           "Failed to advance offset to after the text node");
    } else {
      MOZ_DIAGNOSTIC_ASSERT(aPointToInsert.IsSetAndValid());
      // Unfortunately, we need to split the text node at the offset.
      ErrorResult error;
      nsCOMPtr<nsIContent> newLeftNode =
          SplitNodeWithTransaction(aPointToInsert, error);
      if (NS_WARN_IF(error.Failed())) {
        error.SuppressException();
        return nullptr;
      }
      Unused << newLeftNode;
      // Insert new <br> before the right node.
      pointInContainer.Set(aPointToInsert.GetContainer());
    }
    // Create a <br> node.
    newBRElement = CreateNodeWithTransaction(*nsGkAtoms::br, pointInContainer);
    if (NS_WARN_IF(!newBRElement)) {
      return nullptr;
    }
  } else {
    newBRElement = CreateNodeWithTransaction(*nsGkAtoms::br, aPointToInsert);
    if (NS_WARN_IF(!newBRElement)) {
      return nullptr;
    }
  }

  switch (aSelect) {
    case eNone:
      break;
    case eNext: {
      SelectionRefPtr()->SetInterlinePosition(true, IgnoreErrors());
      // Collapse selection after the <br> node.
      EditorRawDOMPoint afterBRElement(newBRElement);
      if (afterBRElement.IsSet()) {
        DebugOnly<bool> advanced = afterBRElement.AdvanceOffset();
        NS_WARNING_ASSERTION(advanced,
                             "Failed to advance offset after the <br> element");
        ErrorResult error;
        SelectionRefPtr()->Collapse(afterBRElement, error);
        NS_WARNING_ASSERTION(
            !error.Failed(),
            "Failed to collapse selection after the <br> element");
      } else {
        NS_WARNING("The <br> node is not in the DOM tree?");
      }
      break;
    }
    case ePrevious: {
      SelectionRefPtr()->SetInterlinePosition(true, IgnoreErrors());
      // Collapse selection at the <br> node.
      EditorRawDOMPoint atBRElement(newBRElement);
      if (atBRElement.IsSet()) {
        ErrorResult error;
        SelectionRefPtr()->Collapse(atBRElement, error);
        NS_WARNING_ASSERTION(
            !error.Failed(),
            "Failed to collapse selection at the <br> element");
      } else {
        NS_WARNING("The <br> node is not in the DOM tree?");
      }
      break;
    }
    default:
      NS_WARNING(
          "aSelect has invalid value, the caller need to set selection "
          "by itself");
      break;
  }

  return newBRElement.forget();
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
              insertionPoint.GetContainerAsText()->GetText();
          uint32_t offset = insertionPoint.Offset();
          if ((offset > 1 && NS_IS_LOW_SURROGATE(data->CharAt(offset - 1)) &&
               NS_IS_HIGH_SURROGATE(data->CharAt(offset - 2))) ||
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult TextEditor::DeleteSelectionAsAction(EDirection aDirection,
                                             EStripWrappers aStripWrappers) {
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

  AutoEditActionDataSetter editActionData(*this, editAction);
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::DeleteSelectionAsSubAction(EDirection aDirection,
                                                EStripWrappers aStripWrappers) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(mPlaceholderBatch);

  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eDeleteSelectedContent, aDirection);
  EditSubActionInfo subActionInfo(EditSubAction::eDeleteSelectedContent);
  subActionInfo.collapsedAction = aDirection;
  subActionInfo.stripWrappers = aStripWrappers;
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!cancel && !handled) {
    rv = DeleteSelectionWithTransaction(aDirection, aStripWrappers);
  }
  if (!cancel) {
    // post-process
    rv = rules->DidDoAction(subActionInfo, rv);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "TextEditRules::DidDoAction() failed");
  }
  return rv;
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
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eDeleteSelectedContent, aDirection);

  if (mRules && mRules->AsHTMLEditRules()) {
    if (!deleteNode) {
      RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
      htmlEditRules->WillDeleteSelection();
    } else if (!deleteCharData) {
      RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
      htmlEditRules->WillDeleteNode(*deleteNode);
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

  if (mRules && mRules->AsHTMLEditRules() && deleteCharData) {
    MOZ_ASSERT(deleteNode);
    RefPtr<HTMLEditRules> htmlEditRules = mRules->AsHTMLEditRules();
    htmlEditRules->DidDeleteText(*deleteNode, deleteCharOffset, 1);
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult TextEditor::InsertTextAsAction(const nsAString& aStringToInsert) {
  // Showing this assertion is fine if this method is called by outside via
  // mutation event listener or something.  Otherwise, this is called by
  // wrong method.
  NS_ASSERTION(!mPlaceholderBatch,
               "Should be called only when this is the only edit action of the "
               "operation "
               "unless mutation event listener nests some operations");

  AutoEditActionDataSetter editActionData(*this, EditAction::eInsertText);
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

  // Protect the edit rules object from dying.
  RefPtr<TextEditRules> rules(mRules);

  EditSubAction editSubAction = ShouldHandleIMEComposition()
                                    ? EditSubAction::eInsertTextComingFromIME
                                    : EditSubAction::eInsertText;

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, editSubAction, nsIEditor::eNext);

  nsAutoString resultString;
  // XXX can we trust instring to outlive subActionInfo,
  // XXX and subActionInfo not to refer to instring in its dtor?
  // nsAutoString instring(aStringToInsert);
  EditSubActionInfo subActionInfo(editSubAction);
  subActionInfo.inString = &aStringToInsert;
  subActionInfo.outString = &resultString;
  subActionInfo.maxLength = mMaxTextLength;

  bool cancel, handled;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!cancel && !handled) {
    // we rely on rules code for now - no default implementation
  }
  if (cancel) {
    return NS_OK;
  }
  // post-process
  rv = rules->DidDoAction(subActionInfo, NS_OK);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
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

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertLineBreak, nsIEditor::eNext);

  EditSubActionInfo subActionInfo(EditSubAction::eInsertLineBreak);
  subActionInfo.maxLength = mMaxTextLength;
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel) {
    return rv;  // We don't need to call DidDoAction() if canceled.
  }
  // XXX DidDoAction() does nothing for eInsertParagraphSeparator.  However,
  //     we should call it until we keep using this style.  Perhaps, each
  //     editor method should call necessary method of
  //     TextEditRules/HTMLEditRules directly.
  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult TextEditor::SetText(const nsAString& aString) {
  MOZ_ASSERT(aString.FindChar(static_cast<char16_t>('\r')) == kNotFound);

  AutoEditActionDataSetter editActionData(*this, EditAction::eSetText);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  nsresult rv = SetTextAsSubAction(aString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

nsresult TextEditor::ReplaceTextAsAction(
    const nsAString& aString, nsRange* aReplaceRange /* = nullptr */) {
  AutoEditActionDataSetter editActionData(*this, EditAction::eReplaceText);
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
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext);

  if (!aReplaceRange) {
    nsresult rv = SetTextAsSubAction(aString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
    return NS_OK;
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
  SelectionRefPtr()->AddRange(*aReplaceRange, error);
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

  if (NS_WARN_IF(!mRules)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eSetText, nsIEditor::eNext);

  EditSubActionInfo subActionInfo(EditSubAction::eSetText);
  subActionInfo.inString = &aString;
  subActionInfo.maxLength = mMaxTextLength;

  bool cancel;
  bool handled;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (cancel) {
    return NS_OK;
  }
  if (!handled) {
    // Note that do not notify selectionchange caused by selecting all text
    // because it's preparation of our delete implementation so web apps
    // shouldn't receive such selectionchange before the first mutation.
    AutoUpdateViewBatch preventSelectionChangeEvent(*this);

    Element* rootElement = GetRoot();
    if (NS_WARN_IF(!rootElement)) {
      return NS_ERROR_FAILURE;
    }

    // We want to select trailing BR node to remove all nodes to replace all,
    // but TextEditor::SelectEntireDocument doesn't select that BR node.
    if (rules->DocumentIsEmpty()) {
      rv = SelectionRefPtr()->Collapse(rootElement, 0);
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "Failed to move caret to start of the editor root element");
    } else {
      ErrorResult error;
      SelectionRefPtr()->SelectAllChildren(*rootElement, error);
      NS_WARNING_ASSERTION(
          !error.Failed(),
          "Failed to select all children of the editor root element");
      rv = error.StealNSResult();
    }
    if (NS_SUCCEEDED(rv)) {
      rv = ReplaceSelectionAsSubAction(aString);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "Failed to replace selection with new string");
    }
  }
  // post-process
  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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

  if (IsPasswordEditor()) {
    if (NS_WARN_IF(!mRules)) {
      return NS_ERROR_FAILURE;
    }
    // Protect the edit rules object from dying
    RefPtr<TextEditRules> rules(mRules);
    rules->ResetIMETextPWBuf();
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

nsresult TextEditor::IsEmpty(bool* aIsEmpty) const {
  if (NS_WARN_IF(!mRules)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aIsEmpty = true;

  if (mRules->HasBogusNode()) {
    return NS_OK;
  }

  // Even if there is no bogus node, we should be detected as empty editor
  // if all the children are text nodes and these have no content.
  Element* rootElement = GetRoot();
  if (!rootElement) {
    // XXX Why don't we return an error in such case??
    return NS_OK;
  }

  for (nsIContent* child = rootElement->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (!EditorBase::IsTextNode(child) ||
        static_cast<nsTextNode*>(child)->TextDataLength()) {
      *aIsEmpty = false;
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
TextEditor::GetDocumentIsEmpty(bool* aDocumentIsEmpty) {
  nsresult rv = IsEmpty(aDocumentIsEmpty);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::GetTextLength(int32_t* aCount) {
  MOZ_ASSERT(aCount);

  // initialize out params
  *aCount = 0;

  // special-case for empty document, to account for the bogus node
  bool isEmpty = false;
  nsresult rv = IsEmpty(&isEmpty);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (isEmpty) {
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

NS_IMETHODIMP
TextEditor::Undo(uint32_t aCount) {
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

  AutoEditActionDataSetter editActionData(*this, EditAction::eUndo);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying.
  RefPtr<TextEditRules> rules(mRules);

  AutoUpdateViewBatch preventSelectionChangeEvent(*this);

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);
  if (NS_WARN_IF(!CanUndo()) || NS_WARN_IF(Destroyed())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  {
    AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
        *this, EditSubAction::eUndo, nsIEditor::eNone);

    EditSubActionInfo subActionInfo(EditSubAction::eUndo);
    bool cancel, handled;
    rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
    if (!cancel && NS_SUCCEEDED(rv)) {
      RefPtr<TransactionManager> transactionManager(mTransactionManager);
      for (uint32_t i = 0; i < aCount; ++i) {
        rv = transactionManager->Undo();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          break;
        }
        DoAfterUndoTransaction();
      }
      rv = rules->DidDoAction(subActionInfo, rv);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "TextEditRules::DidDoAction() failed");
    }
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::Redo(uint32_t aCount) {
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

  AutoEditActionDataSetter editActionData(*this, EditAction::eRedo);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying.
  RefPtr<TextEditRules> rules(mRules);

  AutoUpdateViewBatch preventSelectionChangeEvent(*this);

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);
  if (NS_WARN_IF(!CanRedo()) || NS_WARN_IF(Destroyed())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  {
    AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
        *this, EditSubAction::eRedo, nsIEditor::eNone);

    EditSubActionInfo subActionInfo(EditSubAction::eRedo);
    bool cancel, handled;
    rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
    if (!cancel && NS_SUCCEEDED(rv)) {
      RefPtr<TransactionManager> transactionManager(mTransactionManager);
      for (uint32_t i = 0; i < aCount; ++i) {
        nsresult rv = transactionManager->Redo();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          break;
        }
        DoAfterRedoTransaction();
      }
      rv = rules->DidDoAction(subActionInfo, rv);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                           "TextEditRules::DidDoAction() failed");
    }
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

bool TextEditor::CanCutOrCopy(
    PasswordFieldAllowed aPasswordFieldAllowed) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (aPasswordFieldAllowed == ePasswordFieldNotAllowed && IsPasswordEditor()) {
    return false;
  }

  return !SelectionRefPtr()->IsCollapsed();
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

NS_IMETHODIMP
TextEditor::Cut() {
  AutoEditActionDataSetter editActionData(*this, EditAction::eCut);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool actionTaken = false;
  if (FireClipboardEvent(eCut, nsIClipboard::kGlobalClipboard, &actionTaken)) {
    // XXX This transaction name is referred by PlaceholderTransaction::Merge()
    //     so that we need to keep using it here.
    AutoPlaceholderBatch treatAsOneTransaction(*this,
                                               *nsGkAtoms::DeleteTxnName);
    DeleteSelectionAsSubAction(eNone, eStrip);
  }
  return EditorBase::ToGenericNSResult(
      actionTaken ? NS_OK : NS_ERROR_EDITOR_ACTION_CANCELED);
}

bool TextEditor::CanCut() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  // Cut is always enabled in HTML documents
  Document* document = GetDocument();
  if (document && document->IsHTMLOrXHTML()) {
    return true;
  }

  return IsModifiable() && CanCutOrCopy(ePasswordFieldNotAllowed);
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

bool TextEditor::CanCopy() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  // Copy is always enabled in HTML documents
  Document* document = GetDocument();
  if (document && document->IsHTMLOrXHTML()) {
    return true;
  }

  return CanCutOrCopy(ePasswordFieldNotAllowed);
}

bool TextEditor::CanDelete() const {
  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return false;
  }

  return IsModifiable() && CanCutOrCopy(ePasswordFieldAllowed);
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // This is low level API for XUL applcation.  So, we should return raw
    // error code here.
    return rv;
  }
  return NS_OK;
}

nsresult TextEditor::ComputeValueInternal(const nsAString& aFormatType,
                                          uint32_t aDocumentEncoderFlags,
                                          nsAString& aOutputString) const {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  EditSubActionInfo subActionInfo(EditSubAction::eComputeTextToOutput);
  subActionInfo.outString = &aOutputString;
  subActionInfo.flags = aDocumentEncoderFlags;
  subActionInfo.outputFormat = &aFormatType;

  bool cancel, handled;
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }
  if (handled) {
    // This case will get triggered by password fields or single text node only.
    return rv;
  }

  nsAutoCString charset;
  rv = GetDocumentCharsetInternal(charset);
  if (NS_FAILED(rv) || charset.IsEmpty()) {
    charset.AssignLiteral("windows-1252");
  }

  nsCOMPtr<nsIDocumentEncoder> encoder =
      GetAndInitDocEncoder(aFormatType, aDocumentEncoderFlags, charset);
  if (NS_WARN_IF(!encoder)) {
    return NS_ERROR_FAILURE;
  }

  // XXX Why don't we call TextEditRules::DidDoAction() here?
  rv = encoder->EncodeToString(aOutputString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult TextEditor::PasteAsQuotationAsAction(int32_t aClipboardType,
                                              bool aDispatchPasteEvent) {
  MOZ_ASSERT(aClipboardType == nsIClipboard::kGlobalClipboard ||
             aClipboardType == nsIClipboard::kSelectionClipboard);

  AutoEditActionDataSetter editActionData(*this, EditAction::ePasteAsQuotation);
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
      rv = InsertWithQuotationsAsSubAction(stuffToPaste);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return EditorBase::ToGenericNSResult(rv);
      }
    }
  }
  return NS_OK;
}

nsresult TextEditor::InsertWithQuotationsAsSubAction(
    const nsAString& aQuotedText) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

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

  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext);

  // XXX This WillDoAction() usage is hacky.  If it returns as handled,
  //     this method cannot work as expected.  So, this should have specific
  //     sub-action rather than using eInsertElement.
  EditSubActionInfo subActionInfo(EditSubAction::eInsertElement);
  bool cancel, handled;
  rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (cancel) {
    return NS_OK;  // Rules canceled the operation.
  }
  MOZ_ASSERT(!handled, "WillDoAction() shouldn't handle in this case");
  if (!handled) {
    rv = InsertTextAsSubAction(quotedStuff);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  // XXX Why don't we call TextEditRules::DidDoAction()?
  return NS_OK;
}

nsresult TextEditor::SharedOutputString(uint32_t aFlags, bool* aIsCollapsed,
                                        nsAString& aResult) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  *aIsCollapsed = SelectionRefPtr()->IsCollapsed();

  if (!*aIsCollapsed) {
    aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  }
  // If the selection isn't collapsed, we'll use the whole document.
  return ComputeValueInternal(NS_LITERAL_STRING("text/plain"), aFlags, aResult);
}

void TextEditor::OnStartToHandleTopLevelEditSubAction(
    EditSubAction aEditSubAction, nsIEditor::EDirection aDirection) {
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  EditorBase::OnStartToHandleTopLevelEditSubAction(aEditSubAction, aDirection);
  if (!rules) {
    return;
  }

  MOZ_ASSERT(GetTopLevelEditSubAction() == aEditSubAction);
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() == aDirection);
  DebugOnly<nsresult> rv = rules->BeforeEdit(aEditSubAction, aDirection);
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "TextEditRules::BeforeEdit() failed to handle something");
}

void TextEditor::OnEndHandlingTopLevelEditSubAction() {
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  // post processing
  DebugOnly<nsresult> rv =
      rules ? rules->AfterEdit(GetTopLevelEditSubAction(),
                               GetDirectionOfTopLevelEditSubAction())
            : NS_OK;
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "TextEditRules::AfterEdit() failed to handle something");
  EditorBase::OnEndHandlingTopLevelEditSubAction();
  MOZ_ASSERT(!GetTopLevelEditSubAction());
  MOZ_ASSERT(GetDirectionOfTopLevelEditSubAction() == eNone);
}

nsresult TextEditor::SelectEntireDocument() {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (!mRules) {
    return NS_ERROR_NULL_POINTER;
  }

  Element* rootElement = GetRoot();
  if (NS_WARN_IF(!rootElement)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);

  // If we're empty, don't select all children because that would select the
  // bogus node.
  if (rules->DocumentIsEmpty()) {
    nsresult rv = SelectionRefPtr()->Collapse(rootElement, 0);
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "Failed to move caret to start of the editor root element");
    return rv;
  }

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

  if (childNode && TextEditUtils::IsMozBR(childNode)) {
    ErrorResult error;
    MOZ_KnownLive(SelectionRefPtr())
        ->SetStartAndEndInLimiter(RawRangeBoundary(rootElement, 0),
                                  EditorRawDOMPoint(childNode), error);
    NS_WARNING_ASSERTION(!error.Failed(),
                         "Failed to select all children of the editor root "
                         "element except the moz-<br> element");
    return error.StealNSResult();
  }

  ErrorResult error;
  SelectionRefPtr()->SelectAllChildren(*rootElement, error);
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

nsresult TextEditor::HideLastPasswordInput() {
  // This method should be called only by TextEditRules::Notify().
  MOZ_ASSERT(mRules);
  MOZ_ASSERT(IsPasswordEditor());
  MOZ_ASSERT(!IsEditActionDataAvailable());

  AutoEditActionDataSetter editActionData(*this, EditAction::eHidePassword);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->HideLastPasswordInput();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return EditorBase::ToGenericNSResult(rv);
  }
  return NS_OK;
}

}  // namespace mozilla
