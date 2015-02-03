/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "engines": {
    "Firefox": "*"
  }
};

const { Toolbar } = require("sdk/ui/toolbar");
const { Loader } = require("sdk/test/loader");
const { identify } = require("sdk/ui/id");
const { getMostRecentBrowserWindow, open, getOuterId } = require("sdk/window/utils");
const { ready, close } = require("sdk/window/helpers");
const { defer } = require("sdk/core/promise");
const { send, stop, Reactor } = require("sdk/event/utils");
const { object } = require("sdk/util/sequence");
const { CustomizationInput } = require("sdk/input/customizable-ui");
const { OutputPort } = require("sdk/output/system");
const output = new OutputPort({ id: "toolbar-change" });
const { cleanUI } = require('sdk/test/utils');
const tabs = require("sdk/tabs");

const wait = (toolbar, event) => {
  let { promise, resolve } = defer();
  toolbar.once(event, resolve);
  return promise;
};

const show = ({id}) => send(output, object([id, {collapsed: false}]));
const hide = ({id}) => send(output, object([id, {collapsed: true}]));
const retitle = ({id}, title) => send(output, object([id, {title: title}]));

const isAttached = ({id}, window=getMostRecentBrowserWindow()) =>
  !!window.document.getElementById(id);

const isCollapsed = ({id}, window=getMostRecentBrowserWindow()) =>
  window.document.getElementById(id).getAttribute("collapsed") === "true";

const closeViaButton = ({id}, window=getMostRecentBrowserWindow()) =>
  window.document.getElementById("close-" + id).click();

const readTitle = ({id}, window=getMostRecentBrowserWindow()) =>
  window.document.getElementById(id).getAttribute("toolbarname");

exports["test toolbar API"] = function*(assert) {
  assert.throws(() => new Toolbar(),
                /The `option.title`/,
                "toolbar requires title");

  assert.throws(() => new Toolbar({ hidden: false }),
                /The `option.title`/,
                "toolbar requires title");

  const t1 = new Toolbar({ title: "foo" });

  assert.throws(() => new Toolbar({ title: "foo" }),
                /already exists/,
                "can't create identical toolbars");

  assert.ok(t1.id, "toolbar has an id");
  assert.equal(t1.id, identify(t1), "identify returns toolbar id");
  assert.deepEqual(t1.items, [], "toolbar items are empty");
  assert.equal(t1.title, void(0), "title is void until attached");
  assert.equal(t1.hidden, void(0), "hidden is void until attached");

  yield wait(t1, "attach");

  assert.equal(t1.title, "foo", "title is set after attach");
  assert.equal(t1.hidden, false, "by default toolbar isn't hidden");

  assert.throws(() => new Toolbar({ title: "foo" }),
                /already exists/,
                "still can't create identical toolbar");


  const t2 = new Toolbar({ title: "bar", hidden: true });
  assert.pass("can create different toolbar though");

  assert.ok(t2.id, "toolbar has an id");
  assert.equal(t2.id, identify(t2), "identify returns toolbar id");
  assert.notEqual(t2.id, t1.id, "each toolbar has unique id");

  yield wait(t2, "attach");

  assert.equal(t2.title, "bar", "title is set after attach");
  assert.equal(t2.hidden, true, "toolbar is hidden as specified");

  t2.destroy();
  t1.destroy();

  yield wait(t1, "detach");

  assert.equal(t1.title, void(0), "title is voided after detach");
  assert.equal(t1.hidden, void(0), "hidden is void fater detach");


  const t3 = new Toolbar({ title: "foo" });
  assert.pass("Can create toolbar after identical was detached");

  assert.equal(t3.id, t1.id, "toolbar has a same id");
  assert.equal(t3.id, identify(t3), "identify returns toolbar.id");
  assert.equal(t3.title, void(0), "title is void before attach");
  assert.equal(t3.hidden, void(0), "hidden is void before attach");

  yield wait(t3, "attach");

  assert.equal(t3.title, "foo", "title is set");
  assert.equal(t3.hidden, false, "toolbar is hidden");

  t3.destroy();

  yield wait(t3, "detach");
};

