/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextEditor_h
#define mozilla_TextEditor_h

#include "mozilla/EditorBase.h"
#include "mozilla/TextControlState.h"
#include "mozilla/UniquePtr.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsINamed.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"
#include "nscore.h"

class nsIContent;
class nsIDocumentEncoder;
class nsIOutputStream;
class nsIPrincipal;
class nsISelectionController;
class nsITransferable;

namespace mozilla {
class DeleteNodeTransaction;
class InsertNodeTransaction;
enum class EditSubAction : int32_t;

namespace dom {
class Selection;
}  // namespace dom

/**
 * The text editor implementation.
 * Use to edit text document represented as a DOM tree.
 */
class TextEditor final : public EditorBase,
                         public nsITimerCallback,
                         public nsINamed {
 public:
  /****************************************************************************
   * NOTE: DO NOT MAKE YOUR NEW METHODS PUBLIC IF they are called by other
   *       classes under libeditor except EditorEventListener and
   *       HTMLEditorEventListener because each public method which may fire
   *       eEditorInput event will need to instantiate new stack class for
   *       managing input type value of eEditorInput and cache some objects
   *       for smarter handling.  In other words, when you add new root
   *       method to edit the DOM tree, you can make your new method public.
   ****************************************************************************/

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextEditor, EditorBase)

  TextEditor();

  /**
   * Note that TextEditor::Init() shouldn't cause running script synchronously.
   * So, `MOZ_CAN_RUN_SCRIPT_BOUNDARY` is safe here.
   *
   * @param aDocument   The document which aAnonymousDivElement belongs to.
   * @param aAnonymousDivElement
   *                    The root editable element for this editor.
   * @param aSelectionController
   *                    The selection controller for independent selections
   *                    in the `<input>` or `<textarea>` element.
   * @param aFlags      Some of nsIEditor::eEditor*Mask flags.
   * @param aPasswordMaskData
   *                    Set to an instance only when aFlags includes
   *                    `nsIEditor::eEditorPasswordMask`.  Otherwise, must be
   *                    `nullptr`.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  Init(Document& aDocument, Element& aAnonymousDivElement,
       nsISelectionController& aSelectionController, uint32_t aFlags,
       UniquePtr<PasswordMaskData>&& aPasswordMaskData);

  /**
   * PostCreate() should be called after Init, and is the time that the editor
   * tells its documentStateObservers that the document has been created.
   * Note that TextEditor::PostCreate() shouldn't cause running script
   * synchronously. So, `MOZ_CAN_RUN_SCRIPT_BOUNDARY` is safe here.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult PostCreate();

  /**
   * PreDestroy() is called before the editor goes away, and gives the editor a
   * chance to tell its documentStateObservers that the document is going away.
   * Note that TextEditor::PreDestroy() shouldn't cause running script
   * synchronously. So, `MOZ_CAN_RUN_SCRIPT_BOUNDARY` is safe here.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT_BOUNDARY UniquePtr<PasswordMaskData>
  PreDestroy();

  static TextEditor* GetFrom(nsIEditor* aEditor) {
    return aEditor ? aEditor->GetAsTextEditor() : nullptr;
  }
  static const TextEditor* GetFrom(const nsIEditor* aEditor) {
    return aEditor ? aEditor->GetAsTextEditor() : nullptr;
  }

  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  // Overrides of nsIEditor
  NS_IMETHOD GetTextLength(uint32_t* aCount) final;
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD Paste(int32_t aClipboardType) final {
    const nsresult rv = TextEditor::PasteAsAction(aClipboardType, true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::PasteAsAction() failed");
    return rv;
  }

  // Shouldn't be used internally, but we need these using declarations for
  // avoiding warnings of clang.
  using EditorBase::CanCopy;
  using EditorBase::CanCut;
  using EditorBase::CanPaste;

  // Overrides of EditorBase
  bool IsEmpty() const final;

  bool CanPaste(int32_t aClipboardType) const final;

  MOZ_CAN_RUN_SCRIPT nsresult PasteTransferableAsAction(
      nsITransferable* aTransferable, nsIPrincipal* aPrincipal = nullptr) final;

  bool CanPasteTransferable(nsITransferable* aTransferable) final;

  MOZ_CAN_RUN_SCRIPT nsresult
  HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent) final;

  dom::EventTarget* GetDOMEventTarget() const final;

  MOZ_CAN_RUN_SCRIPT nsresult
  PasteAsAction(int32_t aClipboardType, bool aDispatchPasteEvent,
                nsIPrincipal* aPrincipal = nullptr) final;

  MOZ_CAN_RUN_SCRIPT nsresult
  PasteAsQuotationAsAction(int32_t aClipboardType, bool aDispatchPasteEvent,
                           nsIPrincipal* aPrincipal = nullptr) final;

  /**
   * The maximum number of characters allowed.
   *   default: -1 (unlimited).
   */
  int32_t MaxTextLength() const { return mMaxTextLength; }
  void SetMaxTextLength(int32_t aLength) { mMaxTextLength = aLength; }

