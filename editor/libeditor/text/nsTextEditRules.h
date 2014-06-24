/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextEditRules_h__
#define nsTextEditRules_h__

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsEditRules.h"
#include "nsEditor.h"
#include "nsIEditor.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"
#include "nsPlaintextEditor.h"
#include "nsString.h"
#include "nscore.h"

class nsIDOMElement;
class nsIDOMNode;
class nsISelection;
namespace mozilla {
namespace dom {
class Selection;
}  // namespace dom
}  // namespace mozilla

/** Object that encapsulates HTML text-specific editing rules.
  *  
  * To be a good citizen, edit rules must live by these restrictions:
  * 1. All data manipulation is through the editor.  
  *    Content nodes in the document tree must <B>not</B> be manipulated directly.
  *    Content nodes in document fragments that are not part of the document itself
  *    may be manipulated at will.  Operations on document fragments must <B>not</B>
  *    go through the editor.
  * 2. Selection must not be explicitly set by the rule method.  
  *    Any manipulation of Selection must be done by the editor.
  */
class nsTextEditRules : public nsIEditRules, public nsITimerCallback
{
public:
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsTextEditRules, nsIEditRules)

  nsTextEditRules();

  // nsIEditRules methods
  NS_IMETHOD Init(nsPlaintextEditor *aEditor);
  NS_IMETHOD SetInitialValue(const nsAString& aValue);
  NS_IMETHOD DetachEditor();
  NS_IMETHOD BeforeEdit(EditAction action,
                        nsIEditor::EDirection aDirection);
  NS_IMETHOD AfterEdit(EditAction action,
                       nsIEditor::EDirection aDirection);
  NS_IMETHOD WillDoAction(mozilla::dom::Selection* aSelection,
                          nsRulesInfo* aInfo, bool* aCancel, bool* aHandled);
  NS_IMETHOD DidDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
  NS_IMETHOD DocumentIsEmpty(bool *aDocumentIsEmpty);
  NS_IMETHOD DocumentModified();

protected:
  virtual ~nsTextEditRules();

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
  static void HandleNewLines(nsString &aString, int32_t aNewLineHandling);

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
  static void FillBufWithPWChars(nsAString *aOutString, int32_t aLength);

