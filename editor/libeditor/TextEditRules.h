/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextEditRules_h
#define mozilla_TextEditRules_h

#include "mozilla/EditAction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/HTMLEditor.h"  // for nsIEditor::AsHTMLEditor()
#include "mozilla/TextEditor.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIEditor.h"
#include "nsINamed.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nscore.h"

namespace mozilla {

class AutoLockRulesSniffing;
class EditSubActionInfo;
class HTMLEditor;
class HTMLEditRules;
namespace dom {
class Selection;
}  // namespace dom

/**
 * Object that encapsulates HTML text-specific editing rules.
 *
 * To be a good citizen, edit rules must live by these restrictions:
 * 1. All data manipulation is through the editor.
 *    Content nodes in the document tree must <B>not</B> be manipulated
 *    directly.  Content nodes in document fragments that are not part of the
 *    document itself may be manipulated at will.  Operations on document
 *    fragments must <B>not</B> go through the editor.
 * 2. Selection must not be explicitly set by the rule method.
 *    Any manipulation of Selection must be done by the editor.
 * 3. Stop handling edit action if method returns NS_ERROR_EDITOR_DESTROYED
 *    since if mutation event lister or selectionchange event listener disables
 *    the editor, we should not modify the DOM tree anymore.
 * 4. Any method callers have to check nsresult return value (both directly or
 *    with simple class like EditActionResult) whether the value is
 *    NS_ERROR_EDITOR_DESTROYED at least.
 * 5. Callers of methods of other classes such as TextEditor, have to check
 *    CanHandleEditAction() before checking its result and if the result is
 *    false, the method have to return NS_ERROR_EDITOR_DESTROYED.  In other
 *    words, any methods which may change Selection or the DOM tree have to
 *    return nsresult directly or with simple class like EditActionResult.
 *    And such methods should be marked as MOZ_MUST_USE.
 */
class TextEditRules : public nsITimerCallback, public nsINamed {
 protected:
  typedef EditorBase::AutoSelectionRestorer AutoSelectionRestorer;
  typedef EditorBase::AutoTopLevelEditSubActionNotifier
      AutoTopLevelEditSubActionNotifier;
  typedef EditorBase::AutoTransactionsConserveSelection
      AutoTransactionsConserveSelection;

 public:
  typedef dom::Element Element;
  typedef dom::Selection Selection;
  typedef dom::Text Text;
  template <typename T>
  using OwningNonNull = OwningNonNull<T>;

  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(TextEditRules, nsITimerCallback)

  TextEditRules();

  HTMLEditRules* AsHTMLEditRules();
  const HTMLEditRules* AsHTMLEditRules() const;

  MOZ_CAN_RUN_SCRIPT
  virtual nsresult Init(TextEditor* aTextEditor);
  virtual nsresult SetInitialValue(const nsAString& aValue);
  virtual nsresult DetachEditor();
  virtual nsresult BeforeEdit(EditSubAction aEditSubAction,
                              nsIEditor::EDirection aDirection);
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult AfterEdit(EditSubAction aEditSubAction,
                             nsIEditor::EDirection aDirection);
  // NOTE: Don't mark WillDoAction() nor DidDoAction() as MOZ_CAN_RUN_SCRIPT
  //       because they are too generic and doing it makes a lot of public
  //       editor methods marked as MOZ_CAN_RUN_SCRIPT too, but some of them
  //       may not causes running script.  So, ideal fix must be that we make
  //       each method callsed by this method public.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult WillDoAction(EditSubActionInfo& aInfo, bool* aCancel,
                                bool* aHandled);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult DidDoAction(EditSubActionInfo& aInfo, nsresult aResult);

  /**
   * Return false if the editor has non-empty text nodes or non-text
   * nodes.  Otherwise, i.e., there is no meaningful content,
   * return true.
   */
  virtual bool DocumentIsEmpty();

 protected:
  virtual ~TextEditRules();

 public:
  void ResetIMETextPWBuf();

