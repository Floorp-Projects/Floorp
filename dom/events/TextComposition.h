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

class nsIEditor;

namespace mozilla {

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

  bool MatchesNativeContext(nsIWidget* aWidget) const;

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
    nsRefPtr<TextComposition> mComposition;
    CompositionChangeEventHandlingMarker();
    CompositionChangeEventHandlingMarker(
      const CompositionChangeEventHandlingMarker& aOther);
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
  nsRefPtr<TabParent> mTabParent;

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
                   uint32_t aMessage,
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
   * Calculate composition offset then notify composition update to widget
   */
  void NotityUpdateComposition(const WidgetCompositionEvent* aCompositionEvent);

  /**
   * CompositionEventDispatcher dispatches the specified composition (or text)
   * event.
   */
  class CompositionEventDispatcher : public nsRunnable
  {
  public:
    CompositionEventDispatcher(TextComposition* aTextComposition,
                               nsINode* aEventTarget,
                               uint32_t aEventMessage,
                               const nsAString& aData,
                               bool aIsSynthesizedEvent = false);
    NS_IMETHOD Run() override;

  private:
    nsRefPtr<TextComposition> mTextComposition;
    nsCOMPtr<nsINode> mEventTarget;
    uint32_t mEventMessage;
    nsString mData;
    bool mIsSynthesizedEvent;

    CompositionEventDispatcher() {};
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
  void DispatchCompositionEventRunnable(uint32_t aEventMessage,
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
