/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { actionCreators: ac, actionTypes: at } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
const { getDefaultOptions } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamStorage.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
});

/*
 * Generators for built in sections, keyed by the pref name for their feed.
 * Built in sections may depend on options stored as serialised JSON in the pref
 * `${feed_pref_name}.options`.
 */
const BUILT_IN_SECTIONS = ({
  newNewtabExperienceEnabled,
  customizationMenuEnabled,
}) => ({
  "feeds.section.topstories": options => ({
    id: "topstories",
    pref: {
      titleString: {
        id: "home-prefs-recommended-by-header",
        values: { provider: options.provider_name },
      },
      descString: {
        id:
          newNewtabExperienceEnabled || customizationMenuEnabled
            ? "home-prefs-recommended-by-description-new"
            : "home-prefs-recommended-by-description-update",
        values: { provider: options.provider_name },
      },
      nestedPrefs: options.show_spocs
        ? [
            {
              name: "showSponsored",
              titleString: "home-prefs-recommended-by-option-sponsored-stories",
              icon: "icon-info",
              eventSource: "POCKET_SPOCS",
            },
          ]
        : [],
      learnMore: {
        link: {
          href: "https://getpocket.com/firefox/new_tab_learn_more",
          id: "home-prefs-recommended-by-learn-more",
        },
      },
    },
    shouldHidePref: options.hidden,
    eventSource: "TOP_STORIES",
    icon: options.provider_icon,
    title: {
      id: "newtab-section-header-pocket",
      values: { provider: options.provider_name },
    },
    learnMore: {
      link: {
        href: "https://getpocket.com/firefox/new_tab_learn_more",
        message: { id: "newtab-pocket-learn-more" },
      },
    },
    compactCards: false,
    rowsPref: "section.topstories.rows",
    maxRows: 4,
    availableLinkMenuOptions: [
      "CheckBookmarkOrArchive",
      "CheckSavedToPocket",
      "Separator",
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "Separator",
      "BlockUrl",
    ],
    emptyState: {
      message: {
        id: "newtab-empty-section-topstories",
        values: { provider: options.provider_name },
      },
      icon: "check",
    },
    shouldSendImpressionStats: true,
    dedupeFrom: ["highlights"],
  }),
  "feeds.section.highlights": options => ({
    id: "highlights",
    pref: {
      titleString: {
        id:
          newNewtabExperienceEnabled || customizationMenuEnabled
            ? "home-prefs-recent-activity-header"
            : "home-prefs-highlights-header",
      },
      descString: {
        id:
          newNewtabExperienceEnabled || customizationMenuEnabled
            ? "home-prefs-recent-activity-description"
            : "home-prefs-highlights-description",
      },
      nestedPrefs: [
        {
          name: "section.highlights.includeVisited",
          titleString: "home-prefs-highlights-option-visited-pages",
        },
        {
          name: "section.highlights.includeBookmarks",
          titleString: "home-prefs-highlights-options-bookmarks",
        },
        {
          name: "section.highlights.includeDownloads",
          titleString: "home-prefs-highlights-option-most-recent-download",
        },
        {
          name: "section.highlights.includePocket",
          titleString: "home-prefs-highlights-option-saved-to-pocket",
          hidden: !Services.prefs.getBoolPref(
            "extensions.pocket.enabled",
            true
          ),
        },
      ],
    },
    shouldHidePref: false,
    eventSource: "HIGHLIGHTS",
    icon: "highlights",
    title: {
      id:
        newNewtabExperienceEnabled || customizationMenuEnabled
          ? "newtab-section-header-recent-activity"
          : "newtab-section-header-highlights",
    },
    compactCards: true,
    rowsPref: "section.highlights.rows",
    maxRows: 4,
    emptyState: {
      message: { id: "newtab-empty-section-highlights" },
      icon: "highlights",
    },
    shouldSendImpressionStats: false,
  }),
});

