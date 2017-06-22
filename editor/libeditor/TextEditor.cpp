/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditor.h"

#include "InternetCiter.h"
#include "TextEditUtils.h"
#include "gfxFontUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditorUtils.h" // AutoEditBatch, AutoRules
#include "mozilla/HTMLEditor.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Element.h"
#include "nsAString.h"
#include "nsCRT.h"
#include "nsCaret.h"
#include "nsCharTraits.h"
#include "nsComponentManagerUtils.h"
#include "nsContentCID.h"
#include "nsCopySupport.h"
#include "nsDebug.h"
#include "nsDependentSubstring.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNode.h"
#include "nsIDocumentEncoder.h"
#include "nsIEditRules.h"
#include "nsINode.h"
#include "nsIPresShell.h"
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
#include "nsSubstringTuple.h"
#include "nsUnicharUtils.h"
#include "nsXPCOM.h"

class nsIOutputStream;
class nsISupports;

namespace mozilla {

using namespace dom;

TextEditor::TextEditor()
  : mWrapColumn(0)
  , mMaxTextLength(-1)
  , mInitTriggerCounter(0)
  , mNewlineHandling(nsIPlaintextEditor::eNewlinesPasteToFirst)
#ifdef XP_WIN
  , mCaretStyle(1)
#else
  , mCaretStyle(0)
#endif
{
  // check the "single line editor newline handling"
  // and "caret behaviour in selection" prefs
  GetDefaultEditorPrefs(mNewlineHandling, mCaretStyle);
}

HTMLEditor*
TextEditor::AsHTMLEditor()
{
  return nullptr;
}

const HTMLEditor*
TextEditor::AsHTMLEditor() const
{
  return nullptr;
}

TextEditor::~TextEditor()
{
  // Remove event listeners. Note that if we had an HTML editor,
  //  it installed its own instead of these
  RemoveEventListeners();

  if (mRules)
    mRules->DetachEditor();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TextEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TextEditor, EditorBase)
  if (tmp->mRules)
    tmp->mRules->DetachEditor();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRules)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedDocumentEncoder)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TextEditor, EditorBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRules)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCachedDocumentEncoder)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(TextEditor, EditorBase)
NS_IMPL_RELEASE_INHERITED(TextEditor, EditorBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextEditor)
  NS_INTERFACE_MAP_ENTRY(nsIPlaintextEditor)
  NS_INTERFACE_MAP_ENTRY(nsIEditorMailSupport)
NS_INTERFACE_MAP_END_INHERITING(EditorBase)


NS_IMETHODIMP
TextEditor::Init(nsIDOMDocument* aDoc,
                 nsIContent* aRoot,
                 nsISelectionController* aSelCon,
                 uint32_t aFlags,
                 const nsAString& aInitialValue)
{
  NS_PRECONDITION(aDoc, "bad arg");
  NS_ENSURE_TRUE(aDoc, NS_ERROR_NULL_POINTER);

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

static int32_t sNewlineHandlingPref = -1,
               sCaretStylePref = -1;

static void
EditorPrefsChangedCallback(const char* aPrefName, void *)
{
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
void
TextEditor::GetDefaultEditorPrefs(int32_t& aNewlineHandling,
                                  int32_t& aCaretStyle)
{
  if (sNewlineHandlingPref == -1) {
    Preferences::RegisterCallbackAndCall(EditorPrefsChangedCallback,
                                         "editor.singleLine.pasteNewlines");
    Preferences::RegisterCallbackAndCall(EditorPrefsChangedCallback,
                                         "layout.selection.caret_style");
  }

  aNewlineHandling = sNewlineHandlingPref;
  aCaretStyle = sCaretStylePref;
}

void
TextEditor::BeginEditorInit()
{
  mInitTriggerCounter++;
}

nsresult
TextEditor::EndEditorInit()
{
  NS_PRECONDITION(mInitTriggerCounter > 0, "ended editor init before we began?");
  mInitTriggerCounter--;
  if (mInitTriggerCounter) {
    return NS_OK;
  }

  nsresult rv = InitRules();
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Throw away the old transaction manager if this is not the first time that
  // we're initializing the editor.
  EnableUndo(false);
  EnableUndo(true);
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::SetDocumentCharacterSet(const nsACString& characterSet)
{
  nsresult rv = EditorBase::SetDocumentCharacterSet(characterSet);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update META charset element.
  nsCOMPtr<nsIDocument> doc = GetDocument();
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
  RefPtr<Element> metaElement = CreateNode(nsGkAtoms::meta, headNode, 0);
  if (NS_WARN_IF(!metaElement)) {
    return NS_OK;
  }

  // Set attributes to the created element
  if (characterSet.IsEmpty()) {
    return NS_OK;
  }

  // not undoable, undo should undo CreateNode
  metaElement->SetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv,
                       NS_LITERAL_STRING("Content-Type"), true);
  metaElement->SetAttr(kNameSpaceID_None, nsGkAtoms::content,
                       NS_LITERAL_STRING("text/html;charset=") +
                         NS_ConvertASCIItoUTF16(characterSet),
                       true);
  return NS_OK;
}

bool
TextEditor::UpdateMetaCharset(nsIDocument& aDocument,
                              const nsACString& aCharacterSet)
{
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
    metaNode->GetAttr(kNameSpaceID_None, nsGkAtoms::httpEquiv, currentValue);

    if (!FindInReadable(NS_LITERAL_STRING("content-type"),
                        currentValue,
                        nsCaseInsensitiveStringComparator())) {
      continue;
    }

    metaNode->GetAttr(kNameSpaceID_None, nsGkAtoms::content, currentValue);

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
    nsresult rv =
      EditorBase::SetAttribute(metaElement, nsGkAtoms::content,
                               Substring(originalStart, start) +
                                 charsetEquals +
                                 NS_ConvertASCIItoUTF16(aCharacterSet));
    return NS_SUCCEEDED(rv);
  }
  return false;
}

NS_IMETHODIMP
TextEditor::InitRules()
{
  if (!mRules) {
    // instantiate the rules for this text editor
    mRules = new TextEditRules();
  }
  return mRules->Init(this);
}

nsresult
TextEditor::HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent)
{
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
    case NS_VK_BACK:
    case NS_VK_DELETE:
      // These keys are handled on EditorBase
      return EditorBase::HandleKeyPressEvent(aKeyboardEvent);
    case NS_VK_TAB: {
      if (IsTabbable()) {
        return NS_OK; // let it be used for focus switching
      }

      if (aKeyboardEvent->IsShift() || aKeyboardEvent->IsControl() ||
          aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
          aKeyboardEvent->IsOS()) {
        return NS_OK;
      }

      // else we insert the tab straight through
      aKeyboardEvent->PreventDefault();
      return TypedText(NS_LITERAL_STRING("\t"), eTypedText);
    }
    case NS_VK_RETURN:
      if (IsSingleLineEditor() || aKeyboardEvent->IsControl() ||
          aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
          aKeyboardEvent->IsOS()) {
        return NS_OK;
      }
      aKeyboardEvent->PreventDefault();
      return TypedText(EmptyString(), eTypedBreak);
  }

  // NOTE: On some keyboard layout, some characters are inputted with Control
  // key or Alt key, but at that time, widget sets FALSE to these keys.
  if (!aKeyboardEvent->mCharCode || aKeyboardEvent->IsControl() ||
      aKeyboardEvent->IsAlt() || aKeyboardEvent->IsMeta() ||
      aKeyboardEvent->IsOS()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  aKeyboardEvent->PreventDefault();
  nsAutoString str(aKeyboardEvent->mCharCode);
  return TypedText(str, eTypedText);
}

