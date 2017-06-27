/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/dom/TabParent.h"

namespace mozilla {

class EditorBase;
class EventDispatchingCallback;
class IMEStateManager;

/**
 * TextComposition represents a text composition.  This class stores the
 * composition event target and its presContext.  At dispatching the event via
 * this class, the instances use the stored event target.
 */

class TextComposition final
{
  friend class IMEStateManager;

  NS_INLINE_DECL_REFCOUNTING(TextComposition)

public:
  typedef dom::TabParent TabParent;

  static bool IsHandlingSelectionEvent() { return sHandlingSelectionEvent; }

  TextComposition(nsPresContext* aPresContext,
                  nsINode* aNode,
                  TabParent* aTabParent,
                  WidgetCompositionEvent* aCompositionEvent);

  bool Destroyed() const { return !mPresContext; }
  nsPresContext* GetPresContext() const { return mPresContext; }
  nsINode* GetEventTargetNode() const { return mNode; }
  // The latest CompositionEvent.data value except compositionstart event.
  // This value is modified at dispatching compositionupdate.
  const nsString& LastData() const { return mLastData; }
  // The composition string which is already handled by the focused editor.
  // I.e., this value must be same as the composition string on the focused
  // editor.  This value is modified at a call of
  // EditorDidHandleCompositionChangeEvent().
  // Note that mString and mLastData are different between dispatcing
  // compositionupdate and compositionchange event handled by focused editor.
  const nsString& String() const { return mString; }
  // The latest clauses range of the composition string.
  // During compositionupdate event, GetRanges() returns old ranges.
  // So if getting on compositionupdate, Use GetLastRange instead of GetRange().
  TextRangeArray* GetLastRanges() const { return mLastRanges; }
  // Returns the clauses and/or caret range of the composition string.
  // This is modified at a call of EditorWillHandleCompositionChangeEvent().
  // This may return null if there is no clauses and caret.
  // XXX We should return |const TextRangeArray*| here, but it causes compile
  //     error due to inaccessible Release() method.
  TextRangeArray* GetRanges() const { return mRanges; }
  // Returns the widget which is proper to call NotifyIME().
  nsIWidget* GetWidget() const
  {
    return mPresContext ? mPresContext->GetRootWidget() : nullptr;
  }
  // Returns true if the composition is started with synthesized event which
  // came from nsDOMWindowUtils.
  bool IsSynthesizedForTests() const { return mIsSynthesizedForTests; }

  const widget::NativeIMEContext& GetNativeIMEContext() const
  {
    return mNativeContext;
  }

  /**
   * This is called when IMEStateManager stops managing the instance.
   */
  void Destroy();

  /**
   * Request to commit (or cancel) the composition to IME.  This method should
   * be called only by IMEStateManager::NotifyIME().
   */
  nsresult RequestToCommit(nsIWidget* aWidget, bool aDiscard);

  /**
   * Send a notification to IME.  It depends on the IME or platform spec what
   * will occur (or not occur).
   */
  nsresult NotifyIME(widget::IMEMessage aMessage);

  /**
   * the offset of first composition string
   */
  uint32_t NativeOffsetOfStartComposition() const
  {
    return mCompositionStartOffset;
  }

  /**
   * the offset of first selected clause or start of composition
   */
  uint32_t NativeOffsetOfTargetClause() const
  {
    return mCompositionStartOffset + mTargetClauseOffsetInComposition;
  }

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
  void StartHandlingComposition(EditorBase* aEditorBase);
  void EndHandlingComposition(EditorBase* aEditorBase);

  /**
   * OnEditorDestroyed() is called when the editor is destroyed but there is
   * active composition.
   */
  void OnEditorDestroyed();

  /**
   * CompositionChangeEventHandlingMarker class should be created at starting
   * to handle text event in focused editor.  This calls
   * EditorWillHandleCompositionChangeEvent() and
   * EditorDidHandleCompositionChangeEvent() automatically.
   */
  class MOZ_STACK_CLASS CompositionChangeEventHandlingMarker
  {
  public:
    CompositionChangeEventHandlingMarker(
      TextComposition* aComposition,
      const WidgetCompositionEvent* aCompositionChangeEvent)
      : mComposition(aComposition)
    {
      mComposition->EditorWillHandleCompositionChangeEvent(
                      aCompositionChangeEvent);
    }