  /**
   * Replace existed string with a string.
   * This is fast path to replace all string when using single line control.
   *
   * @param aString             The string to be set
   * @param aAllowBeforeInputEventCancelable
   *                            Whether `beforeinput` event which will be
   *                            dispatched for this can be cancelable or not.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetTextAsAction(
      const nsAString& aString,
      AllowBeforeInputEventCancelable aAllowBeforeInputEventCancelable,
      nsIPrincipal* aPrincipal = nullptr);

  MOZ_CAN_RUN_SCRIPT nsresult
  InsertLineBreakAsAction(nsIPrincipal* aPrincipal = nullptr) final;

  /**
   * ComputeTextValue() computes plaintext value of this editor.  This may be
   * too expensive if it's in hot path.
   *
   * @param aDocumentEncoderFlags   Flags of nsIDocumentEncoder.
   * @param aCharset                Encoding of the document.
   */
  nsresult ComputeTextValue(uint32_t aDocumentEncoderFlags,
                            nsAString& aOutputString) const {
    AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
    if (NS_WARN_IF(!editActionData.CanHandle())) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    nsresult rv = ComputeValueInternal(u"text/plain"_ns, aDocumentEncoderFlags,
                                       aOutputString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return EditorBase::ToGenericNSResult(rv);
    }
    return NS_OK;
  }

