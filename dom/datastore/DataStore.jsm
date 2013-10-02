/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict'

var EXPORTED_SYMBOLS = ["DataStore"];

/* DataStore object */

function DataStore(aAppId, aName, aOwner, aReadOnly) {
  this.appId = aAppId;
  this.name = aName;
  this.owner = aOwner;
  this.readOnly = aReadOnly;
}

DataStore.prototype = {
  appId: null,
  name: null,
  owner: null,
  readOnly: null,

  exposeObject: function(aWindow) {
    let self = this;
    let chromeObject = {
      get name() {
        return self.name;
      },

      get owner() {
        return self.owner;
      },

      get readOnly() {
        return self.readOnly;
      },

      /* TODO:
         Promise<Object> get(unsigned long id);
         Promise<void> update(unsigned long id, any obj);
         Promise<int> add(any obj)
         Promise<boolean> remove(unsigned long id)
         Promise<void> clear();

         readonly attribute DOMString revisionId
         attribute EventHandler onchange;
         Promise<DataStoreChanges> getChanges(DOMString revisionId)
         getAll(), getLength()
       */

      __exposedProps__: {
        name: 'r',
        owner: 'r',
        readOnly: 'r'
      }
    };

    return chromeObject;
  }
};
