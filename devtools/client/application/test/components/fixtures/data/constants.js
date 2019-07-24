/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const INITSTATE = {
  domain: "domain",
  fluentBundles: [],
  serviceContainer: {},
  canDebugWorkers: true,
};
const CLIENT = {};
const EMPTY_WORKER_LIST = [];

const SINGLE_WORKER_LIST = [
  {
    active: true,
    name: "worker1",
    scope: "SCOPE 123",
    registrationFront: "",
    workerTargetFront: "",
  },
];

const MULTIPLE_WORKER_LIST = [
  {
    active: true,
    name: "worker1",
    scope: "SCOPE 123",
    registrationFront: "",
    workerTargetFront: "",
  },
  {
    active: false,
    name: "worker2",
    scope: "SCOPE 456",
    registrationFront: "",
    workerTargetFront: "",
  },
  {
    active: true,
    name: "worker3",
    scope: "SCOPE 789",
    registrationFront: "",
    workerTargetFront: "",
  },
];

module.exports = {
  INITSTATE,
  CLIENT,
  EMPTY_WORKER_LIST,
  SINGLE_WORKER_LIST,
  MULTIPLE_WORKER_LIST,
};
