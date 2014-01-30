/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict'

this.EXPORTED_SYMBOLS = ['DataStoreCursor'];

function debug(s) {
  //dump('DEBUG DataStoreCursor: ' + s + '\n');
}

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const STATE_INIT = 0;
const STATE_REVISION_INIT = 1;
const STATE_REVISION_CHECK = 2;
const STATE_SEND_ALL = 3;
const STATE_REVISION_SEND = 4;
const STATE_DONE = 5;

const REVISION_ADDED = 'added';
const REVISION_UPDATED = 'updated';
const REVISION_REMOVED = 'removed';
const REVISION_VOID = 'void';
const REVISION_SKIP = 'skip'

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

/**
 * legend:
 * - RID = revision ID
 * - R = revision object (with the internalRevisionId that is a number)
 * - X = current object ID.
 * - L = the list of revisions that we have to send
 *
 * State: init: do you have RID ?
 *   YES: state->initRevision; loop
 *   NO: get R; X=0; state->sendAll; send a 'clear'
 *
 * State: initRevision. Get R from RID. Done?
 *   YES: state->revisionCheck; loop
 *   NO: RID = null; state->init; loop
 *
 * State: revisionCheck: get all the revisions between R and NOW. Done?
 *   YES and R == NOW: state->done; loop
 *   YES and R != NOW: Store this revisions in L; state->revisionSend; loop
 *   NO: R = NOW; X=0; state->sendAll; send a 'clear'
 *
 * State: sendAll: is R still the last revision?
 *   YES get the first object with id > X. Done?
 *     YES: X = object.id; send 'add'
 *     NO: state->revisionCheck; loop
 *   NO: R = NOW; X=0; send a 'clear'
 *
 * State: revisionSend: do you have something from L to send?
 *   YES and L[0] == 'removed': R=L[0]; send 'remove' with ID
 *   YES and L[0] == 'added': R=L[0]; get the object; found?
 *     NO: loop
 *     YES: send 'add' with ID and object
 *   YES and L[0] == 'updated': R=L[0]; get the object; found?
 *     NO: loop
 *     YES and object.R > R: continue
 *     YES and object.R <= R: send 'update' with ID and object
 *   YES L[0] == 'void': R=L[0]; state->init; loop
 *   NO: state->revisionCheck; loop
 *
 * State: done: send a 'done' with R
 */

/* Helper functions */
function createDOMError(aWindow, aEvent) {
  return new aWindow.DOMError(aEvent.target.error.name);
}

/* DataStoreCursor object */
this.DataStoreCursor = function(aWindow, aDataStore, aRevisionId) {
  this.init(aWindow, aDataStore, aRevisionId);
}

