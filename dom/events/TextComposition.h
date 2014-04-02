/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextComposition_h
#define mozilla_TextComposition_h

#include "nsCOMPtr.h"
#include "nsINode.h"
#include "nsIWeakReference.h"
#include "nsIWidget.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsPresContext.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TextRange.h"

class nsIEditor;

namespace mozilla {

class EventDispatchingCallback;
class IMEStateManager;

/**
 * TextComposition represents a text composition.  This class stores the
 * composition event target and its presContext.  At dispatching the event via
 * this class, the instances use the stored event target.
 */

class TextComposition MOZ_FINAL
{
  friend class IMEStateManager;

  NS_INLINE_DECL_REFCOUNTING(TextComposition)

public:
  TextComposition(nsPresContext* aPresContext,
                  nsINode* aNode,
                  WidgetGUIEvent* aEvent);

  bool Destroyed() const { return !mPresContext; }
  nsPresContext* GetPresContext() const { return mPresContext; }
  nsINode* GetEventTargetNode() const { return mNode; }
  // The latest CompositionEvent.data value except compositionstart event.
  // This value is modified at dispatching compositionupdate.
  const nsString& LastData() const { return mLastData; }
  // The composition string which is already handled by the focused editor.
  // I.e., this value must be same as the composition string on the focused
  // editor.  This value is modified at a call of EditorDidHandleTextEvent().
  // Note that mString and mLastData are different between dispatcing
  // compositionupdate and text event handled by focused editor.
  const nsString& String() const { return mString; }
  // Returns the clauses and/or caret range of the composition string.
  // This is modified at a call of EditorWillHandleTextEvent().
  // This may return null if there is no clauses and caret.
  // XXX We should return |const TextRangeArray*| here, but it causes compile
  //     error due to inaccessible Release() method.
  TextRangeArray* GetRanges() const { return mRanges; }
  // Returns true if the composition is started with synthesized event which
  // came from nsDOMWindowUtils.
  bool IsSynthesizedForTests() const { return mIsSynthesizedForTests; }

  bool MatchesNativeContext(nsIWidget* aWidget) const;

  /**
   * This is called when IMEStateManager stops managing the instance.
   */
  void Destroy();

  /**
   * SynthesizeCommit() dispatches compositionupdate, text and compositionend
   * events for emulating commit on the content.
   *
   * @param aDiscard true when committing with empty string.  Otherwise, false.
   */
  void SynthesizeCommit(bool aDiscard);

  /**
   * Send a notification to IME.  It depends on the IME or platform spec what
   * will occur (or not occur).
   */
  nsresult NotifyIME(widget::IMEMessage aMessage);

  /**
   * the offset of first selected clause or start of of compositon
   */
  uint32_t OffsetOfTargetClause() const { return mCompositionTargetOffset; }

  /**
   * Returns true if there is non-empty composition string and it's not fixed.
   * Otherwise, false.
   */
  bool IsComposing() const { return mIsComposing; }

  /**
   * Returns true while editor is handling an event which is modifying the
   * composition string.
   */
  bool IsEditorHandlingEvent() const
  {
    return mIsEditorHandlingEvent;
  }

  /**
   * StartHandlingComposition() and EndHandlingComposition() are called by
   * editor when it holds a TextComposition instance and release it.
   */
  void StartHandlingComposition(nsIEditor* aEditor);
  void EndHandlingComposition(nsIEditor* aEditor);

  /**
   * TextEventHandlingMarker class should be created at starting to handle text
   * event in focused editor.  This calls EditorWillHandleTextEvent() and
   * EditorDidHandleTextEvent() automatically.
   */
  class MOZ_STACK_CLASS TextEventHandlingMarker
  {
  public:
    TextEventHandlingMarker(TextComposition* aComposition,
                            const WidgetTextEvent* aTextEvent)
      : mComposition(aComposition)
    {
      mComposition->EditorWillHandleTextEvent(aTextEvent);
    }

    ~TextEventHandlingMarker()
    {
      mComposition->EditorDidHandleTextEvent();
    }

  private:
    nsRefPtr<TextComposition> mComposition;
    TextEventHandlingMarker();
    TextEventHandlingMarker(const TextEventHandlingMarker& aOther);
  };

private:
  // Private destructor, to discourage deletion outside of Release():
  ~TextComposition()
  {
    // WARNING: mPresContext may be destroying, so, be careful if you touch it.
  }

  // This class holds nsPresContext weak.  This instance shouldn't block
  // destroying it.  When the presContext is being destroyed, it's notified to
  // IMEStateManager::OnDestroyPresContext(), and then, it destroy
  // this instance.
  nsPresContext* mPresContext;
  nsCOMPtr<nsINode> mNode;

  // This is the clause and caret range information which is managed by
  // the focused editor.  This may be null if there is no clauses or caret.
  nsRefPtr<TextRangeArray> mRanges;

