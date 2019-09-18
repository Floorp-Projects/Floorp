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
  identity: [{ key: "name", value: "foo", type: "string" }],
  presentation: [
    { key: "lorem", value: "ipsum", type: "string" },
    { key: "foo", value: "bar", type: "string" },
  ],
  validation: [{ level: "warning", message: "This is a warning" }],
};

const MANIFEST_STRING_MEMBERS = {
  icons: [],
  identity: [{ key: "name", value: "foo", type: "string" }],
  presentation: [],
  validation: [],
};

// props for a manifest with color values
const MANIFEST_COLOR_MEMBERS = {
  icons: [],
  identity: [],
  presentation: [
    { key: "background_color", value: "red", type: "color" },
    { key: "theme_color", value: "rgb(0, 0, 0)", type: "color" },
  ],
  validation: [],
};

const MANIFEST_UNKNOWN_TYPE_MEMBERS = {
  icons: [],
  identity: [{ key: "lorem", value: "ipsum", type: "foo" }],
  presentation: [],
  validation: [],
};

const MANIFEST_WITH_ISSUES = {
  icons: [{ key: "1x1", value: "something.png" }],
  identity: [{ key: "name", value: "foo", type: "string" }],
  presentation: [
    { key: "lorem", value: "ipsum", type: "string" },
    { key: "foo", value: "bar", type: "string" },
  ],
  validation: [{ level: "warning", message: "This is a warning" }],
};

// props for a manifest with no validation issues
const MANIFEST_NO_ISSUES = {
  icons: [{ key: "1x1", value: "something.png" }],
  identity: [{ key: "name", value: "foo", type: "string" }],
  presentation: [
    { key: "lorem", value: "ipsum", type: "string" },
    { key: "foo", value: "bar", type: "string" },
  ],
  validation: [],
};

module.exports = {
  EMPTY_WORKER_LIST,
  SINGLE_WORKER_DEFAULT_DOMAIN_LIST,
  SINGLE_WORKER_DIFFERENT_DOMAIN_LIST,
  MANIFEST_NO_ISSUES,
  MANIFEST_WITH_ISSUES,
  MANIFEST_SIMPLE,
  MANIFEST_COLOR_MEMBERS,
  MANIFEST_STRING_MEMBERS,
  MANIFEST_UNKNOWN_TYPE_MEMBERS,
  MULTIPLE_WORKER_LIST,
  MULTIPLE_WORKER_MIXED_DOMAINS_LIST,
};
