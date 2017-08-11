"use strict";

const EventEmitter = require("devtools/shared/old-event-emitter");
const Services = require("Services");
const { Preferences } = require("resource://gre/modules/Preferences.jsm");
const OPTIONS_SHOWN_EVENT = "options-shown";
const OPTIONS_HIDDEN_EVENT = "options-hidden";
const PREF_CHANGE_EVENT = "pref-changed";

/**
 * OptionsView constructor. Takes several options, all required:
 * - branchName: The name of the prefs branch, like "devtools.debugger."
 * - menupopup: The XUL `menupopup` item that contains the pref buttons.
 *
 * Fires an event, PREF_CHANGE_EVENT, with the preference name that changed as
 * the second argument. Fires events on opening/closing the XUL panel
 * (OPTIONS_SHOW_EVENT, OPTIONS_HIDDEN_EVENT) as the second argument in the
 * listener, used for tests mostly.
 */
const OptionsView = function (options = {}) {
  this.branchName = options.branchName;
  this.menupopup = options.menupopup;
  this.window = this.menupopup.ownerDocument.defaultView;
  let { document } = this.window;
  this.$ = document.querySelector.bind(document);
  this.$$ = (selector, parent = document) => parent.querySelectorAll(selector);
  // Get the corresponding button that opens the popup by looking
  // for an element with a `popup` attribute matching the menu's ID
  this.button = this.$(`[popup=${this.menupopup.getAttribute("id")}]`);

  this.prefObserver = new PrefObserver(this.branchName);

  EventEmitter.decorate(this);
};
exports.OptionsView = OptionsView;

OptionsView.prototype = {
  /**
   * Binds the events and observers for the OptionsView.
   */
  initialize: function () {
    let { MutationObserver } = this.window;
    this._onPrefChange = this._onPrefChange.bind(this);
    this._onOptionChange = this._onOptionChange.bind(this);
    this._onPopupShown = this._onPopupShown.bind(this);
    this._onPopupHidden = this._onPopupHidden.bind(this);

    // We use a mutation observer instead of a click handler
    // because the click handler is fired before the XUL menuitem updates its
    // checked status, which cascades incorrectly with the Preference observer.
    this.mutationObserver = new MutationObserver(this._onOptionChange);
    let observerConfig = { attributes: true, attributeFilter: ["checked"]};

    // Sets observers and default options for all options
    for (let $el of this.$$("menuitem", this.menupopup)) {
      let prefName = $el.getAttribute("data-pref");

      if (this.prefObserver.get(prefName)) {
        $el.setAttribute("checked", "true");
      } else {
        $el.removeAttribute("checked");
      }
      this.mutationObserver.observe($el, observerConfig);
    }

    // Listen to any preference change in the specified branch
    this.prefObserver.register();
    this.prefObserver.on(PREF_CHANGE_EVENT, this._onPrefChange);

    // Bind to menupopup's open and close event
    this.menupopup.addEventListener("popupshown", this._onPopupShown);
    this.menupopup.addEventListener("popuphidden", this._onPopupHidden);
  },

  /**
   * Removes event handlers for all of the option buttons and
   * preference observer.
   */
  destroy: function () {
    this.mutationObserver.disconnect();
    this.prefObserver.off(PREF_CHANGE_EVENT, this._onPrefChange);
    this.menupopup.removeEventListener("popupshown", this._onPopupShown);
    this.menupopup.removeEventListener("popuphidden", this._onPopupHidden);
  },

  /**
   * Returns the value for the specified `prefName`
   */
  getPref: function (prefName) {
    return this.prefObserver.get(prefName);
  },

  /**
   * Called when a preference is changed (either via clicking an option
   * button or by changing it in about:config). Updates the checked status
   * of the corresponding button.
   */
  _onPrefChange: function (_, prefName) {
    let $el = this.$(`menuitem[data-pref="${prefName}"]`, this.menupopup);
    let value = this.prefObserver.get(prefName);

    // If options panel does not contain a menuitem for the
    // pref, emit an event and do nothing.
    if (!$el) {
      this.emit(PREF_CHANGE_EVENT, prefName);
      return;
    }

    if (value) {
      $el.setAttribute("checked", value);
    } else {
      $el.removeAttribute("checked");
    }

    this.emit(PREF_CHANGE_EVENT, prefName);
  },

  /**
   * Mutation handler for handling a change on an options button.
   * Sets the preference accordingly.
   */
  _onOptionChange: function (mutations) {
    let { target } = mutations[0];
    let prefName = target.getAttribute("data-pref");
    let value = target.getAttribute("checked") === "true";

    this.prefObserver.set(prefName, value);
  },

  /**
   * Fired when the `menupopup` is opened, bound via XUL.
   * Fires an event used in tests.
   */
  _onPopupShown: function () {
    this.button.setAttribute("open", true);
    this.emit(OPTIONS_SHOWN_EVENT);
  },

  /**
   * Fired when the `menupopup` is closed, bound via XUL.
   * Fires an event used in tests.
   */
  _onPopupHidden: function () {
    this.button.removeAttribute("open");
    this.emit(OPTIONS_HIDDEN_EVENT);
  }
};

/**
 * Constructor for PrefObserver. Small helper for observing changes
 * on a preference branch. Takes a `branchName`, like "devtools.debugger."
 *
 * Fires an event of PREF_CHANGE_EVENT with the preference name that changed
 * as the second argument in the listener.
 */
const PrefObserver = function (branchName) {
  this.branchName = branchName;
  this.branch = Services.prefs.getBranch(branchName);
  EventEmitter.decorate(this);
};

PrefObserver.prototype = {
  /**
   * Returns `prefName`'s value. Does not require the branch name.
   */
  get: function (prefName) {
    let fullName = this.branchName + prefName;
    return Preferences.get(fullName);
  },
  /**
   * Sets `prefName`'s `value`. Does not require the branch name.
   */
  set: function (prefName, value) {
    let fullName = this.branchName + prefName;
    Preferences.set(fullName, value);
  },
  register: function () {
    this.branch.addObserver("", this);
  },
  unregister: function () {
    this.branch.removeObserver("", this);
  },
  observe: function (subject, topic, prefName) {
    this.emit(PREF_CHANGE_EVENT, prefName);
  }
};