  // mNativeContext stores a opaque pointer.  This works as the "ID" for this
  // composition.  Don't access the instance, it may not be available.
  void* mNativeContext;

  // mEditorWeak is a weak reference to the focused editor handling composition.
  nsWeakPtr mEditorWeak;

  // mLastData stores the data attribute of the latest composition event (except
  // the compositionstart event).
  nsString mLastData;

  // mString stores the composition text which has been handled by the focused
  // editor.
  nsString mString;

  // Offset of the composition string from start of the editor
  uint32_t mCompositionStartOffset;
  // Offset of the selected clause of the composition string from start of the
  // editor
  uint32_t mCompositionTargetOffset;

  // See the comment for IsSynthesizedForTests().
  bool mIsSynthesizedForTests;

  // See the comment for IsComposing().
  bool mIsComposing;

  // mIsEditorHandlingEvent is true while editor is modifying the composition
  // string.
  bool mIsEditorHandlingEvent;

  // Hide the default constructor and copy constructor.
  TextComposition() {}
  TextComposition(const TextComposition& aOther);

  /**
   * GetEditor() returns nsIEditor pointer of mEditorWeak.
   */
  already_AddRefed<nsIEditor> GetEditor() const;

  /**
   * HasEditor() returns true if mEditorWeak holds nsIEditor instance which is
   * alive.  Otherwise, false.
   */
  bool HasEditor() const;

  /**
   * EditorWillHandleTextEvent() must be called before the focused editor
   * handles the text event.
   */
  void EditorWillHandleTextEvent(const WidgetTextEvent* aTextEvent);

  /**
   * EditorDidHandleTextEvent() must be called after the focused editor handles
   * a text event.
   */
  void EditorDidHandleTextEvent();

  /**
   * DispatchEvent() dispatches the aEvent to the mContent synchronously.
   * The caller must ensure that it's safe to dispatch the event.
   */
  void DispatchEvent(WidgetGUIEvent* aEvent,
                     nsEventStatus* aStatus,
                     EventDispatchingCallback* aCallBack);

  /**
   * Calculate composition offset then notify composition update to widget
   */
  void NotityUpdateComposition(WidgetGUIEvent* aEvent);

  /**
   * CompositionEventDispatcher dispatches the specified composition (or text)
   * event.
   */
  class CompositionEventDispatcher : public nsRunnable
  {
  public:
    CompositionEventDispatcher(nsPresContext* aPresContext,
                               nsINode* aEventTarget,
                               uint32_t aEventMessage,
                               const nsAString& aData);
    NS_IMETHOD Run() MOZ_OVERRIDE;

  private:
    nsRefPtr<nsPresContext> mPresContext;
    nsCOMPtr<nsINode> mEventTarget;
    nsCOMPtr<nsIWidget> mWidget;
    uint32_t mEventMessage;
    nsString mData;

    CompositionEventDispatcher() {};
  };

  /**
   * DispatchCompositionEventRunnable() dispatches a composition or text event
   * to the content.  Be aware, if you use this method, nsPresShellEventCB
   * isn't used.  That means that nsIFrame::HandleEvent() is never called.
   * WARNING: The instance which is managed by IMEStateManager may be
   *          destroyed by this method call.
   *
   * @param aEventMessage       Must be one of composition event or text event.
   * @param aData               Used for data value if aEventMessage is
   *                            NS_COMPOSITION_UPDATE or NS_COMPOSITION_END.
   *                            Used for theText value if aEventMessage is
   *                            NS_TEXT_TEXT.
   */
  void DispatchCompositionEventRunnable(uint32_t aEventMessage,
                                        const nsAString& aData);
};

/**
 * TextCompositionArray manages the instances of TextComposition class.
 * Managing with array is enough because only one composition is typically
 * there.  Even if user switches native IME context, it's very rare that
 * second or more composition is started.
 * It's assumed that this is used by IMEStateManager for storing all active
 * compositions in the process.  If the instance is it, each TextComposition
 * in the array can be destroyed by calling some methods of itself.
 */

class TextCompositionArray MOZ_FINAL :
  public nsAutoTArray<nsRefPtr<TextComposition>, 2>
{
public:
  index_type IndexOf(nsIWidget* aWidget);
  index_type IndexOf(nsPresContext* aPresContext);
  index_type IndexOf(nsPresContext* aPresContext, nsINode* aNode);

  TextComposition* GetCompositionFor(nsIWidget* aWidget);
  TextComposition* GetCompositionFor(nsPresContext* aPresContext,
                                     nsINode* aNode);
  TextComposition* GetCompositionInContent(nsPresContext* aPresContext,
                                           nsIContent* aContent);
};

} // namespace mozilla

#endif // #ifndef mozilla_TextComposition_h
