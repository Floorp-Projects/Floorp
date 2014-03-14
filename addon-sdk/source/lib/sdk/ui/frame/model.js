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

const { Class } = require("../../core/heritage");
const { EventTarget } = require("../../event/target");
const { emit, off, setListeners } = require("../../event/core");
const { Reactor, foldp, send, merges } = require("../../event/utils");
const { Disposable } = require("../../core/disposable");
const { OutputPort } = require("../../output/system");
const { InputPort } = require("../../input/system");
const { identify } = require("../id");
const { pairs, object, map, each } = require("../../util/sequence");
const { patch, diff } = require("diffpatcher/index");
const { isLocalURL } = require("../../url");
const { compose } = require("../../lang/functional");
const { contract } = require("../../util/contract");
const { id: addonID, data: { url: resolve }} = require("../../self");
const { Frames } = require("../../input/frame");


const output = new OutputPort({ id: "frame-change" });
const mailbox = new OutputPort({ id: "frame-mailbox" });
const input = Frames;


const makeID = url =>
  ("frame-" + addonID + "-" + url).
    split("/").join("-").
    split(".").join("-").
    replace(/[^A-Za-z0-9_\-]/g, "");

const validate = contract({
  name: {
    is: ["string", "undefined"],
    ok: x => /^[a-z][a-z0-9-_]+$/i.test(x),
    msg: "The `option.name` must be a valid alphanumeric string (hyphens and " +
         "underscores are allowed) starting with letter."
  },
  url: {
    map: x => x.toString(),
    is: ["string"],
    ok: x => isLocalURL(x),
    msg: "The `options.url` must be a valid local URI."
  }
});

const Source = function({id, ownerID}) {
  this.id = id;
  this.ownerID = ownerID;
};
Source.postMessage = ({id, ownerID}, data, origin) => {
  send(mailbox, object([id, {
    inbox: {
      target: {id: id, ownerID: ownerID},
      timeStamp: Date.now(),
      data: data,
      origin: origin
    }
  }]));
};
Source.prototype.postMessage = function(data, origin) {
  Source.postMessage(this, data, origin);
};

const Message = function({type, data, source, origin, timeStamp}) {
  this.type = type;
  this.data = data;
  this.origin = origin;
  this.timeStamp = timeStamp;
  this.source = new Source(source);
};


const frames = new Map();
const sources = new Map();

const Frame = Class({
  extends: EventTarget,
  implements: [Disposable, Source],
  initialize: function(params={}) {
    const options = validate(params);
    const id = makeID(options.name || options.url);

    if (frames.has(id))
      throw Error("Frame with this id already exists: " + id);

    const initial = { id: id, url: resolve(options.url) };
    this.id = id;

    setListeners(this, params);

    frames.set(this.id, this);

    send(output, object([id, initial]));
  },
  get url() {
    const state = reactor.value[this.id];
    return state && state.url;
  },
  destroy: function() {
    send(output, object([this.id, null]));
    frames.delete(this.id);
    off(this);
  },
  // `JSON.stringify` serializes objects based of the return
  // value of this method. For convinienc we provide this method
  // to serialize actual state data.
  toJSON: function() {
    return { id: this.id, url: this.url };
  }
});
identify.define(Frame, frame => frame.id);

exports.Frame = Frame;

const reactor = new Reactor({
  onStep: (present, past) => {
    const delta = diff(past, present);

    each(([id, update]) => {
      const frame = frames.get(id);
      if (update) {
        if (!past[id])
          emit(frame, "register");

        if (update.outbox)
          emit(frame, "message", new Message(present[id].outbox));

        each(([ownerID, state]) => {
          const readyState = state ? state.readyState : "detach";
          const type = readyState === "loading" ? "attach" :
                       readyState === "interactive" ? "ready" :
                       readyState === "complete" ? "load" :
                       readyState;

          // TODO: Cache `Source` instances somewhere to preserve
          // identity.
          emit(frame, type, {type: type,
                             source: new Source({id: id, ownerID: ownerID})});
        }, pairs(update.owners));
      }
    }, pairs(delta));
  }
});
reactor.run(input);
