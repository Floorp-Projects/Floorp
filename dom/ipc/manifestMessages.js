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
/*globals Task, ManifestObtainer, ManifestFinder, content, sendAsyncMessage, addMessageListener, Components*/
"use strict";
const {
  utils: Cu,
} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ManifestObtainer",
				  "resource://gre/modules/ManifestObtainer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ManifestFinder",
				  "resource://gre/modules/ManifestFinder.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ManifestIcons",
				  "resource://gre/modules/ManifestIcons.jsm");

const MessageHandler = {
  registerListeners() {
    addMessageListener(
      "DOM:WebManifest:hasManifestLink",
      this.hasManifestLink.bind(this)
    );
    addMessageListener(
      "DOM:ManifestObtainer:Obtain",
      this.obtainManifest.bind(this)
    );
    addMessageListener(
      "DOM:Manifest:FireAppInstalledEvent",
      this.fireAppInstalledEvent.bind(this)
    );
    addMessageListener(
      "DOM:WebManifest:fetchIcon",
      this.fetchIcon.bind(this)
    );
  },

  /**
   * Check if the content document includes a link to a web manifest.
   * @param {Object} aMsg The IPC message, which is destructured to just
   *                      get the id.
   */
  hasManifestLink({data: {id}}) {
    const response = makeMsgResponse(id);
    response.result = ManifestFinder.contentHasManifestLink(content);
    response.success = true;
    sendAsyncMessage("DOM:WebManifest:hasManifestLink", response);
  },

  /**
   * Asynchronously obtains a web manifest from content by using the
   * ManifestObtainer and messages back the result.
   * @param {Object} aMsg The IPC message, which is destructured to just
   *                      get the id.
   */
  async obtainManifest({data: {id}}) {
    const response = makeMsgResponse(id);
    try {
      response.result = await ManifestObtainer.contentObtainManifest(content);
      response.success = true;
    } catch (err) {
      response.result = serializeError(err);
    }
    sendAsyncMessage("DOM:ManifestObtainer:Obtain", response);
  },

  fireAppInstalledEvent({data: {id}}){
    const ev = new Event("appinstalled");
    const response = makeMsgResponse(id);
    if (!content || content.top !== content) {
      const msg = "Can only dispatch install event on top-level browsing contexts.";
      response.result = serializeError(new Error(msg));
    } else {
      response.success = true;
      content.dispatchEvent(ev);
    }
    sendAsyncMessage("DOM:Manifest:FireAppInstalledEvent", response);
  },

  /**
   * Given a manifest and an expected icon size, ask ManifestIcons
   * to fetch the appropriate icon and send along result
   */
  async fetchIcon({data: {id, manifest, iconSize}}) {
    const response = makeMsgResponse(id);
    try {
      response.result =
        await ManifestIcons.contentFetchIcon(content, manifest, iconSize);
      response.success = true;
    } catch (err) {
      response.result = serializeError(err);
    }
    sendAsyncMessage("DOM:WebManifest:fetchIcon", response);
  },

};
/**
 * Utility function to Serializes an JS Error, so it can be transferred over
 * the message channel.
 * FIX ME: https://bugzilla.mozilla.org/show_bug.cgi?id=1172586
 * @param  {Error} aError The error to serialize.
 * @return {Object} The serialized object.
 */
function serializeError(aError) {
  const clone = {
    "fileName": aError.fileName,
    "lineNumber": aError.lineNumber,
    "columnNumber": aError.columnNumber,
    "stack": aError.stack,
    "message": aError.message,
    "name": aError.name
  };
  return clone;
}

function makeMsgResponse(aId) {
    return {
      id: aId,
      success: false,
      result: undefined
    };
  }

MessageHandler.registerListeners();
