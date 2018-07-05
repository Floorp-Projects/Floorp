/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PluralForm", "resource://gre/modules/PluralForm.jsm");
const {actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const PREFERENCES_LOADED_EVENT = "home-pane-loaded";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// These "section" objects are formatted in a way to be similar to the ones from
// SectionsManager to construct the preferences view.
const PREFS_BEFORE_SECTIONS = [
  {
    id: "search",
    pref: {
      feed: "showSearch",
      titleString: "prefs_search_header"
    },
    icon: "chrome://browser/skin/search-glass.svg"
  },
  {
    id: "topsites",
    pref: {
      feed: "feeds.topsites",
      titleString: "settings_pane_topsites_header",
      descString: "prefs_topsites_description"
    },
    icon: "topsites",
    maxRows: 4,
    rowsPref: "topSitesRows"
  }
];
const PREFS_AFTER_SECTIONS = [
  {
    id: "snippets",
    pref: {
      feed: "feeds.snippets",
      titleString: "settings_pane_snippets_header",
      descString: "prefs_snippets_description"
    },
    icon: "info"
  }
];

// This CSS is added to the whole about:preferences page
const CUSTOM_CSS = `
#homeContentsGroup checkbox[src] .checkbox-icon {
  margin-inline-end: 8px;
  margin-inline-start: 4px;
  width: 16px;
}
#homeContentsGroup [data-subcategory] {
  margin-top: 14px;
}
#homeContentsGroup [data-subcategory] > checkbox {
  font-weight: 600;
}
#homeContentsGroup [data-subcategory] > vbox menulist {
  margin-top: 0;
  margin-bottom: 0;
}
`;

this.AboutPreferences = class AboutPreferences {
  init() {
    Services.obs.addObserver(this, PREFERENCES_LOADED_EVENT);
  }

  uninit() {
    Services.obs.removeObserver(this, PREFERENCES_LOADED_EVENT);
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.uninit();
        break;
      case at.SETTINGS_OPEN:
        action._target.browser.ownerGlobal.openPreferences("paneHome", {origin: "aboutHome"});
        break;
      // This is used to open the web extension settings page for an extension
      case at.OPEN_WEBEXT_SETTINGS:
        action._target.browser.ownerGlobal.BrowserOpenAddonsMgr(`addons://detail/${encodeURIComponent(action.data)}`);
        break;
    }
  }

  async observe(window) {
    this.renderPreferences(window, await this.strings, [...PREFS_BEFORE_SECTIONS,
      ...this.store.getState().Sections, ...PREFS_AFTER_SECTIONS]);
  }

  /**
   * Get strings from a js file that the content page would have loaded. The
   * file should be a single variable assignment of a JSON/JS object of strings.
   */
  get strings() {
    return this._strings || (this._strings = new Promise(async resolve => {
      let data = {};
      try {
        const locale = Cc["@mozilla.org/browser/aboutnewtab-service;1"]
          .getService(Ci.nsIAboutNewTabService).activityStreamLocale;
        const request = await fetch(`resource://activity-stream/prerendered/${locale}/activity-stream-strings.js`);
        const text = await request.text();
        const [json] = text.match(/{[^]*}/);
        data = JSON.parse(json);
      } catch (ex) {
        Cu.reportError("Failed to load strings for Activity Stream about:preferences");
      }
      resolve(data);
    }));
  }

  /**
   * Render preferences to an about:preferences content window with the provided
   * strings and preferences structure.
   */
  renderPreferences({document, Preferences, gHomePane}, strings, prefStructure) {
    // Helper to create a new element and append it
    const createAppend = (tag, parent) => parent.appendChild(
      document.createElementNS(XUL_NS, tag));

    // Helper to get strings and format with values if necessary
    const formatString = id => {
      if (typeof id !== "object") {
        return strings[id] || id;
      }
      let string = strings[id.id] || JSON.stringify(id);
      if (id.values) {
        Object.entries(id.values).forEach(([key, val]) => {
          string = string.replace(new RegExp(`{${key}}`, "g"), val);
        });
      }
      return string;
    };

    // Helper to link a UI element to a preference for updating
    const linkPref = (element, name, type) => {
      const fullPref = `browser.newtabpage.activity-stream.${name}`;
      element.setAttribute("preference", fullPref);
      Preferences.add({id: fullPref, type});

      // Prevent changing the UI if the preference can't be changed
      element.disabled = Preferences.get(fullPref).locked;
    };

    // Add in custom styling
    document.insertBefore(document.createProcessingInstruction("xml-stylesheet",
      `href="data:text/css,${encodeURIComponent(CUSTOM_CSS)}" type="text/css"`),
      document.documentElement);

    // Insert a new group immediately after the homepage one
    const homeGroup = document.getElementById("homepageGroup");
    const contentsGroup = homeGroup.insertAdjacentElement("afterend", homeGroup.cloneNode());
    contentsGroup.id = "homeContentsGroup";
    contentsGroup.setAttribute("data-subcategory", "contents");
    const caption = createAppend("caption", contentsGroup);
    caption.setAttribute("label", formatString("prefs_home_header"));
    const description = createAppend("description", contentsGroup);
    description.textContent = formatString("prefs_home_description");

    // Add preferences for each section
    prefStructure.forEach(sectionData => {
      const {
        id,
        pref: prefData,
        icon = "webextension",
        maxRows,
        rowsPref,
        shouldHidePref
      } = sectionData;
      const {
        feed: name,
        titleString,
        descString,
        nestedPrefs = []
      } = prefData || {};

      // Don't show any sections that we don't want to expose in preferences UI
      if (shouldHidePref) {
        return;
      }

      // Use full icon spec for certain protocols or fall back to packaged icon
      const iconUrl = !icon.search(/^(chrome|moz-extension|resource):/) ? icon :
        `resource://activity-stream/data/content/assets/glyph-${icon}-16.svg`;

      // Add the main preference for turning on/off a section
      const sectionVbox = createAppend("vbox", contentsGroup);
      sectionVbox.setAttribute("data-subcategory", id);
      const checkbox = createAppend("checkbox", sectionVbox);
      checkbox.setAttribute("label", formatString(titleString));
      checkbox.setAttribute("src", iconUrl);
      linkPref(checkbox, name, "bool");

      // Add more details for the section (e.g., description, more prefs)
      const detailVbox = createAppend("vbox", sectionVbox);
      detailVbox.classList.add("indent");
      if (descString) {
        const label = createAppend("label", detailVbox);
        label.classList.add("indent");
        label.textContent = formatString(descString);

        // Specially add a link for stories
        if (id === "topstories") {
          const sponsoredHbox = createAppend("hbox", detailVbox);
          sponsoredHbox.setAttribute("align", "center");
          sponsoredHbox.appendChild(label);
          label.classList.add("tail-with-learn-more");

          const link = createAppend("label", sponsoredHbox);
          link.classList.add("learn-sponsored");
          link.classList.add("text-link");
          link.setAttribute("href", sectionData.disclaimer.link.href);
          link.textContent = formatString("prefs_topstories_sponsored_learn_more");
        }

        // Add a rows dropdown if we have a pref to control and a maximum
        if (rowsPref && maxRows) {
          const detailHbox = createAppend("hbox", detailVbox);
          detailHbox.setAttribute("align", "center");
          label.setAttribute("flex", 1);
          detailHbox.appendChild(label);

          // Add appropriate number of localized entries to the dropdown
          const menulist = createAppend("menulist", detailHbox);
          menulist.setAttribute("crop", "none");
          const menupopup = createAppend("menupopup", menulist);
          for (let num = 1; num <= maxRows; num++) {
            const plurals = formatString({id: "prefs_section_rows_option", values: {num}});
            const item = createAppend("menuitem", menupopup);
            item.setAttribute("label", PluralForm.get(num, plurals));
            item.setAttribute("value", num);
          }
          linkPref(menulist, rowsPref, "int");
        }
      }

      // Add a checkbox pref for any nested preferences
      nestedPrefs.forEach(nested => {
        const subcheck = createAppend("checkbox", detailVbox);
        subcheck.classList.add("indent");
        subcheck.setAttribute("label", formatString(nested.titleString));
        linkPref(subcheck, nested.name, "bool");
      });
    });

    // Update the visibility of the Restore Defaults btn based on checked prefs
    gHomePane.toggleRestoreDefaultsBtn();
  }
};

this.PREFERENCES_LOADED_EVENT = PREFERENCES_LOADED_EVENT;
const EXPORTED_SYMBOLS = ["AboutPreferences", "PREFERENCES_LOADED_EVENT"];
