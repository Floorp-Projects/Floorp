/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "engines": {
    "Firefox": "*"
  }
};

const { Tool } = require("dev/toolbox");
const { Panel } = require("dev/panel");
const { Class } = require("sdk/core/heritage");
const { openToolbox, closeToolbox, getCurrentPanel } = require("dev/utils");
const { MessageChannel } = require("sdk/messaging");
const { when } = require("sdk/dom/events");
const { viewFor } = require("sdk/view/core");
const { createView } = require("dev/panel/view");

const iconURI = "data:image/png;base64,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TUw6Y29tLmFkb2JlLnhtcAAAAAAAPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyIgeDp4bXB0az0iWE1QIENvcmUgNS40LjAiPgogICA8cmRmOlJERiB4bWxuczpyZGY9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkvMDIvMjItcmRmLXN5bnRheC1ucyMiPgogICAgICA8cmRmOkRlc2NyaXB0aW9uIHJkZjphYm91dD0iIgogICAgICAgICAgICB4bWxuczp4bXBNTT0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wL21tLyIKICAgICAgICAgICAgeG1sbnM6c3RSZWY9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9zVHlwZS9SZXNvdXJjZVJlZiMiCiAgICAgICAgICAgIHhtbG5zOnhtcD0iaHR0cDovL25zLmFkb2JlLmNvbS94YXAvMS4wLyI+CiAgICAgICAgIDx4bXBNTTpEb2N1bWVudElEPnhtcC5kaWQ6M0ExNEY4NjZBNkU1MTFFMTlGMkFGQ0QyNTUyN0VDRjY8L3htcE1NOkRvY3VtZW50SUQ+CiAgICAgICAgIDx4bXBNTTpEZXJpdmVkRnJvbSByZGY6cGFyc2VUeXBlPSJSZXNvdXJjZSI+CiAgICAgICAgICAgIDxzdFJlZjppbnN0YW5jZUlEPnhtcC5paWQ6M0ExNEY4NjNBNkU1MTFFMTlGMkFGQ0QyNTUyN0VDRjY8L3N0UmVmOmluc3RhbmNlSUQ+CiAgICAgICAgICAgIDxzdFJlZjpkb2N1bWVudElEPnhtcC5kaWQ6M0ExNEY4NjRBNkU1MTFFMTlGMkFGQ0QyNTUyN0VDRjY8L3N0UmVmOmRvY3VtZW50SUQ+CiAgICAgICAgIDwveG1wTU06RGVyaXZlZEZyb20+CiAgICAgICAgIDx4bXBNTTpJbnN0YW5jZUlEPnhtcC5paWQ6M0ExNEY4NjVBNkU1MTFFMTlGMkFGQ0QyNTUyN0VDRjY8L3htcE1NOkluc3RhbmNlSUQ+CiAgICAgICAgIDx4bXA6Q3JlYXRvclRvb2w+QWRvYmUgUGhvdG9zaG9wIENTNSBNYWNpbnRvc2g8L3htcDpDcmVhdG9yVG9vbD4KICAgICAgPC9yZGY6RGVzY3JpcHRpb24+CiAgIDwvcmRmOlJERj4KPC94OnhtcG1ldGE+ChetDDYAAACaSURBVEgNY2AYboAR3UNnzpx5DxQTQBP/YGJiIogmBuYSq54Ji2Z0S0BKsInBtGKTwxDDZhHMAKrSdLOIqq7GZxjj+fPnBf7+/bseqMgBn0IK5A4wMzMHMtHYEpD7HEB2gOLIAcSjMXCgW2IYtYjsqBwNutGgg4fAaGKABwWpDLoG3QFSXUeG+gNMoEoJqJGWloErPjIcR54WALqPHeiJgl15AAAAAElFTkSuQmCC";
const makeHTML = fn =>
  "data:text/html;charset=utf-8,<script>(" + fn + ")();</script>";


const test = function(unit) {
  return function*(assert) {
    assert.isRendered = (panel, toolbox) => {
      const doc = toolbox.doc;
      assert.ok(doc.querySelector("[value='" + panel.label + "']"),
                "panel.label is found in the developer toolbox DOM");
      assert.ok(doc.querySelector("[tooltiptext='" + panel.tooltip + "']"),
                "panel.tooltip is found in the developer toolbox DOM");

      assert.ok(doc.querySelector("#toolbox-panel-" + panel.id),
                "toolbar panel with a matching id is present");
    };


    yield* unit(assert);
  };
};

exports["test Panel API"] = test(function*(assert) {
  const MyPanel = Class({
    extends: Panel,
    label: "test panel",
    tooltip: "my test panel",
    icon: iconURI,
    url: makeHTML(() => {
      document.documentElement.innerHTML = "hello world";
    }),
    setup: function({debuggee}) {
      this.debuggee = debuggee;
      assert.equal(this.readyState, "uninitialized",
                   "at construction time panel document is not inited");
    },
    dispose: function() {
      delete this.debuggee;
    }
  });
  assert.ok(MyPanel, "panel is defined");

  const myTool = new Tool({
    panels: {
      myPanel: MyPanel
    }
  });
  assert.ok(myTool, "tool is defined");


  var toolbox = yield openToolbox(MyPanel);
  var panel = yield getCurrentPanel(toolbox);
  assert.ok(panel instanceof MyPanel, "is instance of MyPanel");

  assert.isRendered(panel, toolbox);

  if (panel.readyState === "uninitialized") {
    yield panel.ready();
    assert.equal(panel.readyState, "interactive", "panel is ready");
  }

  yield panel.loaded();
  assert.equal(panel.readyState, "complete", "panel is loaded");

  yield closeToolbox();

  assert.equal(panel.readyState, "destroyed", "panel is destroyed");
});


