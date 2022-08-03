/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { manifestSpec } = require("devtools/shared/specs/manifest");

loader.lazyImporter(
  this,
  "ManifestObtainer",
  "resource://gre/modules/ManifestObtainer.jsm"
);

/**
 * An actor for a Web Manifest
 */
const ManifestActor = ActorClassWithSpec(manifestSpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
  },

  async fetchCanonicalManifest() {
    try {
      const manifest = await ManifestObtainer.contentObtainManifest(
        this.targetActor.window,
        { checkConformance: true }
      );
      return { manifest };
    } catch (error) {
      return { manifest: null, errorMessage: error.message };
    }
  },
});

exports.ManifestActor = ManifestActor;
