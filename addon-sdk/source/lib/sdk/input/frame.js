/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");
const { InputPort } = require("./system");
const { getFrameElement, getOuterId,
        getOwnerBrowserWindow } = require("../window/utils");
const { isnt } = require("../lang/functional");
const { foldp, lift, merges, keepIf } = require("../event/utils");
const { object } = require("../util/sequence");
const { compose } = require("../lang/functional");
const { LastClosed } = require("./browser");
const { patch } = require("diffpatcher/index");

const Document = Ci.nsIDOMDocument;

const isntNull = isnt(null);

const frameID = frame => frame.id;
const browserID = compose(getOuterId, getOwnerBrowserWindow);

const isInnerFrame = frame =>
  frame && frame.hasAttribute("data-is-sdk-inner-frame");

// Utility function that given content window loaded in our frame views returns
// an actual frame. This basically takes care of fact that actual frame document
// is loaded in the nested iframe. If content window is not loaded in the nested
// frame of the frame view it returs null.
const getFrame = document =>
  document && document.defaultView && getFrameElement(document.defaultView);

const FrameInput = function(options) {
  const input = keepIf(isInnerFrame, null,
                       lift(getFrame, new InputPort(options)));
  return lift(frame => {
    if (!frame) return frame;
    const [id, owner] = [frameID(frame), browserID(frame)];
    return object([id, {owners: object([owner, options.update])}]);
  }, input);
};

const LastLoading = new FrameInput({topic: "document-element-inserted",
                                    update: {readyState: "loading"}});
exports.LastLoading = LastLoading;

const LastInteractive = new FrameInput({topic: "content-document-interactive",
                                        update: {readyState: "interactive"}});
exports.LastInteractive = LastInteractive;

const LastLoaded = new FrameInput({topic: "content-document-loaded",
                                   update: {readyState: "complete"}});
exports.LastLoaded = LastLoaded;

const LastUnloaded = new FrameInput({topic: "content-page-hidden",
                                    update: null});
exports.LastUnloaded = LastUnloaded;

// Represents state of SDK frames in form of data structure:
// {"frame#1": {"id": "frame#1",
//              "inbox": {"data": "ping",
//                        "target": {"id": "frame#1", "owner": "outerWindowID#2"},
//                        "source": {"id": "frame#1"}}
//              "url": "resource://addon-1/data/index.html",
//              "owners": {"outerWindowID#1": {"readyState": "loading"},
//                         "outerWindowID#2": {"readyState": "complete"}}
//
//
//  frame#2: {"id": "frame#2",
//            "url": "resource://addon-1/data/main.html",
//            "outbox": {"data": "pong",
//                       "source": {"id": "frame#2", "owner": "outerWindowID#1"}
//                       "target": {"id": "frame#2"}}
//            "owners": {outerWindowID#1: {readyState: "interacitve"}}}}
const Frames = foldp(patch, {}, merges([
  LastLoading,
  LastInteractive,
  LastLoaded,
  LastUnloaded,
  new InputPort({ id: "frame-mailbox" }),
  new InputPort({ id: "frame-change" }),
  new InputPort({ id: "frame-changed" })
]));
exports.Frames = Frames;
