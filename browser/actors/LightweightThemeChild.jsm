/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemeChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * LightweightThemeChild forwards theme data to in-content pages.
 * It is both instantiated by the traditional Actor mechanism,
 * and also manually within the sidebar JS global (which has no message manager)
 */
class LightweightThemeChild extends ActorChild {
  constructor(dispatcher) {
    if (dispatcher.mm) {
      // This is being instantiated by the Actor mechanism.
      super(dispatcher);
    } else {
      // Manually instantiated by the sidebar.
      let fakeDispatcher = {
        mm: dispatcher,
        window: dispatcher.content,
        addEventListener: dispatcher.content.addEventListener,
      };
      super(fakeDispatcher);
    }

    this.init();
  }

  /**
   * Initializes the actor for the current page, sending it any existing
   * theme data, and adding shared data change listeners so it can
   * notify the page of future updates.
   *
   * This is called when the actor is constructed, and any time
   * ActorManagerChild receives a pageshow event for the page we're
   * attached to.
   */
  init() {
    Services.cpmm.sharedData.addEventListener("change", this);
    this.update(this.mm.chromeOuterWindowID, this.content);
  }

  /**
   * Cleans up any global listeners registered by the actor.
   *
   * This is called by ActorManagerChild any time it receives a pagehide
   * event for the page we're attached to.
   */
  cleanup() {
    Services.cpmm.sharedData.removeEventListener("change", this);
  }

  /**
   * Handles "change" events on the child sharedData map, and notifies
   * our content page if its theme data was among the changed keys.
   */
  handleEvent(event) {
    if (event.type === "change") {
      if (event.changedKeys.includes(`theme/${this.mm.chromeOuterWindowID}`)) {
        this.update(this.mm.chromeOuterWindowID, this.content);
      }
    }
  }

  /**
   * Forward the theme data to the page.
   * @param {Object} outerWindowID The outerWindowID the parent process window has.
   * @param {Object} content The receiving global
   */
  update(outerWindowID, content) {
    const event = Cu.cloneInto(
      {
        detail: {
          data: Services.cpmm.sharedData.get(`theme/${outerWindowID}`),
        },
      },
      content
    );
    content.dispatchEvent(
      new content.CustomEvent("LightweightTheme:Set", event)
    );
  }
}
