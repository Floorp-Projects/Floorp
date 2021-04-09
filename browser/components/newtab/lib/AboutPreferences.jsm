/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { actionTypes: at, actionCreators: ac } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);

const HTML_NS = "http://www.w3.org/1999/xhtml";
const PREFERENCES_LOADED_EVENT = "home-pane-loaded";

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

// These "section" objects are formatted in a way to be similar to the ones from
// SectionsManager to construct the preferences view.
const PREFS_BEFORE_SECTIONS = ({
  newNewtabExperienceEnabled,
  customizationMenuEnabled,
}) => [
  {
    id: "search",
    pref: {
      feed: "showSearch",
      titleString: "home-prefs-search-header",
    },
    icon: "chrome://global/skin/icons/search-glass.svg",
  },
  {
    id: "topsites",
    pref: {
      feed: "feeds.topsites",
      titleString:
        newNewtabExperienceEnabled || customizationMenuEnabled
          ? "home-prefs-shortcuts-header"
          : "home-prefs-topsites-header",
      descString:
        newNewtabExperienceEnabled || customizationMenuEnabled
          ? "home-prefs-shortcuts-description"
          : "home-prefs-topsites-description",
      get nestedPrefs() {
        return Services.prefs.getBoolPref("browser.topsites.useRemoteSetting")
          ? [
              {
                name: "showSponsoredTopSites",
                titleString:
                  newNewtabExperienceEnabled || customizationMenuEnabled
                    ? "home-prefs-shortcuts-by-option-sponsored"
                    : "home-prefs-topsites-by-option-sponsored",
                eventSource: "SPONSORED_TOP_SITES",
              },
            ]
          : [];
      },
    },
    icon: "topsites",
    maxRows: 4,
    rowsPref: "topSitesRows",
    eventSource: "TOP_SITES",
  },
];