/* This routine is needed to provide a bottleneck for typing for logging
   purposes.  Can't use HandleKeyPress() (above) for that since it takes
   a nsIDOMKeyEvent* parameter.  So instead we pass enough info through
   to TypedText() to determine what action to take, but without passing
   an event.
   */
NS_IMETHODIMP
TextEditor::TypedText(const nsAString& aString, ETypingAction aAction)
{
  AutoPlaceHolderBatch batch(this, nsGkAtoms::TypingTxnName);

  switch (aAction) {
    case eTypedText:
      return InsertText(aString);
    case eTypedBreak:
      return InsertLineBreak();
    default:
      // eTypedBR is only for HTML
      return NS_ERROR_FAILURE;
  }
}

already_AddRefed<Element>
TextEditor::CreateBRImpl(nsCOMPtr<nsINode>* aInOutParent,
                         int32_t* aInOutOffset,
                         EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent(GetAsDOMNode(*aInOutParent));
  nsCOMPtr<nsIDOMNode> br;
  // We ignore the retval, and assume it's fine if the br is non-null
  CreateBRImpl(address_of(parent), aInOutOffset, address_of(br), aSelect);
  *aInOutParent = do_QueryInterface(parent);
  nsCOMPtr<Element> ret(do_QueryInterface(br));
  return ret.forget();
}

