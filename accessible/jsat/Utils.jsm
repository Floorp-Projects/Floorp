/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported Utils, Logger, PivotContext, PrefCache */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Services", // jshint ignore:line
  "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "Rect", // jshint ignore:line
  "resource://gre/modules/Geometry.jsm");
ChromeUtils.defineModuleGetter(this, "Roles", // jshint ignore:line
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "Events", // jshint ignore:line
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "Relations", // jshint ignore:line
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "States", // jshint ignore:line
  "resource://gre/modules/accessibility/Constants.jsm");
ChromeUtils.defineModuleGetter(this, "PluralForm", // jshint ignore:line
  "resource://gre/modules/PluralForm.jsm");

var EXPORTED_SYMBOLS = ["Utils", "Logger", "PivotContext", "PrefCache"]; // jshint ignore:line

var Utils = { // jshint ignore:line
  _buildAppMap: {
    "{3c2e2abc-06d4-11e1-ac3b-374f68613e61}": "b2g",
    "{d1bfe7d9-c01e-4237-998b-7b5f960a4314}": "graphene",
    "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}": "browser",
    "{aa3c5121-dab2-40e2-81ca-7ea25febc110}": "mobile/android"
  },

  get AccService() {
    if (!this._AccService) {
      this._AccService = Cc["@mozilla.org/accessibilityService;1"].
        getService(Ci.nsIAccessibilityService);
    }

    return this._AccService;
  },

  set MozBuildApp(value) {
    this._buildApp = value;
  },

  get MozBuildApp() {
    if (!this._buildApp) {
      this._buildApp = this._buildAppMap[Services.appinfo.ID];
    }
    return this._buildApp;
  },

  get ScriptName() {
    if (!this._ScriptName) {
      this._ScriptName =
        (Services.appinfo.processType == 2) ? "AccessFuContent" : "AccessFu";
    }
    return this._ScriptName;
  },

  get AndroidSdkVersion() {
    if (!this._AndroidSdkVersion) {
      if (Services.appinfo.OS == "Android") {
        this._AndroidSdkVersion = Services.sysinfo.getPropertyAsInt32(
          "version");
      } else {
        // Most useful in desktop debugging.
        this._AndroidSdkVersion = 16;
      }
    }
    return this._AndroidSdkVersion;
  },

  set AndroidSdkVersion(value) {
    // When we want to mimic another version.
    this._AndroidSdkVersion = value;
  },

  getCurrentBrowser: function getCurrentBrowser(aWindow) {
    let win = aWindow ||
      Services.wm.getMostRecentWindow("navigator:browser") ||
      Services.wm.getMostRecentWindow("navigator:geckoview");
    return win.document.querySelector("browser[type=content][primary=true]");
  },

  getAllMessageManagers: function getAllMessageManagers(aWindow) {
    let messageManagers = new Set();

    function collectLeafMessageManagers(mm) {
      for (let i = 0; i < mm.childCount; i++) {
        let childMM = mm.getChildAt(i);

        if ("sendAsyncMessage" in childMM) {
          messageManagers.add(childMM);
        } else {
          collectLeafMessageManagers(childMM);
        }
      }
    }

    collectLeafMessageManagers(aWindow.messageManager);

    let browser = this.getCurrentBrowser(aWindow);
    let document = browser ? browser.contentDocument : null;

    if (document) {
      let remoteframes = document.querySelectorAll("iframe");

      for (let i = 0; i < remoteframes.length; ++i) {
        let mm = this.getMessageManager(remoteframes[i]);
        if (mm) {
          messageManagers.add(mm);
        }
      }

    }

    return messageManagers;
  },

  get isContentProcess() {
    delete this.isContentProcess;
    this.isContentProcess =
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;
    return this.isContentProcess;
  },

  localize: function localize(aOutput) {
    let outputArray = Array.isArray(aOutput) ? aOutput : [aOutput];
    let localized =
      outputArray.map(details => this.stringBundle.get(details));
    // Clean up the white space.
    return localized.filter(word => word).map(word => word.trim()).
      filter(trimmed => trimmed);
  },

  get stringBundle() {
    delete this.stringBundle;
    let bundle = Services.strings.createBundle(
      "chrome://global/locale/AccessFu.properties");
    this.stringBundle = {
      get: function stringBundle_get(aDetails = {}) {
        if (!aDetails || typeof aDetails === "string") {
          return aDetails;
        }
        let str = "";
        let string = aDetails.string;
        if (!string) {
          return str;
        }
        try {
          let args = aDetails.args;
          let count = aDetails.count;
          if (args) {
            str = bundle.formatStringFromName(string, args, args.length);
          } else {
            str = bundle.GetStringFromName(string);
          }
          if (count) {
            str = PluralForm.get(count, str);
            str = str.replace("#1", count);
          }
        } catch (e) {
          Logger.debug("Failed to get a string from a bundle for", string);
        } finally {
          return str;
        }
      }
    };
    return this.stringBundle;
  },

  getMessageManager: function getMessageManager(aBrowser) {
    let browser = aBrowser || this.getCurrentBrowser();
    try {
      return browser.frameLoader.messageManager;
    } catch (x) {
      return null;
    }
  },

  getState: function getState(aAccessibleOrEvent) {
    if (aAccessibleOrEvent instanceof Ci.nsIAccessibleStateChangeEvent) {
      return new State(
        aAccessibleOrEvent.isExtraState ? 0 : aAccessibleOrEvent.state,
        aAccessibleOrEvent.isExtraState ? aAccessibleOrEvent.state : 0);
    }
      let state = {};
      let extState = {};
      aAccessibleOrEvent.getState(state, extState);
      return new State(state.value, extState.value);

  },

  getAttributes: function getAttributes(aAccessible) {
    let attributes = {};

    if (aAccessible && aAccessible.attributes) {
      let attributesEnum = aAccessible.attributes.enumerate();

      // Populate |attributes| object with |aAccessible|'s attribute key-value
      // pairs.
      while (attributesEnum.hasMoreElements()) {
        let attribute = attributesEnum.getNext().QueryInterface(
          Ci.nsIPropertyElement);
        attributes[attribute.key] = attribute.value;
      }
    }

    return attributes;
  },

  getVirtualCursor: function getVirtualCursor(aDocument) {
    let doc = (aDocument instanceof Ci.nsIAccessible) ? aDocument :
      this.AccService.getAccessibleFor(aDocument);

    return doc.QueryInterface(Ci.nsIAccessibleDocument).virtualCursor;
  },

  getContentResolution: function _getContentResolution(aAccessible) {
    let res = { value: 1 };
    aAccessible.document.window.QueryInterface(
      Ci.nsIInterfaceRequestor).getInterface(
      Ci.nsIDOMWindowUtils).getResolution(res);
    return res.value;
  },

  getBounds: function getBounds(aAccessible) {
    let objX = {}, objY = {}, objW = {}, objH = {};
    aAccessible.getBounds(objX, objY, objW, objH);

    return new Rect(objX.value, objY.value, objW.value, objH.value);
  },

  getTextBounds: function getTextBounds(aAccessible, aStart, aEnd,
                                        aPreserveContentScale) {
    let accText = aAccessible.QueryInterface(Ci.nsIAccessibleText);
    let objX = {}, objY = {}, objW = {}, objH = {};
    accText.getRangeExtents(aStart, aEnd, objX, objY, objW, objH,
      Ci.nsIAccessibleCoordinateType.COORDTYPE_SCREEN_RELATIVE);

    return new Rect(objX.value, objY.value, objW.value, objH.value);
  },

  isInSubtree: function isInSubtree(aAccessible, aSubTreeRoot) {
    let acc = aAccessible;

    // If aSubTreeRoot is an accessible document, we will only walk up the
    // ancestry of documents and skip everything else.
    if (aSubTreeRoot instanceof Ci.nsIAccessibleDocument) {
      while (acc) {
        let parentDoc = acc instanceof Ci.nsIAccessibleDocument ?
          acc.parentDocument : acc.document;
        if (parentDoc === aSubTreeRoot) {
          return true;
        }
        acc = parentDoc;
      }
      return false;
    }

    while (acc) {
      if (acc == aSubTreeRoot) {
        return true;
      }

      try {
        acc = acc.parent;
      } catch (x) {
        Logger.debug("Failed to get parent:", x);
        acc = null;
      }
    }

    return false;
  },

  visibleChildCount: function visibleChildCount(aAccessible) {
    let count = 0;
    for (let child = aAccessible.firstChild; child; child = child.nextSibling) {
      ++count;
    }
    return count;
  },

  isAliveAndVisible: function isAliveAndVisible(aAccessible, aIsOnScreen) {
    if (!aAccessible) {
      return false;
    }

    try {
      let state = this.getState(aAccessible);
      if (state.contains(States.DEFUNCT) || state.contains(States.INVISIBLE) ||
          (aIsOnScreen && state.contains(States.OFFSCREEN))) {
        return false;
      }
    } catch (x) {
      return false;
    }

    return true;
  },

  matchAttributeValue: function matchAttributeValue(aAttributeValue, values) {
    let attrSet = new Set(aAttributeValue.split(" "));
    for (let value of values) {
      if (attrSet.has(value)) {
        return value;
      }
    }
  },

  getLandmarkName: function getLandmarkName(aAccessible) {
    return this.matchRoles(aAccessible, [
      "banner",
      "complementary",
      "contentinfo",
      "main",
      "navigation",
      "search"
    ]);
  },

  getMathRole: function getMathRole(aAccessible) {
    return this.matchRoles(aAccessible, [
      "base",
      "close-fence",
      "denominator",
      "numerator",
      "open-fence",
      "overscript",
      "presubscript",
      "presuperscript",
      "root-index",
      "subscript",
      "superscript",
      "underscript"
    ]);
  },

  matchRoles: function matchRoles(aAccessible, aRoles) {
    let roles = this.getAttributes(aAccessible)["xml-roles"];
    if (!roles) {
      return;
    }

    // Looking up a role that would match any in the provided roles.
    return this.matchAttributeValue(roles, aRoles);
  },

  getEmbeddedControl: function getEmbeddedControl(aLabel) {
    if (aLabel) {
      let relation = aLabel.getRelationByType(Relations.LABEL_FOR);
      for (let i = 0; i < relation.targetsCount; i++) {
        let target = relation.getTarget(i);
        if (target.parent === aLabel) {
          return target;
        }
      }
    }

    return null;
  },

  isListItemDecorator: function isListItemDecorator(aStaticText,
                                                    aExcludeOrdered) {
    let parent = aStaticText.parent;
    if (aExcludeOrdered && parent.parent.DOMNode.nodeName === "OL") {
      return false;
    }

    return parent.role === Roles.LISTITEM && parent.childCount > 1 &&
      aStaticText.indexInParent === 0;
  },

  isActivatableOnFingerUp: function isActivatableOnFingerUp(aAccessible) {
    if (aAccessible.role === Roles.KEY) {
      return true;
    }
    let quick_activate = this.getAttributes(aAccessible)["moz-quick-activate"];
    return quick_activate && JSON.parse(quick_activate);
  }
};

