/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemeChild"];

/**
 * LightweightThemeChild forwards theme data to in-content pages.
 * It is both instantiated by the traditional Actor mechanism,
 * and also manually within the sidebar JS global (which has no message manager).
 * The manual instantiation is necessary due to Bug 1596852.
 */
class LightweightThemeChild extends JSWindowActorChild {
  constructor() {
    super();
    this._initted = false;
    Services.cpmm.sharedData.addEventListener("change", this);
  }

  didDestroy() {
    Services.cpmm.sharedData.removeEventListener("change", this);
  }

  _getChromeOuterWindowID() {
    try {
      // Getting the browserChild throws an exception when it is null.
      let browserChild = this.docShell.browserChild;
      if (browserChild) {
        return browserChild.chromeOuterWindowID;
      }
    } catch (ex) {}

    if (
      Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT
    ) {
      return this.browsingContext.topChromeWindow.docShell.outerWindowID;
    }

    // Return a false-y outerWindowID if we can't manage to get a proper one.
    // Note that no outerWindowID will ever have this ID.
    return 0;
  }

  /**
   * Handles "change" events on the child sharedData map, and notifies
   * our content page if its theme data was among the changed keys.
   */
  handleEvent(event) {
    switch (event.type) {
      // Make sure to update the theme data on first page show.
      case "pageshow":
      case "DOMContentLoaded":
        if (!this._initted && this._getChromeOuterWindowID()) {
          this._initted = true;
          this.update();
        }
        break;

      case "change":
        if (
          event.changedKeys.includes(`theme/${this._getChromeOuterWindowID()}`)
        ) {
          this.update();
        }
        break;
    }
  }

  /**
   * Forward the theme data to the page.
   */
  update() {
    const event = Cu.cloneInto(
      {
        detail: {
          data: Services.cpmm.sharedData.get(
            `theme/${this._getChromeOuterWindowID()}`
          ),
        },
      },
      this.contentWindow
    );
    this.contentWindow.dispatchEvent(
      new this.contentWindow.CustomEvent("LightweightTheme:Set", event)
    );
  }
}