nsresult
TextEditor::CreateBRImpl(nsCOMPtr<nsIDOMNode>* aInOutParent,
                         int32_t* aInOutOffset,
                         nsCOMPtr<nsIDOMNode>* outBRNode,
                         EDirection aSelect)
{
  NS_ENSURE_TRUE(aInOutParent && *aInOutParent && aInOutOffset && outBRNode, NS_ERROR_NULL_POINTER);
  *outBRNode = nullptr;

  // we need to insert a br.  unfortunately, we may have to split a text node to do it.
  nsCOMPtr<nsINode> node = do_QueryInterface(*aInOutParent);
  int32_t theOffset = *aInOutOffset;
  RefPtr<Element> brNode;
  if (IsTextNode(node)) {
    int32_t offset;
    nsCOMPtr<nsINode> tmp = GetNodeLocation(node, &offset);
    NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
    if (!theOffset) {
      // we are already set to go
    } else if (theOffset == static_cast<int32_t>(node->Length())) {
      // update offset to point AFTER the text node
      offset++;
    } else {
      // split the text node
      ErrorResult rv;
      SplitNode(*node->AsContent(), theOffset, rv);
      if (NS_WARN_IF(rv.Failed())) {
        return rv.StealNSResult();
      }
      tmp = GetNodeLocation(node, &offset);
    }
    // create br
    brNode = CreateNode(nsGkAtoms::br, tmp, offset);
    if (NS_WARN_IF(!brNode)) {
      return NS_ERROR_FAILURE;
    }
    *aInOutParent = GetAsDOMNode(tmp);
    *aInOutOffset = offset+1;
  } else {
    brNode = CreateNode(nsGkAtoms::br, node, theOffset);
    if (NS_WARN_IF(!brNode)) {
      return NS_ERROR_FAILURE;
    }
    (*aInOutOffset)++;
  }

  *outBRNode = GetAsDOMNode(brNode);
  if (*outBRNode && (aSelect != eNone)) {
    int32_t offset;
    nsCOMPtr<nsINode> parent = GetNodeLocation(brNode, &offset);

    RefPtr<Selection> selection = GetSelection();
    NS_ENSURE_STATE(selection);
    if (aSelect == eNext) {
      // position selection after br
      selection->SetInterlinePosition(true);
      selection->Collapse(parent, offset + 1);
    } else if (aSelect == ePrevious) {
      // position selection before br
      selection->SetInterlinePosition(true);
      selection->Collapse(parent, offset);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
TextEditor::CreateBR(nsIDOMNode* aNode,
                     int32_t aOffset,
                     nsCOMPtr<nsIDOMNode>* outBRNode,
                     EDirection aSelect)
{
  nsCOMPtr<nsIDOMNode> parent = aNode;
  int32_t offset = aOffset;
  return CreateBRImpl(address_of(parent), &offset, outBRNode, aSelect);
}

nsresult
TextEditor::ExtendSelectionForDelete(Selection* aSelection,
                                     nsIEditor::EDirection* aAction)
{
  bool bCollapsed = aSelection->Collapsed();

  if (*aAction == eNextWord ||
      *aAction == ePreviousWord ||
      (*aAction == eNext && bCollapsed) ||
      (*aAction == ePrevious && bCollapsed) ||
      *aAction == eToBeginningOfLine ||
      *aAction == eToEndOfLine) {
    nsCOMPtr<nsISelectionController> selCont;
    GetSelectionController(getter_AddRefs(selCont));
    NS_ENSURE_TRUE(selCont, NS_ERROR_NO_INTERFACE);

    nsresult rv;
    switch (*aAction) {
      case eNextWord:
        rv = selCont->WordExtendForDelete(true);
        // DeleteSelectionImpl doesn't handle these actions
        // because it's inside batching, so don't confuse it:
        *aAction = eNone;
        break;
      case ePreviousWord:
        rv = selCont->WordExtendForDelete(false);
        *aAction = eNone;
        break;
      case eNext:
        rv = selCont->CharacterExtendForDelete();
        // Don't set aAction to eNone (see Bug 502259)
        break;
      case ePrevious: {
        // Only extend the selection where the selection is after a UTF-16
        // surrogate pair or a variation selector.
        // For other cases we don't want to do that, in order
        // to make sure that pressing backspace will only delete the last
        // typed character.
        nsCOMPtr<nsINode> node;
        int32_t offset;
        rv = GetStartNodeAndOffset(aSelection, getter_AddRefs(node), &offset);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

        // node might be anonymous DIV, so we find better text node
        FindBetterInsertionPoint(node, offset);

        if (IsTextNode(node)) {
          const nsTextFragment* data = node->GetAsText()->GetText();
          if ((offset > 1 &&
               NS_IS_LOW_SURROGATE(data->CharAt(offset - 1)) &&
               NS_IS_HIGH_SURROGATE(data->CharAt(offset - 2))) ||
              (offset > 0 &&
               gfxFontUtils::IsVarSelector(data->CharAt(offset - 1)))) {
            rv = selCont->CharacterExtendForBackspace();
          }
        }
        break;
      }
      case eToBeginningOfLine:
        selCont->IntraLineMove(true, false);          // try to move to end
        rv = selCont->IntraLineMove(false, true); // select to beginning
        *aAction = eNone;
        break;
      case eToEndOfLine:
        rv = selCont->IntraLineMove(true, true);
        *aAction = eNext;
        break;
      default:       // avoid several compiler warnings
        rv = NS_OK;
        break;
    }
    return rv;
  }
  return NS_OK;
}

nsresult
TextEditor::DeleteSelection(EDirection aAction,
                            EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  // delete placeholder txns merge.
  AutoPlaceHolderBatch batch(this, nsGkAtoms::DeleteTxnName);
  AutoRules beginRulesSniffing(this, EditAction::deleteSelection, aAction);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // If there is an existing selection when an extended delete is requested,
  //  platforms that use "caret-style" caret positioning collapse the
  //  selection to the  start and then create a new selection.
  //  Platforms that use "selection-style" caret positioning just delete the
  //  existing selection without extending it.
  if (!selection->Collapsed() &&
      (aAction == eNextWord || aAction == ePreviousWord ||
       aAction == eToBeginningOfLine || aAction == eToEndOfLine)) {
    if (mCaretStyle == 1) {
      nsresult rv = selection->CollapseToStart();
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      aAction = eNone;
    }
  }

  TextRulesInfo ruleInfo(EditAction::deleteSelection);
  ruleInfo.collapsedAction = aAction;
  ruleInfo.stripWrappers = aStripWrappers;
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!cancel && !handled) {
    rv = DeleteSelectionImpl(aAction, aStripWrappers);
  }
  if (!cancel) {
    // post-process
    rv = rules->DidDoAction(selection, &ruleInfo, rv);
  }
  return rv;
}

NS_IMETHODIMP
TextEditor::InsertText(const nsAString& aStringToInsert)
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  EditAction opID = EditAction::insertText;
  if (ShouldHandleIMEComposition()) {
    opID = EditAction::insertIMEText;
  }
  AutoPlaceHolderBatch batch(this, nullptr);
  AutoRules beginRulesSniffing(this, opID, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  nsAutoString resultString;
  // XXX can we trust instring to outlive ruleInfo,
  // XXX and ruleInfo not to refer to instring in its dtor?
  //nsAutoString instring(aStringToInsert);
  TextRulesInfo ruleInfo(opID);
  ruleInfo.inString = &aStringToInsert;
  ruleInfo.outString = &resultString;
  ruleInfo.maxLength = mMaxTextLength;

  bool cancel, handled;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!cancel && !handled) {
    // we rely on rules code for now - no default implementation
  }
  if (cancel) {
    return NS_OK;
  }
  // post-process
  return rules->DidDoAction(selection, &ruleInfo, rv);
}

NS_IMETHODIMP
TextEditor::InsertLineBreak()
{
  if (!mRules) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertBreak, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  TextRulesInfo ruleInfo(EditAction::insertBreak);
  ruleInfo.maxLength = mMaxTextLength;
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!cancel && !handled) {
    // get the (collapsed) selection location
    NS_ENSURE_STATE(selection->GetRangeAt(0));
    nsCOMPtr<nsINode> selNode = selection->GetRangeAt(0)->GetStartParent();
    int32_t selOffset = selection->GetRangeAt(0)->StartOffset();
    NS_ENSURE_STATE(selNode);

    // don't put text in places that can't have it
    if (!IsTextNode(selNode) && !CanContainTag(*selNode,
                                               *nsGkAtoms::textTagName)) {
      return NS_ERROR_FAILURE;
    }

    // we need to get the doc
    nsCOMPtr<nsIDocument> doc = GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

    // don't spaz my selection in subtransactions
    AutoTransactionsConserveSelection dontSpazMySelection(this);

    // insert a linefeed character
    rv = InsertTextImpl(NS_LITERAL_STRING("\n"), address_of(selNode),
                        &selOffset, doc);
    if (!selNode) {
      rv = NS_ERROR_NULL_POINTER; // don't return here, so DidDoAction is called
    }
    if (NS_SUCCEEDED(rv)) {
      // set the selection to the correct location
      rv = selection->Collapse(selNode, selOffset);
      if (NS_SUCCEEDED(rv)) {
        // see if we're at the end of the editor range
        nsCOMPtr<nsIDOMNode> endNode;
        int32_t endOffset;
        rv = GetEndNodeAndOffset(selection,
                                 getter_AddRefs(endNode), &endOffset);

        if (NS_SUCCEEDED(rv) &&
            endNode == GetAsDOMNode(selNode) && endOffset == selOffset) {
          // SetInterlinePosition(true) means we want the caret to stick to the content on the "right".
          // We want the caret to stick to whatever is past the break.  This is
          // because the break is on the same line we were on, but the next content
          // will be on the following line.
          selection->SetInterlinePosition(true);
        }
      }
    }
  }

  if (!cancel) {
    // post-process, always called if WillInsertBreak didn't return cancel==true
    rv = rules->DidDoAction(selection, &ruleInfo, rv);
  }
  return rv;
}

