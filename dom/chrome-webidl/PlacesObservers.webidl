/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback PlacesEventCallback = void (sequence<PlacesEvent> events);

[ChromeOnly, Exposed=Window]
interface PlacesWeakCallbackWrapper {
  constructor(PlacesEventCallback callback);
};

// Global singleton which should handle all events for places.
[ChromeOnly, Exposed=Window]
namespace PlacesObservers {
  [Throws]
  void addListener(sequence<PlacesEventType> eventTypes,
                   PlacesEventCallback listener);
  [Throws]
  void addListener(sequence<PlacesEventType> eventTypes,
                   PlacesWeakCallbackWrapper listener);
  [Throws]
  void removeListener(sequence<PlacesEventType> eventTypes,
                      PlacesEventCallback listener);
  [Throws]
  void removeListener(sequence<PlacesEventType> eventTypes,
                      PlacesWeakCallbackWrapper listener);
  [Throws]
  void notifyListeners(sequence<PlacesEvent> events);
};