const SectionsManager = {
  ACTIONS_TO_PROXY: ["WEBEXT_CLICK", "WEBEXT_DISMISS"],
  CONTEXT_MENU_PREFS: { CheckSavedToPocket: "extensions.pocket.enabled" },
  CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES: {
    history: [
      "CheckBookmark",
      "CheckSavedToPocket",
      "Separator",
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "Separator",
      "BlockUrl",
      "DeleteUrl",
    ],
    bookmark: [
      "CheckBookmark",
      "CheckSavedToPocket",
      "Separator",
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "Separator",
      "BlockUrl",
      "DeleteUrl",
    ],
    pocket: [
      "ArchiveFromPocket",
      "CheckSavedToPocket",
      "Separator",
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "Separator",
      "BlockUrl",
    ],
    download: [
      "OpenFile",
      "ShowFile",
      "Separator",
      "GoToDownloadPage",
      "CopyDownloadLink",
      "Separator",
      "RemoveDownload",
      "BlockUrl",
    ],
  },
  initialized: false,
  sections: new Map(),
  async init(prefs = {}, storage) {
    this._storage = storage;
    const featureConfig = NimbusFeatures.newtab.getValue() || {};

    for (const feedPrefName of Object.keys(BUILT_IN_SECTIONS(featureConfig))) {
      const optionsPrefName = `${feedPrefName}.options`;
      await this.addBuiltInSection(feedPrefName, prefs[optionsPrefName]);

      this._dedupeConfiguration = [];
      this.sections.forEach(section => {
        if (section.dedupeFrom) {
          this._dedupeConfiguration.push({
            id: section.id,
            dedupeFrom: section.dedupeFrom,
          });
        }
      });
    }

    Object.keys(this.CONTEXT_MENU_PREFS).forEach(k =>
      Services.prefs.addObserver(this.CONTEXT_MENU_PREFS[k], this)
    );

    this.initialized = true;
    this.emit(this.INIT);
  },
  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        for (const pref of Object.keys(this.CONTEXT_MENU_PREFS)) {
          if (data === this.CONTEXT_MENU_PREFS[pref]) {
            this.updateSections();
          }
        }
        break;
    }
  },
  updateSectionPrefs(id, collapsed) {
    const section = this.sections.get(id);
    if (!section) {
      return;
    }

    const updatedSection = Object.assign({}, section, {
      pref: Object.assign({}, section.pref, collapsed),
    });
    this.updateSection(id, updatedSection, true);
  },
  async addBuiltInSection(feedPrefName, optionsPrefValue = "{}") {
    let options;
    let storedPrefs;
    const featureConfig = NimbusFeatures.newtab.getValue() || {};
    try {
      options = JSON.parse(optionsPrefValue);
    } catch (e) {
      options = {};
      Cu.reportError(`Problem parsing options pref for ${feedPrefName}`);
    }
    try {
      storedPrefs = (await this._storage.get(feedPrefName)) || {};
    } catch (e) {
      storedPrefs = {};
      Cu.reportError(`Problem getting stored prefs for ${feedPrefName}`);
    }
    const defaultSection = BUILT_IN_SECTIONS(featureConfig)[feedPrefName](
      options
    );
    const section = Object.assign({}, defaultSection, {
      pref: Object.assign(
        {},
        defaultSection.pref,
        getDefaultOptions(storedPrefs)
      ),
    });
    section.pref.feed = feedPrefName;
    this.addSection(section.id, Object.assign(section, { options }));
  },
  addSection(id, options) {
    this.updateLinkMenuOptions(options, id);
    this.sections.set(id, options);
    this.emit(this.ADD_SECTION, id, options);
  },
  removeSection(id) {
    this.emit(this.REMOVE_SECTION, id);
    this.sections.delete(id);
  },
  enableSection(id, isStartup = false) {
    this.updateSection(id, { enabled: true }, true, isStartup);
    this.emit(this.ENABLE_SECTION, id);
  },
  disableSection(id) {
    this.updateSection(
      id,
      { enabled: false, rows: [], initialized: false },
      true
    );
    this.emit(this.DISABLE_SECTION, id);
  },
  updateSections() {
    this.sections.forEach((section, id) =>
      this.updateSection(id, section, true)
    );
  },
  updateSection(id, options, shouldBroadcast, isStartup = false) {
    this.updateLinkMenuOptions(options, id);
    if (this.sections.has(id)) {
      const optionsWithDedupe = Object.assign({}, options, {
        dedupeConfigurations: this._dedupeConfiguration,
      });
      this.sections.set(id, Object.assign(this.sections.get(id), options));
      this.emit(
        this.UPDATE_SECTION,
        id,
        optionsWithDedupe,
        shouldBroadcast,
        isStartup
      );
    }
  },

  /**
   * Save metadata to places db and add a visit for that URL.
   */
  updateBookmarkMetadata({ url }) {
    this.sections.forEach((section, id) => {
      if (id === "highlights") {
        // Skip Highlights cards, we already have that metadata.
        return;
      }
      if (section.rows) {
        section.rows.forEach(card => {
          if (
            card.url === url &&
            card.description &&
            card.title &&
            card.image
          ) {
            PlacesUtils.history.update({
              url: card.url,
              title: card.title,
              description: card.description,
              previewImageURL: card.image,
            });
            // Highlights query skips bookmarks with no visits.
            PlacesUtils.history.insert({
              url,
              title: card.title,
              visits: [{}],
            });
          }
        });
      }
    });
  },

  /**
   * Sets the section's context menu options. These are all available context menu
   * options minus the ones that are tied to a pref (see CONTEXT_MENU_PREFS) set
   * to false.
   *
   * @param options section options
   * @param id      section ID
   */
  updateLinkMenuOptions(options, id) {
    if (options.availableLinkMenuOptions) {
      options.contextMenuOptions = options.availableLinkMenuOptions.filter(
        o =>
          !this.CONTEXT_MENU_PREFS[o] ||
          Services.prefs.getBoolPref(this.CONTEXT_MENU_PREFS[o])
      );
    }

    // Once we have rows, we can give each card it's own context menu based on it's type.
    // We only want to do this for highlights because those have different data types.
    // All other sections (built by the web extension API) will have the same context menu per section
    if (options.rows && id === "highlights") {
      this._addCardTypeLinkMenuOptions(options.rows);
    }
  },

  /**
   * Sets each card in highlights' context menu options based on the card's type.
   * (See types.js for a list of types)
   *
   * @param rows section rows containing a type for each card
   */
  _addCardTypeLinkMenuOptions(rows) {
    for (let card of rows) {
      if (!this.CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES[card.type]) {
        Cu.reportError(
          `No context menu for highlight type ${card.type} is configured`
        );
      } else {
        card.contextMenuOptions = this.CONTEXT_MENU_OPTIONS_FOR_HIGHLIGHT_TYPES[
          card.type
        ];

        // Remove any options that shouldn't be there based on CONTEXT_MENU_PREFS.
        // For example: If the Pocket extension is disabled, we should remove the CheckSavedToPocket option
        // for each card that has it
        card.contextMenuOptions = card.contextMenuOptions.filter(
          o =>
            !this.CONTEXT_MENU_PREFS[o] ||
            Services.prefs.getBoolPref(this.CONTEXT_MENU_PREFS[o])
        );
      }
    }
  },

  /**
   * Update a specific section card by its url. This allows an action to be
   * broadcast to all existing pages to update a specific card without having to
   * also force-update the rest of the section's cards and state on those pages.
   *
   * @param id              The id of the section with the card to be updated
   * @param url             The url of the card to update
   * @param options         The options to update for the card
   * @param shouldBroadcast Whether or not to broadcast the update
   * @param isStartup       If this update is during startup.
   */
  updateSectionCard(id, url, options, shouldBroadcast, isStartup = false) {
    if (this.sections.has(id)) {
      const card = this.sections.get(id).rows.find(elem => elem.url === url);
      if (card) {
        Object.assign(card, options);
      }
      this.emit(
        this.UPDATE_SECTION_CARD,
        id,
        url,
        options,
        shouldBroadcast,
        isStartup
      );
    }
  },
  removeSectionCard(sectionId, url) {
    if (!this.sections.has(sectionId)) {
      return;
    }
    const rows = this.sections
      .get(sectionId)
      .rows.filter(row => row.url !== url);
    this.updateSection(sectionId, { rows }, true);
  },
  onceInitialized(callback) {
    if (this.initialized) {
      callback();
    } else {
      this.once(this.INIT, callback);
    }
  },
  uninit() {
    Object.keys(this.CONTEXT_MENU_PREFS).forEach(k =>
      Services.prefs.removeObserver(this.CONTEXT_MENU_PREFS[k], this)
    );
    SectionsManager.initialized = false;
  },
};

