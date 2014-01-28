/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextComposition_h
#define mozilla_TextComposition_h

#include "nsCOMPtr.h"
#include "nsINode.h"
#include "nsIWidget.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsPresContext.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"

class nsDispatchingCallback;
class nsIMEStateManager;
class nsIWidget;

namespace mozilla {

/**
 * TextComposition represents a text composition.  This class stores the
 * composition event target and its presContext.  At dispatching the event via
 * this class, the instances use the stored event target.
 */

class TextComposition MOZ_FINAL
{
  friend class ::nsIMEStateManager;

  NS_INLINE_DECL_REFCOUNTING(TextComposition)

public:
  TextComposition(nsPresContext* aPresContext,
                  nsINode* aNode,
                  WidgetGUIEvent* aEvent);

  ~TextComposition()
  {
    // WARNING: mPresContext may be destroying, so, be careful if you touch it.
  }

  nsPresContext* GetPresContext() const { return mPresContext; }
  nsINode* GetEventTargetNode() const { return mNode; }
  // The latest CompositionEvent.data value except compositionstart event.
  const nsString& GetLastData() const { return mLastData; }
  // Returns true if the composition is started with synthesized event which
  // came from nsDOMWindowUtils.
  bool IsSynthesizedForTests() const { return mIsSynthesizedForTests; }

  bool MatchesNativeContext(nsIWidget* aWidget) const;

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
  nsresult NotifyIME(widget::NotificationToIME aNotification);

  /**
   * the offset of first selected clause or start of of compositon
   */
  uint32_t OffsetOfTargetClause() const { return mCompositionTargetOffset; }

private:
  // This class holds nsPresContext weak.  This instance shouldn't block
  // destroying it.  When the presContext is being destroyed, it's notified to
  // nsIMEStateManager::OnDestroyPresContext(), and then, it destroy
  // this instance.
  nsPresContext* mPresContext;
  nsCOMPtr<nsINode> mNode;

  // mNativeContext stores a opaque pointer.  This works as the "ID" for this
  // composition.  Don't access the instance, it may not be available.
  void* mNativeContext;

  // mLastData stores the data attribute of the latest composition event (except
  // the compositionstart event).
  nsString mLastData;

  // Offset of the composition string from start of the editor
  uint32_t mCompositionStartOffset;
  // Offset of the selected clause of the composition string from start of the
  // editor
  uint32_t mCompositionTargetOffset;

  // See the comment for IsSynthesizedForTests().
  bool mIsSynthesizedForTests;

  // Hide the default constructor and copy constructor.
  TextComposition() {}
  TextComposition(const TextComposition& aOther);


  /**
   * DispatchEvent() dispatches the aEvent to the mContent synchronously.
   * The caller must ensure that it's safe to dispatch the event.
   */
  void DispatchEvent(WidgetGUIEvent* aEvent,
                     nsEventStatus* aStatus,
                     nsDispatchingCallback* aCallBack);

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
   * DispatchCompsotionEventRunnable() dispatches a composition or text event
   * to the content.  Be aware, if you use this method, nsPresShellEventCB
   * isn't used.  That means that nsIFrame::HandleEvent() is never called.
   * WARNING: The instance which is managed by nsIMEStateManager may be
   *          destroyed by this method call.
   *
   * @param aEventMessage       Must be one of composition event or text event.
   * @param aData               Used for data value if aEventMessage is
   *                            NS_COMPOSITION_UPDATE or NS_COMPOSITION_END.
   *                            Used for theText value if aEventMessage is
   *                            NS_TEXT_TEXT.
   */
  void DispatchCompsotionEventRunnable(uint32_t aEventMessage,
                                       const nsAString& aData);
};

/**
 * TextCompositionArray manages the instances of TextComposition class.
 * Managing with array is enough because only one composition is typically
 * there.  Even if user switches native IME context, it's very rare that
 * second or more composition is started.
 * It's assumed that this is used by nsIMEStateManager for storing all active
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
