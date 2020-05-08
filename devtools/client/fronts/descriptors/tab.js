/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { tabDescriptorSpec } = require("devtools/shared/specs/descriptors/tab");

loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/client/fronts/targets/browsing-context",
  true
);
loader.lazyRequireGetter(
  this,
  "LocalTabTargetFront",
  "devtools/client/fronts/targets/local-tab",
  true
);
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class TabDescriptorFront extends FrontClassWithSpec(tabDescriptorSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._client = client;

    // The tab descriptor can be configured to create either local tab targets
    // (eg, regular tab toolbox) or browsing context targets (eg tab remote
    // debugging).
    this._localTab = null;

    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);
  }

  form(json) {
    this.actorID = json.actor;
    this._form = json;
    this.traits = json.traits || {};
  }

  setLocalTab(localTab) {
    this._localTab = localTab;
  }

  get outerWindowID() {
    return this._form.outerWindowID;
  }

  get selected() {
    return this._form.selected;
  }

  get title() {
    return this._form.title;
  }

  get url() {
    return this._form.url;
  }

  get favicon() {
    // Note: the favicon is not part of the default form() payload, it will be
    // added in `retrieveAsyncFormData`.
    return this._form.favicon;
  }

  _createTabTarget(form) {
    let front;
    if (this._localTab) {
      // Instanciate a specialized class for a local tab as it needs some more
      // client side integration with the Firefox frontend.
      front = new LocalTabTargetFront(this._client, null, this, this._localTab);
    } else {
      front = new BrowsingContextTargetFront(this._client, null, this);
    }
    // As these fronts aren't instantiated by protocol.js, we have to set their actor ID
    // manually like that:
    front.actorID = form.actor;
    front.form(form);
    this.manage(front);
    front.on("target-destroyed", this._onTargetDestroyed);
    return front;
  }

  _onTargetDestroyed() {
    // Clear the cached targetFront when the target is destroyed.
    // Note that we are also checking that _targetFront has a valid actorID
    // in getTarget, this acts as an additional security to avoid races.
    this._targetFront = null;
  }

  /**
   * This method is mostly intended for backward compatibility (FF76 and older).
   *
   * It also retrieves the favicon via getFavicon() for regular servers, but
   * the main reason this is done here is to keep it close to the solution
   * used to get the favicon for FF75.
   *
   * Once FF75 & FF76 hit release, we could let callers explicitly retrieve the
   * favicon instead of inserting it in the form dynamically.
   */
  async retrieveAsyncFormData() {
    try {
      if (!this.traits.hasTabInfo) {
        // Backward compatibility for FF76 or older
        const targetForm = await super.getTarget();
        this._form.outerWindowID = targetForm.outerWindowID;
        this._form.title = targetForm.title;
        this._form.url = targetForm.url;

        if (!this.traits.getFavicon) {
          // Backward compatibility for FF75 or older.
          this._form.favicon = targetForm.favicon;
        }
      }

      if (this.traits.getFavicon) {
        this._form.favicon = await this.getFavicon();
      }
    } catch (e) {
      // We might request the data for a tab which is going to be destroyed.
      // In this case targetFront.actorID will be null. Otherwise log an error.
      if (this.actorID) {
        console.error(
          "Failed to retrieve the async form data for " + this.url,
          e
        );
      }
    }
  }

  async getTarget() {
    if (this._targetFront && this._targetFront.actorID) {
      return this._targetFront;
    }

    if (this._targetFrontPromise) {
      return this._targetFrontPromise;
    }

    this._targetFrontPromise = (async () => {
      let targetFront = null;
      try {
        const targetForm = await super.getTarget();
        targetFront = this._createTabTarget(targetForm);
        await targetFront.attach();
      } catch (e) {
        console.log(
          `Request to connect to TabDescriptor "${this.id}" failed: ${e}`
        );
      }
      this._targetFront = targetFront;
      this._targetFrontPromise = null;
      return targetFront;
    })();
    return this._targetFrontPromise;
  }

  getCachedWatcher() {
    for (const child of this.poolChildren()) {
      if (child.typeName == "watcher") {
        return child;
      }
    }
    return null;
  }
}

exports.TabDescriptorFront = TabDescriptorFront;
registerFront(TabDescriptorFront);
