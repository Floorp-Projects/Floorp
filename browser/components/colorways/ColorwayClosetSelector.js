/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

class ColorwaySelector extends HTMLFieldSetElement {
  constructor() {
    super();
    XPCOMUtils.defineLazyModuleGetters(this, {
      AddonManager: "resource://gre/modules/AddonManager.jsm",
      BuiltInThemes: "resource:///modules/BuiltInThemes.jsm",
    });
    this.colorways;
  }

  getColorways = async () => {
    this.BuiltInThemes.ensureBuiltInThemes();
    let themes = await this.AddonManager.getAddonsByTypes(["theme"]);
    this.colorways = themes.filter(theme =>
      this.BuiltInThemes.isMonochromaticTheme(theme.id)
    );
    return this.colorways;
  };

  render() {
    for (const theme of this.colorways) {
      let input = document.createElement("input");
      input.type = "radio";
      input.name = "colorway";
      input.value = theme.id;
      input.style.setProperty("--icon-url", `url(${theme.iconURL})`);
      this.appendChild(input);
    }
  }

  connectedCallback() {
    this.getColorways().then(value => {
      this.render();
    });
  }
}

customElements.define("colorway-selector", ColorwaySelector, {
  extends: "fieldset",
});