exports["test Panel communication"] = test(function*(assert) {
  const MyPanel = Class({
    extends: Panel,
    label: "communication",
    tooltip: "test palen communication",
    icon: iconURI,
    url: makeHTML(() => {
      window.addEventListener("message", event => {
        if (event.source === window) {
          var port = event.ports[0];
          port.start();
          port.postMessage("ping");;
          port.onmessage = (event) => {
            if (event.data === "pong") {
              port.postMessage("bye");
              port.close();
            }
          };
        }
      });
    }),
    dispose: function() {
      delete this.port;
    }
  });


  const myTool = new Tool({
    panels: {
      myPanel: MyPanel
    }
  });


  const toolbox = yield openToolbox(MyPanel);
  const panel = yield getCurrentPanel(toolbox);
  assert.ok(panel instanceof MyPanel, "is instance of MyPanel");

  assert.isRendered(panel, toolbox);

  yield panel.ready();
  const { port1, port2 } = new MessageChannel();
  panel.port = port1;
  panel.postMessage("connect", [port2]);
  panel.port.start();

  const ping = yield when(panel.port, "message");

  assert.equal(ping.data, "ping", "received ping from panel doc");

  panel.port.postMessage("pong");

  const bye = yield when(panel.port, "message");

  assert.equal(bye.data, "bye", "received bye from panel doc");

  panel.port.close();

  yield closeToolbox();

  assert.equal(panel.readyState, "destroyed", "panel is destroyed");
});

exports["test communication with debuggee"] = test(function*(assert) {
  const MyPanel = Class({
    extends: Panel,
    label: "debuggee",
    tooltip: "test debuggee",
    icon: iconURI,
    url: makeHTML(() => {
      window.addEventListener("message", event => {
        if (event.source === window) {
          var debuggee = event.ports[0];
          var port = event.ports[1];
          debuggee.start();
          port.start();


          debuggee.onmessage = (event) => {
            port.postMessage(event.data);
          };
          port.onmessage = (event) => {
            debuggee.postMessage(event.data);
          };
        }
      });
    }),
    setup: function({debuggee}) {
      this.debuggee = debuggee;
    },
    onReady: function() {
      const { port1, port2 } = new MessageChannel();
      this.port = port1;
      this.port.start();
      this.debuggee.start();

      this.postMessage("connect", [this.debuggee, port2]);
    },
    dispose: function() {
      this.port.close();
      this.debuggee.close();

      delete this.port;
      delete this.debuggee;
    }
  });


  const myTool = new Tool({
    panels: {
      myPanel: MyPanel
    }
  });


  const toolbox = yield openToolbox(MyPanel);
  const panel = yield getCurrentPanel(toolbox);
  assert.ok(panel instanceof MyPanel, "is instance of MyPanel");

  assert.isRendered(panel, toolbox);

  yield panel.ready();
  const intro = yield when(panel.port, "message");

  assert.equal(intro.data.from, "root", "intro message from root");

  panel.port.postMessage({
    to: "root",
    type: "echo",
    text: "ping"
  });

  const pong = yield when(panel.port, "message");

  assert.deepEqual(pong.data, {
    to: "root",
    from: "root",
    type: "echo",
    text: "ping"
  }, "received message back from root");

  yield closeToolbox();

  assert.equal(panel.readyState, "destroyed", "panel is destroyed");
});


exports["test viewFor panel"] = test(function*(assert) {
  const url = "data:text/html;charset=utf-8,viewFor";
  const MyPanel = Class({
    extends: Panel,
    label: "view for panel",
    tooltip: "my panel view",
    icon: iconURI,
    url: url
  });

  const myTool = new Tool({
    panels: {
      myPanel: MyPanel
    }
  });


  const toolbox = yield openToolbox(MyPanel);
  const panel = yield getCurrentPanel(toolbox);
  assert.ok(panel instanceof MyPanel, "is instance of MyPanel");

  const frame = viewFor(panel);

  assert.equal(frame.nodeName.toLowerCase(), "iframe",
               "viewFor(panel) returns associated iframe");

  yield panel.loaded();

  assert.equal(frame.contentDocument.URL, url, "is expected iframe");

  yield closeToolbox();
});


exports["test createView panel"] = test(function*(assert) {
  var frame = null;
  var panel = null;

  const url = "data:text/html;charset=utf-8,createView";
  const id = Math.random().toString(16).substr(2);
  const MyPanel = Class({
    extends: Panel,
    label: "create view",
    tooltip: "panel creator",
    icon: iconURI,
    url: url
  });

  createView.define(MyPanel, (instance, document) => {
    var view = document.createElement("iframe");
    view.setAttribute("type", "content");

    // save instances for later asserts
    frame = view;
    panel = instance;

    return view;
  });

  const myTool = new Tool({
    panels: {
      myPanel: MyPanel
    }
  });


  const toolbox = yield openToolbox(MyPanel);
  const myPanel = yield getCurrentPanel(toolbox);

  assert.equal(myPanel, panel,
               "panel passed to createView is one instantiated");
  assert.equal(viewFor(panel), frame,
               "createView has created an iframe");

  yield panel.loaded();

  assert.equal(frame.contentDocument.URL, url, "is expected iframe");

  yield closeToolbox();
});


require("test").run(exports);

