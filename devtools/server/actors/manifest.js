/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  manifestSpec,
} = require("resource://devtools/shared/specs/manifest.js");

const lazy = {};

ChromeUtils.defineESModuleGetters(
  lazy,
  {
    ManifestObtainer: "resource://gre/modules/ManifestObtainer.sys.mjs",
  },
  { global: "contextual" }
);

/**
 * An actor for a Web Manifest
 */
class ManifestActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, manifestSpec);
    this.targetActor = targetActor;
  }

  async fetchCanonicalManifest() {
    try {
      const manifest = await lazy.ManifestObtainer.contentObtainManifest(
        this.targetActor.window,
        { checkConformance: true }
      );
      return { manifest };
    } catch (error) {
      return { manifest: null, errorMessage: error.message };
    }
  }
}

exports.ManifestActor = ManifestActor;
