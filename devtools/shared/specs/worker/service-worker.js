/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {RetVal, generateActorSpec} = require("devtools/shared/protocol");

const pushSubscriptionSpec = generateActorSpec({
  typeName: "pushSubscription",
});

exports.pushSubscriptionSpec = pushSubscriptionSpec;

const serviceWorkerRegistrationSpec = generateActorSpec({
  typeName: "serviceWorkerRegistration",

  events: {
    "push-subscription-modified": {
      type: "push-subscription-modified"
    },
    "registration-changed": {
      type: "registration-changed"
    }
  },

  methods: {
    start: {
      request: {},
      response: RetVal("json")
    },
    unregister: {
      request: {},
      response: RetVal("json")
    },
    getPushSubscription: {
      request: {},
      response: {
        subscription: RetVal("nullable:pushSubscription")
      }
    },
  },
});

exports.serviceWorkerRegistrationSpec = serviceWorkerRegistrationSpec;

const serviceWorkerSpec = generateActorSpec({
  typeName: "serviceWorker",
});

exports.serviceWorkerSpec = serviceWorkerSpec;

