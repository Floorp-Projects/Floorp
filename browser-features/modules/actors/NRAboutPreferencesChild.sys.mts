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
      // The bundle imports shared chunks, so it must load as a module.
      const script = doc.createElement("script");
      script.type = "module";
      script.src = "chrome://noraneko-startup/content/about-preferences.js";
      doc.head.append(script);
    }
  }
}
