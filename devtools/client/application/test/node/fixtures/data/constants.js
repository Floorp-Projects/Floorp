/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// NOTE: worker state values are defined in an enum in nsIServiceWorkerManager
// https://searchfox.org/mozilla-central/source/dom/interfaces/base/nsIServiceWorkerManager.idl

const EMPTY_WORKER_LIST = [];

const WORKER_RUNNING = {
  id: "id-worker-1-example",
  isActive: true,
  workerTargetFront: true,
  url: "http://example.com/worker.js",
  state: 4,
  stateText: "activated",
};

const WORKER_STOPPED = {
  id: "id-worker-1-example",
  isActive: true,
  workerTargetFront: false,
  url: "http://example.com/worker.js",
  state: 4,
  stateText: "activated",
};

const WORKER_WAITING = {
  id: "id-worker-1-example",
  isActive: false,
  workerTargetFront: false,
  url: "http://example.com/worker.js",
  state: 2,
  stateText: "installed",
};

const REGISTRATION_SINGLE_WORKER = {
  id: "id-reg-1-example",
  scope: "SCOPE 123",
  registrationFront: "",
  workers: [
    {
      id: "id-worker-1-example",
      isActive: true,
      workerTargetFront: "",
      url: "http://example.com/worker.js",
      state: 4,
      stateText: "activated",
    },
  ],
};

const REGISTRATION_MULTIPLE_WORKERS = {
  id: "id-reg-1-example",
  scope: "SCOPE 123",
  registrationFront: "",
  workers: [
    {
      id: "id-worker-1-example",
      isActive: true,
      workerTargetFront: "",
      url: "http://example.com/worker.js",
      state: 4,
      stateText: "activated",
    },
    {
      id: "id-worker-2-example",
      isActive: false,
      workerTargetFront: "",
      url: "http://example.com/worker.js",
      state: 2,
      stateText: "installed",
    },
  ],
};

const SINGLE_WORKER_DEFAULT_DOMAIN_LIST = [
  {
    id: "id-reg-1-example",
    scope: "SCOPE 123",
    registrationFront: "",
    workers: [
      {
        id: "id-worker-1-example",
        isActive: true,
        workerTargetFront: "",
        url: "http://example.com/worker.js",
        state: 4,
        stateText: "activated",
      },
    ],
  },
];

const SINGLE_WORKER_DIFFERENT_DOMAIN_LIST = [
  {
    id: "id-reg-1-example",
    scope: "SCOPE 123",
    registrationFront: "",
    workers: [
      {
        id: "id-worker-1-example",
        isActive: true,
        workerTargetFront: "",
        url: "http://different-example.com/worker.js",
        state: 4,
        stateText: "activated",
      },
    ],
  },
];

const MULTIPLE_WORKER_LIST = [
  {
    id: "id-reg-1-example",
    scope: "SCOPE1",
    registrationFront: "",
    workers: [
      {
        id: "id-worker-1-example",
        isActive: true,
        workerTargetFront: "",
        url: "http://example.com/worker.js",
        state: 4,
        stateText: "activated",
      },
    ],
  },
  {
    id: "id-reg-1-example",
    scope: "SCOPE2",
    registrationFront: "",
    workers: [
      {
        id: "id-worker-2-example",
        isActive: false,
        workerTargetFront: "",
        url: "http://example.com/worker.js",
        state: 2,
        stateText: "installed",
      },
    ],
  },
  {
    id: "id-reg-3-example",
    scope: "SCOPE3",
    registrationFront: "",
    workers: [
      {
        id: "id-worker-3-example",
        isActive: true,
        workerTargetFront: "",
        url: "http://example.com/worker.js",
        state: 4,
        stateText: "activated",
      },
    ],
  },
];

const MULTIPLE_WORKER_MIXED_DOMAINS_LIST = [
  {
    id: "id-reg-1-example",
    scope: "SCOPE1",
    registrationFront: "",
    workers: [
      {
        id: "id-worker-1-example",
        isActive: true,
        workerTargetFront: "",
        url: "http://example.com/worker.js",
        state: 4,
        stateText: "activated",
      },
    ],
  },
  {
    id: "id-reg-2-example",
    scope: "SCOPE2",
    registrationFront: "",
    workers: [
      {
        id: "id-worker-2-example",
        isActive: true,
        workerTargetFront: "",
        url: "http://example.com/worker.js",
        state: 4,
        stateText: "activated",
      },
    ],
  },
  {
    id: "id-reg-3-example",
    scope: "SCOPE3",
    registrationFront: "",
    workers: [
      {
        id: "id-worker-3-example",
        isActive: true,
        workerTargetFront: "",
        url: "http://different-example.com/worker.js",
        state: 4,
        stateText: "activated",
      },
    ],
  },
];

// props for a simple manifest
const MANIFEST_SIMPLE = {
  icons: [
    {
      key: { sizes: "1x1", contentType: "image/png" },
      value: { src: "something.png", purpose: "any" },
      type: "icon",
    },
  ],
  identity: [{ key: "name", value: "foo", type: "string" }],
  presentation: [
    { key: "lorem", value: "ipsum", type: "string" },
    { key: "foo", value: "bar", type: "string" },
  ],
  validation: [{ level: "warning", message: "This is a warning" }],
};

// props for a manifest with string values
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

// props for a manifest with icon values
const MANIFEST_ICON_MEMBERS = {
  icons: [
    {
      key: { sizes: "1x1", contentType: "image/png" },
      value: { src: "something.png", purpose: "any" },
      type: "icon",
    },
    {
      key: { sizes: "", contentType: "" },
      value: { src: "something.svg", purpose: "any maskable" },
      type: "icon",
    },
  ],
  identity: [],
  presentation: [],
  validation: [],
};

// props for a manifest with values that have an unrecognized type
const MANIFEST_UNKNOWN_TYPE_MEMBERS = {
  icons: [],
  identity: [{ key: "lorem", value: "ipsum", type: "foo" }],
  presentation: [],
  validation: [],
};

const MANIFEST_WITH_ISSUES = {
  icons: [],
  identity: [{ key: "name", value: "foo", type: "string" }],
  presentation: [
    { key: "lorem", value: "ipsum", type: "string" },
    { key: "foo", value: "bar", type: "string" },
  ],
  validation: [{ level: "warning", message: "This is a warning" }],
};

// props for a manifest with no validation issues
const MANIFEST_NO_ISSUES = {
  icons: [],
  identity: [{ key: "name", value: "foo", type: "string" }],
  presentation: [
    { key: "lorem", value: "ipsum", type: "string" },
    { key: "foo", value: "bar", type: "string" },
  ],
  validation: [],
};

module.exports = {
  // service worker related fixtures
  EMPTY_WORKER_LIST,
  MULTIPLE_WORKER_LIST,
  MULTIPLE_WORKER_MIXED_DOMAINS_LIST,
  REGISTRATION_MULTIPLE_WORKERS,
  REGISTRATION_SINGLE_WORKER,
  SINGLE_WORKER_DEFAULT_DOMAIN_LIST,
  SINGLE_WORKER_DIFFERENT_DOMAIN_LIST,
  WORKER_RUNNING,
  WORKER_STOPPED,
  WORKER_WAITING,
  // manifest related fixtures
  MANIFEST_NO_ISSUES,
  MANIFEST_WITH_ISSUES,
  MANIFEST_SIMPLE,
  MANIFEST_COLOR_MEMBERS,
  MANIFEST_ICON_MEMBERS,
  MANIFEST_STRING_MEMBERS,
  MANIFEST_UNKNOWN_TYPE_MEMBERS,
};
