/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 *
 */

[Pref="dom.serviceWorkers.enabled",
 Constructor(DOMString type, optional ServiceWorkerMessageEventInit eventInitDict),
 Exposed=Window]
interface ServiceWorkerMessageEvent : Event {
  /**
   * Custom data associated with this event.
   */
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
   * The service worker or port which originated this event.
   * FIXME: Use SameOject after IDL spec is updated
   * https://bugzilla.mozilla.org/show_bug.cgi?id=1196097
   */
  [Constant] readonly attribute (ServiceWorker or MessagePort)? source;

  [Constant, Cached, Frozen]
  readonly attribute sequence<MessagePort> ports;
};

dictionary ServiceWorkerMessageEventInit : EventInit
{
  any data = null;
  DOMString origin = "";
  DOMString lastEventId = "";
  (ServiceWorker or MessagePort)? source = null;
  sequence<MessagePort> ports = [];
};