for (const action of [
  "ACTION_DISPATCHED",
  "ADD_SECTION",
  "REMOVE_SECTION",
  "ENABLE_SECTION",
  "DISABLE_SECTION",
  "UPDATE_SECTION",
  "UPDATE_SECTION_CARD",
  "INIT",
  "UNINIT",
]) {
  SectionsManager[action] = action;
}

EventEmitter.decorate(SectionsManager);

class SectionsFeed {
  constructor() {
    this.init = this.init.bind(this);
    this.onAddSection = this.onAddSection.bind(this);
    this.onRemoveSection = this.onRemoveSection.bind(this);
    this.onUpdateSection = this.onUpdateSection.bind(this);
    this.onUpdateSectionCard = this.onUpdateSectionCard.bind(this);
  }

  init() {
    SectionsManager.on(SectionsManager.ADD_SECTION, this.onAddSection);
    SectionsManager.on(SectionsManager.REMOVE_SECTION, this.onRemoveSection);
    SectionsManager.on(SectionsManager.UPDATE_SECTION, this.onUpdateSection);
    SectionsManager.on(
      SectionsManager.UPDATE_SECTION_CARD,
      this.onUpdateSectionCard
    );
    // Catch any sections that have already been added
    SectionsManager.sections.forEach((section, id) =>
      this.onAddSection(
        SectionsManager.ADD_SECTION,
        id,
        section,
        true /* isStartup */
      )
    );
  }

