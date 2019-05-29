/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Localization: "resource://gre/modules/Localization.jsm",
});

/**
 * Executes a XUL command on the top window. Called by the callbacks in each
 * TouchBarInput.
 * @param {string} commandName
 *        A XUL command.
 * @param {string} telemetryKey
 *        A string describing the command, sent for telemetry purposes.
 *        Intended to be shorter and more readable than the XUL command.
 */
function execCommand(commandName, telemetryKey) {
  let win = BrowserWindowTracker.getTopWindow();
  let command = win.document.getElementById(commandName);
  if (command) {
    command.doCommand();
  }
  let telemetry = Services.telemetry.getHistogramById("TOUCHBAR_BUTTON_PRESSES");
  telemetry.add(telemetryKey);
}

/**
 * Static helper function to convert a hexadecimal string to its integer
 * value. Used to convert colours to a format accepted by Apple's NSColor code.
 * @param {string} hexString
 *        A hexadecimal string, optionally beginning with '#'.
 */
function hexToInt(hexString) {
  if (!hexString) {
    return null;
  }
  if (hexString.charAt(0) == "#") {
    hexString = hexString.slice(1);
  }
  let val = parseInt(hexString, 16);
  return isNaN(val) ? null : val;
}

/**
 * An object containing all implemented TouchBarInput objects.
 */
const kBuiltInInputs = {
  Back: {
    title: "back",
    image: "back.pdf",
    type: "button",
    callback: () => execCommand("Browser:Back", "Back"),
  },
  Forward: {
    title: "forward",
    image: "forward.pdf",
    type: "button",
    callback: () => execCommand("Browser:Forward", "Forward"),
  },
  Reload: {
    title: "reload",
    image: "refresh.pdf",
    type: "button",
    callback: () => execCommand("Browser:Reload", "Reload"),
  },
  Home: {
    title: "home",
    image: "home.pdf",
    type: "button",
    callback: () => execCommand("Browser:Home", "Home"),
  },
  Fullscreen: {
    title: "fullscreen",
    image: "fullscreen.pdf",
    type: "button",
    callback: () => execCommand("View:FullScreen", "Fullscreen"),
  },
  Find: {
    title: "find",
    image: "search.pdf",
    type: "button",
    callback: () => execCommand("cmd_find", "Find"),
  },
  NewTab: {
    title: "new-tab",
    image: "new.pdf",
    type: "button",
    callback: () => execCommand("cmd_newNavigatorTabNoEvent", "NewTab"),
  },
  Sidebar: {
    title: "open-bookmarks-sidebar",
    image: "sidebar-left.pdf",
    type: "button",
    callback: () => execCommand("viewBookmarksSidebar", "Sidebar"),
  },
  AddBookmark: {
    title: "add-bookmark",
    image: "bookmark.pdf",
    type: "button",
    callback: () => execCommand("Browser:AddBookmarkAs", "AddBookmark"),
  },
  ReaderView: {
    title: "reader-view",
    image: "reader-mode.pdf",
    type: "button",
    callback: () => execCommand("View:ReaderView", "ReaderView"),
    disabled: true,  // Updated when the page is found to be Reader View-able.
  },
  OpenLocation: {
    title: "open-location",
    image: "search.pdf",
    type: "mainButton",
    callback: () => execCommand("Browser:OpenLocation", "OpenLocation"),
  },
  // This is a special-case `type: "scrubber"` element.
  // Scrubbers are not yet generally implemented.
  // See follow-up bug 1502539.
  Share: {
    title: "share",
    type: "scrubber",
    image: "share.pdf",
    callback: () => execCommand("cmd_share", "Share"),
  },
};

const kHelperObservers = new Set(["bookmark-icon-updated", "reader-mode-available",
  "touchbar-location-change", "quit-application", "intl:app-locales-changed"]);

/**
 * JS-implemented TouchBarHelper class.
 * Provides services to the Mac Touch Bar.
 */
class TouchBarHelper {
  constructor() {
    for (let topic of kHelperObservers) {
      Services.obs.addObserver(this, topic);
    }

    XPCOMUtils.defineLazyPreferenceGetter(this, "_touchBarLayout",
      "ui.touchbar.layout", "Back,Forward,Reload,OpenLocation,NewTab,Share");
  }

