/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Sample from https://wiki.mozilla.org/Loop/Architecture/Rooms#GET_.2Frooms
var fakeRooms = [
  {
    "roomToken": "_nxD4V4FflQ",
    "decryptedContext": {
      "roomName": "First Room Name",
      "urls": [{
        description: "The mozilla page",
        location: "https://www.mozilla.org",
        thumbnail: "https://www.mozilla.org/favicon.ico"
      }]
    },
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
    "decryptedContext": {
      "roomName": "Second Room Name"
    },
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
    "decryptedContext": {
      "roomName": "UX Discussion"
    },
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

var fakeContacts = [{
  id: 1,
  _guid: 1,
  name: ["Ally Avocado"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "ally@mail.com"
  }],
  tel: [{
    "pref": true,
    "type": ["mobile"],
    "value": "+31-6-12345678"
  }],
  category: ["google"],
  published: 1406798311748,
  updated: 1406798311748
}, {
  id: 2,
  _guid: 2,
  name: ["Bob Banana"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "bob@gmail.com"
  }],
  tel: [{
    "pref": true,
    "type": ["mobile"],
    "value": "+1-214-5551234"
  }],
  category: ["local"],
  published: 1406798311748,
  updated: 1406798311748
}, {
  id: 3,
  _guid: 3,
  name: ["Caitlin Cantaloupe"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "caitlin.cant@hotmail.com"
  }],
  category: ["local"],
  published: 1406798311748,
  updated: 1406798311748
}, {
  id: 4,
  _guid: 4,
  name: ["Dave Dragonfruit"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "dd@dragons.net"
  }],
  category: ["google"],
  published: 1406798311748,
  updated: 1406798311748
}];

/**
 * Faking the mozLoop object which doesn't exist in regular web pages.
 * @type {Object}
 */
navigator.mozLoop = {
  ensureRegistered: function() {},
  getAudioBlob: function(){},
  getLoopPref: function(pref) {
    switch(pref) {
      // Ensure we skip FTE completely.
      case "gettingStarted.seen":
      case "contacts.gravatars.promo":
      case "contextInConversations.enabled":
        return true;
      case "contacts.gravatars.show":
        return false;
    }
  },
  hasEncryptionKey: true,
  setLoopPref: function(){},
  releaseCallData: function() {},
  copyString: function() {},
  getUserAvatar: function(emailAddress) {
    return "http://www.gravatar.com/avatar/" + (Math.ceil(Math.random() * 3) === 2 ?
      "0a996f0fe2727ef1668bdb11897e4459" : "foo") + ".jpg?default=blank&s=40";
  },
  getSelectedTabMetadata: function(callback) {
    callback({
      previews: ["chrome://branding/content/about-logo.png"],
      description: "sample webpage description",
      url: "https://www.example.com"
    });
  },
  contacts: {
    getAll: function(callback) {
      callback(null, [].concat(fakeContacts));
    },
    on: function() {}
  },
  rooms: {
    getAll: function(version, callback) {
      callback(null, [].concat(fakeRooms));
    },
    on: function() {}
  },
  fxAEnabled: true,
  startAlerting: function() {},
  stopAlerting: function() {}
};
