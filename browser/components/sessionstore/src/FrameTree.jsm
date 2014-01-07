/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FrameTree"];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

const EXPORTED_METHODS = ["addObserver", "contains", "map", "forEach"];

/**
 * A FrameTree represents all frames that were reachable when the document
 * was loaded. We use this information to ignore frames when collecting
 * sessionstore data as we can't currently restore anything for frames that
 * have been created dynamically after or at the load event.
 *
 * @constructor
 */
function FrameTree(chromeGlobal) {
  let internal = new FrameTreeInternal(chromeGlobal);
  let external = {};

  for (let method of EXPORTED_METHODS) {
    external[method] = internal[method].bind(internal);
  }

  return Object.freeze(external);
}

/**
 * The internal frame tree API that the public one points to.
 *
 * @constructor
 */
function FrameTreeInternal(chromeGlobal) {
  // A WeakMap that uses frames (DOMWindows) as keys and their initial indices
  // in their parents' child lists as values. Suppose we have a root frame with
  // three subframes i.e. a page with three iframes. The WeakMap would have
  // four entries and look as follows:
  //
  // root -> 0
  // subframe1 -> 0
  // subframe2 -> 1
  // subframe3 -> 2
  //
  // Should one of the subframes disappear we will stop collecting data for it
  // as |this._frames.has(frame) == false|. All other subframes will maintain
  // their initial indices to ensure we can restore frame data appropriately.
  this._frames = new WeakMap();

  // The Set of observers that will be notified when the frame changes.
  this._observers = new Set();

  // The chrome global we use to retrieve the current DOMWindow.
  this._chromeGlobal = chromeGlobal;

  // Register a web progress listener to be notified about new page loads.
  let docShell = chromeGlobal.docShell;
  let ifreq = docShell.QueryInterface(Ci.nsIInterfaceRequestor);
  let webProgress = ifreq.getInterface(Ci.nsIWebProgress);
  webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);
}

FrameTreeInternal.prototype = {

  // Returns the docShell's current global.
  get content() {
    return this._chromeGlobal.content;
  },

  /**
   * Adds a given observer |obs| to the set of observers that will be notified
   * when the frame tree is reset (when a new document starts loading) or
   * recollected (when a document finishes loading).
   *
   * @param obs (object)
   */
  addObserver: function (obs) {
    this._observers.add(obs);
  },

  /**
   * Notifies all observers that implement the given |method|.
   *
   * @param method (string)
   */
  notifyObservers: function (method) {
    for (let obs of this._observers) {
      if (obs.hasOwnProperty(method)) {
        obs[method]();
      }
    }
  },

  /**
   * Checks whether a given |frame| is contained in the collected frame tree.
   * If it is not, this indicates that we should not collect data for it.
   *
   * @param frame (nsIDOMWindow)
   * @return bool
   */
  contains: function (frame) {
    return this._frames.has(frame);
  },

  /**
   * Recursively applies the given function |cb| to the stored frame tree. Use
   * this method to collect sessionstore data for all reachable frames stored
   * in the frame tree.
   *
   * If a given function |cb| returns a value, it must be an object. It may
   * however return "null" to indicate that there is no data to be stored for
   * the given frame.
   *
   * The object returned by |cb| cannot have any property named "children" as
   * that is used to store information about subframes in the tree returned
   * by |map()| and might be overridden.
   *
   * @param cb (function)
   * @return object
   */
  map: function (cb) {
    let frames = this._frames;

    function walk(frame) {
      let obj = cb(frame) || {};

      if (frames.has(frame)) {
        let children = [];

        Array.forEach(frame.frames, subframe => {
          // Don't collect any data if the frame is not contained in the
          // initial frame tree. It's a dynamic frame added later.
          if (!frames.has(subframe)) {
            return;
          }

          // Retrieve the frame's original position in its parent's child list.
          let index = frames.get(subframe);

          // Recursively collect data for the current subframe.
          let result = walk(subframe, cb);
          if (result && Object.keys(result).length) {
            children[index] = result;
          }
        });

        if (children.length) {
          obj.children = children;
        }
      }

      return Object.keys(obj).length ? obj : null;
    }

    return walk(this.content);
  },

  /**
   * Applies the given function |cb| to all frames stored in the tree. Use this
   * method if |map()| doesn't suit your needs and you want more control over
   * how data is collected.
   *
   * @param cb (function)
   *        This callback receives the current frame as the only argument.
   */
  forEach: function (cb) {
    let frames = this._frames;

    function walk(frame) {
      cb(frame);

      if (!frames.has(frame)) {
        return;
      }

      Array.forEach(frame.frames, subframe => {
        if (frames.has(subframe)) {
          cb(subframe);
        }
      });
    }

    walk(this.content);
  },

  /**
   * Stores a given |frame| and its children in the frame tree.
   *
   * @param frame (nsIDOMWindow)
   * @param index (int)
   *        The index in the given frame's parent's child list.
   */
  collect: function (frame, index = 0) {
    // Mark the given frame as contained in the frame tree.
    this._frames.set(frame, index);

    // Mark the given frame's subframes as contained in the tree.
    Array.forEach(frame.frames, this.collect, this);
  },

  /**
   * @see nsIWebProgressListener.onStateChange
   *
   * We want to be notified about:
   *  - new documents that start loading to clear the current frame tree;
   *  - completed document loads to recollect reachable frames.
   */
  onStateChange: function (webProgress, request, stateFlags, status) {
    // Ignore state changes for subframes because we're only interested in the
    // top-document starting or stopping its load. We thus only care about any
    // changes to the root of the frame tree, not to any of its nodes/leafs.
    if (!webProgress.isTopLevel || webProgress.DOMWindow != this.content) {
      return;
    }

    if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
      // Clear the list of frames until we can recollect it.
      this._frames.clear();

      // Notify observers that the frame tree has been reset.
      this.notifyObservers("onFrameTreeReset");
    } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      // The document and its resources have finished loading.
      this.collect(webProgress.DOMWindow);

      // Notify observers that the frame tree has been reset.
      this.notifyObservers("onFrameTreeCollected");
    }
  },

  // Unused nsIWebProgressListener methods.
  onLocationChange: function () {},
  onProgressChange: function () {},
  onSecurityChange: function () {},
  onStatusChange: function () {},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference])
};
