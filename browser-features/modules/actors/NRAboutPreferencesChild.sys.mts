export class NRAboutPreferencesChild extends JSWindowActorChild {
  handleEvent(event: Event): void {
    if (event.type === "DOMDocElementInserted") {
      const doc = this.contentWindow?.document;
      if (!doc?.documentElement) {
        return;
      }
      const styleId = "floorp-ipprotection-preferences-guard";
      if (doc.getElementById(styleId)) {
        return;
      }
      const style = doc.createElement("style");
      style.id = styleId;
      style.textContent = `
        setting-group[groupid="ipprotection"]:not([data-floorp-ipprotection-ready="true"]) {
          visibility: hidden !important;
          pointer-events: none !important;
        }
      `;
      doc.documentElement.append(style);
      return;
    }
    if (event.type === "DOMContentLoaded") {
      //https://searchfox.org/mozilla-central/rev/3a34b4616994bd8d2b6ede2644afa62eaec817d1/browser/actors/AboutNewTabChild.sys.mjs#70
      Services.scriptloader.loadSubScript(
        "chrome://noraneko-startup/content/about-preferences.js",
        this.contentWindow,
      );
    }
  }
}
