/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var { Assert } = require("resource://testing-common/Assert.jsm");
var { gDevTools } = require("devtools/client/framework/devtools");
var { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});
var flags = require("devtools/shared/flags");
var { Task } = require("devtools/shared/task");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");

flags.testing = true;
var { require: browserRequire } = BrowserLoader({
  baseURI: "resource://devtools/client/shared/",
  window
});

let ReactDOM = browserRequire("devtools/client/shared/vendor/react-dom");
let React = browserRequire("devtools/client/shared/vendor/react");
var TestUtils = React.addons.TestUtils;

function renderComponent(component, props) {
  const el = React.createElement(component, props, {});
  // By default, renderIntoDocument() won't work for stateless components, but
  // it will work if the stateless component is wrapped in a stateful one.
  // See https://github.com/facebook/react/issues/4839
  const wrappedEl = React.DOM.span({}, [el]);
  const renderedComponent = TestUtils.renderIntoDocument(wrappedEl);
  return ReactDOM.findDOMNode(renderedComponent).children[0];
}

function shallowRenderComponent(component, props) {
  const el = React.createElement(component, props);
  const renderer = TestUtils.createRenderer();
  renderer.render(el, {});
  return renderer.getRenderOutput();
}

/**
 * Test that a rep renders correctly across different modes.
 */
function testRepRenderModes(modeTests, testName, componentUnderTest, gripStub,
  props = {}) {
  modeTests.forEach(({mode, expectedOutput, message}) => {
    const modeString = typeof mode === "undefined" ? "no mode" : mode.toString();
    if (!message) {
      message = `${testName}: ${modeString} renders correctly.`;
    }

    const rendered = renderComponent(
      componentUnderTest.rep,
      Object.assign({}, { object: gripStub, mode }, props)
    );
    is(rendered.textContent, expectedOutput, message);
  });
}
