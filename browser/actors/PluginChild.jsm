/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PluginChild"];

// Handle GMP crashes
class PluginChild extends JSWindowActorChild {
  handleEvent(event) {
    // Ignore events for other frames.
    let eventDoc = event.target.ownerDocument || event.target.document;
    if (eventDoc && eventDoc != this.document) {
      return;
    }

    let eventType = event.type;
    if (eventType == "PluginCrashed") {
      this.onPluginCrashed(event);
    }
  }

  /**
   * Determines whether or not the crashed plugin is contained within current
   * full screen DOM element.
   * @param fullScreenElement (DOM element)
   *   The DOM element that is currently full screen, or null.
   * @param domElement
   *   The DOM element which contains the crashed plugin, or the crashed plugin
   *   itself.
   * @returns bool
   *   True if the plugin is a descendant of the full screen DOM element, false otherwise.
   **/
  isWithinFullScreenElement(fullScreenElement, domElement) {
    /**
     * Traverses down iframes until it find a non-iframe full screen DOM element.
     * @param fullScreenIframe
     *  Target iframe to begin searching from.
     * @returns DOM element
     *  The full screen DOM element contained within the iframe (could be inner iframe), or the original iframe if no inner DOM element is found.
     **/
    let getTrueFullScreenElement = fullScreenIframe => {
      if (
        typeof fullScreenIframe.contentDocument !== "undefined" &&
        fullScreenIframe.contentDocument.mozFullScreenElement
      ) {
        return getTrueFullScreenElement(
          fullScreenIframe.contentDocument.mozFullScreenElement
        );
      }
      return fullScreenIframe;
    };

    if (fullScreenElement.tagName === "IFRAME") {
      fullScreenElement = getTrueFullScreenElement(fullScreenElement);
    }

    if (fullScreenElement.contains(domElement)) {
      return true;
    }
    let parentIframe = domElement.ownerGlobal.frameElement;
    if (parentIframe) {
      return this.isWithinFullScreenElement(fullScreenElement, parentIframe);
    }
    return false;
  }

  /**
   * The PluginCrashed event handler. The target of the event is the
   * document that GMP is being used in.
   */
  async onPluginCrashed(aEvent) {
    if (!(aEvent instanceof this.contentWindow.PluginCrashedEvent)) {
      return;
    }

    let { target, gmpPlugin, pluginID } = aEvent;
    let fullScreenElement = this.contentWindow.top.document
      .mozFullScreenElement;
    if (fullScreenElement) {
      if (this.isWithinFullScreenElement(fullScreenElement, target)) {
        this.contentWindow.top.document.mozCancelFullScreen();
      }
    }

    if (!gmpPlugin || !target.document) {
      // TODO: Throw exception? How did we get here?
      return;
    }

    this.sendAsyncMessage("PluginContent:ShowPluginCrashedNotification", {
      pluginCrashID: { pluginID },
    });
  }
}
