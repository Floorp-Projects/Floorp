/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental",
  "engines": {
    "Firefox": "> 28"
  }
};

const { Cu, Ci } = require("chrome");
const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
const { subscribe, send, Reactor, foldp, lift, merges, keepIf } = require("../../event/utils");
const { InputPort } = require("../../input/system");
const { OutputPort } = require("../../output/system");
const { LastClosed } = require("../../input/browser");
const { pairs, keys, object, each } = require("../../util/sequence");
const { curry, compose } = require("../../lang/functional");
const { getFrameElement, getOuterId,
        getByOuterId, getOwnerBrowserWindow } = require("../../window/utils");
const { patch, diff } = require("diffpatcher/index");
const { encode } = require("../../base64");
const { Frames } = require("../../input/frame");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HTML_NS = "http://www.w3.org/1999/xhtml";
const OUTER_FRAME_URI = module.uri.replace(/\.js$/, ".html");

const mailbox = new OutputPort({ id: "frame-mailbox" });

const frameID = frame => frame.id.replace("outer-", "");
const windowID = compose(getOuterId, getOwnerBrowserWindow);

const getOuterFrame = (windowID, frameID) =>
    getByOuterId(windowID).document.getElementById("outer-" + frameID);

const listener = ({target, source, data, origin, timeStamp}) => {
  // And sent received message to outbox so that frame API model
  // will deal with it.
  if (source && source !== target) {
    const frame = getFrameElement(target);
    const id = frameID(frame);
    send(mailbox, object([id, {
      outbox: {type: "message",
               source: {id: id, ownerID: windowID(frame)},
               data: data,
               origin: origin,
               timeStamp: timeStamp}}]));
  }
};

// Utility function used to create frame with a given `state` and
// inject it into given `window`.
const registerFrame = ({id, url}) => {
  CustomizableUI.createWidget({
    id: id,
    type: "custom",
    removable: true,
    onBuild: document => {
      let view = document.createElementNS(XUL_NS, "toolbaritem");
      view.setAttribute("id", id);
      view.setAttribute("flex", 2);

      let outerFrame = document.createElementNS(XUL_NS, "iframe");
      outerFrame.setAttribute("src", OUTER_FRAME_URI);
      outerFrame.setAttribute("id", "outer-" + id);
      outerFrame.setAttribute("data-is-sdk-outer-frame", true);
      outerFrame.setAttribute("type", "content");
      outerFrame.setAttribute("transparent", true);
      outerFrame.setAttribute("flex", 2);
      outerFrame.setAttribute("style", "overflow: hidden;");
      outerFrame.setAttribute("scrolling", "no");
      outerFrame.setAttribute("disablehistory", true);
      outerFrame.setAttribute("seamless", "seamless");
      outerFrame.addEventListener("load", function onload() {
        outerFrame.removeEventListener("load", onload, true);

        let doc = outerFrame.contentDocument;

        let innerFrame = doc.createElementNS(HTML_NS, "iframe");
        innerFrame.setAttribute("id", id);
        innerFrame.setAttribute("src", url);
        innerFrame.setAttribute("seamless", "seamless");
        innerFrame.setAttribute("sandbox", "allow-scripts");
        innerFrame.setAttribute("scrolling", "no");
        innerFrame.setAttribute("data-is-sdk-inner-frame", true);
        innerFrame.setAttribute("style", [ "border:none",
          "position:absolute", "width:100%", "top: 0",
          "left: 0", "overflow: hidden"].join(";"));

        doc.body.appendChild(innerFrame);
      }, true);

      view.appendChild(outerFrame);

      return view;
    }
  });
};

const unregisterFrame = CustomizableUI.destroyWidget;

const deliverMessage = curry((frameID, data, windowID) => {
  const frame = getOuterFrame(windowID, frameID);
  const content = frame && frame.contentWindow;

  if (content)
    content.postMessage(data, content.location.origin);
});

const updateFrame = (id, {inbox, owners}, present) => {
  if (inbox) {
    const { data, target:{ownerID}, source } = present[id].inbox;
    if (ownerID)
      deliverMessage(id, data, ownerID);
    else
      each(deliverMessage(id, data), keys(present[id].owners));
  }

  each(setupView(id), pairs(owners));
};

const setupView = curry((frameID, [windowID, state]) => {
  if (state && state.readyState === "loading") {
    const frame = getOuterFrame(windowID, frameID);
    // Setup a message listener on contentWindow.
    frame.contentWindow.addEventListener("message", listener);
  }
});


const reactor = new Reactor({
  onStep: (present, past) => {
    const delta = diff(past, present);

    // Apply frame changes
    each(([id, update]) => {
      if (update === null)
        unregisterFrame(id);
      else if (past[id])
        updateFrame(id, update, present);
      else
        registerFrame(update);
    }, pairs(delta));
  },
  onEnd: state => each(unregisterFrame, keys(state))
});
reactor.run(Frames);
