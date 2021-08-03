/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SessionWorkerCache"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  SessionWorker: "resource:///modules/sessionstore/SessionWorker.jsm",
  requestIdleCallback: "resource://gre/modules/Timer.jsm",
});

// The intent of this cache is to minimize the cost of sending the session
// store data to the worker to be written to disk. The session store can be
// very large, and we can spend a lot of time structured cloning the data which
// is necessary to hand it off to the other thread. There are enormous
// rearchitectures which we could probably do to eliminate these costs almost
// entirely. However those are risky and cost prohibitive, so this cache just
// aims to take any big objects and cache them on the worker, so we don't have
// to send them over every time, and can simply send them as they change.
var SessionWorkerCache = {
  getById(id) {
    return SessionWorkerCacheInternal.getById(id);
  },

  clear() {
    SessionWorkerCacheInternal.clear();
  },

  getCacheObjects() {
    return SessionWorkerCacheInternal.getCacheObjects();
  },

  import(objs) {
    return SessionWorkerCacheInternal.import(objs);
  },

  addRef(strObj) {
    return SessionWorkerCacheInternal.addRef(strObj);
  },

  release(strObj) {
    return SessionWorkerCacheInternal.release(strObj);
  },
};

Object.freeze(SessionWorkerCache);

var SessionWorkerCacheInternal = {
  _epoch: 0,
  _lastObjId: 0,
  _objsToObjIds: new Map(),
  _objIdsToReferences: new Map(),

  getById(id) {
    return this._objIdsToReferences.get(id)?.value;
  },

  getCacheObjects() {
    return [...this._objIdsToReferences.entries()].map(([id, ref]) => [
      id,
      ref.value,
    ]);
  },

  clear() {
    this._epoch++;
    this._objsToObjIds.clear();
    this._objIdsToReferences.clear();
    SessionWorker.post("clearSessionWorkerCache", []);
  },

  import(objs) {
    let epoch = this._epoch;
    for (let [id, strObj] of objs) {
      if (id > this._lastObjId) {
        this._lastObjId = id;
      }
      this._objsToObjIds.set(strObj, id);
      // As we're importing these objects, we set the ref count to 1 to keep
      // them alive. Once SessionStore has restored all the windows, we can
      // clean up all of these temporary references as they should all be
      // legitimately referenced by the tabs that have been restored.
      this._objIdsToReferences.set(id, { count: 1, value: strObj });
    }

    let index = 0;
    let length = objs.length;
    let idleCallback = deadline => {
      if (epoch != this._epoch) {
        return;
      }
      for (; index < length; index++) {
        let [id, strObj] = objs[index];
        SessionWorker.post("define", [id, strObj]);

        if (deadline.timeRemaining() < 1) {
          requestIdleCallback(idleCallback);
          return;
        }
      }

      // If we made it to this point, we've finished defining everything and
      // now can clean up our temporary references once the session store has
      // finished restoring all windows.
      SessionStore.promiseAllWindowsRestored.then(() => {
        for (let [id] of objs) {
          this._releaseById(id);
        }
      });
    };

    requestIdleCallback(idleCallback);
  },

  addRef(strObj) {
    let id = this._objsToObjIds.get(strObj);
    if (id !== undefined) {
      let reference = this._objIdsToReferences.get(id);
      reference.count++;
    } else {
      id = ++this._lastObjId;
      this._objsToObjIds.set(strObj, id);
      let reference = { count: 1, value: strObj };
      this._objIdsToReferences.set(id, reference);
      SessionWorker.post("define", [id, strObj]);
    }
    return id;
  },

  release(strObj) {
    let id = this._objsToObjIds.get(strObj);
    if (id !== undefined) {
      this._releaseById(id);
    }
    return id;
  },

  _releaseById(id) {
    let reference = this._objIdsToReferences.get(id);
    reference.count--;
    if (!reference.count) {
      this._objsToObjIds.delete(reference.value);
      this._objIdsToReferences.delete(id);
      SessionWorker.post("delete", [id]);
    }
  },
};
