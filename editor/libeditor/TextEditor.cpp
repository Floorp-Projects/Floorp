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
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDocumentEncoder.h"
#include "nsIEditorIMESupport.h"
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
class nsISupportsArray;

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
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TextEditor, EditorBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRules)
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

  nsresult res = NS_OK, rulesRes = NS_OK;
  if (mRules) {
    mRules->DetachEditor();
  }

  {
    // block to scope AutoEditInitRulesTrigger
    AutoEditInitRulesTrigger rulesTrigger(this, rulesRes);

    // Init the base editor
    res = EditorBase::Init(aDoc, aRoot, aSelCon, aFlags, aInitialValue);
  }

  NS_ENSURE_SUCCESS(rulesRes, rulesRes);

  // mRules may not have been initialized yet, when this is called via
  // HTMLEditor::Init.
  if (mRules) {
    mRules->SetInitialValue(aInitialValue);
  }

  return res;
}

static int32_t sNewlineHandlingPref = -1,
               sCaretStylePref = -1;

static void
EditorPrefsChangedCallback(const char* aPrefName, void *)
{
  if (nsCRT::strcmp(aPrefName, "editor.singleLine.pasteNewlines") == 0) {
    sNewlineHandlingPref =
      Preferences::GetInt("editor.singleLine.pasteNewlines",
                          nsIPlaintextEditor::eNewlinesPasteToFirst);
  } else if (nsCRT::strcmp(aPrefName, "layout.selection.caret_style") == 0) {
    sCaretStylePref = Preferences::GetInt("layout.selection.caret_style",
#ifdef XP_WIN
                                                 1);
    if (sCaretStylePref == 0)
      sCaretStylePref = 1;
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
    Preferences::RegisterCallback(EditorPrefsChangedCallback,
                                  "editor.singleLine.pasteNewlines");
    EditorPrefsChangedCallback("editor.singleLine.pasteNewlines", nullptr);
    Preferences::RegisterCallback(EditorPrefsChangedCallback,
                                  "layout.selection.caret_style");
    EditorPrefsChangedCallback("layout.selection.caret_style", nullptr);
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
  nsresult res = NS_OK;
  NS_PRECONDITION(mInitTriggerCounter > 0, "ended editor init before we began?");
  mInitTriggerCounter--;
  if (mInitTriggerCounter == 0)
  {
    res = InitRules();
    if (NS_SUCCEEDED(res)) {
      // Throw away the old transaction manager if this is not the first time that
      // we're initializing the editor.
      EnableUndo(false);
      EnableUndo(true);
    }
  }
  return res;
}

NS_IMETHODIMP
TextEditor::SetDocumentCharacterSet(const nsACString& characterSet)
{
  nsresult rv = EditorBase::SetDocumentCharacterSet(characterSet);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update META charset element.
  nsCOMPtr<nsIDOMDocument> domdoc = GetDOMDocument();
  NS_ENSURE_TRUE(domdoc, NS_ERROR_NOT_INITIALIZED);

  if (UpdateMetaCharset(domdoc, characterSet)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNodeList> headList;
  rv = domdoc->GetElementsByTagName(NS_LITERAL_STRING("head"), getter_AddRefs(headList));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(headList, NS_OK);

  nsCOMPtr<nsIDOMNode> headNode;
  headList->Item(0, getter_AddRefs(headNode));
  NS_ENSURE_TRUE(headNode, NS_OK);

  // Create a new meta charset tag
  nsCOMPtr<nsIDOMNode> resultNode;
  rv = CreateNode(NS_LITERAL_STRING("meta"), headNode, 0, getter_AddRefs(resultNode));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(resultNode, NS_OK);

  // Set attributes to the created element
  if (characterSet.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<dom::Element> metaElement = do_QueryInterface(resultNode);
  if (!metaElement) {
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
TextEditor::UpdateMetaCharset(nsIDOMDocument* aDocument,
                              const nsACString& aCharacterSet)
{
  MOZ_ASSERT(aDocument);
  // get a list of META tags
  nsCOMPtr<nsIDOMNodeList> list;
  nsresult rv = aDocument->GetElementsByTagName(NS_LITERAL_STRING("meta"),
                                                getter_AddRefs(list));
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(list, false);

  nsCOMPtr<nsINodeList> metaList = do_QueryInterface(list);

  uint32_t listLength = 0;
  metaList->GetLength(&listLength);

  for (uint32_t i = 0; i < listLength; ++i) {
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
    nsCOMPtr<nsIDOMElement> metaElement = do_QueryInterface(metaNode);
    MOZ_ASSERT(metaElement);
    rv = EditorBase::SetAttribute(metaElement, NS_LITERAL_STRING("content"),
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


NS_IMETHODIMP
TextEditor::GetIsDocumentEditable(bool* aIsDocumentEditable)
{
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);

  nsCOMPtr<nsIDOMDocument> doc = GetDOMDocument();
  *aIsDocumentEditable = doc && IsModifiable();

  return NS_OK;
}

bool
TextEditor::IsModifiable()
{
  return !IsReadonly();
}

nsresult
TextEditor::HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent)
{
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/tests/test_texteditor_keyevent_handling.html
  //   * editor/libeditor/tests/test_htmleditor_keyevent_handling.html
  //
  // And also when you add new key handling, you need to change the subclass's
  // HandleKeyPressEvent()'s switch statement.

  if (IsReadonly() || IsDisabled()) {
    // When we're not editable, the events handled on EditorBase.
    return EditorBase::HandleKeyPressEvent(aKeyEvent);
  }

  WidgetKeyboardEvent* nativeKeyEvent =
    aKeyEvent->AsEvent()->WidgetEventPtr()->AsKeyboardEvent();
  NS_ENSURE_TRUE(nativeKeyEvent, NS_ERROR_UNEXPECTED);
  NS_ASSERTION(nativeKeyEvent->mMessage == eKeyPress,
               "HandleKeyPressEvent gets non-keypress event");

  switch (nativeKeyEvent->mKeyCode) {
    case NS_VK_META:
    case NS_VK_WIN:
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
    case NS_VK_BACK:
    case NS_VK_DELETE:
      // These keys are handled on EditorBase
      return EditorBase::HandleKeyPressEvent(aKeyEvent);
    case NS_VK_TAB: {
      if (IsTabbable()) {
        return NS_OK; // let it be used for focus switching
      }

      if (nativeKeyEvent->IsShift() || nativeKeyEvent->IsControl() ||
          nativeKeyEvent->IsAlt() || nativeKeyEvent->IsMeta() ||
          nativeKeyEvent->IsOS()) {
        return NS_OK;
      }

      // else we insert the tab straight through
      aKeyEvent->AsEvent()->PreventDefault();
      return TypedText(NS_LITERAL_STRING("\t"), eTypedText);
    }
    case NS_VK_RETURN:
      if (IsSingleLineEditor() || nativeKeyEvent->IsControl() ||
          nativeKeyEvent->IsAlt() || nativeKeyEvent->IsMeta() ||
          nativeKeyEvent->IsOS()) {
        return NS_OK;
      }
      aKeyEvent->AsEvent()->PreventDefault();
      return TypedText(EmptyString(), eTypedBreak);
  }

  // NOTE: On some keyboard layout, some characters are inputted with Control
  // key or Alt key, but at that time, widget sets FALSE to these keys.
  if (!nativeKeyEvent->mCharCode || nativeKeyEvent->IsControl() ||
      nativeKeyEvent->IsAlt() || nativeKeyEvent->IsMeta() ||
      nativeKeyEvent->IsOS()) {
    // we don't PreventDefault() here or keybindings like control-x won't work
    return NS_OK;
  }
  aKeyEvent->AsEvent()->PreventDefault();
  nsAutoString str(nativeKeyEvent->mCharCode);
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
  nsresult res;

  // we need to insert a br.  unfortunately, we may have to split a text node to do it.
  nsCOMPtr<nsIDOMNode> node = *aInOutParent;
  int32_t theOffset = *aInOutOffset;
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(node);
  NS_NAMED_LITERAL_STRING(brType, "br");
  nsCOMPtr<nsIDOMNode> brNode;
  if (nodeAsText)
  {
    int32_t offset;
    uint32_t len;
    nodeAsText->GetLength(&len);
    nsCOMPtr<nsIDOMNode> tmp = GetNodeLocation(node, &offset);
    NS_ENSURE_TRUE(tmp, NS_ERROR_FAILURE);
    if (!theOffset)
    {
      // we are already set to go
    }
    else if (theOffset == (int32_t)len)
    {
      // update offset to point AFTER the text node
      offset++;
    }
    else
    {
      // split the text node
      res = SplitNode(node, theOffset, getter_AddRefs(tmp));
      NS_ENSURE_SUCCESS(res, res);
      tmp = GetNodeLocation(node, &offset);
    }
    // create br
    res = CreateNode(brType, tmp, offset, getter_AddRefs(brNode));
    NS_ENSURE_SUCCESS(res, res);
    *aInOutParent = tmp;
    *aInOutOffset = offset+1;
  }
  else
  {
    res = CreateNode(brType, node, theOffset, getter_AddRefs(brNode));
    NS_ENSURE_SUCCESS(res, res);
    (*aInOutOffset)++;
  }

  *outBRNode = brNode;
  if (*outBRNode && (aSelect != eNone))
  {
    int32_t offset;
    nsCOMPtr<nsIDOMNode> parent = GetNodeLocation(*outBRNode, &offset);

    RefPtr<Selection> selection = GetSelection();
    NS_ENSURE_STATE(selection);
    if (aSelect == eNext)
    {
      // position selection after br
      selection->SetInterlinePosition(true);
      res = selection->Collapse(parent, offset+1);
    }
    else if (aSelect == ePrevious)
    {
      // position selection before br
      selection->SetInterlinePosition(true);
      res = selection->Collapse(parent, offset);
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
TextEditor::InsertBR(nsCOMPtr<nsIDOMNode>* outBRNode)
{
  NS_ENSURE_TRUE(outBRNode, NS_ERROR_NULL_POINTER);
  *outBRNode = nullptr;

  // calling it text insertion to trigger moz br treatment by rules
  AutoRules beginRulesSniffing(this, EditAction::insertText, nsIEditor::eNext);

  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  nsresult res;
  if (!selection->Collapsed()) {
    res = DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(res, res);
  }

  nsCOMPtr<nsIDOMNode> selNode;
  int32_t selOffset;
  res = GetStartNodeAndOffset(selection, getter_AddRefs(selNode), &selOffset);
  NS_ENSURE_SUCCESS(res, res);

  res = CreateBR(selNode, selOffset, outBRNode);
  NS_ENSURE_SUCCESS(res, res);

  // position selection after br
  selNode = GetNodeLocation(*outBRNode, &selOffset);
  selection->SetInterlinePosition(true);
  return selection->Collapse(selNode, selOffset+1);
}

nsresult
TextEditor::ExtendSelectionForDelete(Selection* aSelection,
                                     nsIEditor::EDirection* aAction)
{
  nsresult result = NS_OK;

  bool bCollapsed = aSelection->Collapsed();

  if (*aAction == eNextWord || *aAction == ePreviousWord
      || (*aAction == eNext && bCollapsed)
      || (*aAction == ePrevious && bCollapsed)
      || *aAction == eToBeginningOfLine || *aAction == eToEndOfLine)
  {
    nsCOMPtr<nsISelectionController> selCont;
    GetSelectionController(getter_AddRefs(selCont));
    NS_ENSURE_TRUE(selCont, NS_ERROR_NO_INTERFACE);

    switch (*aAction)
    {
      case eNextWord:
        result = selCont->WordExtendForDelete(true);
        // DeleteSelectionImpl doesn't handle these actions
        // because it's inside batching, so don't confuse it:
        *aAction = eNone;
        break;
      case ePreviousWord:
        result = selCont->WordExtendForDelete(false);
        *aAction = eNone;
        break;
      case eNext:
        result = selCont->CharacterExtendForDelete();
        // Don't set aAction to eNone (see Bug 502259)
        break;
      case ePrevious: {
        // Only extend the selection where the selection is after a UTF-16
        // surrogate pair or a variation selector.
        // For other cases we don't want to do that, in order
        // to make sure that pressing backspace will only delete the last
        // typed character.
        nsCOMPtr<nsIDOMNode> node;
        int32_t offset;
        result = GetStartNodeAndOffset(aSelection, getter_AddRefs(node), &offset);
        NS_ENSURE_SUCCESS(result, result);
        NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

        if (IsTextNode(node)) {
          nsCOMPtr<nsIDOMCharacterData> charData = do_QueryInterface(node);
          if (charData) {
            nsAutoString data;
            result = charData->GetData(data);
            NS_ENSURE_SUCCESS(result, result);

            if ((offset > 1 &&
                 NS_IS_LOW_SURROGATE(data[offset - 1]) &&
                 NS_IS_HIGH_SURROGATE(data[offset - 2])) ||
                (offset > 0 &&
                 gfxFontUtils::IsVarSelector(data[offset - 1]))) {
              result = selCont->CharacterExtendForBackspace();
            }
          }
        }
        break;
      }
      case eToBeginningOfLine:
        selCont->IntraLineMove(true, false);          // try to move to end
        result = selCont->IntraLineMove(false, true); // select to beginning
        *aAction = eNone;
        break;
      case eToEndOfLine:
        result = selCont->IntraLineMove(true, true);
        *aAction = eNext;
        break;
      default:       // avoid several compiler warnings
        result = NS_OK;
        break;
    }
  }
  return result;
}

nsresult
TextEditor::DeleteSelection(EDirection aAction,
                            EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  nsresult result;

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
       aAction == eToBeginningOfLine || aAction == eToEndOfLine))
  {
    if (mCaretStyle == 1)
    {
      result = selection->CollapseToStart();
      NS_ENSURE_SUCCESS(result, result);
    }
    else
    {
      aAction = eNone;
    }
  }

  TextRulesInfo ruleInfo(EditAction::deleteSelection);
  ruleInfo.collapsedAction = aAction;
  ruleInfo.stripWrappers = aStripWrappers;
  bool cancel, handled;
  result = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(result, result);
  if (!cancel && !handled)
  {
    result = DeleteSelectionImpl(aAction, aStripWrappers);
  }
  if (!cancel)
  {
    // post-process
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }

  return result;
}

NS_IMETHODIMP
TextEditor::InsertText(const nsAString& aStringToInsert)
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

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
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled)
  {
    // we rely on rules code for now - no default implementation
  }
  if (!cancel)
  {
    // post-process
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
}

NS_IMETHODIMP
TextEditor::InsertLineBreak()
{
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertBreak, nsIEditor::eNext);

  // pre-process
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  TextRulesInfo ruleInfo(EditAction::insertBreak);
  ruleInfo.maxLength = mMaxTextLength;
  bool cancel, handled;
  nsresult res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(res, res);
  if (!cancel && !handled)
  {
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
    res = InsertTextImpl(NS_LITERAL_STRING("\n"), address_of(selNode),
                         &selOffset, doc);
    if (!selNode) res = NS_ERROR_NULL_POINTER; // don't return here, so DidDoAction is called
    if (NS_SUCCEEDED(res))
    {
      // set the selection to the correct location
      res = selection->Collapse(selNode, selOffset);

      if (NS_SUCCEEDED(res))
      {
        // see if we're at the end of the editor range
        nsCOMPtr<nsIDOMNode> endNode;
        int32_t endOffset;
        res = GetEndNodeAndOffset(selection, getter_AddRefs(endNode), &endOffset);

        if (NS_SUCCEEDED(res) && endNode == GetAsDOMNode(selNode)
            && endOffset == selOffset) {
          // SetInterlinePosition(true) means we want the caret to stick to the content on the "right".
          // We want the caret to stick to whatever is past the break.  This is
          // because the break is on the same line we were on, but the next content
          // will be on the following line.
          selection->SetInterlinePosition(true);
        }
      }
    }
  }
  if (!cancel)
  {
    // post-process, always called if WillInsertBreak didn't return cancel==true
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }

  return res;
}

nsresult
TextEditor::BeginIMEComposition(WidgetCompositionEvent* aEvent)
{
  NS_ENSURE_TRUE(!mComposition, NS_OK);

  if (IsPasswordEditor()) {
    NS_ENSURE_TRUE(mRules, NS_ERROR_NULL_POINTER);
    // Protect the edit rules object from dying
    nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

    TextEditRules* textEditRules = static_cast<TextEditRules*>(mRules.get());
    textEditRules->ResetIMETextPWBuf();
  }

  return EditorBase::BeginIMEComposition(aEvent);
}

nsresult
TextEditor::UpdateIMEComposition(nsIDOMEvent* aDOMTextEvent)
{
  MOZ_ASSERT(aDOMTextEvent, "aDOMTextEvent must not be nullptr");

  WidgetCompositionEvent* compositionChangeEvent =
    aDOMTextEvent->WidgetEventPtr()->AsCompositionEvent();
  NS_ENSURE_TRUE(compositionChangeEvent, NS_ERROR_INVALID_ARG);
  MOZ_ASSERT(compositionChangeEvent->mMessage == eCompositionChange,
             "The internal event should be eCompositionChange");

  EnsureComposition(compositionChangeEvent);

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
  MOZ_ASSERT(!mPlaceHolderBatch,
    "UpdateIMEComposition() must be called without place holder batch");
  TextComposition::CompositionChangeEventHandlingMarker
    compositionChangeEventHandlingMarker(mComposition, compositionChangeEvent);

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);

  RefPtr<nsCaret> caretP = ps->GetCaret();

  nsresult rv;
  {
    AutoPlaceHolderBatch batch(this, nsGkAtoms::IMETxnName);

    rv = InsertText(compositionChangeEvent->mData);

    if (caretP) {
      caretP->SetSelection(selection);
    }
  }

  // If still composing, we should fire input event via observer.
  // Note that if the composition will be committed by the following
  // compositionend event, we don't need to notify editor observes of this
  // change.
  // NOTE: We must notify after the auto batch will be gone.
  if (!compositionChangeEvent->IsFollowedByCompositionEnd()) {
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
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  return mRules->DocumentIsEmpty(aDocumentIsEmpty);
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
  if (docEmpty)
    return NS_OK;

  dom::Element *rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContentIterator> iter =
    do_CreateInstance("@mozilla.org/content/post-content-iterator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t totalLength = 0;
  iter->Init(rootElement);
  for (; !iter->IsDone(); iter->Next()) {
    nsCOMPtr<nsIDOMNode> currentNode = do_QueryInterface(iter->GetCurrentNode());
    nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(currentNode);
    if (textNode && IsEditable(currentNode)) {
      uint32_t length;
      textNode->GetLength(&length);
      totalLength += length;
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
  NS_ENSURE_TRUE(aMaxTextLength, NS_ERROR_INVALID_POINTER);
  *aMaxTextLength = mMaxTextLength;
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
  if (styleStart >= 0)
  {
    int32_t styleEnd = styleValue.Find(";", false, styleStart);
    if (styleEnd > styleStart)
      styleValue.Cut(styleStart, styleEnd - styleStart + 1);
    else
      styleValue.Cut(styleStart, styleValue.Length() - styleStart);
  }
}

NS_IMETHODIMP
TextEditor::SetWrapWidth(int32_t aWrapColumn)
{
  SetWrapColumn(aWrapColumn);

  // Make sure we're a plaintext editor, otherwise we shouldn't
  // do the rest of this.
  if (!IsPlaintextEditor())
    return NS_OK;

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
  if (!styleValue.IsEmpty())
  {
    styleValue.Trim("; \t", false, true);
    styleValue.AppendLiteral("; ");
  }

  // Make sure we have fixed-width font.  This should be done for us,
  // but it isn't, see bug 22502, so we have to add "font: -moz-fixed;".
  // Only do this if we're wrapping.
  if (IsWrapHackEnabled() && aWrapColumn >= 0)
    styleValue.AppendLiteral("font-family: -moz-fixed; ");

  // and now we're ready to set the new whitespace/wrapping style.
  if (aWrapColumn > 0) {
    // Wrap to a fixed column.
    styleValue.AppendLiteral("white-space: pre-wrap; width: ");
    styleValue.AppendInt(aWrapColumn);
    styleValue.AppendLiteral("ch;");
  } else if (aWrapColumn == 0) {
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
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  AutoUpdateViewBatch beginViewBatching(this);

  ForceCompositionEnd();

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);

  AutoRules beginRulesSniffing(this, EditAction::undo, nsIEditor::eNone);

  TextRulesInfo ruleInfo(EditAction::undo);
  RefPtr<Selection> selection = GetSelection();
  bool cancel, handled;
  nsresult result = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);

  if (!cancel && NS_SUCCEEDED(result))
  {
    result = EditorBase::Undo(aCount);
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  return result;
}

NS_IMETHODIMP
TextEditor::Redo(uint32_t aCount)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  AutoUpdateViewBatch beginViewBatching(this);

  ForceCompositionEnd();

  NotifyEditorObservers(eNotifyEditorObserversOfBefore);

  AutoRules beginRulesSniffing(this, EditAction::redo, nsIEditor::eNone);

  TextRulesInfo ruleInfo(EditAction::redo);
  RefPtr<Selection> selection = GetSelection();
  bool cancel, handled;
  nsresult result = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);

  if (!cancel && NS_SUCCEEDED(result))
  {
    result = EditorBase::Redo(aCount);
    result = mRules->DidDoAction(selection, &ruleInfo, result);
  }

  NotifyEditorObservers(eNotifyEditorObserversOfEnd);
  return result;
}

bool
TextEditor::CanCutOrCopy(PasswordFieldAllowed aPasswordFieldAllowed)
{
  RefPtr<Selection> selection = GetSelection();
  if (!selection) {
    return false;
  }

  if (aPasswordFieldAllowed == ePasswordFieldNotAllowed &&
      IsPasswordEditor())
    return false;

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
NS_IMETHODIMP
TextEditor::GetAndInitDocEncoder(const nsAString& aFormatType,
                                 uint32_t aFlags,
                                 const nsACString& aCharset,
                                 nsIDocumentEncoder** encoder)
{
  nsresult rv = NS_OK;

  nsAutoCString formatType(NS_DOC_ENCODER_CONTRACTID_BASE);
  LossyAppendUTF16toASCII(aFormatType, formatType);
  nsCOMPtr<nsIDocumentEncoder> docEncoder (do_CreateInstance(formatType.get(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryReferent(mDocWeak);
  NS_ASSERTION(domDoc, "Need a document");

  rv = docEncoder->Init(domDoc, aFormatType, aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aCharset.IsEmpty() && !aCharset.EqualsLiteral("null")) {
    docEncoder->SetCharset(aCharset);
  }

  int32_t wc;
  (void) GetWrapWidth(&wc);
  if (wc >= 0)
    (void) docEncoder->SetWrapColumn(wc);

  // Set the selection, if appropriate.
  // We do this either if the OutputSelectionOnly flag is set,
  // in which case we use our existing selection ...
  if (aFlags & nsIDocumentEncoder::OutputSelectionOnly)
  {
    RefPtr<Selection> selection = GetSelection();
    NS_ENSURE_STATE(selection);
    rv = docEncoder->SetSelection(selection);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // ... or if the root element is not a body,
  // in which case we set the selection to encompass the root.
  else
  {
    dom::Element* rootElement = GetRoot();
    NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);
    if (!rootElement->IsHTMLElement(nsGkAtoms::body)) {
      rv = docEncoder->SetNativeContainerNode(rootElement);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  docEncoder.forget(encoder);
  return NS_OK;
}


NS_IMETHODIMP
TextEditor::OutputToString(const nsAString& aFormatType,
                           uint32_t aFlags,
                           nsAString& aOutputString)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  nsString resultString;
  TextRulesInfo ruleInfo(EditAction::outputText);
  ruleInfo.outString = &resultString;
  // XXX Struct should store a nsAReadable*
  nsAutoString str(aFormatType);
  ruleInfo.outputFormat = &str;
  bool cancel, handled;
  nsresult rv = mRules->WillDoAction(nullptr, &ruleInfo, &cancel, &handled);
  if (cancel || NS_FAILED(rv)) { return rv; }
  if (handled)
  { // this case will get triggered by password fields
    aOutputString.Assign(*(ruleInfo.outString));
    return rv;
  }

  nsAutoCString charsetStr;
  rv = GetDocumentCharacterSet(charsetStr);
  if(NS_FAILED(rv) || charsetStr.IsEmpty())
    charsetStr.AssignLiteral("ISO-8859-1");

  nsCOMPtr<nsIDocumentEncoder> encoder;
  rv = GetAndInitDocEncoder(aFormatType, aFlags, charsetStr, getter_AddRefs(encoder));
  NS_ENSURE_SUCCESS(rv, rv);
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
  if (aFormatType.EqualsLiteral("text/plain"))
  {
    bool docEmpty;
    rv = GetDocumentIsEmpty(&docEmpty);
    NS_ENSURE_SUCCESS(rv, rv);

    if (docEmpty)
       return NS_OK;    // output nothing
  }

  nsCOMPtr<nsIDocumentEncoder> encoder;
  rv = GetAndInitDocEncoder(aFormatType, aFlags, aCharset,
                            getter_AddRefs(encoder));

  NS_ENSURE_SUCCESS(rv, rv);

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
  if (NS_SUCCEEDED(rv) && trans)
  {
    // Get the Data from the clipboard
    clipboard->GetData(trans, aSelectionType);

    // Now we ask the transferable for the data
    // it still owns the data, we just have a pointer to it.
    // If it can't support a "text" output of the data the call will fail
    nsCOMPtr<nsISupports> genericDataObj;
    uint32_t len;
    char* flav = nullptr;
    rv = trans->GetAnyTransferData(&flav, getter_AddRefs(genericDataObj),
                                   &len);
    if (NS_FAILED(rv) || !flav)
    {
      return rv;
    }

    if (0 == nsCRT::strcmp(flav, kUnicodeMime) ||
        0 == nsCRT::strcmp(flav, kMozTextInternal))
    {
      nsCOMPtr<nsISupportsString> textDataObj ( do_QueryInterface(genericDataObj) );
      if (textDataObj && len > 0)
      {
        nsAutoString stuffToPaste;
        textDataObj->GetData ( stuffToPaste );
        AutoEditBatch beginBatching(this);
        rv = InsertAsQuotation(stuffToPaste, 0);
      }
    }
    free(flav);
  }

  return rv;
}

NS_IMETHODIMP
TextEditor::InsertAsQuotation(const nsAString& aQuotedText,
                              nsIDOMNode** aNodeInserted)
{
  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  // Let the citer quote it for us:
  nsString quotedStuff;
  nsresult rv = InternetCiter::GetCiteString(aQuotedText, quotedStuff);
  NS_ENSURE_SUCCESS(rv, rv);

  // It's best to put a blank line after the quoted text so that mails
  // written without thinking won't be so ugly.
  if (!aQuotedText.IsEmpty() && (aQuotedText.Last() != char16_t('\n')))
    quotedStuff.Append(char16_t('\n'));

  // get selection
  RefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  AutoEditBatch beginBatching(this);
  AutoRules beginRulesSniffing(this, EditAction::insertText, nsIEditor::eNext);

  // give rules a chance to handle or cancel
  TextRulesInfo ruleInfo(EditAction::insertElement);
  bool cancel, handled;
  rv = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (cancel) return NS_OK; // rules canceled the operation
  if (!handled)
  {
    rv = InsertText(quotedStuff);

    // XXX Should set *aNodeInserted to the first node inserted
    if (aNodeInserted && NS_SUCCEEDED(rv))
    {
      *aNodeInserted = 0;
      //NS_IF_ADDREF(*aNodeInserted);
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

  if (!*aIsCollapsed)
    aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
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
  if (wrapCol <= 0)
    wrapCol = 72;

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

  if (isCollapsed)    // rewrap the whole document
    SelectAll();

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

  if (isCollapsed)    // rewrap the whole document
  {
    rv = SelectAll();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return InsertText(stripped);
}

NS_IMETHODIMP
TextEditor::GetEmbeddedObjects(nsISupportsArray** aNodeList)
{
  *aNodeList = 0;
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
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  EditorBase::StartOperation(opID, aDirection);  // will set mAction, mDirection
  if (mRules) return mRules->BeforeEdit(mAction, mDirection);
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
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  // post processing
  nsresult res = NS_OK;
  if (mRules) res = mRules->AfterEdit(mAction, mDirection);
  EditorBase::EndOperation();  // will clear mAction, mDirection
  return res;
}

nsresult
TextEditor::SelectEntireDocument(Selection* aSelection)
{
  if (!aSelection || !mRules) { return NS_ERROR_NULL_POINTER; }

  // Protect the edit rules object from dying
  nsCOMPtr<nsIEditRules> kungFuDeathGrip(mRules);

  // is doc empty?
  bool bDocIsEmpty;
  if (NS_SUCCEEDED(mRules->DocumentIsEmpty(&bDocIsEmpty)) && bDocIsEmpty)
  {
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
TextEditor::SetAttributeOrEquivalent(nsIDOMElement* aElement,
                                     const nsAString& aAttribute,
                                     const nsAString& aValue,
                                     bool aSuppressTransaction)
{
  return EditorBase::SetAttribute(aElement, aAttribute, aValue);
}

nsresult
TextEditor::RemoveAttributeOrEquivalent(nsIDOMElement* aElement,
                                        const nsAString& aAttribute,
                                        bool aSuppressTransaction)
{
  return EditorBase::RemoveAttribute(aElement, aAttribute);
}

} // namespace mozilla
