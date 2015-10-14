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
    "maxSize": 2,
    "clientMaxSize": 2,
    "creationTime": 1405517546,
    "ctime": 1405517818,
    "expiresAt": 1405534180,
    "participants": [
       { "displayName": "Alexis", "account": "alexis@example.com", "roomConnectionId": "2a1787a6-4a73-43b5-ae3e-906ec1e763cb" },
       { "displayName": "Adam", "roomConnectionId": "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7" }
     ]
  },
  {
    "roomToken": "REJRFfkdfkf",
    "decryptedContext": {
      "roomName": "Third Room Name"
    },
    "roomUrl": "http://localhost:3000/rooms/REJRFfkdfkf",
    "roomOwner": "Alexis",
    "maxSize": 2,
    "creationTime": 1405537485,
    "ctime": 1405537485,
    "expiresAt": 1405554180,
    "participants": []
  },
  {
    "roomToken": "fjdkreFJDer",
    "decryptedContext": {
      "roomName": "Forth Room Name"
    },
    "roomUrl": "http://localhost:3000/rooms/fjdkreFJDer",
    "roomOwner": "Alexis",
    "maxSize": 2,
    "creationTime": 1405546564,
    "ctime": 1405546564,
    "expiresAt": 1405564180,
    "participants": []
  },
  {
    "roomToken": "preFDREJhdf",
    "decryptedContext": {
      "roomName": "Fifth Room Name"
    },
    "roomUrl": "http://localhost:3000/rooms/preFDREJhdf",
    "roomOwner": "Alexis",
    "maxSize": 2,
    "creationTime": 1405566934,
    "ctime": 1405566934,
    "expiresAt": 1405584180,
    "participants": []
  },
  {
    "roomToken": "preFLighdf",
    "decryptedContext": {
      "roomName": "Sixth Room Name"
    },
    "roomUrl": "http://localhost:3000/rooms/preFLighdf",
    "roomOwner": "Alexis",
    "maxSize": 2,
    "creationTime": 1405576934,
    "ctime": 1405576934,
    "expiresAt": 1405614180,
    "participants": []
  }
];

var fakeManyContacts = [{
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
}, {
  id: 5,
  _guid: 5,
  name: ["Erin J. Bazile"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "erinjbazile@armyspy.com"
  }],
  category: ["google"],
  published: 1406798311748,
  updated: 1406798311748
}, {
  id: 6,
  _guid: 6,
  name: ["Kelly F. Maldanado"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "kellyfmaldonado@jourrapide.com"
  }],
  category: ["google"],
  published: 1406798311748,
  updated: 1406798311748
}, {
  id: 7,
  _guid: 7,
  name: ["John J. Brown"],
  email: [{
    "pref": true,
    "type": ["work"],
    "value": "johnjbrow@johndoe.com"
  }],
  category: ["google"],
  published: 1406798311748,
  updated: 1406798311748
}];
var fakeFewerContacts = fakeManyContacts.slice(0, 4);

(function() {
  "use strict";

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
      var avatarUrl = "http://www.gravatar.com/avatar/0a996f0fe2727ef1668bdb11897e4459.jpg?default=blank&s=40";
      return Math.ceil(Math.random() * 3) === 2 ? avatarUrl : null;
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
        callback(null, [].concat(fakeManyContacts));
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
    stopAlerting: function() {},
    userProfile: null
  };
})();
