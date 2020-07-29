/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarSearchOneOffs"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  SearchOneOffs: "resource:///modules/SearchOneOffs.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

// Maps from RESULT_SOURCE values to { restrict, pref } objects.
const LOCAL_MODES = new Map([
  [
    UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    {
      restrict: UrlbarTokenizer.RESTRICT.BOOKMARK,
      pref: "suggest.bookmark",
    },
  ],
  [
    UrlbarUtils.RESULT_SOURCE.TABS,
    {
      restrict: UrlbarTokenizer.RESTRICT.OPENPAGE,
      pref: "suggest.openpage",
    },
  ],
  [
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    {
      restrict: UrlbarTokenizer.RESTRICT.HISTORY,
      pref: "suggest.history",
    },
  ],
]);

/**
 * The one-off search buttons in the urlbar.
 */
class UrlbarSearchOneOffs extends SearchOneOffs {
  /**
   * Constructor.
   *
   * @param {UrlbarView} view
   *   The parent UrlbarView.
   */
  constructor(view) {
    super(view.panel.querySelector(".search-one-offs"));
    this.view = view;
    this.input = view.input;
    UrlbarPrefs.addObserver(pref => this._onPrefChanged(pref));
  }

  /**
   * Returns the local search mode one-off buttons.
   *
   * @returns {array}
   *   The local one-off buttons.
   */
  get localButtons() {
    return this.getSelectableButtons(false).filter(b => b.source);
  }

  /**
   * Enables (shows) or disables (hides) the one-offs.
   *
   * @param {boolean} enable
   *   True to enable, false to disable.
   */
  enable(enable) {
    if (enable) {
      this.telemetryOrigin = "urlbar";
      this.style.display = "";
      this.textbox = this.view.input.inputField;
      if (this.view.isOpen) {
        this._rebuild();
      }
      this.view.controller.addQueryListener(this);
    } else {
      this.telemetryOrigin = null;
      this.style.display = "none";
      this.textbox = null;
      this.view.controller.removeQueryListener(this);
    }
  }

  /**
   * Query listener method.  Delegates to the superclass.
   */
  onViewOpen() {
    this._on_popupshowing();
  }

  /**
   * Query listener method.  Delegates to the superclass.
   */
  onViewClose() {
    this._on_popuphidden();
  }

  /**
   * @returns {boolean}
   *   True if the view is open.
   */
  get isViewOpen() {
    return this.view.isOpen;
  }

  /**
   * @returns {number}
   *   The selected index in the view.
   */
  get selectedViewIndex() {
    return this.view.selectedElementIndex;
  }

  /**
   * Sets the selected index in the view.
   *
   * @param {number} val
   *   The selected index or -1 if no selection.
   */
  set selectedViewIndex(val) {
    this.view.selectedElementIndex = val;
  }

  /**
   * Closes the view.
   */
  closeView() {
    if (this.view) {
      this.view.close();
    }
  }

  /**
   * Called when a one-off is clicked or the "Search in New Tab" context menu
   * item is picked.  This is not called for the settings button.
   *
   * @param {event} event
   *   The event that triggered the pick.
   * @param {nsISearchEngine|SearchEngine|UrlbarUtils.RESULT_SOURCE} engineOrSource
   *   The engine that was picked, or for local search mode sources, the source
   *   that was picked as a UrlbarUtils.RESULT_SOURCE value.
   * @param {boolean} forceNewTab
   *   True if the search results page should be loaded in a new tab.
   */
  handleSearchCommand(event, engineOrSource, forceNewTab = false) {
    if (!this.view.oneOffsRefresh) {
      let { where, params } = this._whereToOpen(event, forceNewTab);
      this.input.handleCommand(event, where, params);
      return;
    }

    this.input.setSearchMode(engineOrSource);
    this.selectedButton = null;
    this.input.startQuery({
      allowAutofill: false,
      event,
    });
  }

  /**
   * Sets the tooltip for a one-off button with an engine.  This should set
   * either the `tooltiptext` attribute or the relevant l10n ID.
   *
   * @param {element} button
   *   The one-off button.
   */
  setTooltipForEngineButton(button) {
    let aliases = UrlbarSearchUtils.aliasesForEngine(button.engine);
    if (!aliases.length) {
      super.setTooltipForEngineButton(button);
      return;
    }
    this.document.l10n.setAttributes(
      button,
      "search-one-offs-engine-with-alias",
      {
        engineName: button.engine.name,
        alias: aliases[0],
      }
    );
  }

  /**
   * Overrides _rebuildEngineList to add the local one-offs.
   *
   * @param {array} engines
   *    The search engines to add.
   */
  _rebuildEngineList(engines) {
    super._rebuildEngineList(engines);

    if (!this.view.oneOffsRefresh || !UrlbarPrefs.get("update2.localOneOffs")) {
      return;
    }

    for (let [source, { restrict, pref }] of LOCAL_MODES) {
      if (!UrlbarPrefs.get(pref)) {
        // By design, don't show a local one-off when the user has disabled its
        // corresponding pref.
        continue;
      }
      let name = UrlbarUtils.getResultSourceName(source);
      let button = this.document.createXULElement("button");
      button.id = `urlbar-engine-one-off-item-${name}`;
      button.setAttribute("class", "searchbar-engine-one-off-item");
      this.document.l10n.setAttributes(button, `search-one-offs-${name}`, {
        restrict,
      });
      button.source = source;
      this.buttons.appendChild(button);
    }
  }

  /**
   * Overrides the superclass's click listener to handle clicks on local
   * one-offs in addition to engine one-offs.
   *
   * @param {event} event
   *   The click event.
   */
  _on_click(event) {
    if (!this.view.oneOffsRefresh) {
      super._on_click(event);
      return;
    }

    // Ignore right clicks.
    if (event.button == 2) {
      return;
    }

    let button = event.originalTarget;
    let engineOrSource = button.engine || button.source;
    if (!engineOrSource) {
      return;
    }

    this.handleSearchCommand(event, engineOrSource);
  }

  /**
   * Called when a pref tracked by UrlbarPrefs changes.
   *
   * @param {string} changedPref
   *   The name of the pref, relative to `browser.urlbar.` if the pref is in
   *   that branch.
   */
  _onPrefChanged(changedPref) {
    // Null out this._engines when the local-one-offs-related prefs change so
    // that they rebuild themselves the next time the view opens.
    let prefs = [...LOCAL_MODES.values()].map(({ pref }) => pref);
    prefs.push("update2", "update2.localOneOffs", "update2.oneOffsRefresh");
    if (prefs.includes(changedPref)) {
      this._engines = null;
    }
  }
}