  /**
   * Handles the newline characters according to the default system prefs
   * (editor.singleLine.pasteNewlines).
   * Each value means:
   *   nsIPlaintextEditor::eNewlinesReplaceWithSpaces (2, Firefox default):
   *     replace newlines with spaces.
   *   nsIPlaintextEditor::eNewlinesStrip (3):
   *     remove newlines from the string.
   *   nsIPlaintextEditor::eNewlinesReplaceWithCommas (4, Thunderbird default):
   *     replace newlines with commas.
   *   nsIPlaintextEditor::eNewlinesStripSurroundingWhitespace (5):
   *     collapse newlines and surrounding whitespace characters and
   *     remove them from the string.
   *   nsIPlaintextEditor::eNewlinesPasteIntact (0):
   *     only remove the leading and trailing newlines.
   *   nsIPlaintextEditor::eNewlinesPasteToFirst (1) or any other value:
   *     remove the first newline and all characters following it.
   *
   * @param aString the string to be modified in place.
   */
  void HandleNewLines(nsString& aString);

  /**
   * Prepare a string buffer for being displayed as the contents of a password
   * field.  This function uses the platform-specific character for representing
   * characters entered into password fields.
   *
   * @param aOutString the output string.  When this function returns,
   *        aOutString will contain aLength password characters.
   * @param aLength the number of password characters that aOutString should
   *        contain.
   */
  static void FillBufWithPWChars(nsAString* aOutString, int32_t aLength);

  bool HasBogusNode() { return !!mBogusNode; }

  /**
   * HideLastPasswordInput() is called when Nodify() calls
   * TextEditor::HideLastPasswordInput().  It guarantees that there is a
   * AutoEditActionDataSetter instance in the editor.
   */
  MOZ_CAN_RUN_SCRIPT nsresult HideLastPasswordInput();

 protected:
  void InitFields();

  // TextEditRules implementation methods

  /**
   * Called before inserting text.
   * This method may actually inserts text into the editor.  Therefore, this
   * might cause destroying the editor.
   *
   * @param aEditSubAction      Must be EditSubAction::eInsertTextComingFromIME
   *                            or EditSubAction::eInsertText.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   * @param inString            String to be inserted.
   * @param outString           String actually inserted.
   * @param aMaxLength          The maximum string length which the editor
   *                            allows to set.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillInsertText(EditSubAction aEditSubAction,
                                       bool* aCancel, bool* aHandled,
                                       const nsAString* inString,
                                       nsAString* outString,
                                       int32_t aMaxLength);

  /**
   * Called before inserting a line break into the editor.
   * This method removes selected text if selection isn't collapsed.
   * Therefore, this might cause destroying the editor.
   *
   * @param aMaxLength          The maximum string length which the editor
   *                            allows to set.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE EditActionResult WillInsertLineBreak(int32_t aMaxLength);

  /**
   * Called before setting text to the text editor.
   * This method may actually set text to it.  Therefore, this might cause
   * destroying the text editor.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   * @param inString            String to be set.
   * @param aMaxLength          The maximum string length which the text editor
   *                            allows to set.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillSetText(bool* aCancel, bool* aHandled,
                                    const nsAString* inString,
                                    int32_t aMaxLength);

  /**
   * Called before inserting something into the editor.
   * This method may removes mBougsNode if there is.  Therefore, this method
   * might cause destroying the editor.
   *
   * @param aCancel             Returns true if the operation is canceled.
   *                            This can be nullptr.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult WillInsert(bool* aCancel = nullptr);

  /**
   * Called before deleting selected content.
   * This method may actually remove the selected content with
   * DeleteSelectionWithTransaction().  So, this might cause destroying the
   * editor.
   *
   * @param aCaollapsedAction   Direction to extend the selection.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillDeleteSelection(
      nsIEditor::EDirection aCollapsedAction, bool* aCancel, bool* aHandled);

  /**
   * DeleteSelectionWithTransaction() is internal method of
   * WillDeleteSelection() since it needs to create SelectionBatcher in
   * big scope and destroying it might causes destroying the editor.
   * So, after calling this method, callers need to check CanHandleEditAction()
   * manually.
   *
   * @param aCaollapsedAction   Direction to extend the selection.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult DeleteSelectionWithTransaction(
      nsIEditor::EDirection aCollapsedAction, bool* aCancel, bool* aHandled);

  /**
   * Called after deleted selected content.
   * This method may remove empty text node and makes guarantee that caret
   * is never at left of <br> element.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult DidDeleteSelection();

  nsresult WillSetTextProperty(bool* aCancel, bool* aHandled);

  nsresult WillRemoveTextProperty(bool* aCancel, bool* aHandled);

  nsresult WillUndo(bool* aCancel, bool* aHandled);
  nsresult DidUndo(nsresult aResult);

  nsresult WillRedo(bool* aCancel, bool* aHandled);
  nsresult DidRedo(nsresult aResult);

  /**
   * Called prior to nsIEditor::OutputToString.
   *
   * @param aInFormat  The format requested for the output, a MIME type.
   * @param aOutText   The string to use for output, if aCancel is set to true.
   * @param aOutCancel If set to true, the caller should cancel the operation
   *                   and use aOutText as the result.
   */
  nsresult WillOutputText(const nsAString* aInFormat, nsAString* aOutText,
                          uint32_t aFlags, bool* aOutCancel, bool* aHandled);