  uninit() {
    SectionsManager.uninit();
    SectionsManager.emit(SectionsManager.UNINIT);
    SectionsManager.off(SectionsManager.ADD_SECTION, this.onAddSection);
    SectionsManager.off(SectionsManager.REMOVE_SECTION, this.onRemoveSection);
    SectionsManager.off(SectionsManager.UPDATE_SECTION, this.onUpdateSection);
    SectionsManager.off(
      SectionsManager.UPDATE_SECTION_CARD,
      this.onUpdateSectionCard
    );
  }

  onAddSection(event, id, options, isStartup = false) {
    if (options) {
      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.SECTION_REGISTER,
          data: Object.assign({ id }, options),
          meta: {
            isStartup,
          },
        })
      );

      // Make sure the section is in sectionOrder pref. Otherwise, prepend it.
      const orderedSections = this.orderedSectionIds;
      if (!orderedSections.includes(id)) {
        orderedSections.unshift(id);
        this.store.dispatch(
          ac.SetPref("sectionOrder", orderedSections.join(","))
        );
      }
    }
  }

  onRemoveSection(event, id) {
    this.store.dispatch(
      ac.BroadcastToContent({ type: at.SECTION_DEREGISTER, data: id })
    );
  }

  onUpdateSection(
    event,
    id,
    options,
    shouldBroadcast = false,
    isStartup = false
  ) {
    if (options) {
      const action = {
        type: at.SECTION_UPDATE,
        data: Object.assign(options, { id }),
        meta: {
          isStartup,
        },
      };
      this.store.dispatch(
        shouldBroadcast
          ? ac.BroadcastToContent(action)
          : ac.AlsoToPreloaded(action)
      );
    }
  }

  onUpdateSectionCard(
    event,
    id,
    url,
    options,
    shouldBroadcast = false,
    isStartup = false
  ) {
    if (options) {
      const action = {
        type: at.SECTION_UPDATE_CARD,
        data: { id, url, options },
        meta: {
          isStartup,
        },
      };
      this.store.dispatch(
        shouldBroadcast
          ? ac.BroadcastToContent(action)
          : ac.AlsoToPreloaded(action)
      );
    }
  }

  get orderedSectionIds() {
    return this.store.getState().Prefs.values.sectionOrder.split(",");
  }

  get enabledSectionIds() {
    let sections = this.store
      .getState()
      .Sections.filter(section => section.enabled)
      .map(s => s.id);
    // Top Sites is a special case. Append if the feed is enabled.
    if (this.store.getState().Prefs.values["feeds.topsites"]) {
      sections.push("topsites");
    }
    return sections;
  }

  moveSection(id, direction) {
    const orderedSections = this.orderedSectionIds;
    const enabledSections = this.enabledSectionIds;
    let index = orderedSections.indexOf(id);
    orderedSections.splice(index, 1);
    if (direction > 0) {
      // "Move Down"
      while (index < orderedSections.length) {
        // If the section at the index is enabled/visible, insert moved section after.
        // Otherwise, move on to the next spot and check it.
        if (enabledSections.includes(orderedSections[index++])) {
          break;
        }
      }
    } else {
      // "Move Up"
      while (index > 0) {
        // If the section at the previous index is enabled/visible, insert moved section there.
        // Otherwise, move on to the previous spot and check it.
        index--;
        if (enabledSections.includes(orderedSections[index])) {
          break;
        }
      }
    }

    orderedSections.splice(index, 0, id);
    this.store.dispatch(ac.SetPref("sectionOrder", orderedSections.join(",")));
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        SectionsManager.onceInitialized(this.init);
        break;
      // Wait for pref values, as some sections have options stored in prefs
      case at.PREFS_INITIAL_VALUES:
        SectionsManager.init(
          action.data,
          this.store.dbStorage.getDbTable("sectionPrefs")
        );
        break;
      case at.PREF_CHANGED: {
        if (action.data) {
          const matched = action.data.name.match(
            /^(feeds.section.(\S+)).options$/i
          );
          if (matched) {
            await SectionsManager.addBuiltInSection(
              matched[1],
              action.data.value
            );
            this.store.dispatch({
              type: at.SECTION_OPTIONS_CHANGED,
              data: matched[2],
            });
          }
        }
        break;
      }
      case at.UPDATE_SECTION_PREFS:
        SectionsManager.updateSectionPrefs(action.data.id, action.data.value);
        break;
      case at.PLACES_BOOKMARK_ADDED:
        SectionsManager.updateBookmarkMetadata(action.data);
        break;
      case at.WEBEXT_DISMISS:
        if (action.data) {
          SectionsManager.removeSectionCard(
            action.data.source,
            action.data.url
          );
        }
        break;
      case at.SECTION_DISABLE:
        SectionsManager.disableSection(action.data);
        break;
      case at.SECTION_ENABLE:
        SectionsManager.enableSection(action.data);
        break;
      case at.SECTION_MOVE:
        this.moveSection(action.data.id, action.data.direction);
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
    if (
      SectionsManager.ACTIONS_TO_PROXY.includes(action.type) &&
      SectionsManager.sections.size > 0
    ) {
      SectionsManager.emit(
        SectionsManager.ACTION_DISPATCHED,
        action.type,
        action.data
      );
    }
  }
}

this.SectionsFeed = SectionsFeed;
this.SectionsManager = SectionsManager;
const EXPORTED_SYMBOLS = ["SectionsFeed", "SectionsManager"];
