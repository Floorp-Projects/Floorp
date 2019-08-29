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

const FrameDescriptorActor = ActorClassWithSpec(frameDescriptorSpec, {
  initialize(connection, browsingContext) {
    if (typeof browsingContext.id != "number") {
      throw Error("Frame Descriptor Connect requires a valid browsingContext.");
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
    const id = this.id;
    return {
      error: "NoJSWindowAPISupport",
      message: `The browsingContext '${id}' can only be accessed via JSWindowAPI`,
    };
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

  form() {
    const url = this._browsingContext.currentWindowGlobal
      ? this._browsingContext.currentWindowGlobal.documentURI.displaySpec
      : null;
    return {
      actor: this.actorID,
      id: this.id,
      url,
      parentID: this._browsingContext.parent
        ? this._browsingContext.parent.id
        : null,
    };
  },

  destroy() {
    this._browsingContext = null;
    this._embedderElement = null;
    Actor.prototype.destroy.call(this);
  },
});

exports.FrameDescriptorActor = FrameDescriptorActor;