  /**
   * Check for and replace a redundant trailing break.
   */
  MOZ_MUST_USE nsresult RemoveRedundantTrailingBR();

  /**
   * Creates a trailing break in the text doc if there is not one already.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult CreateTrailingBRIfNeeded();

  /**
   * Creates a bogus <br> node if the root element has no editable content.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult CreateBogusNodeIfNeeded();

  /**
   * Returns a truncated insertion string if insertion would place us over
   * aMaxLength
   */
  nsresult TruncateInsertionIfNeeded(const nsAString* aInString,
                                     nsAString* aOutString, int32_t aMaxLength,
                                     bool* aTruncated);

  /**
   * Remove IME composition text from password buffer.
   */
  void RemoveIMETextFromPWBuf(uint32_t& aStart, nsAString* aIMEString);

  /**
   * Create a normal <br> element and insert it to aPointToInsert.
   *
   * @param aPointToInsert  The point where the new <br> element will be
   *                        inserted.
   * @return                Returns created <br> element or an error code
   *                        if couldn't create new <br> element.
   */
  MOZ_CAN_RUN_SCRIPT CreateElementResult
  CreateBR(const EditorDOMPoint& aPointToInsert) {
    CreateElementResult ret = CreateBRInternal(aPointToInsert, false);
#ifdef DEBUG
    // If editor is destroyed, it must return NS_ERROR_EDITOR_DESTROYED.
    if (!CanHandleEditAction()) {
      MOZ_ASSERT(ret.Rv() == NS_ERROR_EDITOR_DESTROYED);
    }
#endif  // #ifdef DEBUG
    return ret;
  }

  /**
   * Create a moz-<br> element and insert it to aPointToInsert.
   *
   * @param aPointToInsert  The point where the new moz-<br> element will be
   *                        inserted.
   * @return                Returns created <br> element or an error code
   *                        if couldn't create new <br> element.
   */
  MOZ_CAN_RUN_SCRIPT CreateElementResult
  CreateMozBR(const EditorDOMPoint& aPointToInsert) {
    CreateElementResult ret = CreateBRInternal(aPointToInsert, true);
#ifdef DEBUG
    // If editor is destroyed, it must return NS_ERROR_EDITOR_DESTROYED.
    if (!CanHandleEditAction()) {
      MOZ_ASSERT(ret.Rv() == NS_ERROR_EDITOR_DESTROYED);
    }
#endif  // #ifdef DEBUG
    return ret;
  }

  void UndefineCaretBidiLevel();

  nsresult CheckBidiLevelForDeletion(const EditorRawDOMPoint& aSelectionPoint,
                                     nsIEditor::EDirection aAction,
                                     bool* aCancel);