/**
 * State object used internally to process accessible's states.
 * @param {Number} aBase     Base state.
 * @param {Number} aExtended Extended state.
 */
function State(aBase, aExtended) {
  this.base = aBase;
  this.extended = aExtended;
}

State.prototype = {
  contains: function State_contains(other) {
    return !!(this.base & other.base || this.extended & other.extended);
  },
  toString: function State_toString() {
    let stateStrings = Utils.AccService.
      getStringStates(this.base, this.extended);
    let statesArray = new Array(stateStrings.length);
    for (let i = 0; i < statesArray.length; i++) {
      statesArray[i] = stateStrings.item(i);
    }
    return "[" + statesArray.join(", ") + "]";
  }
};

var Logger = { // jshint ignore:line
  GESTURE: -1,
  DEBUG: 0,
  INFO: 1,
  WARNING: 2,
  ERROR: 3,
  _LEVEL_NAMES: ["GESTURE", "DEBUG", "INFO", "WARNING", "ERROR"],

  logLevel: 1, // INFO;

  test: false,

  log: function log(aLogLevel) {
    if (aLogLevel < this.logLevel) {
      return;
    }

    let args = Array.prototype.slice.call(arguments, 1);
    let message = (typeof(args[0]) === "function" ? args[0]() : args).join(" ");
    message = "[" + Utils.ScriptName + "] " + this._LEVEL_NAMES[aLogLevel + 1] +
      " " + message + "\n";
    dump(message);
    // Note: used for testing purposes. If |this.test| is true, also log to
    // the console service.
    if (this.test) {
      try {
        Services.console.logStringMessage(message);
      } catch (ex) {
        // There was an exception logging to the console service.
      }
    }
  },

  info: function info() {
    this.log.apply(
      this, [this.INFO].concat(Array.prototype.slice.call(arguments)));
  },

  gesture: function gesture() {
    this.log.apply(
      this, [this.GESTURE].concat(Array.prototype.slice.call(arguments)));
  },

  debug: function debug() {
    this.log.apply(
      this, [this.DEBUG].concat(Array.prototype.slice.call(arguments)));
  },

  warning: function warning() {
    this.log.apply(
      this, [this.WARNING].concat(Array.prototype.slice.call(arguments)));
  },

  error: function error() {
    this.log.apply(
      this, [this.ERROR].concat(Array.prototype.slice.call(arguments)));
  },

  logException: function logException(
    aException, aErrorMessage = "An exception has occured") {
    try {
      let stackMessage = "";
      if (aException.stack) {
        stackMessage = "  " + aException.stack.replace(/\n/g, "\n  ");
      } else if (aException.location) {
        let frame = aException.location;
        let stackLines = [];
        while (frame && frame.lineNumber) {
          stackLines.push(
            "  " + frame.name + "@" + frame.filename + ":" + frame.lineNumber);
          frame = frame.caller;
        }
        stackMessage = stackLines.join("\n");
      } else {
        stackMessage =
          "(" + aException.fileName + ":" + aException.lineNumber + ")";
      }
      this.error(aErrorMessage + ":\n " +
                 aException.message + "\n" +
                 stackMessage);
    } catch (x) {
      this.error(x);
    }
  },

  accessibleToString: function accessibleToString(aAccessible) {
    if (!aAccessible) {
      return "[ null ]";
    }

    try {
      return "[ " + Utils.AccService.getStringRole(aAccessible.role) +
        " | " + aAccessible.name + " ]";
    } catch (x) {
      return "[ defunct ]";
    }
  },

  eventToString: function eventToString(aEvent) {
    let str = Utils.AccService.getStringEventType(aEvent.eventType);
    if (aEvent.eventType == Events.STATE_CHANGE) {
      let event = aEvent.QueryInterface(Ci.nsIAccessibleStateChangeEvent);
      let stateStrings = event.isExtraState ?
        Utils.AccService.getStringStates(0, event.state) :
        Utils.AccService.getStringStates(event.state, 0);
      str += " (" + stateStrings.item(0) + ")";
    }

    if (aEvent.eventType == Events.VIRTUALCURSOR_CHANGED) {
      let event = aEvent.QueryInterface(
        Ci.nsIAccessibleVirtualCursorChangeEvent);
      let pivot = aEvent.accessible.QueryInterface(
        Ci.nsIAccessibleDocument).virtualCursor;
      str += " (" + this.accessibleToString(event.oldAccessible) + " -> " +
        this.accessibleToString(pivot.position) + ")";
    }

    return str;
  },

  statesToString: function statesToString(aAccessible) {
    return Utils.getState(aAccessible).toString();
  },

  dumpTree: function dumpTree(aLogLevel, aRootAccessible) {
    if (aLogLevel < this.logLevel) {
      return;
    }

    this._dumpTreeInternal(aLogLevel, aRootAccessible, 0);
  },

  _dumpTreeInternal:
    function _dumpTreeInternal(aLogLevel, aAccessible, aIndent) {
      let indentStr = "";
      for (let i = 0; i < aIndent; i++) {
        indentStr += " ";
      }
      this.log(aLogLevel, indentStr,
               this.accessibleToString(aAccessible),
               "(" + this.statesToString(aAccessible) + ")");
      for (let i = 0; i < aAccessible.childCount; i++) {
        this._dumpTreeInternal(aLogLevel, aAccessible.getChildAt(i),
          aIndent + 1);
      }
    }
};