    ~CompositionChangeEventHandlingMarker()
    {
      mComposition->EditorDidHandleCompositionChangeEvent();
    }

  private:
    RefPtr<TextComposition> mComposition;
    CompositionChangeEventHandlingMarker();
    CompositionChangeEventHandlingMarker(
      const CompositionChangeEventHandlingMarker& aOther);
  };

private:
  // Private destructor, to discourage deletion outside of Release():
  ~TextComposition();

  // sHandlingSelectionEvent is true while TextComposition sends a selection
  // event to ContentEventHandler.
  static bool sHandlingSelectionEvent;

  // This class holds nsPresContext weak.  This instance shouldn't block
  // destroying it.  When the presContext is being destroyed, it's notified to
  // IMEStateManager::OnDestroyPresContext(), and then, it destroy
  // this instance.
  nsPresContext* mPresContext;
  nsCOMPtr<nsINode> mNode;
  RefPtr<TabParent> mTabParent;

  // This is the clause and caret range information which is managed by
  // the focused editor.  This may be null if there is no clauses or caret.
  RefPtr<TextRangeArray> mRanges;
  // Same as mRange, but mRange will have old data during compositionupdate.
  // So this will be valied during compositionupdate.
  RefPtr<TextRangeArray> mLastRanges;

  // mNativeContext stores a opaque pointer.  This works as the "ID" for this
  // composition.  Don't access the instance, it may not be available.
  widget::NativeIMEContext mNativeContext;

  // mEditorBaseWeak is a weak reference to the focused editor handling
  // composition.
  nsWeakPtr mEditorBaseWeak;

  // mLastData stores the data attribute of the latest composition event (except
  // the compositionstart event).
  nsString mLastData;

  // mString stores the composition text which has been handled by the focused
  // editor.
  nsString mString;

  // Offset of the composition string from start of the editor
  uint32_t mCompositionStartOffset;
  // Offset of the selected clause of the composition string from
  // mCompositionStartOffset
  uint32_t mTargetClauseOffsetInComposition;

  // See the comment for IsSynthesizedForTests().
  bool mIsSynthesizedForTests;

  // See the comment for IsComposing().
  bool mIsComposing;

  // mIsEditorHandlingEvent is true while editor is modifying the composition
  // string.
  bool mIsEditorHandlingEvent;

  // mIsRequestingCommit or mIsRequestingCancel is true *only* while we're
  // requesting commit or canceling the composition.  In other words, while
  // one of these values is true, we're handling the request.
  bool mIsRequestingCommit;
  bool mIsRequestingCancel;

  // mRequestedToCommitOrCancel is true *after* we requested IME to commit or
  // cancel the composition.  In other words, we already requested of IME that
  // it commits or cancels current composition.
  // NOTE: Before this is set true, both mIsRequestingCommit and
  //       mIsRequestingCancel are set false.
  bool mRequestedToCommitOrCancel;

  // mWasNativeCompositionEndEventDiscarded is true if this composition was
  // requested commit or cancel itself but native compositionend event is
  // discarded by PresShell due to not safe to dispatch events.
  bool mWasNativeCompositionEndEventDiscarded;

  // Allow control characters appear in composition string.
  // When this is false, control characters except
  // CHARACTER TABULATION (horizontal tab) are removed from
  // both composition string and data attribute of compositionupdate
  // and compositionend events.
  bool mAllowControlCharacters;

  // mWasCompositionStringEmpty is true if the composition string was empty
  // when DispatchCompositionEvent() is called.
  bool mWasCompositionStringEmpty;

  // mHasDispatchedCompositionEvents is true if the instance has dispatched
  // one or more composition events.
  bool mHasDispatchedCompositionEvents;

