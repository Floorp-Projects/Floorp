/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsEventShell_H_
#define _nsEventShell_H_

#include "AccEvent.h"

namespace mozilla {
template<typename T> class StaticRefPtr;
}
class nsIPersistentProperties;

/**
 * Used for everything about events.
 */
class nsEventShell
{
public:

  /**
   * Fire the accessible event.
   */
  static void FireEvent(mozilla::a11y::AccEvent* aEvent);

  /**
   * Fire accessible event of the given type for the given accessible.
   *
   * @param  aEventType   [in] the event type
   * @param  aAccessible  [in] the event target
   */
  static void FireEvent(uint32_t aEventType,
                        mozilla::a11y::Accessible* aAccessible,
                        mozilla::a11y::EIsFromUserInput aIsFromUserInput = mozilla::a11y::eAutoDetect);

  /**
   * Fire state change event.
   */
  static void FireEvent(mozilla::a11y::Accessible* aTarget, uint64_t aState,
                        bool aIsEnabled, bool aIsFromUserInput)
  {
    RefPtr<mozilla::a11y::AccStateChangeEvent> stateChangeEvent =
      new mozilla::a11y::AccStateChangeEvent(aTarget, aState, aIsEnabled,
                                             (aIsFromUserInput ?
                                               mozilla::a11y::eFromUserInput :
                                               mozilla::a11y::eNoUserInput));
    FireEvent(stateChangeEvent);
  }

  /**
   * Append 'event-from-input' object attribute if the accessible event has
   * been fired just now for the given node.
   *
   * @param  aNode        [in] the DOM node
   * @param  aAttributes  [in, out] the attributes
   */
  static void GetEventAttributes(nsINode *aNode,
                                 nsIPersistentProperties *aAttributes);

private:
  static mozilla::StaticRefPtr<nsINode> sEventTargetNode;
  static bool sEventFromUserInput;
};

#endif
