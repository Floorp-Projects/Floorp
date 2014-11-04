/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


var loop = loop || {};

/**
 * Monkeypatch getUserMedia in a way that prevents additional camera and
 * microphone prompts, at the cost of ignoring all constraints other than
 * the first set passed in.
 *
 * The first call to navigator.getUserMedia (also now aliased to
 * multiplexGum.getPermsAndCacheMedia to allow for explicit calling code)
 * will cause the underlying gUM implementation to be called.
 *
 * While permission is pending, subsequent calls will result in the callbacks
 * being queued. Once the call succeeds or fails, all queued success or
 * failure callbacks will be invoked.  Subsequent calls to either function will
 * cause the success or failure callback to be invoked immediately.
 */
loop.standaloneMedia = (function() {
  "use strict";

  function patchSymbolIfExtant(objectName, propertyName, replacement) {
    var object;
    if (window[objectName]) {
      object = window[objectName];
    }
    if (object && object[propertyName]) {
      object[propertyName] = replacement;
    }
  }

  // originalGum _must_ be on navigator; otherwise things blow up
  navigator.originalGum = navigator.getUserMedia ||
                          navigator.mozGetUserMedia ||
                          navigator.webkitGetUserMedia ||
                          (window["TBPlugin"] && TBPlugin.getUserMedia);

  function _MultiplexGum() {
    this.reset();
  }

  _MultiplexGum.prototype = {
    /**
     * @see The docs at the top of this file for overall semantics,
     * & http://developer.mozilla.org/en-US/docs/NavigatorUserMedia.getUserMedia
     * for params, since this is intended to be purely a passthrough to gUM.
     */
    getPermsAndCacheMedia: function(constraints, onSuccess, onError) {
      function handleResult(callbacks, param) {
        // Operate on a copy of the array in case any of the callbacks
        // calls reset, which would cause an infinite-recursion.
        this.userMedia.successCallbacks = [];
        this.userMedia.errorCallbacks = [];
        callbacks.forEach(function(cb) {
          if (typeof cb == "function") {
            cb(param);
          }
        })
      }
      function handleSuccess(localStream) {
        this.userMedia.pending = false;
        this.userMedia.localStream = localStream;
        this.userMedia.error = null;
        handleResult.call(this, this.userMedia.successCallbacks.slice(0), localStream);
      }

      function handleError(error) {
        this.userMedia.pending = false;
        this.userMedia.error = error;
        handleResult.call(this, this.userMedia.errorCallbacks.slice(0), error);
        this.error = null;
      }

      if (this.userMedia.localStream &&
          this.userMedia.localStream.ended) {
        this.userMedia.localStream = null;
      }

      this.userMedia.errorCallbacks.push(onError);
      this.userMedia.successCallbacks.push(onSuccess);

      if (this.userMedia.localStream) {
        handleSuccess.call(this, this.userMedia.localStream);
        return;
      } else if (this.userMedia.error) {
        handleError.call(this, this.userMedia.error);
        return;
      }

      if (this.userMedia.pending) {
        return;
      }
      this.userMedia.pending = true;

      navigator.originalGum(constraints, handleSuccess.bind(this),
        handleError.bind(this));
    },

    /**
     * Reset the cached permissions, callbacks, and media to their default
     * state and call any error callbacks to let any waiting callers know
     * not to ever expect any more callbacks.  We use "PERMISSION_DENIED",
     * for lack of a better, more specific gUM code that callers are likely
     * to be prepared to handle.
     */
    reset: function() {
      // When called from the ctor, userMedia is not created yet.
      if (this.userMedia) {
        this.userMedia.errorCallbacks.forEach(function(cb) {
          if (typeof cb == "function") {
            cb("PERMISSION_DENIED");
          }
        });
        if (this.userMedia.localStream &&
            typeof this.userMedia.localStream.stop == "function") {
          this.userMedia.localStream.stop();
        }
      }
      this.userMedia = {
        error: null,
        localStream: null,
        pending: false,
        errorCallbacks: [],
        successCallbacks: [],
      };
    }
  };

  var singletonMultiplexGum = new _MultiplexGum();
  function myGetUserMedia() {
    // This function is needed to pull in the instance
    // of the singleton for tests to overwrite the used instance.
    singletonMultiplexGum.getPermsAndCacheMedia.apply(singletonMultiplexGum, arguments);
  };
  patchSymbolIfExtant("navigator", "mozGetUserMedia", myGetUserMedia);
  patchSymbolIfExtant("navigator", "webkitGetUserMedia", myGetUserMedia);
  patchSymbolIfExtant("navigator", "getUserMedia", myGetUserMedia);
  patchSymbolIfExtant("TBPlugin", "getUserMedia", myGetUserMedia);

  return {
    multiplexGum: singletonMultiplexGum,
    _MultiplexGum: _MultiplexGum,
    setSingleton: function(singleton) {
      singletonMultiplexGum = singleton;
    },
  };
})();