/**
 * PivotContext: An object that generates and caches context information
 * for a given accessible and its relationship with another accessible.
 *
 * If the given accessible is a label for a nested control, then this
 * context will represent the nested control instead of the label.
 * With the exception of bounds calculation, which will use the containing
 * label. In this case the |accessible| field would be the embedded control,
 * and the |accessibleForBounds| field would be the label.
 */
function PivotContext(aAccessible, aOldAccessible, // jshint ignore:line
  aStartOffset, aEndOffset, aIgnoreAncestry = false,
  aIncludeInvisible = false) {
  this._accessible = aAccessible;
  this._nestedControl = Utils.getEmbeddedControl(aAccessible);
  this._oldAccessible =
    this._isDefunct(aOldAccessible) ? null : aOldAccessible;
  this.startOffset = aStartOffset;
  this.endOffset = aEndOffset;
  this._ignoreAncestry = aIgnoreAncestry;
  this._includeInvisible = aIncludeInvisible;
}

PivotContext.prototype = {
  get accessible() {
    // If the current pivot accessible has a nested control,
    // make this context use it publicly.
    return this._nestedControl || this._accessible;
  },

  get oldAccessible() {
    return this._oldAccessible;
  },

  get isNestedControl() {
    return !!this._nestedControl;
  },

  get accessibleForBounds() {
    return this._accessible;
  },

  get textAndAdjustedOffsets() {
    if (this.startOffset === -1 && this.endOffset === -1) {
      return null;
    }

    if (!this._textAndAdjustedOffsets) {
      let result = {startOffset: this.startOffset,
                    endOffset: this.endOffset,
                    text: this._accessible.QueryInterface(Ci.nsIAccessibleText).
                          getText(0,
                            Ci.nsIAccessibleText.TEXT_OFFSET_END_OF_TEXT)};
      let hypertextAcc = this._accessible.QueryInterface(
        Ci.nsIAccessibleHyperText);

      // Iterate through the links in backwards order so text replacements don't
      // affect the offsets of links yet to be processed.
      for (let i = hypertextAcc.linkCount - 1; i >= 0; i--) {
        let link = hypertextAcc.getLinkAt(i);
        let linkText = "";
        if (link instanceof Ci.nsIAccessibleText) {
          linkText = link.QueryInterface(Ci.nsIAccessibleText).
                          getText(0,
                            Ci.nsIAccessibleText.TEXT_OFFSET_END_OF_TEXT);
        }

        let start = link.startIndex;
        let end = link.endIndex;
        for (let offset of ["startOffset", "endOffset"]) {
          if (this[offset] >= end) {
            result[offset] += linkText.length - (end - start);
          }
        }
        result.text = result.text.substring(0, start) + linkText +
                      result.text.substring(end);
      }

      this._textAndAdjustedOffsets = result;
    }

    return this._textAndAdjustedOffsets;
  },

  /**
   * Get a list of |aAccessible|'s ancestry up to the root.
   * @param  {nsIAccessible} aAccessible.
   * @return {Array} Ancestry list.
   */
  _getAncestry: function _getAncestry(aAccessible) {
    let ancestry = [];
    let parent = aAccessible;
    try {
      while (parent && (parent = parent.parent)) {
       ancestry.push(parent);
      }
    } catch (x) {
      // A defunct accessible will raise an exception geting parent.
      Logger.debug("Failed to get parent:", x);
    }
    return ancestry.reverse();
  },

  /**
   * A list of the old accessible's ancestry.
   */
  get oldAncestry() {
    if (!this._oldAncestry) {
      if (!this._oldAccessible || this._ignoreAncestry) {
        this._oldAncestry = [];
      } else {
        this._oldAncestry = this._getAncestry(this._oldAccessible);
        this._oldAncestry.push(this._oldAccessible);
      }
    }
    return this._oldAncestry;
  },

  /**
   * A list of the current accessible's ancestry.
   */
  get currentAncestry() {
    if (!this._currentAncestry) {
      this._currentAncestry = this._ignoreAncestry ? [] :
        this._getAncestry(this.accessible);
    }
    return this._currentAncestry;
  },

  /*
   * This is a list of the accessible's ancestry up to the common ancestor
   * of the accessible and the old accessible. It is useful for giving the
   * user context as to where they are in the heirarchy.
   */
  get newAncestry() {
    if (!this._newAncestry) {
      this._newAncestry = this._ignoreAncestry ? [] :
        this.currentAncestry.filter(
          (currentAncestor, i) => currentAncestor !== this.oldAncestry[i]);
    }
    return this._newAncestry;
  },

  /*
   * Traverse the accessible's subtree in pre or post order.
   * It only includes the accessible's visible chidren.
   * Note: needSubtree is a function argument that can be used to determine
   * whether aAccessible's subtree is required.
   */
  _traverse: function* _traverse(aAccessible, aPreorder, aStop) {
    if (aStop && aStop(aAccessible)) {
      return;
    }
    let child = aAccessible.firstChild;
    while (child) {
      let include = true;
      if (include) {
        if (aPreorder) {
          yield child;
          for (let node of this._traverse(child, aPreorder, aStop)) {
            yield node;
          }
        } else {
          for (let node of this._traverse(child, aPreorder, aStop)) {
            yield node;
          }
          yield child;
        }
      }
      child = child.nextSibling;
    }
  },

  /**
   * Get interaction hints for the context ancestry.
   * @return {Array} Array of interaction hints.
   */
  get interactionHints() {
    let hints = [];
    this.newAncestry.concat(this.accessible).reverse().forEach(aAccessible => {
      let hint = Utils.getAttributes(aAccessible)["moz-hint"];
      if (hint) {
        hints.push(hint);
      } else if (aAccessible.actionCount > 0) {
        hints.push({
          string: Utils.AccService.getStringRole(
            aAccessible.role).replace(/\s/g, "") + "-hint"
        });
      }
    });
    return hints;
  },

  /*
   * A subtree generator function, used to generate a flattened
   * list of the accessible's subtree in pre or post order.
   * It only includes the accessible's visible chidren.
   * @param {boolean} aPreorder A flag for traversal order. If true, traverse
   * in preorder; if false, traverse in postorder.
   * @param {function} aStop An optional function, indicating whether subtree
   * traversal should stop.
   */
  subtreeGenerator: function subtreeGenerator(aPreorder, aStop) {
    return this._traverse(this.accessible, aPreorder, aStop);
  },

  getCellInfo: function getCellInfo(aAccessible) {
    if (!this._cells) {
      this._cells = new WeakMap();
    }

    let domNode = aAccessible.DOMNode;
    if (this._cells.has(domNode)) {
      return this._cells.get(domNode);
    }

    let cellInfo = {};
    let getAccessibleCell = function getAccessibleCell(aAccessible) {
      if (!aAccessible) {
        return null;
      }
      if (![
            Roles.CELL,
            Roles.COLUMNHEADER,
            Roles.ROWHEADER,
            Roles.MATHML_CELL
          ].includes(aAccessible.role)) {
          return null;
      }
      try {
        return aAccessible.QueryInterface(Ci.nsIAccessibleTableCell);
      } catch (x) {
        Logger.logException(x);
        return null;
      }
    };
    let getHeaders = function* getHeaders(aHeaderCells) {
      let enumerator = aHeaderCells.enumerate();
      while (enumerator.hasMoreElements()) {
        yield enumerator.getNext().QueryInterface(Ci.nsIAccessible).name;
      }
    };

    cellInfo.current = getAccessibleCell(aAccessible);

    if (!cellInfo.current) {
      Logger.warning(aAccessible,
        "does not support nsIAccessibleTableCell interface.");
      this._cells.set(domNode, null);
      return null;
    }

    let table = cellInfo.current.table;
    if (table.isProbablyForLayout()) {
      this._cells.set(domNode, null);
      return null;
    }

    cellInfo.previous = null;
    let oldAncestry = this.oldAncestry.reverse();
    let ancestor = oldAncestry.shift();
    while (!cellInfo.previous && ancestor) {
      let cell = getAccessibleCell(ancestor);
      if (cell && cell.table === table) {
        cellInfo.previous = cell;
      }
      ancestor = oldAncestry.shift();
    }

    if (cellInfo.previous) {
      cellInfo.rowChanged = cellInfo.current.rowIndex !==
        cellInfo.previous.rowIndex;
      cellInfo.columnChanged = cellInfo.current.columnIndex !==
        cellInfo.previous.columnIndex;
    } else {
      cellInfo.rowChanged = true;
      cellInfo.columnChanged = true;
    }

    cellInfo.rowExtent = cellInfo.current.rowExtent;
    cellInfo.columnExtent = cellInfo.current.columnExtent;
    cellInfo.columnIndex = cellInfo.current.columnIndex;
    cellInfo.rowIndex = cellInfo.current.rowIndex;

    cellInfo.columnHeaders = [];
    if (cellInfo.columnChanged && cellInfo.current.role !==
      Roles.COLUMNHEADER) {
      cellInfo.columnHeaders = [...getHeaders(cellInfo.current.columnHeaderCells)];
    }
    cellInfo.rowHeaders = [];
    if (cellInfo.rowChanged &&
        (cellInfo.current.role === Roles.CELL ||
         cellInfo.current.role === Roles.MATHML_CELL)) {
      cellInfo.rowHeaders = [...getHeaders(cellInfo.current.rowHeaderCells)];
    }

    this._cells.set(domNode, cellInfo);
    return cellInfo;
  },

  get bounds() {
    if (!this._bounds) {
      this._bounds = Utils.getBounds(this.accessibleForBounds);
    }

    return this._bounds.clone();
  },

  _isDefunct: function _isDefunct(aAccessible) {
    try {
      return Utils.getState(aAccessible).contains(States.DEFUNCT);
    } catch (x) {
      return true;
    }
  }
};

