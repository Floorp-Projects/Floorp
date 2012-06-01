/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsEventShell_H_
#define _nsEventShell_H_

#include "AccEvent.h"

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
  static void FireEvent(AccEvent* aEvent);

  /**
   * Fire accessible event of the given type for the given accessible.
   *
   * @param  aEventType   [in] the event type
   * @param  aAccessible  [in] the event target
   */
  static void FireEvent(PRUint32 aEventType, Accessible* aAccessible,
                        EIsFromUserInput aIsFromUserInput = eAutoDetect);

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
  static nsCOMPtr<nsINode> sEventTargetNode;
  static bool sEventFromUserInput;
};

#endif
