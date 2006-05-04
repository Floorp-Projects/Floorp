/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// This file implements an event registrar, an object with which you
// can register handlers for arbitrary programmer-defined
// events. Events are arbitrary strings and listeners are functions
// taking an object (stuffed with arguments) as a parameter. When you
// fire an event through the registrar, all listeners are invoked in
// an unspecified order. The firing function takes an object to be
// passed into each handler (it is _not_ copied, so be careful). We
// chose this calling convention so we don't have to change handler
// signatures when adding new information.
//
// Why not just use notifier/observers? Because passing data around
// with them requires either serialization or a new xpcom interface,
// both of which are a pain in the ass.
//
// Example:
//
// // Set up a listener
// this.handleTabload = function(e) {
//   foo(e.url);
//   bar(e.browser);
// };
//
// // Set up the registrar
// var eventTypes = ["tabload", "tabunload", "tabswitch"];
// var registrar = new EventRegistrar(eventTypes);
// var handler = BindToObject(this.handleTabload, this);
//
// // Register a listener
// registrar.registerListener("tabload", handler);
//
// // Fire an event and remove the listener
// var event = { "url": "http://www", "browser": browser };
// registrar.fire("tabload", event);
// registrar.removeListener("tabload", handler);
//
// TODO: could add ability to cancel further handlers by having listeners
//       return a boolean

/**
 * EventRegistrars are used to manage user-defined events. 
 *
 * @constructor
 * @param eventTypes {Array or Object} Array holding names of events or
 *                   Object holding properties the values of which are 
 *                   names (strings) for which listeners can register
 */
function EventRegistrar(eventTypes) {
  this.eventTypes = [];
  this.listeners_ = {};          // Listener sets, index by event type

  if (eventTypes instanceof Array) {
    var events = eventTypes;
  } else if (typeof eventTypes == "object") {
    var events = [];
    for (var e in eventTypes)
      events.push(eventTypes[e]);
  } else {
    throw new Error("Unrecognized init parameter to EventRegistrar");
  }

  for (var i = 0; i < events.length; i++) {
    this.eventTypes.push(events[i]);          // Copy in case caller mutates
    this.listeners_[events[i]] = 
      new ListDictionary(events[i] + "Listeners");
  }
}

/**
 * Indicates whether the given event is one the registrar can handle.
 * 
 * @param eventType {String} The name of the event to look up
 * @returns {Boolean} false if the event type is not known or the 
 *          event type string itself if it is
 */
EventRegistrar.prototype.isKnownEventType = function(eventType) {
  for (var i=0; i < this.eventTypes.length; i++)
    if (eventType == this.eventTypes[i])
      return eventType;
  return false;
}

/**
 * Add an event type to listen for.
 * @param eventType {String} The name of the event to add
 */
EventRegistrar.prototype.addEventType = function(eventType) {
  if (this.isKnownEventType(eventType))
    throw new Error("Event type already known: " + eventType);

  this.eventTypes.push(eventType);
  this.listeners_[eventType] = new ListDictionary(eventType + "Listeners");
}

/**
 * Register to receive events of the type passed in. 
 * 
 * @param eventType {String} indicating the event type (one of this.eventTypes)
 * @param listener {Function} to invoke when the event occurs.
 */
EventRegistrar.prototype.registerListener = function(eventType, listener) {
  if (this.isKnownEventType(eventType) === false)
    throw new Error("Unknown event type: " + eventType);

  this.listeners_[eventType].addMember(listener);
}

/**
 * Unregister a listener.
 * 
 * @param eventType {String} One of EventRegistrar.eventTypes' members
 * @param listener {Function} Function to remove as listener
 */
EventRegistrar.prototype.removeListener = function(eventType, listener) {
  if (this.isKnownEventType(eventType) === false)
    throw new Error("Unknown event type: " + eventType);

  this.listeners_[eventType].removeMember(listener);
}

/**
 * Invoke the handlers for the given eventType. 
 *
 * @param eventType {String} The event to fire
 * @param e {Object} Object containing the parameters of the event
 */
EventRegistrar.prototype.fire = function(eventType, e) {
  if (this.isKnownEventType(eventType) === false)
    throw new Error("Unknown event type: " + eventType);

  var invoke = function(listener) {
    listener(e);
  };

  this.listeners_[eventType].forEach(invoke);
}
