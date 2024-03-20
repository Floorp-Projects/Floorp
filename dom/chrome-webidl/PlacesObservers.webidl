/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback PlacesEventCallback = undefined (sequence<PlacesEvent> events);

[ChromeOnly, Exposed=Window]
interface PlacesWeakCallbackWrapper {
  constructor(PlacesEventCallback callback);
};

// Counters for number of events sent in the current session.
[ChromeOnly, Exposed=Window]
interface PlacesEventCounts {
  readonly maplike<DOMString, unsigned long long>;
};

// Global singleton which should handle all events for places.
[ChromeOnly, Exposed=Window]
namespace PlacesObservers {
  [Throws]
  undefined addListener(sequence<PlacesEventType> eventTypes,
                        PlacesEventCallback listener);
  [Throws]
  undefined addListener(sequence<PlacesEventType> eventTypes,
                        PlacesWeakCallbackWrapper listener);
  [Throws]
  undefined removeListener(sequence<PlacesEventType> eventTypes,
                           PlacesEventCallback listener);
  [Throws]
  undefined removeListener(sequence<PlacesEventType> eventTypes,
                           PlacesWeakCallbackWrapper listener);
  [Throws]
  undefined notifyListeners(sequence<PlacesEvent> events);

  readonly attribute PlacesEventCounts counts;
};
