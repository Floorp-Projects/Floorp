/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface, please see
 * http://www.whatwg.org/specs/web-apps/current-work/#messageevent
 */

[Constructor(DOMString type, optional MessageEventInit eventInitDict),
 Exposed=(Window,Worker,System)]
interface MessageEvent : Event {
  /**
   * Custom data associated with this event.
   */
  [GetterThrows]
  readonly attribute any data;

  /**
   * The origin of the site from which this event originated, which is the
   * scheme, ":", and if the URI has a host, "//" followed by the
   * host, and if the port is not the default for the given scheme,
   * ":" followed by that port.  This value does not have a trailing slash.
   */
  readonly attribute DOMString origin;

  /**
   * The last event ID string of the event source, for server-sent DOM events; this
   * value is the empty string for cross-origin messaging.
   */
  readonly attribute DOMString lastEventId;

  /**
   * The window or port which originated this event.
   */
  readonly attribute (WindowProxy or MessagePort)? source;

  /**
   * Initializes this event with the given data, in a manner analogous to
   * the similarly-named method on the nsIDOMEvent interface, also setting the
   * data, origin, source, and lastEventId attributes of this appropriately.
   */
  readonly attribute MessagePortList? ports;

  [Throws]
  void initMessageEvent(DOMString type, boolean bubbles, boolean cancelable,
                        any data, DOMString origin, DOMString lastEventId,
                        (WindowProxy or MessagePort)? source,
                        sequence<MessagePort>? ports);
};

dictionary MessageEventInit : EventInit {
  any data;
  DOMString origin;
  DOMString lastEventId;
  (Window or MessagePort)? source = null;
  sequence<MessagePort>? ports;
};
