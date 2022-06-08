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
    window.addEventListener("unload", () => {
      this.AddonManager.removeAddonListener(this);
    });
  }

  getColorways = async () => {
    this.AddonManager.addAddonListener(this);

    this.BuiltInThemes.ensureBuiltInThemes();
    let themes = await this.AddonManager.getAddonsByTypes(["theme"]);
    this.colorways = themes.filter(theme =>
      this.BuiltInThemes.isMonochromaticTheme(theme.id)
    );
    this.activeTheme = themes.find(theme => theme.isActive);
    return this.colorways;
  };

  onEnabled(addon) {
    if (addon.type == "theme") {
      this.activeTheme = addon;
      this.refresh();
    }
  }

  refresh() {
    for (let input of this.children) {
      if (input.value == this.activeTheme.id) {
        input.classList.add("active");
        input.setAttribute("aria-current", true);
        this.updateName(this.selectedTheme.name);
        this.updateDescription(input.value);
      } else {
        input.classList.remove("active");
        input.setAttribute("aria-current", false);
      }
    }
  }
  render() {
    let isFirst = true;
    for (const theme of this.colorways) {
      let input = document.createElement("input");
      input.type = "radio";
      input.name = "colorway";
      input.value = theme.id;
      input.setAttribute("title", theme.name);
      input.style.setProperty("--colorway-icon", `url(${theme.iconURL})`);
      input.onclick = () => {
        this.selectedTheme = theme;
        this.updateName(theme.name);
        this.updateDescription(theme.id);
      };
      this.appendChild(input);
      if (theme.isActive) {
        input.classList.add("active");
      }
      if (isFirst) {
        input.checked = true;
        input.onclick();
        isFirst = false;
      }
    }
  }

  updateDescription(text) {
    text = text.replace("@mozilla.org", "-description");
    document.querySelector("#colorway-description").innerText = text;
    // TODO: localize strings with Fluent
  }
  updateName(text) {
    // TODO: localize strings with Fluent
    document.querySelector("#colorway-name").innerText = text;
  }

  connectedCallback() {
    this.getColorways().then(() => {
      this.render();
    });
  }
}

customElements.define("colorway-selector", ColorwaySelector, {
  extends: "fieldset",
});