protected:

  void InitFields();

  // nsTextEditRules implementation methods
  nsresult WillInsertText(  EditAction aAction,
                            mozilla::dom::Selection* aSelection,
                            bool            *aCancel,
                            bool            *aHandled,
                            const nsAString *inString,
                            nsAString       *outString,
                            int32_t          aMaxLength);
  nsresult DidInsertText(nsISelection *aSelection, nsresult aResult);
  nsresult GetTopEnclosingPre(nsIDOMNode *aNode, nsIDOMNode** aOutPreNode);

  nsresult WillInsertBreak(mozilla::dom::Selection* aSelection, bool* aCancel,
                           bool *aHandled, int32_t aMaxLength);
  nsresult DidInsertBreak(nsISelection *aSelection, nsresult aResult);

  nsresult WillInsert(nsISelection *aSelection, bool *aCancel);
  nsresult DidInsert(nsISelection *aSelection, nsresult aResult);

  nsresult WillDeleteSelection(mozilla::dom::Selection* aSelection,
                               nsIEditor::EDirection aCollapsedAction, 
                               bool *aCancel,
                               bool *aHandled);
  nsresult DidDeleteSelection(nsISelection *aSelection, 
                              nsIEditor::EDirection aCollapsedAction, 
                              nsresult aResult);

  nsresult WillSetTextProperty(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult DidSetTextProperty(nsISelection *aSelection, nsresult aResult);

  nsresult WillRemoveTextProperty(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult DidRemoveTextProperty(nsISelection *aSelection, nsresult aResult);

  nsresult WillUndo(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult DidUndo(nsISelection *aSelection, nsresult aResult);

  nsresult WillRedo(nsISelection *aSelection, bool *aCancel, bool *aHandled);
  nsresult DidRedo(nsISelection *aSelection, nsresult aResult);

  /** called prior to nsIEditor::OutputToString
    * @param aSelection
    * @param aInFormat  the format requested for the output, a MIME type
    * @param aOutText   the string to use for output, if aCancel is set to true
    * @param aOutCancel if set to true, the caller should cancel the operation
    *                   and use aOutText as the result.
    */
  nsresult WillOutputText(nsISelection *aSelection,
                          const nsAString  *aInFormat,
                          nsAString *aOutText, 
                          bool     *aOutCancel, 
                          bool *aHandled);

  nsresult DidOutputText(nsISelection *aSelection, nsresult aResult);


  // helper functions

  /** check for and replace a redundant trailing break */
  nsresult RemoveRedundantTrailingBR();
  
  /** creates a trailing break in the text doc if there is not one already */
  nsresult CreateTrailingBRIfNeeded();
  
 /** creates a bogus text node if the document has no editable content */
  nsresult CreateBogusNodeIfNeeded(nsISelection *aSelection);

  /** returns a truncated insertion string if insertion would place us
      over aMaxLength */
  nsresult TruncateInsertionIfNeeded(mozilla::dom::Selection*  aSelection,
                                     const nsAString          *aInString,
                                     nsAString                *aOutString,
                                     int32_t                   aMaxLength,
                                     bool                     *aTruncated);

  /** Remove IME composition text from password buffer */
  void RemoveIMETextFromPWBuf(int32_t &aStart, nsAString *aIMEString);

  nsresult CreateMozBR(nsIDOMNode* inParent, int32_t inOffset,
                       nsIDOMNode** outBRNode = nullptr);

  nsresult CheckBidiLevelForDeletion(nsISelection         *aSelection,
                                     nsIDOMNode           *aSelNode, 
                                     int32_t               aSelOffset, 
                                     nsIEditor::EDirection aAction,
                                     bool                 *aCancel);

  nsresult HideLastPWInput();

  nsresult CollapseSelectionToTrailingBRIfNeeded(nsISelection *aSelection);

  bool IsPasswordEditor() const
  {
    return mEditor ? mEditor->IsPasswordEditor() : false;
  }
  bool IsSingleLineEditor() const
  {
    return mEditor ? mEditor->IsSingleLineEditor() : false;
  }
  bool IsPlaintextEditor() const
  {
    return mEditor ? mEditor->IsPlaintextEditor() : false;
  }
  bool IsReadonly() const
  {
    return mEditor ? mEditor->IsReadonly() : false;
  }
  bool IsDisabled() const
  {
    return mEditor ? mEditor->IsDisabled() : false;
  }
  bool IsMailEditor() const
  {
    return mEditor ? mEditor->IsMailEditor() : false;
  }
  bool DontEchoPassword() const
  {
    return mEditor ? mEditor->DontEchoPassword() : false;
  }

  // data members
  nsPlaintextEditor   *mEditor;        // note that we do not refcount the editor
  nsString             mPasswordText;  // a buffer we use to store the real value of password editors
  nsString             mPasswordIMEText;  // a buffer we use to track the IME composition string
  uint32_t             mPasswordIMEIndex;
  nsCOMPtr<nsIDOMNode> mBogusNode;     // magic node acts as placeholder in empty doc
  nsCOMPtr<nsIDOMNode> mCachedSelectionNode;    // cached selected node
  int32_t              mCachedSelectionOffset;  // cached selected offset
  uint32_t             mActionNesting;
  bool                 mLockRulesSniffing;
  bool                 mDidExplicitlySetInterline;
  bool                 mDeleteBidiImmediately; // in bidirectional text, delete
                                               // characters not visually 
                                               // adjacent to the caret without
                                               // moving the caret first.
  EditAction mTheAction;     // the top level editor action
  nsCOMPtr<nsITimer>   mTimer;
  uint32_t             mLastStart, mLastLength;

  // friends
  friend class nsAutoLockRulesSniffing;

};



class nsTextRulesInfo : public nsRulesInfo
{
 public:
 
  nsTextRulesInfo(EditAction aAction) :
    nsRulesInfo(aAction),
    inString(0),
    outString(0),
    outputFormat(0),
    maxLength(-1),
    collapsedAction(nsIEditor::eNext),
    stripWrappers(nsIEditor::eStrip),
    bOrdered(false),
    entireList(false),
    bulletType(0),
    alignType(0),
    blockType(0),
    insertElement(0)
    {}

  virtual ~nsTextRulesInfo() {}
  
  // kInsertText
  const nsAString *inString;
  nsAString *outString;
  const nsAString *outputFormat;
  int32_t maxLength;
  
  // kDeleteSelection
  nsIEditor::EDirection collapsedAction;
  nsIEditor::EStripWrappers stripWrappers;
  
  // kMakeList
  bool bOrdered;
  bool entireList;
  const nsAString *bulletType;

  // kAlign
  const nsAString *alignType;
  
  // kMakeBasicBlock
  const nsAString *blockType;
  
  // kInsertElement
  const nsIDOMElement* insertElement;
};


/***************************************************************************
 * stack based helper class for StartOperation()/EndOperation() sandwich.
 * this class sets a bool letting us know to ignore any rules sniffing
 * that tries to occur reentrantly. 
 */
class nsAutoLockRulesSniffing
{
  public:
  
  nsAutoLockRulesSniffing(nsTextEditRules *rules) : mRules(rules) 
                 {if (mRules) mRules->mLockRulesSniffing = true;}
  ~nsAutoLockRulesSniffing() 
                 {if (mRules) mRules->mLockRulesSniffing = false;}
  
  protected:
  nsTextEditRules *mRules;
};



/***************************************************************************
 * stack based helper class for turning on/off the edit listener.
 */
class nsAutoLockListener
{
  public:
  
  nsAutoLockListener(bool *enabled) : mEnabled(enabled)
                 {if (mEnabled) { mOldState=*mEnabled; *mEnabled = false;}}
  ~nsAutoLockListener() 
                 {if (mEnabled) *mEnabled = mOldState;}
  
  protected:
  bool *mEnabled;
  bool mOldState;
};

#endif //nsTextEditRules_h__