NS_IMETHODIMP
TextEditor::SetText(const nsAString& aString)
{
  if (NS_WARN_IF(!mRules)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  // delete placeholder txns merge.
  AutoPlaceHolderBatch batch(this, nullptr);
  AutoRules beginRulesSniffing(this, EditAction::setText, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_NULL_POINTER;
  }
  TextRulesInfo ruleInfo(EditAction::setText);
  ruleInfo.inString = &aString;
  ruleInfo.maxLength = mMaxTextLength;

  bool cancel;
  bool handled;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (cancel) {
    return NS_OK;
  }
  if (!handled) {
    // We want to select trailing BR node to remove all nodes to replace all,
    // but TextEditor::SelectEntireDocument doesn't select that BR node.
    if (rules->DocumentIsEmpty()) {
      // if it's empty, don't select entire doc - that would select
      // the bogus node
      Element* rootElement = GetRoot();
      if (NS_WARN_IF(!rootElement)) {
        return NS_ERROR_FAILURE;
      }
      rv = selection->Collapse(rootElement, 0);
    } else {
      rv = EditorBase::SelectEntireDocument(selection);
    }
    if (NS_SUCCEEDED(rv)) {
      if (aString.IsEmpty()) {
        rv = DeleteSelection(eNone, eStrip);
      } else {
        rv = InsertText(aString);
      }
    }
  }
  // post-process
  return rules->DidDoAction(selection, &ruleInfo, rv);
}

nsresult
TextEditor::BeginIMEComposition(WidgetCompositionEvent* aEvent)
{
  NS_ENSURE_TRUE(!mComposition, NS_OK);

  if (IsPasswordEditor()) {
    NS_ENSURE_TRUE(mRules, NS_ERROR_NULL_POINTER);
    // Protect the edit rules object from dying
    nsCOMPtr<nsIEditRules> rules(mRules);

    TextEditRules* textEditRules = static_cast<TextEditRules*>(rules.get());
    textEditRules->ResetIMETextPWBuf();
  }

  return EditorBase::BeginIMEComposition(aEvent);
}

nsresult
TextEditor::UpdateIMEComposition(WidgetCompositionEvent* aCompsitionChangeEvent)
{
  MOZ_ASSERT(aCompsitionChangeEvent,
             "aCompsitionChangeEvent must not be nullptr");

  if (NS_WARN_IF(!aCompsitionChangeEvent)) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(aCompsitionChangeEvent->mMessage == eCompositionChange,
             "The event should be eCompositionChange");

  if (!EnsureComposition(aCompsitionChangeEvent)) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  // NOTE: TextComposition should receive selection change notification before
  //       CompositionChangeEventHandlingMarker notifies TextComposition of the
  //       end of handling compositionchange event because TextComposition may
  //       need to ignore selection changes caused by composition.  Therefore,
  //       CompositionChangeEventHandlingMarker must be destroyed after a call
  //       of NotifiyEditorObservers(eNotifyEditorObserversOfEnd) or
  //       NotifiyEditorObservers(eNotifyEditorObserversOfCancel) which notifies
  //       TextComposition of a selection change.
  MOZ_ASSERT(!mPlaceholderBatch,
    "UpdateIMEComposition() must be called without place holder batch");
  TextComposition::CompositionChangeEventHandlingMarker
    compositionChangeEventHandlingMarker(mComposition, aCompsitionChangeEvent);

  RefPtr<nsCaret> caretP = ps->GetCaret();

  nsresult rv;
  {
    AutoPlaceHolderBatch batch(this, nsGkAtoms::IMETxnName);

    MOZ_ASSERT(mIsInEditAction,
      "AutoPlaceHolderBatch should've notified the observes of before-edit");
    rv = InsertText(aCompsitionChangeEvent->mData);

    if (caretP) {
      caretP->SetSelection(selection);
    }
  }

  // If still composing, we should fire input event via observer.
  // Note that if the composition will be committed by the following
  // compositionend event, we don't need to notify editor observes of this
  // change.
  // NOTE: We must notify after the auto batch will be gone.
  if (!aCompsitionChangeEvent->IsFollowedByCompositionEnd()) {
    NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  }

  return rv;
}

already_AddRefed<nsIContent>
TextEditor::GetInputEventTargetContent()
{
  nsCOMPtr<nsIContent> target = do_QueryInterface(mEventTarget);
  return target.forget();
}

NS_IMETHODIMP
TextEditor::GetDocumentIsEmpty(bool* aDocumentIsEmpty)
{
  NS_ENSURE_TRUE(aDocumentIsEmpty, NS_ERROR_NULL_POINTER);

  NS_ENSURE_TRUE(mRules, NS_ERROR_NOT_INITIALIZED);

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);
  *aDocumentIsEmpty = rules->DocumentIsEmpty();
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::GetTextLength(int32_t* aCount)
{
  NS_ASSERTION(aCount, "null pointer");

  // initialize out params
  *aCount = 0;

  // special-case for empty document, to account for the bogus node
  bool docEmpty;
  nsresult rv = GetDocumentIsEmpty(&docEmpty);
  NS_ENSURE_SUCCESS(rv, rv);
  if (docEmpty) {
    return NS_OK;
  }

  dom::Element *rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContentIterator> iter =
    do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t totalLength = 0;
  iter->Init(rootElement);
  for (; !iter->IsDone(); iter->Next()) {
    nsCOMPtr<nsINode> currentNode = iter->GetCurrentNode();
    if (IsTextNode(currentNode) && IsEditable(currentNode)) {
      totalLength += currentNode->Length();
    }
  }

  *aCount = totalLength;
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::SetMaxTextLength(int32_t aMaxTextLength)
{
  mMaxTextLength = aMaxTextLength;
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::GetMaxTextLength(int32_t* aMaxTextLength)
{
  // NOTE: If you need to override this method, you need to make
  //       MaxTextLength() virtual.
  if (NS_WARN_IF(!aMaxTextLength)) {
    return NS_ERROR_INVALID_POINTER;
  }
  *aMaxTextLength = MaxTextLength();
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::GetWrapWidth(int32_t* aWrapColumn)
{
  NS_ENSURE_TRUE( aWrapColumn, NS_ERROR_NULL_POINTER);

  *aWrapColumn = mWrapColumn;
  return NS_OK;
}

//
// See if the style value includes this attribute, and if it does,
// cut out everything from the attribute to the next semicolon.
//
static void CutStyle(const char* stylename, nsString& styleValue)
{
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
TextEditor::SetWrapWidth(int32_t aWrapColumn)
{
  SetWrapColumn(aWrapColumn);

  // Make sure we're a plaintext editor, otherwise we shouldn't
  // do the rest of this.
  if (!IsPlaintextEditor()) {
    return NS_OK;
  }

  // Ought to set a style sheet here ...
  // Probably should keep around an mPlaintextStyleSheet for this purpose.
  dom::Element *rootElement = GetRoot();
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

  return rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::style, styleValue, true);
}

NS_IMETHODIMP
TextEditor::SetWrapColumn(int32_t aWrapColumn)
{
  mWrapColumn = aWrapColumn;
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::GetNewlineHandling(int32_t* aNewlineHandling)
{
  NS_ENSURE_ARG_POINTER(aNewlineHandling);

  *aNewlineHandling = mNewlineHandling;
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::SetNewlineHandling(int32_t aNewlineHandling)
{
  mNewlineHandling = aNewlineHandling;

  return NS_OK;
}

NS_IMETHODIMP
TextEditor::Undo(uint32_t aCount)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  AutoUpdateViewBatch beginViewBatching(this);

  ForceCompositionEnd();

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);

  AutoRules beginRulesSniffing(this, EditAction::undo, nsIEditor::eNone);

  TextRulesInfo ruleInfo(EditAction::undo);
  RefPtr<Selection> selection = GetSelection();
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);

  if (!cancel && NS_SUCCEEDED(rv)) {
    rv = EditorBase::Undo(aCount);
    rv = rules->DidDoAction(selection, &ruleInfo, rv);
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  return rv;
}

NS_IMETHODIMP
TextEditor::Redo(uint32_t aCount)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  AutoUpdateViewBatch beginViewBatching(this);

  ForceCompositionEnd();

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);

  AutoRules beginRulesSniffing(this, EditAction::redo, nsIEditor::eNone);

  TextRulesInfo ruleInfo(EditAction::redo);
  RefPtr<Selection> selection = GetSelection();
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);

  if (!cancel && NS_SUCCEEDED(rv)) {
    rv = EditorBase::Redo(aCount);
    rv = rules->DidDoAction(selection, &ruleInfo, rv);
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  return rv;
}

bool
TextEditor::CanCutOrCopy(PasswordFieldAllowed aPasswordFieldAllowed)
{
  RefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return false;
  }

  if (aPasswordFieldAllowed == ePasswordFieldNotAllowed &&
      IsPasswordEditor()) {
    return false;
  }

  return !selection->Collapsed();
}

bool
TextEditor::FireClipboardEvent(EventMessage aEventMessage,
                               int32_t aSelectionType,
                               bool* aActionTaken)
{
  if (aEventMessage == ePaste) {
    ForceCompositionEnd();
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, false);

  RefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return false;
  }

  if (!nsCopySupport::FireClipboardEvent(aEventMessage, aSelectionType,
                                         presShell, selection, aActionTaken)) {
    return false;
  }

  // If the event handler caused the editor to be destroyed, return false.
  // Otherwise return true to indicate that the event was not cancelled.
  return !mDidPreDestroy;
}

