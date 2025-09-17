// SPDX-License-Identifier: MPL-2.0



export class NRAboutPreferencesChild extends JSWindowActorChild {
  handleEvent(event) {
    if (event.type === "DOMContentLoaded") {
      //https://searchfox.org/mozilla-central/rev/3a34b4616994bd8d2b6ede2644afa62eaec817d1/browser/actors/AboutNewTabChild.sys.mjs#70
      Services.scriptloader.loadSubScript(
        "chrome://noraneko-startup/content/about-preferences.js",
        this.contentWindow,
      );
    }
  }
}
