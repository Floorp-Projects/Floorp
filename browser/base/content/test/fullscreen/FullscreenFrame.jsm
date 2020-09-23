/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * test helper JSWindowActors used by the browser_fullscreen_api_fission.js test.
 */

var EXPORTED_SYMBOLS = ["FullscreenFrameChild"];

class FullscreenFrameChild extends JSWindowActorChild {
  actorCreated() {
    this.fullscreen_events = [];
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "ExitFullscreen":
        return this.contentWindow.document.exitFullscreen();
      case "RequestFullscreen":
        let changed = new Promise(resolve => {
          this.contentWindow.document.addEventListener(
            "fullscreenchange",
            () => resolve(),
            {
              once: true,
            }
          );
        });
        this.docShell.isActive = true;
        this.contentWindow.document.getElementById("request").click();
        return changed;
      case "CreateChild":
        let child = msg.data;
        let iframe = this.contentWindow.document.createElement("iframe");
        iframe.allow = child.allow_fullscreen ? "fullscreen" : "";
        iframe.name = child.name;

        let loaded = new Promise(resolve => {
          iframe.addEventListener(
            "load",
            () => resolve(iframe.browsingContext),
            { once: true }
          );
        });
        iframe.src = child.url;
        this.contentWindow.document.body.appendChild(iframe);
        return loaded;
      case "GetEvents":
        return Promise.resolve(this.fullscreen_events);
      case "ClearEvents":
        this.fullscreen_events = [];
        return Promise.resolve();
      case "GetFullscreenElement":
        let document = this.contentWindow.document;
        let child_iframe = this.contentWindow.document.getElementsByTagName(
          "iframe"
        )
          ? this.contentWindow.document.getElementsByTagName("iframe")[0]
          : null;
        switch (document.fullscreenElement) {
          case null:
            return Promise.resolve("null");
          case document:
            return Promise.resolve("document");
          case document.body:
            return Promise.resolve("body");
          case child_iframe:
            return Promise.resolve("child_iframe");
          default:
            return Promise.resolve("other");
        }
    }

    return Promise.reject("Unexpected Message");
  }

  async handleEvent(event) {
    switch (event.type) {
      case "fullscreenchange":
        this.fullscreen_events.push(true);
        break;
      case "fullscreenerror":
        this.fullscreen_events.push(false);
        break;
    }
  }
}