exports["test show / hide toolbar"] = function*(assert) {
  const t1 = new Toolbar({ title: "foo" });

  yield wait(t1, "attach");

  assert.equal(t1.title, "foo", "title is set after attach");
  assert.equal(t1.hidden, false, "by default toolbar isn't hidden");
  assert.ok(isAttached(t1), "toolbar was actually attarched");
  assert.ok(!isCollapsed(t1), "toolbar isn't collapsed");

  hide(t1);
  assert.equal(t1.hidden, false, "not hidden yet");

  yield wait(t1, "hide");
  assert.equal(t1.hidden, true, "toolbar got hidden");
  assert.ok(isCollapsed(t1), "toolbar is collapsed");

  show(t1);

  yield wait(t1, "show");
  assert.equal(t1.hidden, false, "toolbar got shown");
  assert.ok(!isCollapsed(t1), "toolbar isn't collapsed");

  t1.destroy();
  yield wait(t1, "detach");
  assert.ok(!isAttached(t1), "toolbar is no longer attached");
};

exports["test multiple windows & toolbars"] = function*(assert) {
  const w1 = getMostRecentBrowserWindow();
  const t1 = new Toolbar({ title: "multi window" });

  yield wait(t1, "attach");

  assert.equal(t1.title, "multi window", "title is set after attach");
  assert.equal(t1.hidden, false, "by default toolbar isn't hidden");
  assert.ok(isAttached(t1, w1), "toolbar was actually attarched");
  assert.ok(!isCollapsed(t1, w1), "toolbar isn't collapsed");

  const w2 = open();
  yield ready(w2);

  assert.ok(isAttached(t1, w2), "toolbar was attached to second window");
  assert.ok(!isCollapsed(t1, w2), "toolbar isn't collabsed");

  hide(t1);
  yield wait(t1, "hide");

  assert.ok(isCollapsed(t1, w1) && isCollapsed(t1, w2),
            "toolbar is collabsed");


  const w3 = open();
  yield ready(w3);

  assert.ok(isAttached(t1, w1) && isAttached(t1, w2) && isAttached(t1, w3),
            "toolbar is attached to all windows");
  assert.ok(isCollapsed(t1, w3) && isCollapsed(t1, w3) && isCollapsed(t1, w3),
            "toolbar still collapsed in all windows");


  const t2 = new Toolbar({ title: "multi hidden", hidden: true });

  yield wait(t2, "attach");

  assert.equal(t2.title, "multi hidden", "title is set after attach");
  assert.equal(t2.hidden, true, "isn't hidden as specified");

  assert.ok(isAttached(t1, w1) && isAttached(t1, w2) && isAttached(t1, w3),
            "toolbar#1 is still attached");
  assert.ok(isAttached(t2, w1) && isAttached(t2, w2) && isAttached(t2, w3),
            "toolbar#2 was attached to all windows");

  assert.ok(isCollapsed(t1, w1) && isCollapsed(t1, w2) && isCollapsed(t1, w3),
            "toolbar#1 is still collapsed");

  assert.ok(isCollapsed(t2, w1) && isCollapsed(t2, w2) && isCollapsed(t2, w3),
            "toolbar#2 is collapsed");

  t1.destroy();
  yield wait(t1, "detach");

  assert.ok(!isAttached(t1, w1) && !isAttached(t1, w2) && !isAttached(t1, w3),
            "toolbar#1 was detached from all windows");
  assert.ok(isAttached(t2, w1) && isAttached(t2, w2) && isAttached(t2, w3),
            "toolbar#2 is still attached to all windows");

  yield close(w2);

  assert.ok(isAttached(t2, w1) && isAttached(t2, w3),
            "toolbar#2 is still attached to remaining windows");
  assert.ok(isCollapsed(t2, w1) && isCollapsed(t2, w3),
            "toolbar#2 is still collapsed");

  show(t2);
  yield wait(t2, "show");

  assert.ok(!isCollapsed(t2, w1) && !isCollapsed(t2, w3),
            "toolbar#2 is not collapsed");

  yield close(w3);

  assert.ok(isAttached(t2, w1), "still attached to last window");
  assert.ok(!isCollapsed(t2, w1), "still isn't collapsed");

  t2.destroy();
  yield wait(t2, "detach");

  assert.ok(!isAttached(t2, w1), "toolbar was removed");

  yield cleanUI();
};

