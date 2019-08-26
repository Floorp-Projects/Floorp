/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EMPTY_WORKER_LIST = [];

const SINGLE_WORKER_DEFAULT_DOMAIN_LIST = [
  {
    active: true,
    name: "worker1",
    scope: "SCOPE 123",
    registrationFront: "",
    workerTargetFront: "",
    url: "http://example.com/worker.js",
  },
];

const SINGLE_WORKER_DIFFERENT_DOMAIN_LIST = [
  {
    active: true,
    name: "worker1",
    scope: "SCOPE 123",
    registrationFront: "",
    workerTargetFront: "",
    url: "http://different-example.com/worker.js",
  },
];

const MULTIPLE_WORKER_LIST = [
  {
    active: true,
    name: "worker1",
    scope: "SCOPE 123",
    registrationFront: "",
    workerTargetFront: "",
    url: "http://example.com/worker.js",
  },
  {
    active: false,
    name: "worker2",
    scope: "SCOPE 456",
    registrationFront: "",
    workerTargetFront: "",
    url: "http://example.com/worker.js",
  },
  {
    active: true,
    name: "worker3",
    scope: "SCOPE 789",
    registrationFront: "",
    workerTargetFront: "",
    url: "http://example.com/worker.js",
  },
];

const MULTIPLE_WORKER_MIXED_DOMAINS_LIST = [
  {
    active: true,
    name: "worker1",
    scope: "SCOPE 123",
    registrationFront: "",
    workerTargetFront: "",
    url: "http://example.com/worker.js",
  },
  {
    active: false,
    name: "worker2",
    scope: "SCOPE 456",
    registrationFront: "",
    workerTargetFront: "",
    url: "http://example.com/worker.js",
  },
  {
    active: true,
    name: "worker3",
    scope: "SCOPE 789",
    registrationFront: "",
    workerTargetFront: "",
    url: "http://different-example.com/worker.js",
  },
];

// props for a simple manifest
const MANIFEST_SIMPLE = {
  icons: [{ key: "1x1", value: "something.png" }],
  identity: [{ key: "name", value: "foo" }],
  presentation: [
    { key: "lorem", value: "ipsum" },
    { key: "foo", value: "bar" },
  ],
  warnings: [{ warn: "This is a warning" }],
};

module.exports = {
  EMPTY_WORKER_LIST,
  SINGLE_WORKER_DEFAULT_DOMAIN_LIST,
  SINGLE_WORKER_DIFFERENT_DOMAIN_LIST,
  MANIFEST_SIMPLE,
  MULTIPLE_WORKER_LIST,
  MULTIPLE_WORKER_MIXED_DOMAINS_LIST,
};
