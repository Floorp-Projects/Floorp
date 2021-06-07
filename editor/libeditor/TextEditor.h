/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextEditor_h
#define mozilla_TextEditor_h

#include "mozilla/EditorBase.h"
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
class TextEditor : public EditorBase, public nsITimerCallback, public nsINamed {
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

  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  // Overrides of nsIEditor
  NS_IMETHOD GetTextLength(int32_t* aCount) override;
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD Paste(int32_t aClipboardType) override {
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
  MOZ_CAN_RUN_SCRIPT virtual nsresult Init(Document& aDoc, Element* aRoot,
                                           nsISelectionController* aSelCon,
                                           uint32_t aFlags,
                                           const nsAString& aValue) override;

  bool IsEmpty() const override;

  bool CanPaste(int32_t aClipboardType) const override;

  MOZ_CAN_RUN_SCRIPT nsresult
  PasteTransferableAsAction(nsITransferable* aTransferable,
                            nsIPrincipal* aPrincipal = nullptr) override;

  virtual bool CanPasteTransferable(nsITransferable* aTransferable) override;

  MOZ_CAN_RUN_SCRIPT virtual nsresult HandleKeyPressEvent(
      WidgetKeyboardEvent* aKeyboardEvent) override;

  virtual dom::EventTarget* GetDOMEventTarget() const override;

  MOZ_CAN_RUN_SCRIPT nsresult
  PasteAsAction(int32_t aClipboardType, bool aDispatchPasteEvent,
                nsIPrincipal* aPrincipal = nullptr) override;

  MOZ_CAN_RUN_SCRIPT nsresult
  PasteAsQuotationAsAction(int32_t aClipboardType, bool aDispatchPasteEvent,
                           nsIPrincipal* aPrincipal = nullptr) override;

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
  InsertLineBreakAsAction(nsIPrincipal* aPrincipal = nullptr) override;

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
  bool IsAllMasked() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mUnmaskedStart == UINT32_MAX && mUnmaskedLength == 0;
  }
  uint32_t UnmaskedStart() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mUnmaskedStart;
  }
  uint32_t UnmaskedLength() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mUnmaskedLength;
  }
  uint32_t UnmaskedEnd() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mUnmaskedStart + mUnmaskedLength;
  }

  /**
   * IsMaskingPassword() returns false when the last caller of `Unmask()`
   * didn't want to mask again automatically.  When this returns true, user
   * input causes masking the password even before timed-out.
   */
  bool IsMaskingPassword() const {
    MOZ_ASSERT(IsPasswordEditor());
    return mIsMaskingPassword;
  }

  /**
   * PasswordMask() returns a character which masks each character in password
   * fields.
   */
  static char16_t PasswordMask();

 protected:  // May be called by friends.
  /****************************************************************************
   * Some friend classes are allowed to call the following protected methods.
   * However, those methods won't prepare caches of some objects which are
   * necessary for them.  So, if you call them from friend classes, you need
   * to make sure that AutoEditActionDataSetter is created.
   ****************************************************************************/

  // Overrides of EditorBase
  MOZ_CAN_RUN_SCRIPT virtual nsresult RemoveAttributeOrEquivalent(
      Element* aElement, nsAtom* aAttribute,
      bool aSuppressTransaction) override;
  MOZ_CAN_RUN_SCRIPT virtual nsresult SetAttributeOrEquivalent(
      Element* aElement, nsAtom* aAttribute, const nsAString& aValue,
      bool aSuppressTransaction) override;
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
      EditSubAction aEditSubAction, const nsAString& aInsertionString) override;

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult InsertDroppedDataTransferAsAction(
      AutoEditActionDataSetter& aEditActionData,
      dom::DataTransfer& aDataTransfer, const EditorDOMPoint& aDroppedAt,
      dom::Document* aSrcDocument) override;

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
                        nsIEditor::EStripWrappers aStripWrappers) override;

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
   * EnsurePaddingBRElementInMultilineEditor() creates a padding `<br>` element
   * at end of multiline text editor.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  EnsurePaddingBRElementInMultilineEditor();

  /**
   * EnsureCaretNotAtEndOfTextNode() collapses selection at the padding `<br>`
   * element (i.e., container becomes the anonymous `<div>` element) if
   * `Selection` is at end of the text node.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult EnsureCaretNotAtEndOfTextNode();

 protected:  // Called by helper classes.
  MOZ_CAN_RUN_SCRIPT virtual void OnStartToHandleTopLevelEditSubAction(
      EditSubAction aTopLevelEditSubAction,
      nsIEditor::EDirection aDirectionOfTopLevelEditSubAction,
      ErrorResult& aRv) override;
  MOZ_CAN_RUN_SCRIPT virtual nsresult OnEndHandlingTopLevelEditSubAction()
      override;

  /**
   * EnsurePaddingBRElementForEmptyEditor() creates padding <br> element for
   * empty editor or changes padding <br> element for empty last line to for
   * empty editor when we're empty.
   */
  MOZ_CAN_RUN_SCRIPT nsresult EnsurePaddingBRElementForEmptyEditor();

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
   * InitEditorContentAndSelection() may insert a padding `<br>` element for
   * if it's required in the anonymous `<div>` element and collapse selection
   * at the end if there is no selection ranges.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult InitEditorContentAndSelection();

  /**
   * CanEchoPasswordNow() returns true if currently we can echo password.
   * If it's direct user input such as pasting or dropping text, this
   * returns false even if we may echo password.
   */
  bool CanEchoPasswordNow() const;

  /**
   * Make the given selection span the entire document.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult SelectEntireDocument() override;

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  InsertWithQuotationsAsSubAction(const nsAString& aQuotedText) override;

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  InsertTextFromTransferable(nsITransferable* transferable);

  bool IsCopyToClipboardAllowedInternal() const final;

  virtual already_AddRefed<Element> GetInputEventTargetElement() const override;

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

 protected:
  // Timer to mask unmasked characters automatically.  Used only when it's
  // a password field.
  nsCOMPtr<nsITimer> mMaskTimer;

  int32_t mMaxTextLength;

  // Unmasked character range.  Used only when it's a password field.
  // If mUnmaskedLength is 0, it means there is no unmasked characters.
  uint32_t mUnmaskedStart;
  uint32_t mUnmaskedLength;

  // Set to true if all characters are masked or waiting notification from
  // `mMaskTimer`.  Otherwise, i.e., part of or all of password is unmasked
  // without setting `mMaskTimer`, set to false.
  bool mIsMaskingPassword;

  friend class DeleteNodeTransaction;
  friend class EditorBase;
  friend class InsertNodeTransaction;
};

}  // namespace mozilla

mozilla::TextEditor* nsIEditor::AsTextEditor() {
  return static_cast<mozilla::TextEditor*>(this);
}

const mozilla::TextEditor* nsIEditor::AsTextEditor() const {
  return static_cast<const mozilla::TextEditor*>(this);
}

#endif  // #ifndef mozilla_TextEditor_h