const PREFS_AFTER_SECTIONS = ({
  newNewtabExperienceEnabled,
  customizationMenuEnabled,
}) => [
  {
    id: "snippets",
    pref: {
      feed: "feeds.snippets",
      titleString: "home-prefs-snippets-header",
      descString:
        newNewtabExperienceEnabled || customizationMenuEnabled
          ? "home-prefs-snippets-description-new"
          : "home-prefs-snippets-description",
    },
    icon: "info",
    eventSource: "SNIPPETS",
  },
];

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
        action._target.browser.ownerGlobal.openPreferences("paneHome");
        break;
      // This is used to open the web extension settings page for an extension
      case at.OPEN_WEBEXT_SETTINGS:
        action._target.browser.ownerGlobal.BrowserOpenAddonsMgr(
          `addons://detail/${encodeURIComponent(action.data)}`
        );
        break;
    }
  }

  handleDiscoverySettings(sections) {
    // Deep copy object to not modify original Sections state in store
    let sectionsCopy = JSON.parse(JSON.stringify(sections));
    sectionsCopy.forEach(obj => {
      if (obj.id === "topstories") {
        obj.rowsPref = "";
      }
    });
    return sectionsCopy;
  }

  setupUserEvent(element, eventSource) {
    element.addEventListener("command", e => {
      const { checked } = e.target;
      if (typeof checked === "boolean") {
        this.store.dispatch(
          ac.UserEvent({
            event: "PREF_CHANGED",
            source: eventSource,
            value: { status: checked, menu_source: "ABOUT_PREFERENCES" },
          })
        );
      }
    });
  }

  observe(window) {
    const discoveryStreamConfig = this.store.getState().DiscoveryStream.config;
    let sections = this.store.getState().Sections;

    if (discoveryStreamConfig.enabled) {
      sections = this.handleDiscoverySettings(sections);
    }

    const featureConfig = NimbusFeatures.newtab.getValue() || {};

    this.renderPreferences(window, [
      ...PREFS_BEFORE_SECTIONS(featureConfig),
      ...sections,
      ...PREFS_AFTER_SECTIONS(featureConfig),
    ]);
  }

  /**
   * Render preferences to an about:preferences content window with the provided
   * preferences structure.
   */
  renderPreferences({ document, Preferences, gHomePane }, prefStructure) {
    // Helper to create a new element and append it
    const createAppend = (tag, parent, options) =>
      parent.appendChild(document.createXULElement(tag, options));

    // Helper to get fluentIDs sometimes encase in an object
    const getString = message =>
      typeof message !== "object" ? message : message.id;

    // Helper to link a UI element to a preference for updating
    const linkPref = (element, name, type) => {
      const fullPref = `browser.newtabpage.activity-stream.${name}`;
      element.setAttribute("preference", fullPref);
      Preferences.add({ id: fullPref, type });

      // Prevent changing the UI if the preference can't be changed
      element.disabled = Preferences.get(fullPref).locked;
    };

    // Insert a new group immediately after the homepage one
    const homeGroup = document.getElementById("homepageGroup");
    const contentsGroup = homeGroup.insertAdjacentElement(
      "afterend",
      homeGroup.cloneNode()
    );
    contentsGroup.id = "homeContentsGroup";
    contentsGroup.setAttribute("data-subcategory", "contents");
    const homeHeader = createAppend("label", contentsGroup).appendChild(
      document.createElementNS(HTML_NS, "h2")
    );
    document.l10n.setAttributes(homeHeader, "home-prefs-content-header");

    const homeDescription = createAppend("description", contentsGroup);
    document.l10n.setAttributes(
      homeDescription,
      "home-prefs-content-description"
    );

    // Add preferences for each section
    prefStructure.forEach(sectionData => {
      const {
        id,
        pref: prefData,
        icon = "webextension",
        maxRows,
        rowsPref,
        shouldHidePref,
        eventSource,
      } = sectionData;
      const { feed: name, titleString = {}, descString, nestedPrefs = [] } =
        prefData || {};

      // Don't show any sections that we don't want to expose in preferences UI
      if (shouldHidePref) {
        return;
      }

      // Use full icon spec for certain protocols or fall back to packaged icon
      const iconUrl = !icon.search(/^(chrome|moz-extension|resource):/)
        ? icon
        : `chrome://activity-stream/content/data/content/assets/glyph-${icon}-16.svg`;

      // Add the main preference for turning on/off a section
      const sectionVbox = createAppend("vbox", contentsGroup);
      sectionVbox.setAttribute("data-subcategory", id);
      const checkbox = createAppend("checkbox", sectionVbox);
      checkbox.classList.add("section-checkbox");
      checkbox.setAttribute("src", iconUrl);
      // Setup a user event if we have an event source for this pref.
      if (eventSource) {
        this.setupUserEvent(checkbox, eventSource);
      }
      document.l10n.setAttributes(
        checkbox,
        getString(titleString),
        titleString.values
      );

      linkPref(checkbox, name, "bool");

      // Specially add a link for stories
      if (id === "topstories") {
        const sponsoredHbox = createAppend("hbox", sectionVbox);
        sponsoredHbox.setAttribute("align", "center");
        sponsoredHbox.appendChild(checkbox);
        checkbox.classList.add("tail-with-learn-more");

        const link = createAppend("label", sponsoredHbox, { is: "text-link" });
        link.classList.add("learn-sponsored");
        link.setAttribute("href", sectionData.pref.learnMore.link.href);
        document.l10n.setAttributes(link, sectionData.pref.learnMore.link.id);
      }

      // Add more details for the section (e.g., description, more prefs)
      const detailVbox = createAppend("vbox", sectionVbox);
      detailVbox.classList.add("indent");
      if (descString) {
        const label = createAppend("label", detailVbox);
        label.classList.add("indent");
        document.l10n.setAttributes(
          label,
          getString(descString),
          descString.values
        );

        // Add a rows dropdown if we have a pref to control and a maximum
        if (rowsPref && maxRows) {
          const detailHbox = createAppend("hbox", detailVbox);
          detailHbox.setAttribute("align", "center");
          label.setAttribute("flex", 1);
          detailHbox.appendChild(label);

          // Add box so the search tooltip is positioned correctly
          const tooltipBox = createAppend("hbox", detailHbox);

          // Add appropriate number of localized entries to the dropdown
          const menulist = createAppend("menulist", tooltipBox);
          menulist.setAttribute("crop", "none");
          const menupopup = createAppend("menupopup", menulist);
          for (let num = 1; num <= maxRows; num++) {
            const item = createAppend("menuitem", menupopup);
            document.l10n.setAttributes(
              item,
              "home-prefs-sections-rows-option",
              { num }
            );
            item.setAttribute("value", num);
          }
          linkPref(menulist, rowsPref, "int");
        }
      }

      const subChecks = [];
      const fullName = `browser.newtabpage.activity-stream.${sectionData.pref.feed}`;
      const pref = Preferences.get(fullName);

      // Add a checkbox pref for any nested preferences
      nestedPrefs.forEach(nested => {
        const subcheck = createAppend("checkbox", detailVbox);
        // Setup a user event if we have an event source for this pref.
        if (nested.eventSource) {
          this.setupUserEvent(subcheck, nested.eventSource);
        }
        subcheck.classList.add("indent");
        document.l10n.setAttributes(subcheck, nested.titleString);
        linkPref(subcheck, nested.name, "bool");
        subChecks.push(subcheck);
        subcheck.disabled = !pref._value;
        subcheck.hidden = nested.hidden;
      });

      // Disable any nested checkboxes if the parent pref is not enabled.
      pref.on("change", () => {
        subChecks.forEach(subcheck => {
          subcheck.disabled = !pref._value;
        });
      });
    });

    // Update the visibility of the Restore Defaults btn based on checked prefs
    gHomePane.toggleRestoreDefaultsBtn();
  }
};

this.PREFERENCES_LOADED_EVENT = PREFERENCES_LOADED_EVENT;
const EXPORTED_SYMBOLS = ["AboutPreferences", "PREFERENCES_LOADED_EVENT"];