  destructor() {
    for (let topic of kHelperObservers) {
      Services.obs.removeObserver(this, topic);
    }
  }

  get activeTitle() {
    let tabbrowser = this.window.ownerGlobal.gBrowser;
    let activeTitle;
    if (tabbrowser) {
      activeTitle = tabbrowser.selectedBrowser.contentTitle;
    }
    return activeTitle;
  }

  get layout() {
    let prefArray = this.storedLayout;
    let layoutItems = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);

    for (let inputName of prefArray) {
      if (typeof kBuiltInInputs[inputName].context == "function") {
        inputName = kBuiltInInputs[inputName].context();
      }
      let input = this.getTouchBarInput(inputName);
      if (!input) {
        continue;
      }
      layoutItems.appendElement(input);
    }

    return layoutItems;
  }

  get window() {
    return BrowserWindowTracker.getTopWindow();
  }

  get bookmarkingUI() {
    return this.window.BookmarkingUI;
  }

  /**
   * Returns a string array of the user's Touch Bar layout preference.
   * Set by a pref ui.touchbar.layout and returned in an array.
   */
  get storedLayout() {
    let prefArray = this._touchBarLayout.split(",");
    prefArray = prefArray.map(str => str.trim());
    // Remove duplicates.
    prefArray = Array.from(new Set(prefArray));

    // Remove unimplemented/mispelled inputs.
    prefArray = prefArray.filter(input =>
      Object.keys(kBuiltInInputs).includes(input));
    this._storedLayout = prefArray;
    return this._storedLayout;
  }

  getTouchBarInput(inputName) {
    // inputName might be undefined if an input's context() returns undefined.
    if (!inputName || !kBuiltInInputs.hasOwnProperty(inputName)) {
      return null;
    }

    // context() may specify that one named input "point" to another.
    if (typeof kBuiltInInputs[inputName].context == "function") {
      inputName = kBuiltInInputs[inputName].context();
    }

    if (!inputName || !kBuiltInInputs.hasOwnProperty(inputName)) {
      return null;
    }

    let inputData = kBuiltInInputs[inputName];

    let item = new TouchBarInput(inputData);

    // Skip localization if there is already a cached localized title.
    if (kBuiltInInputs[inputName].hasOwnProperty("localTitle")) {
      return item;
    }

    // Async l10n fills in the localized input labels after the initial load.
    this._l10n.formatValue(item.key).then((result) => {
      item.title = result;
      kBuiltInInputs[inputName].localTitle = result; // Cache result.
      // Checking this.window since this callback can fire after all windows are closed.
      if (this.window) {
        let baseWindow = this.window.docShell.treeOwner.QueryInterface(Ci.nsIBaseWindow);
        let updater = Cc["@mozilla.org/widget/touchbarupdater;1"]
                        .getService(Ci.nsITouchBarUpdater);
        updater.updateTouchBarInputs(baseWindow, [item]);
      }
    });

    return item;
  }

  /**
   * Fetches a specific Touch Bar Input by name and updates it on the Touch Bar.
   * @param {string} inputName
   *        A key to a value in the kBuiltInInputs object in this file.
   * @param {...*} [otherInputs] (optional)
   *        Additional keys to values in the kBuiltInInputs object in this file.
   */
  _updateTouchBarInputs(...inputNames) {
    if (!this.window) {
      return;
    }

    let inputs = [];
    for (let inputName of inputNames) {
      // Updating Touch Bar items isn't cheap in terms of performance, so
      // only consider this input if it's actually part of the layout.
      if (!this._storedLayout.includes(inputName)) {
        continue;
      }

      let input = this.getTouchBarInput(inputName);
      if (!input) {
        continue;
      }
      inputs.push(input);
    }

    let baseWindow = this.window.docShell.treeOwner.QueryInterface(Ci.nsIBaseWindow);
    let updater = Cc["@mozilla.org/widget/touchbarupdater;1"]
                    .getService(Ci.nsITouchBarUpdater);
    updater.updateTouchBarInputs(baseWindow, inputs);
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "touchbar-location-change":
        this.activeUrl = data;
        // ReaderView button is disabled on every location change since
        // Reader View must determine if the new page can be Reader Viewed.
        kBuiltInInputs.ReaderView.disabled = !data.startsWith("about:reader");
        kBuiltInInputs.Back.disabled = !this.window.gBrowser.canGoBack;
        kBuiltInInputs.Forward.disabled = !this.window.gBrowser.canGoForward;
        this._updateTouchBarInputs("ReaderView", "Back", "Forward");
        break;
      case "bookmark-icon-updated":
        data == "starred" ?
          kBuiltInInputs.AddBookmark.image = "bookmark-filled.pdf"
          : kBuiltInInputs.AddBookmark.image = "bookmark.pdf";
        this._updateTouchBarInputs("AddBookmark");
        break;
      case "reader-mode-available":
        kBuiltInInputs.ReaderView.disabled = false;
        this._updateTouchBarInputs("ReaderView");
        break;
      case "intl:app-locales-changed":
        // On locale change, refresh all inputs after loading new localTitle.
        for (let inputName of this._storedLayout) {
          delete kBuiltInInputs[inputName].localTitle;
        }
        this._updateTouchBarInputs(...this._storedLayout);
        break;
      case "quit-application":
        this.destructor();
        break;
    }
  }
}

