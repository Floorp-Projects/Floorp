/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
const {actionCreators: ac, actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
Cu.import("resource://gre/modules/EventEmitter.jsm");

/*
 * Generators for built in sections, keyed by the pref name for their feed.
 * Built in sections may depend on options stored as serialised JSON in the pref
 * `${feed_pref_name}.options`.
 */
const BUILT_IN_SECTIONS = {
  "feeds.section.topstories": options => ({
    id: "topstories",
    pref: {
      titleString: {id: "header_recommended_by", values: {provider: options.provider_name}},
      descString: {id: options.provider_description || "pocket_feedback_body"}
    },
    shouldHidePref:  options.hidden,
    eventSource: "TOP_STORIES",
    icon: options.provider_icon,
    title: {id: "header_recommended_by", values: {provider: options.provider_name}},
    maxRows: 1,
    availableContextMenuOptions: ["CheckBookmark", "SaveToPocket", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"],
    infoOption: {
      header: {id: options.provider_header || "pocket_feedback_header"},
      body: {id: options.provider_description || "pocket_feedback_body"},
      link: {href: options.info_link, id: "section_info_privacy_notice"}
    },
    emptyState: {
      message: {id: "topstories_empty_state", values: {provider: options.provider_name}},
      icon: "check"
    }
  })
};

const SectionsManager = {
  ACTIONS_TO_PROXY: ["SYSTEM_TICK", "NEW_TAB_LOAD"],
  CONTEXT_MENU_PREFS: {"SaveToPocket": "extensions.pocket.enabled"},
  initialized: false,
  sections: new Map(),
  init(prefs = {}) {
    for (const feedPrefName of Object.keys(BUILT_IN_SECTIONS)) {
      const optionsPrefName = `${feedPrefName}.options`;
      this.addBuiltInSection(feedPrefName, prefs[optionsPrefName]);
    }

    Object.keys(this.CONTEXT_MENU_PREFS).forEach(k =>
      Services.prefs.addObserver(this.CONTEXT_MENU_PREFS[k], this));

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
  addBuiltInSection(feedPrefName, optionsPrefValue = "{}") {
    let options;
    try {
      options = JSON.parse(optionsPrefValue);
    } catch (e) {
      options = {};
      Cu.reportError("Problem parsing options pref", e);
    }
    const section = BUILT_IN_SECTIONS[feedPrefName](options);
    section.pref.feed = feedPrefName;
    this.addSection(section.id, Object.assign(section, {options}));
  },
  addSection(id, options) {
    this.updateSectionContextMenuOptions(options);
    this.sections.set(id, options);
    this.emit(this.ADD_SECTION, id, options);
  },
  removeSection(id) {
    this.emit(this.REMOVE_SECTION, id);
    this.sections.delete(id);
  },
  enableSection(id) {
    this.updateSection(id, {enabled: true}, true);
    this.emit(this.ENABLE_SECTION, id);
  },
  disableSection(id) {
    this.updateSection(id, {enabled: false, rows: [], initialized: false}, true);
    this.emit(this.DISABLE_SECTION, id);
  },
  updateSections() {
    this.sections.forEach((section, id) => this.updateSection(id, section, true));
  },
  updateSection(id, options, shouldBroadcast) {
    this.updateSectionContextMenuOptions(options);

    if (this.sections.has(id)) {
      this.sections.set(id, Object.assign(this.sections.get(id), options));
      this.emit(this.UPDATE_SECTION, id, options, shouldBroadcast);
    }
  },

  /**
   * Sets the section's context menu options. These are all available context menu
   * options minus the ones that are tied to a pref (see CONTEXT_MENU_PREFS) set
   * to false.
   *
   * @param options section options
   */
  updateSectionContextMenuOptions(options) {
    if (options.availableContextMenuOptions) {
      options.contextMenuOptions = options.availableContextMenuOptions.filter(
        o => !this.CONTEXT_MENU_PREFS[o] || Services.prefs.getBoolPref(this.CONTEXT_MENU_PREFS[o]));
    }
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
      Services.prefs.removeObserver(this.CONTEXT_MENU_PREFS[k], this));
    SectionsManager.initialized = false;
  }
};

for (const action of [
  "ACTION_DISPATCHED",
  "ADD_SECTION",
  "REMOVE_SECTION",
  "ENABLE_SECTION",
  "DISABLE_SECTION",
  "UPDATE_SECTION",
  "INIT",
  "UNINIT"
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
  }

  init() {
    SectionsManager.on(SectionsManager.ADD_SECTION, this.onAddSection);
    SectionsManager.on(SectionsManager.REMOVE_SECTION, this.onRemoveSection);
    SectionsManager.on(SectionsManager.UPDATE_SECTION, this.onUpdateSection);
    // Catch any sections that have already been added
    SectionsManager.sections.forEach((section, id) =>
      this.onAddSection(SectionsManager.ADD_SECTION, id, section));
  }

  uninit() {
    SectionsManager.uninit();
    SectionsManager.emit(SectionsManager.UNINIT);
    SectionsManager.off(SectionsManager.ADD_SECTION, this.onAddSection);
    SectionsManager.off(SectionsManager.REMOVE_SECTION, this.onRemoveSection);
    SectionsManager.off(SectionsManager.UPDATE_SECTION, this.onUpdateSection);
  }

  onAddSection(event, id, options) {
    if (options) {
      this.store.dispatch(ac.BroadcastToContent({type: at.SECTION_REGISTER, data: Object.assign({id}, options)}));
    }
  }

  onRemoveSection(event, id) {
    this.store.dispatch(ac.BroadcastToContent({type: at.SECTION_DEREGISTER, data: id}));
  }

  onUpdateSection(event, id, options, shouldBroadcast = false) {
    if (options) {
      const action = {type: at.SECTION_UPDATE, data: Object.assign(options, {id})};
      this.store.dispatch(shouldBroadcast ? ac.BroadcastToContent(action) : action);
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        SectionsManager.onceInitialized(this.init);
        break;
      // Wait for pref values, as some sections have options stored in prefs
      case at.PREFS_INITIAL_VALUES:
        SectionsManager.init(action.data);
        break;
      case at.PREF_CHANGED:
        if (action.data && action.data.name.match(/^feeds.section.(\S+).options$/i)) {
          SectionsManager.addBuiltInSection(action.data.name.slice(0, -8), action.data.value);
        }
        break;
      case at.SECTION_DISABLE:
        SectionsManager.disableSection(action.data);
        break;
      case at.SECTION_ENABLE:
        SectionsManager.enableSection(action.data);
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
    if (SectionsManager.ACTIONS_TO_PROXY.includes(action.type) && SectionsManager.sections.size > 0) {
      SectionsManager.emit(SectionsManager.ACTION_DISPATCHED, action.type, action.data);
    }
  }
}

this.SectionsFeed = SectionsFeed;
this.SectionsManager = SectionsManager;
this.EXPORTED_SYMBOLS = ["SectionsFeed", "SectionsManager"];
