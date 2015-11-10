/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * http://www.whatwg.org/specs/web-apps/current-work/#messageevent
 */

[Constructor(DOMString type, optional ExtendableMessageEventInit eventInitDict),
 Exposed=(ServiceWorker)]
interface ExtendableMessageEvent : ExtendableEvent {
  /**
   * Custom data associated with this event.
   */
  [GetterThrows]
  readonly attribute any data;

  /**
   * The origin of the site from which this event originated.
   */
  readonly attribute DOMString origin;

  /**
   * The last event ID string of the event source.
   */
  readonly attribute DOMString lastEventId;

  /**
   * The client, service worker or port which originated this event.
   */
  readonly attribute (Client or ServiceWorker or MessagePort)? source;

  readonly attribute MessagePortList? ports;
};

dictionary ExtendableMessageEventInit : ExtendableEventInit {
  any data;
  DOMString origin = "";
  DOMString lastEventId = "";
  (Client or ServiceWorker or MessagePort)? source;
  sequence<MessagePort>? ports;
};