const helperProto = TouchBarHelper.prototype;
helperProto.classDescription = "Services the Mac Touch Bar";
helperProto.classID = Components.ID("{ea109912-3acc-48de-b679-c23b6a122da5}");
helperProto.contractID = "@mozilla.org/widget/touchbarhelper;1";
helperProto.QueryInterface = ChromeUtils.generateQI([Ci.nsITouchBarHelper]);
helperProto._l10n = new Localization(["browser/touchbar/touchbar.ftl"]);

/**
 * A representation of a Touch Bar input.
 * Uses async update() in lieu of a constructor to accomodate async l10n code.
 *     @param {object} input
 *            An object representing a Touch Bar Input.
 *            Contains listed properties.
 *     @param {string} input.title
 *            The lookup key for the button's localized text title.
 *     @param {string} input.image
 *            The name of a icon file added to
 *            /widget/cocoa/resources/touchbar_icons.
 *     @param {string} input.type
 *            The type of Touch Bar input represented by the object.
 *            One of `button`, `mainButton`.
 *     @param {Function} input.callback
 *            A callback invoked when a touchbar item is touched.
 *     @param {string} input.color (optional)
 *            A string in hex format specifying the button's background color.
 *            If omitted, the default background color is used.
 *     @param {bool} input.disabled (optional)
 *            If `true`, the Touch Bar input is greyed out and inoperable.
 */
class TouchBarInput {
  constructor(input) {
    this._key = input.title;
    this._title = input.hasOwnProperty("localTitle") ? input.localTitle : "";
    this._image = input.image;
    this._type = input.type;
    this._callback = input.callback;
    this._color = hexToInt(input.color);
    this._disabled = input.hasOwnProperty("disabled") ? input.disabled : false;
  }

  get key() {
    return this._key;
  }
  get title() {
    return this._title;
  }
  set title(title) {
    this._title = title;
  }
  get image() {
    return this._image;
  }
  set image(image) {
    this._image = image;
  }
  get type() {
    return this._type == "" ? "button" : this._type;
  }
  set type(type) {
    this._type = type;
  }
  get callback() {
    return this._callback;
  }
  set callback(callback) {
    this._callback = callback;
  }
  get color() {
    return this._color;
  }
  set color(color) {
    this._color = this.hexToInt(color);
  }
  get disabled() {
    return this._disabled || false;
  }
  set disabled(disabled) {
    this._disabled = disabled;
  }
}

const inputProto = TouchBarInput.prototype;
inputProto.classDescription = "Represents an input on the Mac Touch Bar";
inputProto.classID = Components.ID("{77441d17-f29c-49d7-982f-f20a5ab5a900}");
inputProto.contractID = "@mozilla.org/widget/touchbarinput;1";
inputProto.QueryInterface = ChromeUtils.generateQI([Ci.nsITouchBarInput]);

this.NSGetFactory =
  XPCOMUtils.generateNSGetFactory([TouchBarHelper, TouchBarInput]);
