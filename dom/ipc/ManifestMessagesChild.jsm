/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.*/
/*
 * Manifest obtainer frame script implementation of:
 * http://www.w3.org/TR/appmanifest/#obtaining
 *
 * It searches a top-level browsing context for
 * a <link rel=manifest> element. Then fetches
 * and processes the linked manifest.
 *
 * BUG: https://bugzilla.mozilla.org/show_bug.cgi?id=1083410
 */
"use strict";

var EXPORTED_SYMBOLS = ["ManifestMessagesChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ManifestObtainer",
  "resource://gre/modules/ManifestObtainer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ManifestFinder",
  "resource://gre/modules/ManifestFinder.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ManifestIcons",
  "resource://gre/modules/ManifestIcons.jsm"
);

class ManifestMessagesChild extends ActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "DOM:WebManifest:hasManifestLink":
        return this.hasManifestLink(message);
      case "DOM:ManifestObtainer:Obtain":
        return this.obtainManifest(message);
      case "DOM:WebManifest:fetchIcon":
        return this.fetchIcon(message);
    }
    return undefined;
  }

  /**
   * Check if the this.mm.content document includes a link to a web manifest.
   * @param {Object} aMsg The IPC message, which is destructured to just
   *                      get the id.
   */
  hasManifestLink({ data: { id } }) {
    const response = makeMsgResponse(id);
    response.result = ManifestFinder.contentHasManifestLink(this.mm.content);
    response.success = true;
    this.mm.sendAsyncMessage("DOM:WebManifest:hasManifestLink", response);
  }

  /**
   * Asynchronously obtains a web manifest from this.mm.content by using the
   * ManifestObtainer and messages back the result.
   * @param {Object} aMsg The IPC message, which is destructured to just
   *                      get the id.
   */
  async obtainManifest(message) {
    const {
      data: { id, checkConformance },
    } = message;
    const response = makeMsgResponse(id);
    try {
      response.result = await ManifestObtainer.contentObtainManifest(
        this.mm.content,
        { checkConformance }
      );
      response.success = true;
    } catch (err) {
      response.result = serializeError(err);
    }
    this.mm.sendAsyncMessage("DOM:ManifestObtainer:Obtain", response);
  }

  /**
   * Given a manifest and an expected icon size, ask ManifestIcons
   * to fetch the appropriate icon and send along result
   */
  async fetchIcon({ data: { id, manifest, iconSize } }) {
    const response = makeMsgResponse(id);
    try {
      response.result = await ManifestIcons.contentFetchIcon(
        this.mm.content,
        manifest,
        iconSize
      );
      response.success = true;
    } catch (err) {
      response.result = serializeError(err);
    }
    this.mm.sendAsyncMessage("DOM:WebManifest:fetchIcon", response);
  }
}

/**
 * Utility function to Serializes an JS Error, so it can be transferred over
 * the message channel.
 * FIX ME: https://bugzilla.mozilla.org/show_bug.cgi?id=1172586
 * @param  {Error} aError The error to serialize.
 * @return {Object} The serialized object.
 */
function serializeError(aError) {
  const clone = {
    fileName: aError.fileName,
    lineNumber: aError.lineNumber,
    columnNumber: aError.columnNumber,
    stack: aError.stack,
    message: aError.message,
    name: aError.name,
  };
  return clone;
}

function makeMsgResponse(aId) {
  return {
    id: aId,
    success: false,
    result: undefined,
  };
}