  // Hide the default constructor and copy constructor.
  TextComposition()
    : mPresContext(nullptr)
    , mNativeContext(nullptr)
    , mCompositionStartOffset(0)
    , mTargetClauseOffsetInComposition(0)
    , mIsSynthesizedForTests(false)
    , mIsComposing(false)
    , mIsEditorHandlingEvent(false)
    , mIsRequestingCommit(false)
    , mIsRequestingCancel(false)
    , mRequestedToCommitOrCancel(false)
    , mWasNativeCompositionEndEventDiscarded(false)
    , mAllowControlCharacters(false)
    , mWasCompositionStringEmpty(true)
    , mHasDispatchedCompositionEvents(false)
  {}
  TextComposition(const TextComposition& aOther);

  /**
   * GetEditorBase() returns EditorBase pointer of mEditorBaseWeak.
   */
  already_AddRefed<EditorBase> GetEditorBase() const;

  /**
   * HasEditor() returns true if mEditorBaseWeak holds EditorBase instance
   * which is alive.  Otherwise, false.
   */
  bool HasEditor() const;

  /**
   * EditorWillHandleCompositionChangeEvent() must be called before the focused
   * editor handles the compositionchange event.
   */
  void EditorWillHandleCompositionChangeEvent(
         const WidgetCompositionEvent* aCompositionChangeEvent);

  /**
   * EditorDidHandleCompositionChangeEvent() must be called after the focused
   * editor handles a compositionchange event.
   */
  void EditorDidHandleCompositionChangeEvent();

  /**
   * IsValidStateForComposition() returns true if it's safe to dispatch an event
   * to the DOM tree.  Otherwise, false.
   * WARNING: This doesn't check script blocker state.  It should be checked
   *          before dispatching the first event.
   */
  bool IsValidStateForComposition(nsIWidget* aWidget) const;

  /**
   * DispatchCompositionEvent() dispatches the aCompositionEvent to the mContent
   * synchronously. The caller must ensure that it's safe to dispatch the event.
   */
  void DispatchCompositionEvent(WidgetCompositionEvent* aCompositionEvent,
                                nsEventStatus* aStatus,
                                EventDispatchingCallback* aCallBack,
                                bool aIsSynthesized);

  /**
   * Simply calling EventDispatcher::Dispatch() with plugin event.
   * If dispatching event has no orginal clone, aOriginalEvent can be null.
   */
  void DispatchEvent(WidgetCompositionEvent* aDispatchEvent,
                     nsEventStatus* aStatus,
                     EventDispatchingCallback* aCallback,
                     const WidgetCompositionEvent *aOriginalEvent = nullptr);

  /**
   * HandleSelectionEvent() sends the selection event to ContentEventHandler
   * or dispatches it to the focused child process.
   */
  void HandleSelectionEvent(WidgetSelectionEvent* aSelectionEvent)
  {
    HandleSelectionEvent(mPresContext, mTabParent, aSelectionEvent);
  }
  static void HandleSelectionEvent(nsPresContext* aPresContext,
                                   TabParent* aTabParent,
                                   WidgetSelectionEvent* aSelectionEvent);

  /**
   * MaybeDispatchCompositionUpdate() may dispatch a compositionupdate event
   * if aCompositionEvent changes composition string.
   * @return Returns false if dispatching the compositionupdate event caused
   *         destroying this composition.
   */
  bool MaybeDispatchCompositionUpdate(
         const WidgetCompositionEvent* aCompositionEvent);

  /**
   * CloneAndDispatchAs() dispatches a composition event which is
   * duplicateed from aCompositionEvent and set the aMessage.
   *
   * @return Returns BaseEventFlags which is the result of dispatched event.
   */
  BaseEventFlags CloneAndDispatchAs(
                   const WidgetCompositionEvent* aCompositionEvent,
                   EventMessage aMessage,
                   nsEventStatus* aStatus = nullptr,
                   EventDispatchingCallback* aCallBack = nullptr);