this.DataStoreCursor.prototype = {
  classDescription: 'DataStoreCursor XPCOM Component',
  classID: Components.ID('{b6d14349-1eab-46b8-8513-584a7328a26b}'),
  contractID: '@mozilla.org/dom/datastore-cursor;1',
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsISupports]),

  _window: null,
  _dataStore: null,
  _revisionId: null,
  _revision: null,
  _revisionsList: null,
  _objectId: 0,

  _state: STATE_INIT,

  init: function(aWindow, aDataStore, aRevisionId) {
    debug('DataStoreCursor init');

    this._window = aWindow;
    this._dataStore = aDataStore;
    this._revisionId = aRevisionId;
  },

  // This is the implementation of the state machine.
  // Read the comments at the top of this file in order to follow what it does.
  stateMachine: function(aStore, aRevisionStore, aResolve, aReject) {
    debug('StateMachine: ' + this._state);

    switch (this._state) {
      case STATE_INIT:
        this.stateMachineInit(aStore, aRevisionStore, aResolve, aReject);
        break;

      case STATE_REVISION_INIT:
        this.stateMachineRevisionInit(aStore, aRevisionStore, aResolve, aReject);
        break;

      case STATE_REVISION_CHECK:
        this.stateMachineRevisionCheck(aStore, aRevisionStore, aResolve, aReject);
        break;

      case STATE_SEND_ALL:
        this.stateMachineSendAll(aStore, aRevisionStore, aResolve, aReject);
        break;

      case STATE_REVISION_SEND:
        this.stateMachineRevisionSend(aStore, aRevisionStore, aResolve, aReject);
        break;

      case STATE_DONE:
        this.stateMachineDone(aStore, aRevisionStore, aResolve, aReject);
        break;
    }
  },

  stateMachineInit: function(aStore, aRevisionStore, aResolve, aReject) {
    debug('StateMachineInit');

    if (this._revisionId) {
      this._state = STATE_REVISION_INIT;
      this.stateMachine(aStore, aRevisionStore, aResolve, aReject);
      return;
    }

    let self = this;
    let request = aRevisionStore.openCursor(null, 'prev');
    request.onsuccess = function(aEvent) {
      self._revision = aEvent.target.result.value;
      self._objectId = 0;
      self._state = STATE_SEND_ALL;
      aResolve(Cu.cloneInto({ operation: 'clear' }, self._window));
    }
  },

  stateMachineRevisionInit: function(aStore, aRevisionStore, aResolve, aReject) {
    debug('StateMachineRevisionInit');

    let self = this;
    let request = this._dataStore._db.getInternalRevisionId(
      self._revisionId,
      aRevisionStore,
      function(aInternalRevisionId) {
        // This revision doesn't exist.
        if (aInternalRevisionId == undefined) {
          self._revisionId = null;
          self._objectId = 0;
          self._state = STATE_INIT;
          self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
          return;
        }

        self._revision = { revisionId: self._revisionId,
                           internalRevisionId: aInternalRevisionId };
        self._state = STATE_REVISION_CHECK;
        self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
      }
    );
  },

  stateMachineRevisionCheck: function(aStore, aRevisionStore, aResolve, aReject) {
    debug('StateMachineRevisionCheck');

    let changes = {
      addedIds: {},
      updatedIds: {},
      removedIds: {}
    };

    let self = this;
    let request = aRevisionStore.mozGetAll(
      self._window.IDBKeyRange.lowerBound(this._revision.internalRevisionId, true));
    request.onsuccess = function(aEvent) {

      // Optimize the operations.
      for (let i = 0; i < aEvent.target.result.length; ++i) {
        let data = aEvent.target.result[i];

        switch (data.operation) {
          case REVISION_ADDED:
            changes.addedIds[data.objectId] = data.internalRevisionId;
            break;

          case REVISION_UPDATED:
            // We don't consider an update if this object has been added
            // or if it has been already modified by a previous
            // operation.
            if (!(data.objectId in changes.addedIds) &&
                !(data.objectId in changes.updatedIds)) {
              changes.updatedIds[data.objectId] = data.internalRevisionId;
            }
            break;

          case REVISION_REMOVED:
            let id = data.objectId;

            // If the object has been added in this range of revisions
            // we can ignore it and remove it from the list.
            if (id in changes.addedIds) {
              delete changes.addedIds[id];
            } else {
              changes.removedIds[id] = data.internalRevisionId;
            }

            if (id in changes.updatedIds) {
              delete changes.updatedIds[id];
            }
            break;

          case REVISION_VOID:
            if (i != 0) {
              dump('Internal error: Revision "' + REVISION_VOID + '" should not be found!!!\n');
              return;
            }

            self._revisionId = null;
            self._objectId = 0;
            self._state = STATE_INIT;
            self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
            return;
        }
      }

      // From changes to a map of internalRevisionId.
      let revisions = {};
      function addRevisions(obj) {
        for (let key in obj) {
          revisions[obj[key]] = true;
        }
      }

      addRevisions(changes.addedIds);
      addRevisions(changes.updatedIds);
      addRevisions(changes.removedIds);

      // Create the list of revisions.
      let list = [];
      for (let i = 0; i < aEvent.target.result.length; ++i) {
        let data = aEvent.target.result[i];

        // If this revision doesn't contain useful data, we still need to keep
        // it in the list because we need to update the internal revision ID.
        if (!(data.internalRevisionId in revisions)) {
          data.operation = REVISION_SKIP;
        }

        list.push(data);
      }

      if (list.length == 0) {
        self._state = STATE_DONE;
        self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
        return;
      }

      // Some revision has to be sent.
      self._revisionsList = list;
      self._state = STATE_REVISION_SEND;
      self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
    };
  },

  stateMachineSendAll: function(aStore, aRevisionStore, aResolve, aReject) {
    debug('StateMachineSendAll');

    let self = this;
    let request = aRevisionStore.openCursor(null, 'prev');
    request.onsuccess = function(aEvent) {
      if (self._revision.revisionId != aEvent.target.result.value.revisionId) {
        self._revision = aEvent.target.result.value;
        self._objectId = 0;
        aResolve(Cu.cloneInto({ operation: 'clear' }, self._window));
        return;
      }

      let request = aStore.openCursor(self._window.IDBKeyRange.lowerBound(self._objectId, true));
      request.onsuccess = function(aEvent) {
        let cursor = aEvent.target.result;
        if (!cursor) {
          self._state = STATE_REVISION_CHECK;
          self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
          return;
        }

        self._objectId = cursor.key;
        aResolve(Cu.cloneInto({ operation: 'add', id: self._objectId,
                                data: cursor.value }, self._window));
      };
    };
  },

  stateMachineRevisionSend: function(aStore, aRevisionStore, aResolve, aReject) {
    debug('StateMachineRevisionSend');

    if (!this._revisionsList.length) {
      this._state = STATE_REVISION_CHECK;
      this.stateMachine(aStore, aRevisionStore, aResolve, aReject);
      return;
    }

    this._revision = this._revisionsList.shift();

    switch (this._revision.operation) {
      case REVISION_REMOVED:
        aResolve(Cu.cloneInto({ operation: 'remove', id: this._revision.objectId },
                              this._window));
        break;

      case REVISION_ADDED: {
        let request = aStore.get(this._revision.objectId);
        let self = this;
        request.onsuccess = function(aEvent) {
          if (aEvent.target.result == undefined) {
            self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
            return;
          }

          aResolve(Cu.cloneInto({ operation: 'add', id: self._revision.objectId,
                                  data: aEvent.target.result }, self._window));
        }
        break;
      }

      case REVISION_UPDATED: {
        let request = aStore.get(this._revision.objectId);
        let self = this;
        request.onsuccess = function(aEvent) {
          if (aEvent.target.result == undefined) {
            self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
            return;
          }

          if (aEvent.target.result.revisionId >  self._revision.internalRevisionId) {
            self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
            return;
          }

          aResolve(Cu.cloneInto({ operation: 'update', id: self._revision.objectId,
                                  data: aEvent.target.result }, self._window));
        }
        break;
      }

      case REVISION_VOID:
        // Internal error!
        dump('Internal error: Revision "' + REVISION_VOID + '" should not be found!!!\n');
        break;

      case REVISION_SKIP:
        // This revision contains data that has already been sent by another one.
        this.stateMachine(aStore, aRevisionStore, aResolve, aReject);
        break;
    }
  },

  stateMachineDone: function(aStore, aRevisionStore, aResolve, aReject) {
    this.close();
    aResolve(Cu.cloneInto({ revisionId: this._revision.revisionId,
                            operation: 'done' }, this._window));
  },

  // public interface

  get store() {
    return this._dataStore.exposedObject;
  },

  next: function() {
    debug('Next');

    let self = this;
    return new this._window.Promise(function(aResolve, aReject) {
      self._dataStore._db.cursorTxn(
        function(aTxn, aStore, aRevisionStore) {
          self.stateMachine(aStore, aRevisionStore, aResolve, aReject);
        },
        function(aEvent) {
          aReject(createDOMError(self._window, aEvent));
        }
      );
    });
  },

  close: function() {
    this._dataStore.syncTerminated(this);
  }
};
