/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { VariablesView } = ChromeUtils.importESModule(
  "resource://devtools/client/storage/VariablesView.sys.mjs"
);

const PENDING = {
  type: "object",
  class: "Promise",
  actor: "conn0.obj35",
  extensible: true,
  frozen: false,
  sealed: false,
  promiseState: {
    state: "pending",
  },
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
};

const FULFILLED = {
  type: "object",
  class: "Promise",
  actor: "conn0.obj35",
  extensible: true,
  frozen: false,
  sealed: false,
  promiseState: {
    state: "fulfilled",
    value: 10,
  },
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
};

const REJECTED = {
  type: "object",
  class: "Promise",
  actor: "conn0.obj35",
  extensible: true,
  frozen: false,
  sealed: false,
  promiseState: {
    state: "rejected",
    reason: 10,
  },
  preview: {
    kind: "Object",
    ownProperties: {},
    ownPropertiesLength: 0,
    safeGetterValues: {},
  },
};

function run_test() {
  equal(VariablesView.getString(PENDING, { concise: true }), "Promise");
  equal(VariablesView.getString(PENDING), 'Promise {<state>: "pending"}');

  equal(VariablesView.getString(FULFILLED, { concise: true }), "Promise");
  equal(
    VariablesView.getString(FULFILLED),
    'Promise {<state>: "fulfilled", <value>: 10}'
  );

  equal(VariablesView.getString(REJECTED, { concise: true }), "Promise");
  equal(
    VariablesView.getString(REJECTED),
    'Promise {<state>: "rejected", <reason>: 10}'
  );
}
