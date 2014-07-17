/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};


const { Cu } = require("chrome");
const { Class } = require("../sdk/core/heritage");
const { curry } = require("../sdk/lang/functional");
const { EventTarget } = require("../sdk/event/target");
const { Disposable, setup, dispose } = require("../sdk/core/disposable");
const { emit, off, setListeners } = require("../sdk/event/core");
const { when } = require("../sdk/event/utils");
const { getFrameElement } = require("../sdk/window/utils");
const { contract, validate } = require("../sdk/util/contract");
const { data: { url: resolve }} = require("../sdk/self");
const { identify } = require("../sdk/ui/id");
const { isLocalURL, URL } = require("../sdk/url");
const { defer } = require("../sdk/core/promise");
const { encode } = require("../sdk/base64");
const { marshal, demarshal } = require("./ports");
const { fromTarget } = require("./debuggee");
const { removed } = require("../sdk/dom/events");
const { id: addonID } = require("../sdk/self");

const OUTER_FRAME_URI = module.uri.replace(/\.js$/, ".html");
const FRAME_SCRIPT = module.uri.replace("/panel.js", "/frame-script.js");
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HTML_NS = "http://www.w3.org/1999/xhtml";

const makeID = name =>
  ("dev-panel-" + addonID + "-" + name).
  split("/").join("-").
  split(".").join("-").
  split(" ").join("-").
  replace(/[^A-Za-z0-9_\-]/g, "");


// Weak mapping between `Panel` instances and their frame's
// `nsIMessageManager`.
const managers = new WeakMap();
// Return `nsIMessageManager` for the given `Panel` instance.
const managerFor = x => managers.get(x);

// Weak mappinging between iframe's and their owner
// `Panel` instances.
const panels = new WeakMap();
const panelFor = frame => panels.get(frame);

// Weak mapping between panels and debugees they're targeting.
const debuggees = new WeakMap();
const debuggeeFor = panel => debuggees.get(panel);

const setAttributes = (node, attributes) => {
  for (var key in attributes)
    node.setAttribute(key, attributes[key]);
};

const onStateChange = ({target, data}) => {
  const panel = panelFor(target);
  panel.readyState = data.readyState;
  emit(panel, data.type, { target: panel, type: data.type });
};

// port event listener on the message manager that demarshalls
// and forwards to the actual receiver. This is a workaround
// until Bug 914974 is fixed.
const onPortMessage = ({data, target}) => {
  const port = demarshal(target, data.port);
  if (port)
    port.postMessage(data.message);
};

// When frame is removed from the toolbox destroy panel
// associated with it to release all the resources.
const onFrameRemove = frame => {
  panelFor(frame).destroy();
};

const onFrameInited = frame => {
  frame.style.visibility = "visible";
}

const inited = frame => new Promise(resolve => {
  const { messageManager } = frame.frameLoader;
  const listener = message => {
    messageManager.removeMessageListener("sdk/event/ready", listener);
    resolve(frame);
  };
  messageManager.addMessageListener("sdk/event/ready", listener);
});

const getTarget = ({target}) => target;

const Panel = Class({
  extends: Disposable,
  implements: [EventTarget],
  get id() {
    return makeID(this.name || this.label);
  },
  readyState: "uninitialized",
  ready: function() {
    const { readyState } = this;
    const isReady = readyState === "complete" ||
                    readyState === "interactive";
    return isReady ? Promise.resolve(this) :
           when(this, "ready").then(getTarget);
  },
  loaded: function() {
    const { readyState } = this;
    const isLoaded = readyState === "complete";
    return isLoaded ? Promise.resolve(this) :
           when(this, "load").then(getTarget);
  },
  unloaded: function() {
    const { readyState } = this;
    const isUninitialized = readyState === "uninitialized";
    return isUninitialized ? Promise.resolve(this) :
           when(this, "unload").then(getTarget);
  },
  postMessage: function(data, ports) {
    const manager = managerFor(this);
    manager.sendAsyncMessage("sdk/event/message", {
      type: "message",
      bubbles: false,
      cancelable: false,
      data: data,
      origin: this.url,
      ports: ports.map(marshal(manager))
    });
  }
});
exports.Panel = Panel;

validate.define(Panel, contract({
  label: {
    is: ["string"],
    msg: "The `option.label` must be a provided"
  },
  tooltip: {
    is: ["string", "undefined"],
    msg: "The `option.tooltip` must be a string"
  },
  icon: {
    is: ["string"],
    map: x => x && resolve(x),
    ok: x => isLocalURL(x),
    msg: "The `options.icon` must be a valid local URI."
  },
  url: {
    map: x => resolve(x.toString()),
    is: ["string"],
    ok: x => isLocalURL(x),
    msg: "The `options.url` must be a valid local URI."
  }
}));

setup.define(Panel, (panel, {window, toolbox, url}) => {
  // Hack: Given that iframe created by devtools API is no good for us,
  // we obtain original iframe and replace it with the one that has
  // desired configuration.
  const original = getFrameElement(window);
  const frame = original.cloneNode(true);

  setAttributes(frame, {
    "src": url,
    "sandbox": "allow-scripts",
    // It would be great if we could allow remote iframes for sandboxing
    // panel documents in a content process, but for now platform implementation
    // is buggy on linux so this is disabled.
    // "remote": true,
    "type": "content",
    "transparent": true,
    "seamless": "seamless"
  });

  original.parentNode.replaceChild(frame, original);
  frame.style.visibility = "hidden";

  // associate panel model with a frame view.
  panels.set(frame, panel);

  const debuggee = fromTarget(toolbox.target);
  // associate debuggee with a panel.
  debuggees.set(panel, debuggee);


  // Setup listeners for the frame message manager.
  const { messageManager } = frame.frameLoader;
  messageManager.addMessageListener("sdk/event/ready", onStateChange);
  messageManager.addMessageListener("sdk/event/load", onStateChange);
  messageManager.addMessageListener("sdk/event/unload", onStateChange);
  messageManager.addMessageListener("sdk/port/message", onPortMessage);
  messageManager.loadFrameScript(FRAME_SCRIPT, false);

  managers.set(panel, messageManager);

  // destroy panel if frame is removed.
  removed(frame).then(onFrameRemove);
  // show frame when it is initialized.
  inited(frame).then(onFrameInited);


  // set listeners if there are ones defined on the prototype.
  setListeners(panel, Object.getPrototypeOf(panel));


  panel.setup({ debuggee: debuggee });
});

dispose.define(Panel, function(panel) {
  debuggeeFor(panel).close();

  debuggees.delete(panel);
  managers.delete(panel);
  panel.readyState = "destroyed";
  panel.dispose();
});
