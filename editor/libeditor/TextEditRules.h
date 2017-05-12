/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextEditRules_h
#define mozilla_TextEditRules_h

#include "mozilla/EditorBase.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIEditRules.h"
#include "nsIEditor.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nscore.h"

class nsIDOMNode;

namespace mozilla {

class AutoLockRulesSniffing;
class TextEditor;
namespace dom {
class Selection;
} // namespace dom

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
 */
class TextEditRules : public nsIEditRules
                    , public nsITimerCallback
{
public:
  typedef dom::Element Element;
  typedef dom::Selection Selection;
  typedef dom::Text Text;
  template<typename T> using OwningNonNull = OwningNonNull<T>;

  NS_DECL_NSITIMERCALLBACK
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(TextEditRules, nsIEditRules)

  TextEditRules();

  // nsIEditRules methods
  NS_IMETHOD Init(TextEditor* aTextEditor) override;
  NS_IMETHOD SetInitialValue(const nsAString& aValue) override;
  NS_IMETHOD DetachEditor() override;
  NS_IMETHOD BeforeEdit(EditAction action,
                        nsIEditor::EDirection aDirection) override;
  NS_IMETHOD AfterEdit(EditAction action,
                       nsIEditor::EDirection aDirection) override;
  NS_IMETHOD WillDoAction(Selection* aSelection, RulesInfo* aInfo,
                          bool* aCancel, bool* aHandled) override;
  NS_IMETHOD DidDoAction(Selection* aSelection, RulesInfo* aInfo,
                         nsresult aResult) override;
  NS_IMETHOD_(bool) DocumentIsEmpty() override;
  NS_IMETHOD DocumentModified() override;

protected:
  virtual ~TextEditRules();

public:
  void ResetIMETextPWBuf();

  /**
   * Handles the newline characters either according to aNewLineHandling
   * or to the default system prefs if aNewLineHandling is negative.
   *
   * @param aString the string to be modified in place.
   * @param aNewLineHandling determine the desired type of newline handling:
   *        * negative values:
   *          handle newlines according to platform defaults.
   *        * nsIPlaintextEditor::eNewlinesReplaceWithSpaces:
   *          replace newlines with spaces.
   *        * nsIPlaintextEditor::eNewlinesStrip:
   *          remove newlines from the string.
   *        * nsIPlaintextEditor::eNewlinesReplaceWithCommas:
   *          replace newlines with commas.
   *        * nsIPlaintextEditor::eNewlinesStripSurroundingWhitespace:
   *          collapse newlines and surrounding whitespace characters and
   *          remove them from the string.
   *        * nsIPlaintextEditor::eNewlinesPasteIntact:
   *          only remove the leading and trailing newlines.
   *        * nsIPlaintextEditor::eNewlinesPasteToFirst or any other value:
   *          remove the first newline and all characters following it.
   */
  static void HandleNewLines(nsString& aString, int32_t aNewLineHandling);

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

protected:

  void InitFields();

  // TextEditRules implementation methods
  nsresult WillInsertText(EditAction aAction,
                          Selection* aSelection,
                          bool* aCancel,
                          bool* aHandled,
                          const nsAString* inString,
                          nsAString* outString,
                          int32_t aMaxLength);
  nsresult DidInsertText(Selection* aSelection, nsresult aResult);

  nsresult WillInsertBreak(Selection* aSelection, bool* aCancel,
                           bool* aHandled, int32_t aMaxLength);
  nsresult DidInsertBreak(Selection* aSelection, nsresult aResult);

  void WillInsert(Selection& aSelection, bool* aCancel);
  nsresult DidInsert(Selection* aSelection, nsresult aResult);

  nsresult WillDeleteSelection(Selection* aSelection,
                               nsIEditor::EDirection aCollapsedAction,
                               bool* aCancel,
                               bool* aHandled);
  nsresult DidDeleteSelection(Selection* aSelection,
                              nsIEditor::EDirection aCollapsedAction,
                              nsresult aResult);

  nsresult WillSetTextProperty(Selection* aSelection, bool* aCancel,
                               bool* aHandled);
  nsresult DidSetTextProperty(Selection* aSelection, nsresult aResult);

  nsresult WillRemoveTextProperty(Selection* aSelection, bool* aCancel,
                                  bool* aHandled);
  nsresult DidRemoveTextProperty(Selection* aSelection, nsresult aResult);

  nsresult WillUndo(Selection* aSelection, bool* aCancel, bool* aHandled);
  nsresult DidUndo(Selection* aSelection, nsresult aResult);

  nsresult WillRedo(Selection* aSelection, bool* aCancel, bool* aHandled);
  nsresult DidRedo(Selection* aSelection, nsresult aResult);

  /**
   * Called prior to nsIEditor::OutputToString.
   * @param aSelection
   * @param aInFormat  The format requested for the output, a MIME type.
   * @param aOutText   The string to use for output, if aCancel is set to true.
   * @param aOutCancel If set to true, the caller should cancel the operation
   *                   and use aOutText as the result.
   */
  nsresult WillOutputText(Selection* aSelection,
                          const nsAString* aInFormat,
                          nsAString* aOutText,
                          bool* aOutCancel,
                          bool* aHandled);

  nsresult DidOutputText(Selection* aSelection, nsresult aResult);

  /**
   * Check for and replace a redundant trailing break.
   */
  nsresult RemoveRedundantTrailingBR();

  /**
   * Creates a trailing break in the text doc if there is not one already.
   */
  nsresult CreateTrailingBRIfNeeded();

  /**
   * Creates a bogus text node if the document has no editable content.
   */
  nsresult CreateBogusNodeIfNeeded(Selection* aSelection);

  /**
   * Returns a truncated insertion string if insertion would place us over
   * aMaxLength
   */
  nsresult TruncateInsertionIfNeeded(Selection* aSelection,
                                     const nsAString* aInString,
                                     nsAString* aOutString,
                                     int32_t aMaxLength,
                                     bool* aTruncated);

  /**
   * Remove IME composition text from password buffer.
   */
  void RemoveIMETextFromPWBuf(uint32_t& aStart, nsAString* aIMEString);

  nsresult CreateMozBR(nsIDOMNode* inParent, int32_t inOffset,
                       nsIDOMNode** outBRNode = nullptr);

  void UndefineCaretBidiLevel(Selection* aSelection);

  nsresult CheckBidiLevelForDeletion(Selection* aSelection,
                                     nsIDOMNode* aSelNode,
                                     int32_t aSelOffset,
                                     nsIEditor::EDirection aAction,
                                     bool* aCancel);

  nsresult HideLastPWInput();

  nsresult CollapseSelectionToTrailingBRIfNeeded(Selection* aSelection);

  bool IsPasswordEditor() const;
  bool IsSingleLineEditor() const;
  bool IsPlaintextEditor() const;
  bool IsReadonly() const;
  bool IsDisabled() const;
  bool IsMailEditor() const;
  bool DontEchoPassword() const;

private:
  // Note that we do not refcount the editor.
  TextEditor* mTextEditor;

protected:
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
  int32_t mCachedSelectionOffset;
  uint32_t mActionNesting;
  bool mLockRulesSniffing;
  bool mDidExplicitlySetInterline;
  // In bidirectional text, delete characters not visually adjacent to the
  // caret without moving the caret first.
  bool mDeleteBidiImmediately;
  // The top level editor action.
  EditAction mTheAction;
  nsCOMPtr<nsITimer> mTimer;
  uint32_t mLastStart;
  uint32_t mLastLength;

  // friends
  friend class AutoLockRulesSniffing;
};

class TextRulesInfo final : public RulesInfo
{
public:
  explicit TextRulesInfo(EditAction aAction)
    : RulesInfo(aAction)
    , inString(nullptr)
    , outString(nullptr)
    , outputFormat(nullptr)
    , maxLength(-1)
    , collapsedAction(nsIEditor::eNext)
    , stripWrappers(nsIEditor::eStrip)
    , bOrdered(false)
    , entireList(false)
    , bulletType(nullptr)
    , alignType(nullptr)
    , blockType(nullptr)
  {}

