/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "engines": {
    "Firefox": "*"
  }
};

const { Frame } = require("sdk/ui/frame");
const { Toolbar } = require("sdk/ui/toolbar");
const { Loader } = require("sdk/test/loader");
const { identify } = require("sdk/ui/id");
const { setTimeout } = require("sdk/timers");
const { getMostRecentBrowserWindow, open } = require("sdk/window/utils");
const { ready, loaded, close } = require("sdk/window/helpers");
const { defer } = require("sdk/core/promise");
const { send } = require("sdk/event/utils");
const { object } = require("sdk/util/sequence");
const { OutputPort } = require("sdk/output/system");
const output = new OutputPort({ id: "toolbar-change" });

const wait = (toolbar, event) => {
  let { promise, resolve } = defer();
  toolbar.once(event, resolve);
  return promise;
};

const stateEventsFor = frame =>
  [wait(frame, "attach"), wait(frame, "ready"),
   wait(frame, "load"), wait(frame, "detach")];


const isAttached = ({id}, window=getMostRecentBrowserWindow()) =>
  !!window.document.getElementById(id);

exports["test frame API"] = function*(assert) {
  const url = "data:text/html,frame-api";
  assert.throws(() => new Frame(),
                /The `options.url`/,
                "must provide url");

  assert.throws(() => new Frame({ url: "http://mozilla.org/" }),
                /The `options.url`/,
                "options.url must be local url");

  assert.throws(() => new Frame({url: url, name: "4you" }),
                /The `option.name` must be a valid/,
                "can only take valid names");

  const f1 = new Frame({ url: url });

  assert.ok(f1.id, "frame has an id");
  assert.equal(f1.url, void(0), "frame has no url until it's loaded");
  assert.equal(typeof(f1.postMessage), "function",
               "frames can postMessages");

  const p1 = wait(f1, "register");

  assert.throws(() => new Frame({ url: url }),
                /Frame with this id already exists/,
                "can't have two identical frames");


  const f2 = new Frame({ name: "frame-2", url: url });
  assert.pass("can create frame with same url but diff name");
  const p2 = wait(f2, "register");

  yield p1;
  assert.pass("frame#1 was registered");
  assert.equal(f1.url, url, "once registered it get's url");

  yield p2;
  assert.pass("frame#2 was registered");
  assert.equal(f2.url, url, "once registered it get's url");

  f1.destroy();
  const f3 = new Frame({ url: url });
  assert.pass("frame identical to destroyed one can be created");

  yield wait(f3, "register");
  assert.equal(f3.url, url, "url is set");
  f2.destroy();
  f3.destroy();
};


exports["test frame in toolbar"] = function*(assert) {
  const assertEvent = (event, type) => {
    assert.ok(event, "`" + type + "` event was dispatched");
    assert.equal(event.type, type, "event.type is: " + type);
    assert.equal(typeof(event.source), "object",
                 "event.source is an object");
    assert.equal(typeof(event.source.postMessage), "function",
                 "messages can be posted to event.source");
  };


  const url = "data:text/html,toolbar-frame";
  const f1 = new Frame({ url: url });
  const t1 = new Toolbar({
    title: "frame toolbar",
    items: [f1]
  });

  const w1 = getMostRecentBrowserWindow();
  const [a1, r1, l1] = stateEventsFor(f1);

  assertEvent((yield a1), "attach");
  assert.ok(isAttached(f1, w1), "frame is in the window#1");
  assertEvent((yield r1), "ready");
  assertEvent((yield l1), "load");

  const [a2, r2, l2] = stateEventsFor(f1);
  const w2 = open();


  assertEvent((yield a2), "attach");
  assert.ok(isAttached(f1, w2), "frame is in the window#2");
  assertEvent((yield r2), "ready");
  assertEvent((yield l2), "load");
  assert.pass("frame attached to window#2");


  const d1 = wait(f1, "detach");
  yield close(w2);

  assertEvent((yield d1), "detach");
  assert.pass("frame detached when window is closed");

  t1.destroy();

  assertEvent((yield wait(f1, "detach")), "detach");
  assert.ok(!isAttached(f1, w1), "frame was removed from window#1");
  assert.pass("toolbar destroy detaches frame");
};


exports["test host to content messaging"] = function*(assert) {
  const url = "data:text/html,<script>new " + function() {
    window.addEventListener("message", (event) => {
      if (event.data === "ping!")
        event.source.postMessage("pong!", event.origin);
    });
  } + "</script>";
  const f1 = new Frame({ name: "mailbox", url: url });
  const t1 = new Toolbar({ title: "mailbox", items: [f1] });

  const e1 = yield wait(f1, "ready");
  e1.source.postMessage("ping!", e1.origin);

  const pong = yield wait(f1, "message");
  assert.equal(pong.data, "pong!", "received ping back");
  t1.destroy();

  yield wait(t1, "detach");
};


exports["test content to host messaging"] = function*(assert) {
  const url = "data:text/html,<script>new " + function() {
    window.addEventListener("message", (event) => {
      if (event.data === "pong!")
        event.source.postMessage("end", event.origin);
    });

    window.parent.postMessage("ping!", "*");
  } + "</script>";

  const f1 = new Frame({ name: "inbox", url: url });
  const t1 = new Toolbar({ title: "inbox", items: [f1] });

  const e1 = yield wait(f1, "message");
  assert.equal(e1.data, "ping!", "received ping from content");

  e1.source.postMessage("pong!", e1.origin);

  const e2 = yield wait(f1, "message");
  assert.equal(e2.data, "end", "received end message");

  t1.destroy();
  yield wait(t1, "detach");
};


exports["test direct messaging"] = function*(assert) {
  const url = "data:text/html,<script>new " + function() {
    var n = 0;
    window.addEventListener("message", (event) => {
      if (event.data === "inc")
        n = n + 1;
      if (event.data === "print")
        event.source.postMessage({ n: n }, event.origin);
    });
  } + "</script>";

  const w1 = getMostRecentBrowserWindow();
  const f1 = new Frame({ url: url, name: "mail-cluster" });
  const t1 = new Toolbar({ title: "claster", items: [f1] });

  yield wait(f1, "ready");
  assert.pass("document loaded in window#1");

  const w2 = open();

  yield wait(f1, "ready");
  assert.pass("document loaded in window#2");

  f1.postMessage("inc", f1.origin);
  f1.postMessage("print", f1.origin);

  const e1 = yield wait(f1, "message");
  assert.deepEqual(e1.data, {n: 1}, "received message from window#1");

  const e2 = yield wait(f1, "message");
  assert.deepEqual(e2.data, {n: 1}, "received message from window#2");

  e1.source.postMessage("inc", e1.origin);

  e1.source.postMessage("print", e1.origin);

  const e3 = yield wait(f1, "message");
  assert.deepEqual(e3.data, {n: 2}, "state changed in window#1");

  e2.source.postMessage("print", e2.origin);
  const e4 = yield wait(f1, "message");

  assert.deepEqual(e2.data, {n:1}, "window#2 didn't received inc message");

  yield close(w2);
  t1.destroy();

  yield wait(t1, "detach");
};

require("sdk/test").run(exports);