  /**
   * HideLastPasswordInputInternal() replaces last password characters which
   * have not been replaced with mask character like '*' with with the mask
   * character.  This method may cause destroying the editor.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult HideLastPasswordInputInternal();

  /**
   * CollapseSelectionToTrailingBRIfNeeded() collapses selection after the
   * text node if:
   * - the editor is text editor
   * - and Selection is collapsed at the end of the text node
   * - and the text node is followed by moz-<br>.
   */
  MOZ_MUST_USE nsresult CollapseSelectionToTrailingBRIfNeeded();

  bool IsPasswordEditor() const;
  bool IsSingleLineEditor() const;
  bool IsPlaintextEditor() const;
  bool IsReadonly() const;
  bool IsDisabled() const;
  bool IsMailEditor() const;
  bool DontEchoPassword() const;

 private:
  TextEditor* MOZ_NON_OWNING_REF mTextEditor;

  /**
   * Create a normal <br> element or a moz-<br> element and insert it to
   * aPointToInsert.
   *
   * @param aParentToInsert     The point where the new <br> element will be
   *                            inserted.
   * @param aCreateMozBR        true if the caller wants to create a moz-<br>
   *                            element.  Otherwise, false.
   * @return                    Returns created <br> element and error code.
   *                            If it succeeded, never returns nullptr.
   */
  MOZ_CAN_RUN_SCRIPT CreateElementResult
  CreateBRInternal(const EditorDOMPoint& aPointToInsert, bool aCreateMozBR);

 protected:
  /**
   * AutoSafeEditorData grabs editor instance and related instances during
   * handling an edit action.  When this is created, its pointer is set to
   * the mSafeData, and this guarantees the lifetime of grabbing objects
   * until it's destroyed.
   */
  class MOZ_STACK_CLASS AutoSafeEditorData {
   public:
    AutoSafeEditorData(TextEditRules& aTextEditRules, TextEditor& aTextEditor)
        : mTextEditRules(aTextEditRules), mHTMLEditor(nullptr) {
      // mTextEditRules may have AutoSafeEditorData instance since in some
      // cases. E.g., while public methods of *EditRules are called, it
      // calls attaching editor's method, then, editor will call public
      // methods of *EditRules again.
      if (mTextEditRules.mData) {
        return;
      }
      mTextEditor = &aTextEditor;
      mHTMLEditor = aTextEditor.AsHTMLEditor();
      mTextEditRules.mData = this;
    }

    ~AutoSafeEditorData() {
      if (mTextEditRules.mData != this) {
        return;
      }
      mTextEditRules.mData = nullptr;
    }

    TextEditor& TextEditorRef() const { return *mTextEditor; }
    HTMLEditor& HTMLEditorRef() const {
      MOZ_ASSERT(mHTMLEditor);
      return *mHTMLEditor;
    }

   private:
    // This class should be created by public methods TextEditRules and
    // HTMLEditRules and in the stack.  So, the lifetime of this should
    // be guaranteed the callers of the public methods.
    TextEditRules& MOZ_NON_OWNING_REF mTextEditRules;
    RefPtr<TextEditor> mTextEditor;
    // Shortcut for HTMLEditorRef().  So, not necessary to use RefPtr.
    HTMLEditor* MOZ_NON_OWNING_REF mHTMLEditor;
  };
  AutoSafeEditorData* mData;

  TextEditor& TextEditorRef() const {
    MOZ_ASSERT(mData);
    return mData->TextEditorRef();
  }
  // SelectionRefPtr() won't return nullptr unless editor instance accidentally
  // ignored result of AutoEditActionDataSetter::CanHandle() and keep handling
  // edit action.
  const RefPtr<Selection>& SelectionRefPtr() const {
    MOZ_ASSERT(mData);
    return TextEditorRef().SelectionRefPtr();
  }
  bool CanHandleEditAction() const {
    if (!mTextEditor) {
      return false;
    }
    if (mTextEditor->Destroyed()) {
      return false;
    }
    MOZ_ASSERT(mTextEditor->IsInitialized());
    return true;
  }

#ifdef DEBUG
  bool IsEditorDataAvailable() const { return !!mData; }
#endif  // #ifdef DEBUG