  // EditAction::insertText / EditAction::insertIMEText
  const nsAString* inString;
  nsAString* outString;
  const nsAString* outputFormat;
  int32_t maxLength;

  // EditAction::deleteSelection
  nsIEditor::EDirection collapsedAction;
  nsIEditor::EStripWrappers stripWrappers;

  // EditAction::removeList
  bool bOrdered;

  // EditAction::makeList
  bool entireList;
  const nsAString* bulletType;

  // EditAction::align
  const nsAString* alignType;

  // EditAction::makeBasicBlock
  const nsAString* blockType;
};

/**
 * Stack based helper class for StartOperation()/EndOperation() sandwich.
 * This class sets a bool letting us know to ignore any rules sniffing
 * that tries to occur reentrantly.
 */
class MOZ_STACK_CLASS AutoLockRulesSniffing final
{
public:
  explicit AutoLockRulesSniffing(TextEditRules* aRules)
    : mRules(aRules)
  {
    if (mRules) {
      mRules->mLockRulesSniffing = true;
    }
  }

  ~AutoLockRulesSniffing()
  {
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
class MOZ_STACK_CLASS AutoLockListener final
{
public:
  explicit AutoLockListener(bool* aEnabled)
    : mEnabled(aEnabled)
    , mOldState(false)
  {
    if (mEnabled) {
      mOldState = *mEnabled;
      *mEnabled = false;
    }
  }

  ~AutoLockListener()
  {
    if (mEnabled) {
      *mEnabled = mOldState;
    }
  }

protected:
  bool* mEnabled;
  bool mOldState;
};

} // namespace mozilla

#endif // #ifndef mozilla_TextEditRules_h