function PrefCache(aName, aCallback, aRunCallbackNow) { // jshint ignore:line
  this.name = aName;
  this.callback = aCallback;

  let branch = Services.prefs;
  this.value = this._getValue(branch);

  if (this.callback && aRunCallbackNow) {
    try {
      this.callback(this.name, this.value, true);
    } catch (x) {
      Logger.logException(x);
    }
  }

  branch.addObserver(aName, this, true);
}

PrefCache.prototype = {
  _getValue: function _getValue(aBranch) {
    try {
      if (!this.type) {
        this.type = aBranch.getPrefType(this.name);
      }
      switch (this.type) {
        case Ci.nsIPrefBranch.PREF_STRING:
          return aBranch.getCharPref(this.name);
        case Ci.nsIPrefBranch.PREF_INT:
          return aBranch.getIntPref(this.name);
        case Ci.nsIPrefBranch.PREF_BOOL:
          return aBranch.getBoolPref(this.name);
        default:
          return null;
      }
    } catch (x) {
      // Pref does not exist.
      return null;
    }
  },

  observe: function observe(aSubject) {
    this.value = this._getValue(aSubject.QueryInterface(Ci.nsIPrefBranch));
    Logger.info("pref changed", this.name, this.value);
    if (this.callback) {
      try {
        this.callback(this.name, this.value, false);
      } catch (x) {
        Logger.logException(x);
      }
    }
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                           Ci.nsISupportsWeakReference])
};
