/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci, Cr, Cu } = require("chrome");
const { emit, on, off } = require("./core");
var observerService = Cc["@mozilla.org/observer-service;1"]
                      .getService(Ci.nsIObserverService);

const { ShimWaiver } = Cu.import("resource://gre/modules/ShimWaiver.jsm");
const addObserver = ShimWaiver.getProperty(observerService, "addObserver");
const removeObserver = ShimWaiver.getProperty(observerService, "removeObserver");

const { when: unload } = require("../system/unload");

// Simple class that can be used to instantiate event channel that
// implements `nsIObserver` interface. It's will is used by `observe`
// function as observer + event target. It basically proxies observer
// notifications as to it's registered listeners.
function ObserverChannel() {}
Object.freeze(Object.defineProperties(ObserverChannel.prototype, {
  QueryInterface: {
    value: function(iid) {
      if (!iid.equals(Ci.nsIObserver) &&
          !iid.equals(Ci.nsISupportsWeakReference) &&
          !iid.equals(Ci.nsISupports))
        throw Cr.NS_ERROR_NO_INTERFACE;
      return this;
    }
  },
  observe: {
    value: function(subject, topic, data) {
      emit(this, "data", {
        type: topic,
        target: subject,
        data: data
      });
    }
  }
}));

function observe(topic) {
  let observerChannel = new ObserverChannel();

  // Note: `nsIObserverService` will not hold a weak reference to a
  // observerChannel (since third argument is `true`). There for if it
  // will be GC-ed with all it's event listeners once no other references
  // will be held.
  addObserver(observerChannel, topic, true);

  // We need to remove any observer added once the add-on is unloaded;
  // otherwise we'll get a "dead object" exception.
  // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1001833
  unload(() => removeObserver(observerChannel, topic), { weak: true });

  return observerChannel;
}

exports.observe = observe;