  /**
   * The following methods are available only when the instance is a password
   * editor.  They return whether there is unmasked range or not and range
   * start and length.
   */
  MOZ_ALWAYS_INLINE bool IsAllMasked() const {
    MOZ_ASSERT(IsPasswordEditor());
    return !mPasswordMaskData || mPasswordMaskData->IsAllMasked();
  }
  MOZ_ALWAYS_INLINE uint32_t UnmaskedStart() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mPasswordMaskData ? mPasswordMaskData->mUnmaskedStart : UINT32_MAX;
  }
  MOZ_ALWAYS_INLINE uint32_t UnmaskedLength() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mPasswordMaskData ? mPasswordMaskData->mUnmaskedLength : 0;
  }
  MOZ_ALWAYS_INLINE uint32_t UnmaskedEnd() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mPasswordMaskData ? mPasswordMaskData->UnmaskedEnd() : UINT32_MAX;
  }

  /**
   * IsMaskingPassword() returns false when the last caller of `Unmask()`
   * didn't want to mask again automatically.  When this returns true, user
   * input causes masking the password even before timed-out.
   */
  bool IsMaskingPassword() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mPasswordMaskData && mPasswordMaskData->mIsMaskingPassword;
  }

  /**
   * PasswordMask() returns a character which masks each character in password
   * fields.
   */
  static char16_t PasswordMask();

  /**
   * If you want to prevent to echo password temporarily, use the following
   * methods.
   */
  bool EchoingPasswordPrevented() const {
    return mPasswordMaskData && mPasswordMaskData->mEchoingPasswordPrevented;
  }
  void PreventToEchoPassword() {
    if (mPasswordMaskData) {
      mPasswordMaskData->mEchoingPasswordPrevented = true;
    }
  }
  void AllowToEchoPassword() {
    if (mPasswordMaskData) {
      mPasswordMaskData->mEchoingPasswordPrevented = false;
    }
  }

 protected:  // May be called by friends.
  /****************************************************************************
   * Some friend classes are allowed to call the following protected methods.
   * However, those methods won't prepare caches of some objects which are
   * necessary for them.  So, if you call them from friend classes, you need
   * to make sure that AutoEditActionDataSetter is created.
   ****************************************************************************/

  // Overrides of EditorBase
  MOZ_CAN_RUN_SCRIPT nsresult RemoveAttributeOrEquivalent(
      Element* aElement, nsAtom* aAttribute, bool aSuppressTransaction) final;
  MOZ_CAN_RUN_SCRIPT nsresult SetAttributeOrEquivalent(
      Element* aElement, nsAtom* aAttribute, const nsAString& aValue,
      bool aSuppressTransaction) final;
  using EditorBase::RemoveAttributeOrEquivalent;
  using EditorBase::SetAttributeOrEquivalent;

  /**
   * Replace existed string with aString.  Caller must guarantee that there
   * is a placeholder transaction which will have the transaction.
   *
   * @ param aString   The string to be set.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  SetTextAsSubAction(const nsAString& aString);

  /**
   * MaybeDoAutoPasswordMasking() may mask password if we're doing auto-masking.
   */
  void MaybeDoAutoPasswordMasking() {
    if (IsPasswordEditor() && IsMaskingPassword()) {
      MaskAllCharacters();
    }
  }

  /**
   * SetUnmaskRange() is available only when the instance is a password
   * editor.  This just updates unmask range.  I.e., caller needs to
   * guarantee to update the layout.
   *
   * @param aStart      First index to show the character.
   *                    If aLength is 0, this value is ignored.
   * @param aLength     Optional, Length to show characters.
   *                    If UINT32_MAX, it means unmasking all characters after
   *                    aStart.
   *                    If 0, it means that masking all characters.
   * @param aTimeout    Optional, specify milliseconds to hide the unmasked
   *                    characters after this call.
   *                    If 0, it means this won't mask the characters
   *                    automatically.
   *                    If aLength is 0, this value is ignored.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult SetUnmaskRange(
      uint32_t aStart, uint32_t aLength = UINT32_MAX, uint32_t aTimeout = 0) {
    return SetUnmaskRangeInternal(aStart, aLength, aTimeout, false, false);
  }

  /**
   * SetUnmaskRangeAndNotify() is available only when the instance is a
   * password editor.  This updates unmask range and notifying the text frame
   * to update the visible characters.
   *
   * @param aStart      First index to show the character.
   *                    If UINT32_MAX, it means masking all.
   * @param aLength     Optional, Length to show characters.
   *                    If UINT32_MAX, it means unmasking all characters after
   *                    aStart.
   * @param aTimeout    Optional, specify milliseconds to hide the unmasked
   *                    characters after this call.
   *                    If 0, it means this won't mask the characters
   *                    automatically.
   *                    If aLength is 0, this value is ignored.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetUnmaskRangeAndNotify(
      uint32_t aStart, uint32_t aLength = UINT32_MAX, uint32_t aTimeout = 0) {
    return SetUnmaskRangeInternal(aStart, aLength, aTimeout, true, false);
  }

  /**
   * MaskAllCharacters() is an alias of SetUnmaskRange() to mask all characters.
   * In other words, this removes existing unmask range.
   * After this is called, TextEditor starts masking password automatically.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult MaskAllCharacters() {
    if (!mPasswordMaskData) {
      return NS_OK;  // Already we don't have masked range data.
    }
    return SetUnmaskRangeInternal(UINT32_MAX, 0, 0, false, true);
  }

  /**
   * MaskAllCharactersAndNotify() is an alias of SetUnmaskRangeAndNotify() to
   * mask all characters and notifies the text frame.  In other words, this
   * removes existing unmask range.
   * After this is called, TextEditor starts masking password automatically.
   */
  MOZ_CAN_RUN_SCRIPT nsresult MaskAllCharactersAndNotify() {
    return SetUnmaskRangeInternal(UINT32_MAX, 0, 0, true, true);
  }

  /**
   * WillDeleteText() is called before `DeleteTextTransaction` or something
   * removes text in a text node.  Note that this won't be called if the
   * instance is `HTMLEditor` since supporting it makes the code complicated
   * due to mutation events.
   *
   * @param aCurrentLength      Current text length of the node.
   * @param aRemoveStartOffset  Start offset of the range to be removed.
   * @param aRemoveLength       Length of the range to be removed.
   */
  void WillDeleteText(uint32_t aCurrentLength, uint32_t aRemoveStartOffset,
                      uint32_t aRemoveLength);

  /**
   * DidInsertText() is called after `InsertTextTransaction` or something
   * inserts text into a text node.  Note that this won't be called if the
   * instance is `HTMLEditor` since supporting it makes the code complicated
   * due to mutatione events.
   *
   * @param aNewLength          New text length after the insertion.
   * @param aInsertedOffset     Start offset of the inserted text.
   * @param aInsertedLength     Length of the inserted text.
   * @return                    NS_OK or NS_ERROR_EDITOR_DESTROYED.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult DidInsertText(
      uint32_t aNewLength, uint32_t aInsertedOffset, uint32_t aInsertedLength);

 protected:  // edit sub-action handler
  /**
   * MaybeTruncateInsertionStringForMaxLength() truncates aInsertionString to
   * `maxlength` if it was not pasted in by the user.
   *
   * @param aInsertionString    [in/out] New insertion string.  This is
   *                            truncated to `maxlength` if it was not pasted in
   *                            by the user.
   * @return                    If aInsertionString is truncated, it returns "as
   *                            handled", else "as ignored."
   */
  EditActionResult MaybeTruncateInsertionStringForMaxLength(
      nsAString& aInsertionString);

  /**
   * InsertLineFeedCharacterAtSelection() inserts a linefeed character at
   * selection.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  InsertLineFeedCharacterAtSelection();

  /**
   * Handles the newline characters according to the default system prefs
   * (editor.singleLine.pasteNewlines).
   * Each value means:
   *   nsIEditor::eNewlinesReplaceWithSpaces (2, Firefox default):
   *     replace newlines with spaces.
   *   nsIEditor::eNewlinesStrip (3):
   *     remove newlines from the string.
   *   nsIEditor::eNewlinesReplaceWithCommas (4, Thunderbird default):
   *     replace newlines with commas.
   *   nsIEditor::eNewlinesStripSurroundingWhitespace (5):
   *     collapse newlines and surrounding white-space characters and
   *     remove them from the string.
   *   nsIEditor::eNewlinesPasteIntact (0):
   *     only remove the leading and trailing newlines.
   *   nsIEditor::eNewlinesPasteToFirst (1) or any other value:
   *     remove the first newline and all characters following it.
   *
   * @param aString the string to be modified in place.
   */
  void HandleNewLinesInStringForSingleLineEditor(nsString& aString) const;

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult HandleInsertText(
      EditSubAction aEditSubAction, const nsAString& aInsertionString) final;

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult InsertDroppedDataTransferAsAction(
      AutoEditActionDataSetter& aEditActionData,
      dom::DataTransfer& aDataTransfer, const EditorDOMPoint& aDroppedAt,
      dom::Document* aSrcDocument) final;

  /**
   * HandleDeleteSelectionInternal() is a helper method of
   * HandleDeleteSelection().  Must be called only when the instance is
   * TextEditor.
   * NOTE: This method creates SelectionBatcher.  Therefore, each caller
   *       needs to check if the editor is still available even if this returns
   *       NS_OK.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteSelectionInternal(nsIEditor::EDirection aDirectionAndAmount,
                                nsIEditor::EStripWrappers aStripWrappers);

  /**
   * This method handles "delete selection" commands.
   *
   * @param aDirectionAndAmount Direction of the deletion.
   * @param aStripWrappers      Must be nsIEditor::eNoStrip.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  HandleDeleteSelection(nsIEditor::EDirection aDirectionAndAmount,
                        nsIEditor::EStripWrappers aStripWrappers) final;

  /**
   * ComputeValueFromTextNodeAndBRElement() tries to compute "value" of
   * this editor content only with text nodes and `<br>` elements.
   * If this succeeds to compute the value, it's returned with aValue and
   * the result is marked as "handled".  Otherwise, the caller needs to
   * compute it with another way.
   */
  EditActionResult ComputeValueFromTextNodeAndBRElement(
      nsAString& aValue) const;

  /**
   * SetTextWithoutTransaction() is optimized method to set `<input>.value`
   * and `<textarea>.value` to aValue without transaction.  This must be
   * called only when it's not `HTMLEditor` and undo/redo is disabled.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT EditActionResult
  SetTextWithoutTransaction(const nsAString& aValue);

  /**
   * EnsureCaretNotAtEndOfTextNode() collapses selection at the padding `<br>`
   * element (i.e., container becomes the anonymous `<div>` element) if
   * `Selection` is at end of the text node.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult EnsureCaretNotAtEndOfTextNode();

 protected:  // Called by helper classes.
  MOZ_CAN_RUN_SCRIPT void OnStartToHandleTopLevelEditSubAction(
      EditSubAction aTopLevelEditSubAction,
      nsIEditor::EDirection aDirectionOfTopLevelEditSubAction,
      ErrorResult& aRv) final;
  MOZ_CAN_RUN_SCRIPT nsresult OnEndHandlingTopLevelEditSubAction() final;

  /**
   * HandleInlineSpellCheckAfterEdit() does spell-check after handling top level
   * edit subaction.
   */
  nsresult HandleInlineSpellCheckAfterEdit() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    if (!GetSpellCheckRestartPoint().IsSet()) {
      return NS_OK;  // Maybe being initialized.
    }
    nsresult rv = HandleInlineSpellCheck(GetSpellCheckRestartPoint());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to spellcheck");
    ClearSpellCheckRestartPoint();
    return rv;
  }

 protected:  // Shouldn't be used by friend classes
  virtual ~TextEditor();

  /**
   * CanEchoPasswordNow() returns true if currently we can echo password.
   * If it's direct user input such as pasting or dropping text, this
   * returns false even if we may echo password.
   */
  bool CanEchoPasswordNow() const;

  /**
   * Make the given selection span the entire document.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SelectEntireDocument() final;

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  InsertWithQuotationsAsSubAction(const nsAString& aQuotedText) final;

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  InsertTextFromTransferable(nsITransferable* transferable);

  bool IsCopyToClipboardAllowedInternal() const final;

  already_AddRefed<Element> GetInputEventTargetElement() const final;

  /**
   * See SetUnmaskRange() and SetUnmaskRangeAndNotify() for the detail.
   *
   * @param aForceStartMasking  If true, forcibly starts masking.  This should
   *                            be used only when `nsIEditor::Mask()` is called.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetUnmaskRangeInternal(uint32_t aStart,
                                                     uint32_t aLength,
                                                     uint32_t aTimeout,
                                                     bool aNotify,
                                                     bool aForceStartMasking);

  MOZ_ALWAYS_INLINE bool HasAutoMaskingTimer() const {
    return mPasswordMaskData && mPasswordMaskData->mTimer;
  }

 protected:
  UniquePtr<PasswordMaskData> mPasswordMaskData;

  int32_t mMaxTextLength = -1;

  friend class DeleteNodeTransaction;
  friend class EditorBase;
  friend class InsertNodeTransaction;
};

}  // namespace mozilla

mozilla::TextEditor* nsIEditor::AsTextEditor() {
  MOZ_DIAGNOSTIC_ASSERT(IsTextEditor());
  return static_cast<mozilla::TextEditor*>(this);
}

const mozilla::TextEditor* nsIEditor::AsTextEditor() const {
  MOZ_DIAGNOSTIC_ASSERT(IsTextEditor());
  return static_cast<const mozilla::TextEditor*>(this);
}

mozilla::TextEditor* nsIEditor::GetAsTextEditor() {
  return AsEditorBase()->IsTextEditor() ? AsTextEditor() : nullptr;
}

const mozilla::TextEditor* nsIEditor::GetAsTextEditor() const {
  return AsEditorBase()->IsTextEditor() ? AsTextEditor() : nullptr;
}

#endif  // #ifndef mozilla_TextEditor_h
