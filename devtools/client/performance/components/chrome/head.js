/* Any copyright is dedicated to the Public Domain.
    yield new Promise(function(){});
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* global window, document, SimpleTest, requestAnimationFrame, is, ok */
/* exported Cc, Ci, Cu, Cr, Assert, Task, Toolbox, browserRequire,
   forceRender, setProps, dumpn, checkOptimizationHeader, checkOptimizationTree */
const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
let { Assert } = require("resource://testing-common/Assert.jsm");
const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);
const flags = require("devtools/shared/flags");
let { Toolbox } = require("devtools/client/framework/toolbox");

let { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/performance/",
  window,
});
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

const $ = (selector, scope = document) => scope.querySelector(selector);
const $$ = (selector, scope = document) => scope.querySelectorAll(selector);

function forceRender(comp) {
  return setState(comp, {}).then(() => setState(comp, {}));
}

// All tests are asynchronous.
SimpleTest.waitForExplicitFinish();

function onNextAnimationFrame(fn) {
  return () => requestAnimationFrame(() => requestAnimationFrame(fn));
}

function setState(component, newState) {
  return new Promise(resolve => {
    component.setState(newState, onNextAnimationFrame(resolve));
  });
}

function setProps(component, newState) {
  return new Promise(resolve => {
    component.setProps(newState, onNextAnimationFrame(resolve));
  });
}

function dumpn(msg) {
  dump(`PERFORMANCE-COMPONENT-TEST: ${msg}\n`);
}

/**
 * Default opts data for testing. First site has a simple IonType,
 * and an IonType with an ObservedType, and a successful outcome.
 * Second site does not have a successful outcome.
 */
const OPTS_DATA_GENERAL = [
  {
    id: 1,
    propertyName: "my property name",
    line: 100,
    column: 200,
    samples: 90,
    data: {
      attempts: [
        {
          id: 1,
          strategy: "GetElem_TypedObject",
          outcome: "AccessNotTypedObject",
        },
        { id: 1, strategy: "GetElem_Dense", outcome: "AccessNotDense" },
        { id: 1, strategy: "GetElem_TypedStatic", outcome: "Disabled" },
        { id: 1, strategy: "GetElem_TypedArray", outcome: "GenericSuccess" },
      ],
      types: [
        {
          id: 1,
          site: "Receiver",
          mirType: "Object",
          typeset: [
            {
              id: 1,
              keyedBy: "constructor",
              name: "MyView",
              location: "http://internet.com/file.js",
              line: "123",
            },
          ],
        },
        {
          id: 1,
          typeset: void 0,
          site: "Index",
          mirType: "Int32",
        },
      ],
    },
  },
  {
    id: 2,
    propertyName: void 0,
    line: 50,
    column: 51,
    samples: 100,
    data: {
      attempts: [
        { id: 2, strategy: "Call_Inline", outcome: "CantInlineBigData" },
      ],
      types: [
        {
          id: 2,
          site: "Call_Target",
          mirType: "Object",
          typeset: [
            { id: 2, keyedBy: "primitive" },
            {
              id: 2,
              keyedBy: "constructor",
              name: "B",
              location: "http://mypage.com/file.js",
              line: "2",
            },
            {
              id: 2,
              keyedBy: "constructor",
              name: "C",
              location: "http://mypage.com/file.js",
              line: "3",
            },
            {
              id: 2,
              keyedBy: "constructor",
              name: "D",
              location: "http://mypage.com/file.js",
              line: "4",
            },
          ],
        },
      ],
    },
  },
];

OPTS_DATA_GENERAL.forEach(site => {
  site.data.types.forEach(type => {
    if (type.typeset) {
      type.typeset.id = site.id;
    }
  });
  site.data.attempts.id = site.id;
  site.data.types.id = site.id;
});

function checkOptimizationHeader(name, file, line) {
  is(
    $(".optimization-header .header-function-name").textContent,
    name,
    "correct optimization header function name"
  );
  is(
    $(".optimization-header .frame-link-filename").textContent,
    file,
    "correct optimization header file name"
  );
  is(
    $(".optimization-header .frame-link-line").textContent,
    `:${line}`,
    "correct optimization header line"
  );
}

function checkOptimizationTree(rowData) {
  const rows = $$(".tree .tree-node");

  for (let i = 0; i < rowData.length; i++) {
    const row = rows[i];
    const expected = rowData[i];

    switch (expected.type) {
      case "site":
        is(
          $(".optimization-site-title", row).textContent,
          `${expected.strategy} – (${expected.samples} samples)`,
          `row ${i}th: correct optimization site row`
        );

        is(
          !!$(".opt-icon.warning", row),
          !!expected.failureIcon,
          `row ${i}th: expected visibility of failure icon for unsuccessful outcomes`
        );
        break;
      case "types":
        is(
          $(".optimization-types", row).textContent,
          `Types (${expected.count})`,
          `row ${i}th: correct types row`
        );
        break;
      case "attempts":
        is(
          $(".optimization-attempts", row).textContent,
          `Attempts (${expected.count})`,
          `row ${i}th: correct attempts row`
        );
        break;
      case "type":
        is(
          $(".optimization-ion-type", row).textContent,
          `${expected.site}:${expected.mirType}`,
          `row ${i}th: correct ion type row`
        );
        break;
      case "observedtype":
        is(
          $(".optimization-observed-type-keyed", row).textContent,
          expected.name
            ? `${expected.keyedBy} → ${expected.name}`
            : expected.keyedBy,
          `row ${i}th: correct observed type row`
        );
        break;
      case "attempt":
        is(
          $(".optimization-strategy", row).textContent,
          expected.strategy,
          `row ${i}th: correct attempt row, attempt item`
        );
        is(
          $(".optimization-outcome", row).textContent,
          expected.outcome,
          `row ${i}th: correct attempt row, outcome item`
        );
        ok(
          $(".optimization-outcome", row).classList.contains(
            expected.success ? "success" : "failure"
          ),
          `row ${i}th: correct attempt row, failure/success status`
        );
        break;
    }
  }
}
