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
          return true;
      }
      return null;
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
