/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Sample from https://wiki.mozilla.org/Loop/Architecture/Rooms#GET_.2Frooms
var fakeRooms = [
  {
    "roomToken": "_nxD4V4FflQ",
    "roomName": "First Room Name",
    "roomUrl": "http://localhost:3000/rooms/_nxD4V4FflQ",
    "roomOwner": "Alexis",
    "maxSize": 2,
    "creationTime": 1405517546,
    "ctime": 1405517546,
    "expiresAt": 1405534180,
    "participants": []
  },
  {
    "roomToken": "QzBbvGmIZWU",
    "roomName": "Second Room Name",
    "roomUrl": "http://localhost:3000/rooms/QzBbvGmIZWU",
    "roomOwner": "Alexis",
    "maxSize": 2,
    "creationTime": 1405517546,
    "ctime": 1405517546,
    "expiresAt": 1405534180,
    "participants": []
  },
  {
    "roomToken": "3jKS_Els9IU",
    "roomName": "UX Discussion",
    "roomUrl": "http://localhost:3000/rooms/3jKS_Els9IU",
    "roomOwner": "Alexis",
    "maxSize": 2,
    "clientMaxSize": 2,
    "creationTime": 1405517546,
    "ctime": 1405517818,
    "expiresAt": 1405534180,
    "participants": [
       { "displayName": "Alexis", "account": "alexis@example.com", "roomConnectionId": "2a1787a6-4a73-43b5-ae3e-906ec1e763cb" },
       { "displayName": "Adam", "roomConnectionId": "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7" }
     ]
  }
];

/**
 * Faking the mozLoop object which doesn't exist in regular web pages.
 * @type {Object}
 */
navigator.mozLoop = {
  ensureRegistered: function() {},
  getLoopPref: function(pref) {
    // Ensure UI for rooms is displayed in the showcase.
    if (pref === "rooms.enabled") {
      return true;
    }
  },
  releaseCallData: function() {},
  copyString: function() {},
  contacts: {
    getAll: function(callback) {
      callback(null, []);
    },
    on: function() {}
  },
  rooms: {
    getAll: function(version, callback) {
      callback(null, fakeRooms);
    },
    on: function() {}
  },
  fxAEnabled: true
};
