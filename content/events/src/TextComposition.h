/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextComposition_h
#define mozilla_TextComposition_h

#include "nsCOMPtr.h"
#include "nsEvent.h"
#include "nsINode.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

class nsCompositionEvent;
class nsDispatchingCallback;
class nsIMEStateManager;
class nsIWidget;
class nsPresContext;

namespace mozilla {

/**
 * TextComposition represents a text composition.  This class stores the
 * composition event target and its presContext.  At dispatching the event via
 * this class, the instances use the stored event target.
 */

class TextComposition MOZ_FINAL
{
  friend class ::nsIMEStateManager;
public:
  TextComposition(nsPresContext* aPresContext,
                  nsINode* aNode,
                  nsGUIEvent* aEvent);

  TextComposition(const TextComposition& aOther);

  ~TextComposition()
  {
    // WARNING: mPresContext may be destroying, so, be careful if you touch it.
  }

  nsPresContext* GetPresContext() const { return mPresContext; }
  nsINode* GetEventTargetNode() const { return mNode; }

  bool MatchesNativeContext(nsIWidget* aWidget) const;
  bool MatchesEventTarget(nsPresContext* aPresContext,
                          nsINode* aNode) const;

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

  // Hide the default constructor
  TextComposition() {}

  /**
   * DispatchEvent() dispatches the aEvent to the mContent synchronously.
   * The caller must ensure that it's safe to dispatch the event.
   */
  void DispatchEvent(nsGUIEvent* aEvent,
                     nsEventStatus* aStatus,
                     nsDispatchingCallback* aCallBack);
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

class TextCompositionArray MOZ_FINAL : public nsAutoTArray<TextComposition, 2>
{
public:
  index_type IndexOf(nsIWidget* aWidget);
  index_type IndexOf(nsPresContext* aPresContext);
  index_type IndexOf(nsPresContext* aPresContext, nsINode* aNode);

  TextComposition* GetCompositionFor(nsIWidget* aWidget);
  TextComposition* GetCompositionFor(nsPresContext* aPresContext);
  TextComposition* GetCompositionFor(nsPresContext* aPresContext,
                                     nsINode* aNode);
  TextComposition* GetCompositionInContent(nsPresContext* aPresContext,
                                           nsIContent* aContent);
};

} // namespace mozilla

#endif // #ifndef mozilla_TextComposition_h
