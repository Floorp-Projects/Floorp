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

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ManifestFinder: "resource://gre/modules/ManifestFinder.sys.mjs",
  ManifestIcons: "resource://gre/modules/ManifestIcons.sys.mjs",
  ManifestObtainer: "resource://gre/modules/ManifestObtainer.sys.mjs",
});

export class ManifestMessagesChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "DOM:WebManifest:hasManifestLink":
        return this.hasManifestLink();
      case "DOM:ManifestObtainer:Obtain":
        return this.obtainManifest(message.data);
      case "DOM:WebManifest:fetchIcon":
        return this.fetchIcon(message);
    }
    return undefined;
  }

  /**
   * Check if the document includes a link to a web manifest.
   */
  hasManifestLink() {
    const response = makeMsgResponse();
    response.result = lazy.ManifestFinder.contentHasManifestLink(
      this.contentWindow
    );
    response.success = true;
    return response;
  }

  /**
   * Asynchronously obtains a web manifest from this window by using the
   * ManifestObtainer and returns the result.
   * @param {Object} checkConformance True if spec conformance messages should be collected.
   */
  async obtainManifest(options) {
    const { checkConformance } = options;
    const response = makeMsgResponse();
    try {
      response.result = await lazy.ManifestObtainer.contentObtainManifest(
        this.contentWindow,
        { checkConformance }
      );
      response.success = true;
    } catch (err) {
      response.result = serializeError(err);
    }
    return response;
  }

  /**
   * Given a manifest and an expected icon size, ask ManifestIcons
   * to fetch the appropriate icon and send along result
   */
  async fetchIcon({ data: { manifest, iconSize } }) {
    const response = makeMsgResponse();
    try {
      response.result = await lazy.ManifestIcons.contentFetchIcon(
        this.contentWindow,
        manifest,
        iconSize
      );
      response.success = true;
    } catch (err) {
      response.result = serializeError(err);
    }
    return response;
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

function makeMsgResponse() {
  return {
    success: false,
    result: undefined,
  };
}
