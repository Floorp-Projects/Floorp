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
      const doc = this.contentWindow?.document;
      if (!doc?.head) {
        return;
      }

      // The startup bundle can import shared chunks, so load it as a module
      // in the preferences document's global.
      const script = doc.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "script",
      );
      script.setAttribute("type", "module");
      script.setAttribute(
        "src",
        "chrome://noraneko-startup/content/about-preferences.js",
      );
      doc.head.append(script);
    }
  }
}
