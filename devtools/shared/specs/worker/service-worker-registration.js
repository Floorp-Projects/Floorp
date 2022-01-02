/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { RetVal, generateActorSpec } = require("devtools/shared/protocol");

const serviceWorkerRegistrationSpec = generateActorSpec({
  typeName: "serviceWorkerRegistration",

  events: {
    "push-subscription-modified": {
      type: "push-subscription-modified",
    },
    "registration-changed": {
      type: "registration-changed",
    },
  },

  methods: {
    allowShutdown: {
      request: {},
    },
    preventShutdown: {
      request: {},
    },
    push: {
      request: {},
    },
    start: {
      request: {},
    },
    unregister: {
      request: {},
    },
    getPushSubscription: {
      request: {},
      response: {
        subscription: RetVal("nullable:pushSubscription"),
      },
    },
  },
});

exports.serviceWorkerRegistrationSpec = serviceWorkerRegistrationSpec;
