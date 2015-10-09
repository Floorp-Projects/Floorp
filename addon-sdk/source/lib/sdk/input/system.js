/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cr, Cu } = require("chrome");
const { Input, start, stop, end, receive, outputs } = require("../event/utils");
const { once, off } = require("../event/core");
const { id: addonID } = require("../self");

const unloadMessage = require("@loader/unload");
const observerService = Cc['@mozilla.org/observer-service;1'].
                          getService(Ci.nsIObserverService);
const { ShimWaiver } = Cu.import("resource://gre/modules/ShimWaiver.jsm");
const addObserver = ShimWaiver.getProperty(observerService, "addObserver");
const removeObserver = ShimWaiver.getProperty(observerService, "removeObserver");


const addonUnloadTopic = "sdk:loader:destroy";

const isXrayWrapper = Cu.isXrayWrapper;
// In the past SDK used to double-wrap notifications dispatched, which
// made them awkward to use outside of SDK. At present they no longer
// do that, although we still supported for legacy reasons.
const isLegacyWrapper = x =>
    x && x.wrappedJSObject &&
    "observersModuleSubjectWrapper" in x.wrappedJSObject;

const unwrapLegacy = x => x.wrappedJSObject.object;

// `InputPort` provides a way to create a signal out of the observer
// notification subject's for the given `topic`. If `options.initial`
// is provided it is used as initial value otherwise `null` is used.
// Constructor can be given `options.id` that will be used to create
// a `topic` which is namespaced to an add-on (this avoids conflicts
// when multiple add-on are used, although in a future host probably
// should just be shared across add-ons). It is also possible to
// specify a specific `topic` via `options.topic` which is used as
// without namespacing. Created signal ends whenever add-on is
// unloaded.
const InputPort = function InputPort({id, topic, initial}) {
  this.id = id || topic;
  this.topic = topic || "sdk:" + addonID + ":" + id;
  this.value = initial === void(0) ? null : initial;
  this.observing = false;
  this[outputs] = [];
};

// InputPort type implements `Input` signal interface.
InputPort.prototype = new Input();
InputPort.prototype.constructor = InputPort;

// When port is started (which is when it's subgraph get's
// first subscriber) actual observer is registered.
InputPort.start = input => {
  input.addListener(input);
  // Also register add-on unload observer to end this signal
  // when that happens.
  addObserver(input, addonUnloadTopic, false);
};
InputPort.prototype[start] = InputPort.start;

InputPort.addListener = input => addObserver(input, input.topic, false);
InputPort.prototype.addListener = InputPort.addListener;

// When port is stopped (which is when it's subgraph has no
// no subcribers left) an actual observer unregistered.
// Note that port stopped once it ends as well (which is when
// add-on is unloaded).
InputPort.stop = input => {
  input.removeListener(input);
  removeObserver(input, addonUnloadTopic);
};
InputPort.prototype[stop] = InputPort.stop;

InputPort.removeListener = input => removeObserver(input, input.topic);
InputPort.prototype.removeListener = InputPort.removeListener;

// `InputPort` also implements `nsIObserver` interface and
// `nsISupportsWeakReference` interfaces as it's going to be used as such.
InputPort.prototype.QueryInterface = function(iid) {
  if (!iid.equals(Ci.nsIObserver) && !iid.equals(Ci.nsISupportsWeakReference))
    throw Cr.NS_ERROR_NO_INTERFACE;

  return this;
};

// `InputPort` instances implement `observe` method, which is invoked when
// observer notifications are dispatched. The `subject` of that notification
// are received on this signal.
InputPort.prototype.observe = function(subject, topic, data) {
  // Unwrap message from the subject. SDK used to have it's own version of
  // wrappedJSObjects which take precedence, if subject has `wrappedJSObject`
  // and it's not an XrayWrapper use it as message. Otherwise use subject as
  // is.
  const message = subject === null ? null :
        isLegacyWrapper(subject) ? unwrapLegacy(subject) :
        isXrayWrapper(subject) ? subject :
        subject.wrappedJSObject ? subject.wrappedJSObject :
        subject;

  // If observer topic matches topic of the input port receive a message.
  if (topic === this.topic) {
    receive(this, message);
  }

  // If observe topic is add-on unload topic we create an end message.
  if (topic === addonUnloadTopic && message === unloadMessage) {
    end(this);
  }
};

exports.InputPort = InputPort;