NS_IMETHODIMP
TextEditor::Cut()
{
  bool actionTaken = false;
  if (FireClipboardEvent(eCut, nsIClipboard::kGlobalClipboard, &actionTaken)) {
    DeleteSelection(eNone, eStrip);
  }
  return actionTaken ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TextEditor::CanCut(bool* aCanCut)
{
  NS_ENSURE_ARG_POINTER(aCanCut);
  // Cut is always enabled in HTML documents
  nsCOMPtr<nsIDocument> doc = GetDocument();
  *aCanCut = (doc && doc->IsHTMLOrXHTML()) ||
    (IsModifiable() && CanCutOrCopy(ePasswordFieldNotAllowed));
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::Copy()
{
  bool actionTaken = false;
  FireClipboardEvent(eCopy, nsIClipboard::kGlobalClipboard, &actionTaken);

  return actionTaken ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TextEditor::CanCopy(bool* aCanCopy)
{
  NS_ENSURE_ARG_POINTER(aCanCopy);
  // Copy is always enabled in HTML documents
  nsCOMPtr<nsIDocument> doc = GetDocument();
  *aCanCopy = (doc && doc->IsHTMLOrXHTML()) ||
    CanCutOrCopy(ePasswordFieldNotAllowed);
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::CanDelete(bool* aCanDelete)
{
  NS_ENSURE_ARG_POINTER(aCanDelete);
  *aCanDelete = IsModifiable() && CanCutOrCopy(ePasswordFieldAllowed);
  return NS_OK;
}

// Shared between OutputToString and OutputToStream
already_AddRefed<nsIDocumentEncoder>
TextEditor::GetAndInitDocEncoder(const nsAString& aFormatType,
                                 uint32_t aFlags,
                                 const nsACString& aCharset)
{
  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  if (!mCachedDocumentEncoder ||
      !mCachedDocumentEncoderType.Equals(aFormatType)) {
    nsAutoCString formatType(NS_DOC_ENCODER_CONTRACTID_BASE);
    LossyAppendUTF16toASCII(aFormatType, formatType);
    docEncoder = do_CreateInstance(formatType.get());
    if (NS_WARN_IF(!docEncoder)) {
      return nullptr;
    }
    mCachedDocumentEncoder = docEncoder;
    mCachedDocumentEncoderType = aFormatType;
  } else {
    docEncoder = mCachedDocumentEncoder;
  }

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ASSERTION(doc, "Need a document");

  nsresult rv =
    docEncoder->NativeInit(
                  doc, aFormatType,
                  aFlags | nsIDocumentEncoder::RequiresReinitAfterOutput);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  if (!aCharset.IsEmpty() && !aCharset.EqualsLiteral("null")) {
    docEncoder->SetCharset(aCharset);
  }

  int32_t wc;
  (void) GetWrapWidth(&wc);
  if (wc >= 0) {
    (void) docEncoder->SetWrapColumn(wc);
  }

  // Set the selection, if appropriate.
  // We do this either if the OutputSelectionOnly flag is set,
  // in which case we use our existing selection ...
  if (aFlags & nsIDocumentEncoder::OutputSelectionOnly) {
    RefPtr<Selection> selection = GetSelection();
    if (NS_WARN_IF(!selection)) {
      return nullptr;
    }
    rv = docEncoder->SetSelection(selection);
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
      rv = docEncoder->SetNativeContainerNode(rootElement);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
    }
  }

  return docEncoder.forget();
}


NS_IMETHODIMP
TextEditor::OutputToString(const nsAString& aFormatType,
                           uint32_t aFlags,
                           nsAString& aOutputString)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  nsString resultString;
  TextRulesInfo ruleInfo(EditAction::outputText);
  ruleInfo.outString = &resultString;
  ruleInfo.flags = aFlags;
  // XXX Struct should store a nsAReadable*
  nsAutoString str(aFormatType);
  ruleInfo.outputFormat = &str;
  bool cancel, handled;
  nsresult rv = rules->WillDoAction(nullptr, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) {
    return rv;
  }
  if (handled) {
    // This case will get triggered by password fields.
    aOutputString.Assign(*(ruleInfo.outString));
    return rv;
  }

  nsAutoCString charsetStr;
  rv = GetDocumentCharacterSet(charsetStr);
  if (NS_FAILED(rv) || charsetStr.IsEmpty()) {
    charsetStr.AssignLiteral("windows-1252");
  }

  nsCOMPtr<nsIDocumentEncoder> encoder =
    GetAndInitDocEncoder(aFormatType, aFlags, charsetStr);
  if (NS_WARN_IF(!encoder)) {
    return NS_ERROR_FAILURE;
  }

  return encoder->EncodeToString(aOutputString);
}

NS_IMETHODIMP
TextEditor::OutputToStream(nsIOutputStream* aOutputStream,
                           const nsAString& aFormatType,
                           const nsACString& aCharset,
                           uint32_t aFlags)
{
  nsresult rv;

  // special-case for empty document when requesting plain text,
  // to account for the bogus text node.
  // XXX Should there be a similar test in OutputToString?
  if (aFormatType.EqualsLiteral("text/plain")) {
    bool docEmpty;
    rv = GetDocumentIsEmpty(&docEmpty);
    NS_ENSURE_SUCCESS(rv, rv);

    if (docEmpty) {
      return NS_OK; // Output nothing.
    }
  }

  nsCOMPtr<nsIDocumentEncoder> encoder =
    GetAndInitDocEncoder(aFormatType, aFlags, aCharset);
  if (NS_WARN_IF(!encoder)) {
    return NS_ERROR_FAILURE;
  }

  return encoder->EncodeToStream(aOutputStream);
}

NS_IMETHODIMP
TextEditor::InsertTextWithQuotations(const nsAString& aStringToInsert)
{
  return InsertText(aStringToInsert);
}

NS_IMETHODIMP
TextEditor::PasteAsQuotation(int32_t aSelectionType)
{
  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> trans;
  rv = PrepareTransferable(getter_AddRefs(trans));
  if (NS_SUCCEEDED(rv) && trans) {
    // Get the Data from the clipboard
    clipboard->GetData(trans, aSelectionType);

    // Now we ask the transferable for the data
    // it still owns the data, we just have a pointer to it.
    // If it can't support a "text" output of the data the call will fail
    nsCOMPtr<nsISupports> genericDataObj;
    uint32_t len;
    nsAutoCString flav;
    rv = trans->GetAnyTransferData(flav, getter_AddRefs(genericDataObj),
                                   &len);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (flav.EqualsLiteral(kUnicodeMime) ||
        flav.EqualsLiteral(kMozTextInternal)) {
      nsCOMPtr<nsISupportsString> textDataObj ( do_QueryInterface(genericDataObj) );
      if (textDataObj && len > 0) {
        nsAutoString stuffToPaste;
        textDataObj->GetData ( stuffToPaste );
        AutoEditBatch beginBatching(this);
        rv = InsertAsQuotation(stuffToPaste, 0);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
TextEditor::InsertAsQuotation(const nsAString& aQuotedText,
                              nsIDOMNode** aNodeInserted)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  // Let the citer quote it for us:
  nsString quotedStuff;
  nsresult rv = InternetCiter::GetCiteString(aQuotedText, quotedStuff);
  NS_ENSURE_SUCCESS(rv, rv);

  // It's best to put a blank line after the quoted text so that mails
  // written without thinking won't be so ugly.
  if (!aQuotedText.IsEmpty() && (aQuotedText.Last() != char16_t('\n'))) {
    quotedStuff.Append(char16_t('\n'));
  }

  // get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertText, nsIEditor::eNext);

  // give rules a chance to handle or cancel
  TextRulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  rv = rules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel) {
    return NS_OK; // Rules canceled the operation.
  }
  if (!handled) {
    rv = InsertText(quotedStuff);

    // XXX Should set *aNodeInserted to the first node inserted
    if (aNodeInserted && NS_SUCCEEDED(rv)) {
      *aNodeInserted = nullptr;
    }
  }
  return rv;
}

NS_IMETHODIMP
TextEditor::PasteAsCitedQuotation(const nsAString& aCitation,
                                  int32_t aSelectionType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TextEditor::InsertAsCitedQuotation(const nsAString& aQuotedText,
                                   const nsAString& aCitation,
                                   bool aInsertHTML,
                                   nsIDOMNode** aNodeInserted)
{
  return InsertAsQuotation(aQuotedText, aNodeInserted);
}

nsresult
TextEditor::SharedOutputString(uint32_t aFlags,
                               bool* aIsCollapsed,
                               nsAString& aResult)
{
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);

  *aIsCollapsed = selection->Collapsed();

  if (!*aIsCollapsed) {
    aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  }
  // If the selection isn't collapsed, we'll use the whole document.

  return OutputToString(NS_LITERAL_STRING("text/plain"), aFlags, aResult);
}

NS_IMETHODIMP
TextEditor::Rewrap(bool aRespectNewlines)
{
  int32_t wrapCol;
  nsresult rv = GetWrapWidth(&wrapCol);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  // Rewrap makes no sense if there's no wrap column; default to 72.
  if (wrapCol <= 0) {
    wrapCol = 72;
  }

  nsAutoString current;
  bool isCollapsed;
  rv = SharedOutputString(nsIDocumentEncoder::OutputFormatted
                          | nsIDocumentEncoder::OutputLFLineBreak,
                          &isCollapsed, current);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString wrapped;
  uint32_t firstLineOffset = 0;   // XXX need to reset this if there is a selection
  rv = InternetCiter::Rewrap(current, wrapCol, firstLineOffset,
                             aRespectNewlines, wrapped);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isCollapsed) {
    SelectAll();
  }

  return InsertTextWithQuotations(wrapped);
}

NS_IMETHODIMP
TextEditor::StripCites()
{
  nsAutoString current;
  bool isCollapsed;
  nsresult rv = SharedOutputString(nsIDocumentEncoder::OutputFormatted,
                                   &isCollapsed, current);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString stripped;
  rv = InternetCiter::StripCites(current, stripped);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isCollapsed) {
    rv = SelectAll();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return InsertText(stripped);
}

NS_IMETHODIMP
TextEditor::GetEmbeddedObjects(nsIArray** aNodeList)
{
  if (NS_WARN_IF(!aNodeList)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aNodeList = nullptr;
  return NS_OK;
}

/**
 * All editor operations which alter the doc should be prefaced
 * with a call to StartOperation, naming the action and direction.
 */
NS_IMETHODIMP
TextEditor::StartOperation(EditAction opID,
                           nsIEditor::EDirection aDirection)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  EditorBase::StartOperation(opID, aDirection);  // will set mAction, mDirection
  if (rules) {
    return rules->BeforeEdit(mAction, mDirection);
  }
  return NS_OK;
}

/**
 * All editor operations which alter the doc should be followed
 * with a call to EndOperation.
 */
NS_IMETHODIMP
TextEditor::EndOperation()
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  // post processing
  nsresult rv = rules ? rules->AfterEdit(mAction, mDirection) : NS_OK;
  EditorBase::EndOperation();  // will clear mAction, mDirection
  return rv;
}

nsresult
TextEditor::SelectEntireDocument(Selection* aSelection)
{
  if (!aSelection || !mRules) {
    return NS_ERROR_NULL_POINTER;
  }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> rules(mRules);

  // is doc empty?
  if (rules->DocumentIsEmpty()) {
    // get root node
    nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(GetRoot());
    NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);

    // if it's empty don't select entire doc - that would select the bogus node
    return aSelection->Collapse(rootElement, 0);
  }

  SelectionBatcher selectionBatcher(aSelection);
  nsresult rv = EditorBase::SelectEntireDocument(aSelection);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't select the trailing BR node if we have one
  int32_t selOffset;
  nsCOMPtr<nsIDOMNode> selNode;
  rv = GetEndNodeAndOffset(aSelection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> childNode = GetChildAt(selNode, selOffset - 1);

  if (childNode && TextEditUtils::IsMozBR(childNode)) {
    int32_t parentOffset;
    nsCOMPtr<nsIDOMNode> parentNode = GetNodeLocation(childNode, &parentOffset);

    return aSelection->Extend(parentNode, parentOffset);
  }

  return NS_OK;
}

already_AddRefed<EventTarget>
TextEditor::GetDOMEventTarget()
{
  nsCOMPtr<EventTarget> copy = mEventTarget;
  return copy.forget();
}


nsresult
TextEditor::SetAttributeOrEquivalent(Element* aElement,
                                     nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     bool aSuppressTransaction)
{
  return EditorBase::SetAttribute(aElement, aAttribute, aValue);
}

nsresult
TextEditor::RemoveAttributeOrEquivalent(Element* aElement,
                                        nsIAtom* aAttribute,
                                        bool aSuppressTransaction)
{
  return EditorBase::RemoveAttribute(aElement, aAttribute);
}

} // namespace mozilla