exports["test toolbar persistence"] = function*(assert) {
  const t1 = new Toolbar({ title: "per sist ence" });

  yield wait(t1, "attach");

  assert.equal(t1.hidden, false, "toolbar is visible");

  hide(t1);
  yield wait(t1, "hide");

  assert.equal(t1.hidden, true, "toolbar is hidden");
  assert.ok(isCollapsed(t1), "toolbar is collapsed");

  t1.destroy();

  yield wait(t1, "detach");

  const t2 = new Toolbar({ title: "per sist ence" });

  yield wait(t2, "attach");

  assert.equal(t2.hidden, true, "toolbar persisted state");
  assert.ok(isCollapsed(t2), "toolbar is collapsed");

  show(t2);
  t2.destroy();

  yield wait(t2, "detach");

  const t3 = new Toolbar({ title: "per sist ence", hidden: true });

  yield wait(t3, "attach");

  assert.equal(t3.hidden, false, "toolbar persisted state & ignored option");
  assert.ok(!isCollapsed(t3), "toolbar isn1t collapsed");

  t3.destroy();

  yield wait(t3, "detach");

  yield cleanUI();
};


exports["test toolbar unload"] = function*(assert) {
  // We override add-on id, otherwise two instances of Toolbar host (view.js)
  // handling same updates, cause message port is bound to add-on id.
  const loader = Loader(module, null, null, {id: "toolbar-unload-addon"});
  const { Toolbar } = loader.require("sdk/ui/toolbar");

  const w1 = getMostRecentBrowserWindow();
  const w2 = open();

  yield ready(w2);

  const t1 = new Toolbar({ title: "unload" });

  yield wait(t1, "attach");

  assert.ok(isAttached(t1, w1) && isAttached(t1, w2),
            "attached to both windows");


  loader.unload();


  assert.ok(!isAttached(t1, w1) && !isAttached(t1, w2),
            "detached from both windows on unload");

  yield cleanUI();
};

exports["test toolbar close button"] = function*(assert) {
  const t1 = new Toolbar({ title: "close with button" });

  yield wait(t1, "attach");
  const w1 = getMostRecentBrowserWindow();
  const w2 = open();

  yield ready(w2);

  assert.ok(!isCollapsed(t1, w1) && !isCollapsed(t1, w2),
            "toolbar isn't collapsed");

  closeViaButton(t1);

  yield wait(t1, "hide");

  assert.ok(isCollapsed(t1, w1) && isCollapsed(t1, w2),
            "toolbar was collapsed");

  t1.destroy();
  yield wait(t1, "detach");
  yield cleanUI();
};

exports["test title change"] = function*(assert) {
  const w1 = getMostRecentBrowserWindow();
  const w2 = open();

  yield ready(w2);

  const t1 = new Toolbar({ title: "first title" });
  const id = t1.id;

  yield wait(t1, "attach");


  assert.equal(t1.title, "first title",
               "correct title is set");
  assert.equal(readTitle(t1, w1), "first title",
               "title set in the view of first window");
  assert.equal(readTitle(t1, w2), "first title",
               "title set in the view of second window");

  retitle(t1, "second title");

  // Hide & show so to make sure changes go through a round
  // loop.
  hide(t1);
  yield wait(t1, "hide");
  show(t1);
  yield wait(t1, "show");

  assert.equal(t1.id, id, "id remains same");
  assert.equal(t1.title, "second title", "instance title was updated");
  assert.equal(readTitle(t1, w1), "second title",
               "title updated in first window");
  assert.equal(readTitle(t1, w2), "second title",
               "title updated in second window");

  t1.destroy();
  yield wait(t1, "detach");
  yield cleanUI();
};

