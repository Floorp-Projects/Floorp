/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const {
  frameDescriptorSpec,
} = require("devtools/shared/specs/descriptors/frame");

loader.lazyRequireGetter(
  this,
  "connectToFrame",
  "devtools/server/connectors/frame-connector",
  true
);
loader.lazyRequireGetter(
  this,
  "connectToFrameWithJsWindowActor",
  "devtools/server/connectors/js-window-actor/frame-js-window-actor-connector",
  true
);

const FrameDescriptorActor = ActorClassWithSpec(frameDescriptorSpec, {
  initialize(connection, browsingContext) {
    if (typeof browsingContext.id != "number") {
      throw Error("Frame Descriptor requires a valid BrowsingContext.");
    }
    Actor.prototype.initialize.call(this, connection);
    this.destroy = this.destroy.bind(this);
    this.id = browsingContext.id;
    this._browsingContext = browsingContext;
    this._embedderElement = browsingContext.embedderElement;
  },

  /**
   * Connect to a remote process actor, always a Frame target.
   */
  async _connectMessageManager() {
    return connectToFrame(this.conn, this._embedderElement, this.destroy);
  },

  async _connectBrowsingContext() {
    return connectToFrameWithJsWindowActor(
      this.conn,
      this._browsingContext,
      this.destroy
    );
  },

  get _mm() {
    // Get messageManager from XUL browser (which might be a specialized tunnel for RDM)
    // or else fallback to asking the frameLoader itself.
    return (
      this._embedderElement &&
      (this._embedderElement.messageManager ||
        this._embedderElement.frameLoader.messageManager)
    );
  },

  /**
   * Connect the a frame actor.
   */
  async getTarget() {
    let form;
    if (this._mm) {
      form = await this._connectMessageManager();
    } else {
      form = await this._connectBrowsingContext();
    }

    return form;
  },

  getParentID() {
    if (this._browsingContext.parent) {
      return this._browsingContext.parent.id;
    }
    // if we are at the top level document for this frame, but
    // we need to access the browser element, then we will not
    // be able to get the parent id from the browsing context.
    // instead, we can get it from the embedderWindowGlobal.
    if (this._browsingContext.embedderWindowGlobal) {
      return this._browsingContext.embedderWindowGlobal.browsingContext.id;
    }
    return null;
  },

  form() {
    const url = this._browsingContext.currentWindowGlobal
      ? this._browsingContext.currentWindowGlobal.documentURI.displaySpec
      : null;
    const parentID = this.getParentID();
    return {
      actor: this.actorID,
      id: this.id,
      url,
      parentID,
    };
  },

  destroy() {
    this._browsingContext = null;
    this._embedderElement = null;
    Actor.prototype.destroy.call(this);
  },
});

exports.FrameDescriptorActor = FrameDescriptorActor;