  /**
   * GetTextNodeAroundSelectionStartContainer() may return a Text node around
   * start container of Selection.  If current selection container is not
   * a text node, this will look for descendants and next siblings of the
   * container.
   */
  inline already_AddRefed<nsINode> GetTextNodeAroundSelectionStartContainer();

  // A buffer we use to store the real value of password editors.
  nsString mPasswordText;
  // A buffer we use to track the IME composition string.
  nsString mPasswordIMEText;
  uint32_t mPasswordIMEIndex;
  // Magic node acts as placeholder in empty doc.
  nsCOMPtr<nsIContent> mBogusNode;
  // Cached selected node.
  nsCOMPtr<nsINode> mCachedSelectionNode;
  // Cached selected offset.
  uint32_t mCachedSelectionOffset;
  uint32_t mActionNesting;
  bool mLockRulesSniffing;
  bool mDidExplicitlySetInterline;
  // In bidirectional text, delete characters not visually adjacent to the
  // caret without moving the caret first.
  bool mDeleteBidiImmediately;
  bool mIsHTMLEditRules;
  // The top level editor action.
  EditSubAction mTopLevelEditSubAction;
  nsCOMPtr<nsITimer> mTimer;
  uint32_t mLastStart;
  uint32_t mLastLength;

  // friends
  friend class AutoLockRulesSniffing;
};

/**
 * An object to encapsulate any additional info needed to be passed
 * to rules system by the editor.
 * TODO: This class (almost struct, though) is ugly and its size isn't
 *       optimized.  Should be refined later.
 */
class MOZ_STACK_CLASS EditSubActionInfo final {
 public:
  explicit EditSubActionInfo(EditSubAction aEditSubAction)
      : mEditSubAction(aEditSubAction),
        inString(nullptr),
        outString(nullptr),
        outputFormat(nullptr),
        maxLength(-1),
        flags(0),
        collapsedAction(nsIEditor::eNext),
        stripWrappers(nsIEditor::eStrip),
        entireList(false),
        bulletType(nullptr),
        alignType(nullptr),
        blockType(nullptr) {}

  EditSubAction mEditSubAction;

  // EditSubAction::eInsertText / EditSubAction::eInsertTextComingFromIME
  const nsAString* inString;
  nsAString* outString;
  const nsAString* outputFormat;
  int32_t maxLength;

  // EditSubAction::eComputeTextToOutput
  uint32_t flags;

  // EditSubAction::eDeleteSelectedContent
  nsIEditor::EDirection collapsedAction;
  nsIEditor::EStripWrappers stripWrappers;

  // EditSubAction::eCreateOrChangeList
  bool entireList;
  const nsAString* bulletType;

  // EditSubAction::eSetOrClearAlignment
  const nsAString* alignType;

  // EditSubAction::eCreateOrRemoveBlock
  const nsAString* blockType;
};

/**
 * Stack based helper class for managing TextEditRules::mLockRluesSniffing.
 * This class sets a bool letting us know to ignore any rules sniffing
 * that tries to occur reentrantly.
 */
class MOZ_STACK_CLASS AutoLockRulesSniffing final {
 public:
  explicit AutoLockRulesSniffing(TextEditRules* aRules) : mRules(aRules) {
    if (mRules) {
      mRules->mLockRulesSniffing = true;
    }
  }

  ~AutoLockRulesSniffing() {
    if (mRules) {
      mRules->mLockRulesSniffing = false;
    }
  }

 protected:
  TextEditRules* mRules;
};

/**
 * Stack based helper class for turning on/off the edit listener.
 */
class MOZ_STACK_CLASS AutoLockListener final {
 public:
  explicit AutoLockListener(bool* aEnabled)
      : mEnabled(aEnabled), mOldState(false) {
    if (mEnabled) {
      mOldState = *mEnabled;
      *mEnabled = false;
    }
  }

  ~AutoLockListener() {
    if (mEnabled) {
      *mEnabled = mOldState;
    }
  }

 protected:
  bool* mEnabled;
  bool mOldState;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_TextEditRules_h