exports["test toolbar is not customizable"] = function*(assert, done) {
  const { window, document, gCustomizeMode } = getMostRecentBrowserWindow();
  const outerId = getOuterId(window);
  const input = new CustomizationInput();
  const customized = defer();
  const customizedEnd = defer();

  // open a new tab so that the customize tab replaces it
  // and does not replace the start tab.
  yield new Promise(resolve => {
    tabs.open({
      url: "about:blank",
      onReady: resolve
    });
  });

  new Reactor({ onStep: value => {
    if (value[outerId] === true)
      customized.resolve();
    if (value[outerId] === null)
      customizedEnd.resolve();
  }}).run(input);

  const toolbar = new Toolbar({ title: "foo" });
  yield wait(toolbar, "attach");

  let view = document.getElementById(toolbar.id);
  let label = view.querySelector("label");
  let inner = view.querySelector("toolbar");

  assert.equal(view.getAttribute("customizable"), "false",
    "The outer toolbar is not customizable.");

  assert.ok(label.collapsed,
    "The label is not displayed.")

  assert.equal(inner.getAttribute("customizable"), "true",
    "The inner toolbar is customizable.");

  assert.equal(window.getComputedStyle(inner).visibility, "visible",
    "The inner toolbar is visible.");

  // Enter in customization mode
  gCustomizeMode.toggle();

  yield customized.promise;

  assert.equal(view.getAttribute("customizable"), "false",
    "The outer toolbar is not customizable.");

  assert.equal(label.collapsed, false,
    "The label is displayed.")

  assert.equal(inner.getAttribute("customizable"), "true",
    "The inner toolbar is customizable.");

  assert.equal(window.getComputedStyle(inner).visibility, "hidden",
    "The inner toolbar is hidden.");

  // Exit from customization mode
  gCustomizeMode.toggle();

  yield customizedEnd.promise;

  assert.equal(view.getAttribute("customizable"), "false",
    "The outer toolbar is not customizable.");

  assert.ok(label.collapsed,
    "The label is not displayed.")

  assert.equal(inner.getAttribute("customizable"), "true",
    "The inner toolbar is customizable.");

  assert.equal(window.getComputedStyle(inner).visibility, "visible",
    "The inner toolbar is visible.");

  toolbar.destroy();

  yield cleanUI();
};

exports["test button are attached to toolbar"] = function*(assert) {
  const { document } = getMostRecentBrowserWindow();
  const { ActionButton, ToggleButton } = require("sdk/ui");
  const { identify } = require("sdk/ui/id");

  let action = ActionButton({
    id: "btn-1",
    label: "action",
    icon: "./placeholder.png"
  });

  let toggle = ToggleButton({
    id: "btn-2",
    label: "toggle",
    icon: "./placeholder.png"
  });

  const toolbar = new Toolbar({
    title: "foo",
    items: [action, toggle]
  });

  yield wait(toolbar, "attach");

  let actionNode = document.getElementById(identify(action));
  let toggleNode = document.getElementById(identify(toggle));

  assert.notEqual(actionNode, null,
    "action button exists in the document");

  assert.notEqual(actionNode, null,
    "action button exists in the document");

  assert.notEqual(toggleNode, null,
    "toggle button exists in the document");

  assert.equal(actionNode.nextElementSibling, toggleNode,
    "action button is placed before toggle button");

  assert.equal(actionNode.parentNode.parentNode.id, toolbar.id,
    "buttons are placed in the correct toolbar");

  toolbar.destroy();

  yield cleanUI();
};

require("sdk/test").run(module.exports);