  /**
   * If IME has already dispatched compositionend event but it was discarded
   * by PresShell due to not safe to dispatch, this returns true.
   */
  bool WasNativeCompositionEndEventDiscarded() const
  {
    return mWasNativeCompositionEndEventDiscarded;
  }

  /**
   * OnCompositionEventDiscarded() is called when PresShell discards
   * compositionupdate, compositionend or compositionchange event due to not
   * safe to dispatch event.
   */
  void OnCompositionEventDiscarded(WidgetCompositionEvent* aCompositionEvent);

  /**
   * OnCompositionEventDispatched() is called after a composition event is
   * dispatched.
   */
  void OnCompositionEventDispatched(
         const WidgetCompositionEvent* aDispatchEvent);

  /**
   * MaybeNotifyIMEOfCompositionEventHandled() notifies IME of composition
   * event handled.  This should be called after dispatching a composition
   * event which came from widget.
   */
  void MaybeNotifyIMEOfCompositionEventHandled(
         const WidgetCompositionEvent* aCompositionEvent);

  /**
   * GetSelectionStartOffset() returns normal selection start offset in the
   * editor which has this composition.
   * If it failed or lost focus, this would return 0.
   */
  uint32_t GetSelectionStartOffset();

  /**
   * OnStartOffsetUpdatedInChild() is called when composition start offset
   * is updated in the child process.  I.e., this is called and never called
   * if the composition is in this process.
   * @param aStartOffset        New composition start offset with native
   *                            linebreaks.
   */
  void OnStartOffsetUpdatedInChild(uint32_t aStartOffset);

  /**
   * CompositionEventDispatcher dispatches the specified composition (or text)
   * event.
   */
  class CompositionEventDispatcher : public Runnable
  {
  public:
    CompositionEventDispatcher(TextComposition* aTextComposition,
                               nsINode* aEventTarget,
                               EventMessage aEventMessage,
                               const nsAString& aData,
                               bool aIsSynthesizedEvent = false);
    NS_IMETHOD Run() override;

  private:
    RefPtr<TextComposition> mTextComposition;
    nsCOMPtr<nsINode> mEventTarget;
    nsString mData;
    EventMessage mEventMessage;
    bool mIsSynthesizedEvent;

    CompositionEventDispatcher()
      : Runnable("TextComposition::CompositionEventDispatcher")
      , mIsSynthesizedEvent(false){};
  };

  /**
   * DispatchCompositionEventRunnable() dispatches a composition event to the
   * content.  Be aware, if you use this method, nsPresShellEventCB isn't used.
   * That means that nsIFrame::HandleEvent() is never called.
   * WARNING: The instance which is managed by IMEStateManager may be
   *          destroyed by this method call.
   *
   * @param aEventMessage       Must be one of composition events.
   * @param aData               Used for mData value.
   * @param aIsSynthesizingCommit   true if this is called for synthesizing
   *                                commit or cancel composition.  Otherwise,
   *                                false.
   */
  void DispatchCompositionEventRunnable(EventMessage aEventMessage,
                                        const nsAString& aData,
                                        bool aIsSynthesizingCommit = false);
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

class TextCompositionArray final :
  public AutoTArray<RefPtr<TextComposition>, 2>
{
public:
  // Looking for per native IME context.
  index_type IndexOf(const widget::NativeIMEContext& aNativeIMEContext);
  index_type IndexOf(nsIWidget* aWidget);

  TextComposition* GetCompositionFor(nsIWidget* aWidget);
  TextComposition* GetCompositionFor(
                     const WidgetCompositionEvent* aCompositionEvent);

  // Looking for per nsPresContext
  index_type IndexOf(nsPresContext* aPresContext);
  index_type IndexOf(nsPresContext* aPresContext, nsINode* aNode);

  TextComposition* GetCompositionFor(nsPresContext* aPresContext);
  TextComposition* GetCompositionFor(nsPresContext* aPresContext,
                                     nsINode* aNode);
  TextComposition* GetCompositionInContent(nsPresContext* aPresContext,
                                           nsIContent* aContent);
};

} // namespace mozilla

#endif // #ifndef mozilla_TextComposition_h
