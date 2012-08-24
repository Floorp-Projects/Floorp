/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let EXPORTED_SYMBOLS = ["TabTitleAbridger"];

const Cu = Components.utils;
const ABRIDGMENT_PREF = "browser.tabs.cropTitleRedundancy";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gETLDService",
  "@mozilla.org/network/effective-tld-service;1",
  "nsIEffectiveTLDService");

function TabTitleAbridger(aBrowserWin) {
  this._tabbrowser = aBrowserWin.gBrowser;
}

TabTitleAbridger.prototype = {
  /*
   * Events we listen to.  We specifically do not listen for TabCreate, as we
   * get TabLabelModified at the appropriate times.
   */
  _eventNames: [
    "TabPinned",
    "TabUnpinned",
    "TabShow",
    "TabHide",
    "TabClose",
    "TabLabelModified"
  ],

  init: function TabTitleAbridger_Initialize() {
    this._cropTitleRedundancy = Services.prefs.getBoolPref(ABRIDGMENT_PREF);
    Services.prefs.addObserver(ABRIDGMENT_PREF, this, false);
    if (this._cropTitleRedundancy) {
      this._domainSets = new DomainSets();
      this._addListeners();
    }
  },

  destroy: function TabTitleAbridger_Destroy() {
    Services.prefs.removeObserver(ABRIDGMENT_PREF, this);
    if (this._cropTitleRedundancy) {
      this._dropListeners();
    }
  },

  /**
   * Preference observer
   */
  observe: function TabTitleAbridger_PrefObserver(aSubject, aTopic, aData) {
    let val = Services.prefs.getBoolPref(aData);
    if (this._cropTitleRedundancy && !val) {
      this._dropListeners();
      this._domainSets.destroy();
      delete this._domainSets;
      this._resetTabTitles();
    } else if (!this._cropTitleRedundancy && val) {
      this._addListeners();
      // We're just turned on, so we want to abridge everything
      this._domainSets = new DomainSets();
      let domains = this._domainSets.bootstrap(this._tabbrowser.visibleTabs);
      this._abridgeTabTitles(domains);
    }
    this._cropTitleRedundancy = val;
  },

  /**
   * Adds all the necessary event listeners and listener-supporting objects for
   * the instance.
   */
  _addListeners: function TabTitleAbridger_addListeners() {
    let tabContainer = this._tabbrowser.tabContainer;
    for (let eventName of this._eventNames) {
      tabContainer.addEventListener(eventName, this, false);
    }
  },

  /**
   * Removes event listeners and listener-supporting objects for the instance.
   */
  _dropListeners: function TabTitleAbridger_dropListeners() {
    let tabContainer = this._tabbrowser.tabContainer;
    for (let eventName of this._eventNames) {
      tabContainer.removeEventListener(eventName, this, false);
    }
  },

  handleEvent: function TabTitleAbridger_handler(aEvent) {
    let tab = aEvent.target;
    let updateSets;

    switch (aEvent.type) {
      case "TabUnpinned":
      case "TabShow":
        updateSets = this._domainSets.addTab(tab);
        break;
      case "TabPinned":
      case "TabHide":
      case "TabClose":
        updateSets = this._domainSets.removeTab(tab);
        tab.visibleLabel = tab.label;
        break;
      case "TabLabelModified":
        if (!tab.hidden && !tab.pinned) {
          aEvent.preventDefault();
          updateSets = this._domainSets.updateTab(tab);
        }
        break;
    }
    this._abridgeTabTitles(updateSets);
  },

  /**
   * Make all tabs have their visibleLabels be their labels.
   */
  _resetTabTitles: function TabTitleAbridger_resetTabTitles() {
    // We're freshly disabled, so reset unpinned, visible tabs (see handleEvent)
    for (let tab of this._tabbrowser.visibleTabs) {
      if (!tab.pinned && tab.visibleLabel != tab.label) {
        tab.visibleLabel = tab.label;
      }
    }
  },

  /**
   * Apply abridgment for the given tabset and chop list.
   * @param aTabSet   Array of tabs to abridge
   * @param aChopList Corresponding array of chop points for the tabs
   */
  _applyAbridgment: function TabTitleAbridger_applyAbridgment(aTabSet,
                                                              aChopList) {
    for (let i = 0; i < aTabSet.length; i++) {
      let tab = aTabSet[i];
      let label = tab.label || "";
      if (label.length > 0) {
        let chop = aChopList[i] || 0;
        if (chop > 0) {
          label = label.substr(chop);
        }
      }
      if (label != tab.visibleLabel) {
        tab.visibleLabel = label;
      }
    }
  },

  /**
   * Abridges the tabs sets of tabs in the aTabSets array.
   * @param aTabSets Array of tab sets needing abridgment
   */
  _abridgeTabTitles: function TabTitleAbridger_abridgeTabtitles(aTabSets) {
    // Process each set
    for (let tabSet of aTabSets) {
      // Get a chop list for the set and apply it
      let chopList = AbridgmentTools.getChopsForSet(tabSet);
      this._applyAbridgment(tabSet, chopList);
    }
  }
};

/**
 * Maintains a mapping between tabs and domains, so that only the tabs involved
 * in a TabLabelModified event need to be modified by the TabTitleAbridger.
 */
function DomainSets() {
  this._domainSets = {};
  this._tabsMappedToDomains = new WeakMap();
}

DomainSets.prototype = {
  _noHostSchemes: {
    chrome: true,
    file: true,
    resource: true,
    data: true,
    about: true
  },

  destroy: function DomainSets_destroy() {
    delete this._domainSets;
    delete this._tabsMappedToDomains;
  },

  /**
   * Used to build the domainsets when enabled in mid-air, as opposed to when
   * the window is coming up.
   * @param The visibleTabs for the browser, or a set of tabs to check.
   * @return An array containing the tabs in the domains they belong to, or
   * an empty array if none of the tabs belonged to domains.
   */
  bootstrap: function DomainSets_bootstrap(aVisibleTabs) {
    let needAbridgment = [];
    for (let tab of aVisibleTabs) {
      let domainSet = this.addTab(aTab)[0] || null;
      if (domainSet && needAbridgment.indexOf(domainSet) == -1) {
        needAbridgment.push(domainSet);
      }
    }
    return needAbridgment;
  },

  /**
   * Given a tab, include it in the domain sets.
   * @param aTab The tab to include in the domain sets
   * @param aTabDomain [optional] The known domain for the tab
   * @return An array containing the tabs in the domain the tab was added to.
   */
  addTab: function DomainSets_addTab(aTab, aTabDomain) {
    let tabDomain = aTabDomain || this._getDomainForTab(aTab);
    if (!this._domainSets.hasOwnProperty(tabDomain)) {
      this._domainSets[tabDomain] = [];
    }
    this._domainSets[tabDomain].push(aTab);
    this._tabsMappedToDomains.set(aTab, tabDomain);
    return [this._domainSets[tabDomain]];
  },

  /**
   * Given a tab, remove it from the domain sets.
   * @param aTab The tab to remove from the domain sets
   * @param aTabDomain [optional] The known domain for the tab
   * @return An array containing the tabs in the domain the tab was removed
   * from, or an empty array if the tab was not removed from a domain set.
   */
  removeTab: function DomainSets_removeTab(aTab, aTabDomain) {
    let oldTabDomain = aTabDomain || this._tabsMappedToDomains.get(aTab);
    if (!this._domainSets.hasOwnProperty(oldTabDomain)) {
      return [];
    }
    let index = this._domainSets[oldTabDomain].indexOf(aTab);
    if (index == -1) {
      return [];
    }
    this._domainSets[oldTabDomain].splice(index, 1);
    this._tabsMappedToDomains.delete(aTab);
    if (!this._domainSets[oldTabDomain].length) {
      // Keep the sets clean of empty domains
      delete this._domainSets[oldTabDomain];
      return [];
    }
    return [this._domainSets[oldTabDomain]];
  },

  /**
   * Given a tab, update the domain set it belongs to.
   * @param aTab The tab to update the domain set for
   * @return An array containing the tabs in the domain the tab belongs to, and
   * (if changed) the domain the tab was removed from.
   */
  updateTab: function DomainSets_updateTab(aTab) {
    let tabDomain = this._getDomainForTab(aTab);
    let oldTabDomain = this._tabsMappedToDomains.get(aTab);
    if (oldTabDomain != tabDomain) {
      let needAbridgment = [];
      // Probably swapping domain sets out; we pass the domains along to avoid
      // re-getting them in addTab/removeTab
      if (oldTabDomain) {
        needAbridgment = needAbridgment.concat(
          this.removeTab(aTab, oldTabDomain));
      }
      return needAbridgment.concat(this.addTab(aTab, tabDomain));
    }
    // No change was needed
    return [this._domainSets[tabDomain]];
  },

  /**
   * Given a tab, determine the URI scheme or host to categorize it.
   * @param aTab The tab to get the domain for
   * @return The domain or scheme for the tab
   */
  _getDomainForTab: function DomainSets_getDomainForTab(aTab) {
    let browserURI = aTab.linkedBrowser.currentURI;
    if (browserURI.scheme in this._noHostSchemes) {
      return browserURI.scheme;
    }

    // throws for empty URI, host is IP, and disallowed characters
    try {
      return gETLDService.getBaseDomain(browserURI);
    }
    catch (e) {}

    // this nsIURI may not be an nsStandardURL nsIURI, which means it
    // might throw for the host
    try {
      return browserURI.host;
    }
    catch (e) {}

    // Treat this URI as unique
    return browserURI.spec;
  }
};

let AbridgmentTools = {
  /**
   * Constant for the minimum remaining length allowed if a label is abridged.
   * I.e., original:"abc - de" might be chopped to just "de", which is too
   * small, so the label would be reverted to the next-longest version.
   */
  MIN_CHOP: 3,

  /**
   * Helper to determine if aStr is URI-like
   * \s?            optional leading space
   * [^\s\/]*       optional scheme or relative path component
   * ([^\s\/]+:\/)? optional scheme separator, with at least one scheme char
   * \/             at least one slash
   * \/?            optional second (or third for eg, file scheme on UNIX) slash
   * [^\s\/]*       optional path component
   * ([^\s\/]+\/?)* optional more path components with optional end slash
   * @param aStr the string to check for URI-likeness
   * @return boolean value of whether aStr matches
   */
  _titleIsURI: function AbridgmentTools_titleIsURI(aStr) {
    return /^\s?[^\s\/]*([^\s\/]+:\/)?\/\/?[^\s\/]*([^\s\/]+\/?)*$/.test(aStr);
  },

  /**
   * Finds the proper abridgment indexes for the given tabs.
   * @param aTabSet the array of tabs to find abridgments for
   * @return an array of abridgment indexes corresponding to the tabs
   */
  getChopsForSet: function AbridgmentTools_getChopsForSet(aTabSet) {
    let chopList = [];
    let pathMode = false;

    aTabSet.sort(function(aTab, bTab) {
      let aLabel = aTab.label;
      let bLabel = bTab.label;
      return (aLabel < bLabel) ? -1 : (aLabel > bLabel) ? 1 : 0;
    });

    // build and apply the chopList for the set
    for (let i = 0, next = 1; next < aTabSet.length; i = next++) {
      next = this._abridgePair(aTabSet, i, next, chopList);
    }
    return chopList;
  },

  /**
   * Handles the abridgment between aIndex and aNext, or in the case where the
   * label at aNext is the same as at aIndex, moves aNext forward appropriately.
   * @param aTabSet   Sorted array of tabs that the indices refer to
   * @param aIndex    First tab index to use in abridgment
   * @param aNext     Second tab index to use as the an initial comparison
   * @param aChopList Array to add chop points to for the given tabs
   * @return Index to replace aNext with, that is the index of the tab that was
   * used in abridging the tab at aIndex
   */
  _abridgePair: function TabTitleAbridger_abridgePair(aTabSet, aIndex, aNext,
                                                      aChopList) {
    let tabStr = aTabSet[aIndex].label;
    let pathMode = this._titleIsURI(tabStr);
    let chop = RedundancyFinder.indexOfSep(pathMode, tabStr);

    // Default to no chop
    if (!aChopList[aIndex]) {
      aChopList[aIndex] = 0;
    }

    // Siblings with same label get proxied by the first
    let nextStr;
    aNext = this._nextUnproxied(aTabSet, tabStr, aNext);
    if (aNext < aTabSet.length) {
      nextStr = aTabSet[aNext].label;
    }

    // Bail on these strings early, using the first as the basis
    if (chop == -1 || aNext == aTabSet.length ||
        !nextStr.startsWith(tabStr.substr(0, chop + 1))) {
      chop = aChopList[aIndex];
      if (aNext != aTabSet.length) {
        aChopList[aNext] = 0;
      }
    } else {
      [pathMode, chop] = this._getCommonChopPoint(pathMode, tabStr, nextStr,
                                                  chop);
      [chop, aChopList[aNext]] = this._adjustChops(pathMode, tabStr, nextStr,
                                                  chop);
      aChopList[aIndex] = chop;
    }

    // Mark chop on the relevant tabs
    for (let j = aIndex; j < aNext; j++) {
      let oldChop = aChopList[j];
      if (!oldChop || oldChop < chop) {
        aChopList[j] = chop;
      }
    }
    return aNext;
  },

  /**
   * Gets the index in aTabSet of the next tab that's not equal to aStr.
   * @param aTabSet Sorted set of tabs to check
   * @param aStr    Label string to check against
   * @param aStart  First item to check for proxying
   * @return The index of the next different tab.
   */
  _nextUnproxied: function AbridgmentTools_nextUnproxied(aTabSet, aTabStr,
                                                              aStart) {
    let nextStr = aTabSet[aStart].label;
    while (aStart < aTabSet.length && aTabStr == nextStr) {
      aStart += 1;
      if (aStart < aTabSet.length) {
        nextStr = aTabSet[aStart].label;
      }
    }
    return aStart;
  },

  /**
   * Get the common index where the aTabStr and aNextStr diverge.
   * @param aPathMode Whether to use path mode
   * @param aTabStr   Tab label
   * @param aNextStr  Second tab label
   * @param aChop     Current chop point being considered (index of aTabStr's
   * first separator)
   * @return An array containing the resulting path mode (in case it changes)
   * and the diverence index for the labels.
   */
  _getCommonChopPoint: function AbridgmentTools_getCommonChopPoint(aPathMode,
                                                                   aTabStr,
                                                                   aNextStr,
                                                                   aChop) {
    aChop = RedundancyFinder.findCommonPrefix(aPathMode, aTabStr, aNextStr,
                                              aChop);
    // Does a URI remain?
    if (!aPathMode) {
      aPathMode = this._titleIsURI(aTabStr.substr(aChop));
      if (aPathMode) {
        aChop = RedundancyFinder.findCommonPrefix(aPathMode, aTabStr, aNextStr,
                                                  aChop);
      }
    }

    return [aPathMode, aChop + 1];
  },

  /**
   * Adjusts the chop points based on their suffixes and lengths.
   * @param aPathMode Whether to use path mode
   * @param aTabStr   Tab label
   * @param aNextStr  Second tab label
   * @param aChop     Current chop point being considered
   * @return An array containing the chop point for the two labels.
   */
  _adjustChops: function AbridgmentTools_adjustChops(aPathMode, aTabStr,
                                                     aNextStr, aChop) {
    let suffix = RedundancyFinder.findCommonSuffix(aPathMode, aTabStr,
                                                   aNextStr);
    let sufPos = aTabStr.length - suffix;
    let nextSufPos = aNextStr.length - suffix;
    let nextChop = aChop;

    // Adjust the chop based on the suffix.
    if (sufPos < aChop) {
      // Only revert based on suffix for tab and any identicals
      aChop = RedundancyFinder.lastIndexOfSep(aPathMode, aTabStr,
                                              sufPos - 1)[1] + 1;
    } else if (nextSufPos < aChop) {
      // Only revert based on suffix for 'next'
      nextChop = RedundancyFinder.lastIndexOfSep(aPathMode, aNextStr,
                                                 nextSufPos - 1)[1] + 1;
    }

    if (aTabStr.length - aChop < this.MIN_CHOP) {
      aChop = RedundancyFinder.lastIndexOfSep(aPathMode, aTabStr,
                                              aChop - 2)[1] + 1;
    }
    if (aNextStr.length - nextChop < this.MIN_CHOP) {
      nextChop = RedundancyFinder.lastIndexOfSep(aPathMode, aNextStr,
                                                 nextChop - 2)[1] + 1;
    }
    return [aChop, nextChop];
  }
};

let RedundancyFinder = {
  /**
   * Finds the first index of a matched separator after aStart.
   * Separators will either be space-padded punctuation or slashes (in pathmode)
   *
   * ^.+?      at least one character, non-greedy match
   * \s+       one or more whitespace characters
   * [-:>\|]+  one or more separator characters
   * \s+       one or more whitespace characters
   *
   * @param aPathMode true for path mode, false otherwise
   * @param aStr      the string to look for a separator in
   * @param aStart    (optional) an index to start the search from
   * @return the next index of a separator or -1 for none
   */
  indexOfSep: function RedundancyFinder_indexOfSep(aPathMode, aStr, aStart) {
    if (aPathMode) {
      return aStr.indexOf('/', aStart);
    }

    let match = aStr.slice(aStart).match(/^.+?\s+[-:>\|]+\s+/);
    if (match) {
      return (aStart || 0) + match[0].length - 1;
    }

    return -1;
  },

  /**
   * Compares a pair of strings, seeking an index where their redundancy ends
   * @param aPathMode true for pathmode, false otherwise
   * @param aStr      the string to decide an abridgment for
   * @param aNextStr  the lexicographically next string to compare with
   * @param aChop     the basis index, a best-known index to begin comparison
   * @return the index at which aStr's abridged title should begin
   */
  findCommonPrefix: function RedundancyFinder_findCommonPrefix(aPathMode, aStr,
                                                               aNextStr,
                                                               aChop) {
    // Advance until the end of the title or the pair diverges
    do {
      aChop = this.indexOfSep(aPathMode, aStr, aChop + 1);
    } while (aChop != -1 && aNextStr.startsWith(aStr.substr(0, aChop + 1)));

    if (aChop < 0) {
      aChop = aStr.length;
    }

    // Return the last valid spot
    return this.lastIndexOfSep(aPathMode, aStr, aChop - 1)[1];
  },

  /**
   * Finds the range of a separator earlier than aEnd in aStr
   * The range is required by findCommonSuffix() needing to know the beginning
   * of the separator.
   * Separators will either be space-padded punctuation or slashes (in pathmode)
   *
   * .+           one or more initial characters
   * (            first group
   *   (          second group
   *     \s+      one or more whitespace characters
   *     [-:>\|]+ one or more separator characters
   *     \s+      one or more whitespace characters
   *   )          end first group
   *   .*?        zero or more characters, non-greedy match
   * )            end second group
   * $            end of input
   *
   * @param aPathMode true for pathmode, false otherwise
   * @param aStr      the string to look for a separator in
   * @param aEnd      (optional) an index to start the backwards search from
   * @return an array containing the endpoints of a separator (-1, -1 for none)
   */
  lastIndexOfSep: function RedundancyFinder_lastIndexOfSep(aPathMode, aStr,
                                                           aEnd) {
    if (aPathMode) {
      let path = aStr.lastIndexOf('/', aEnd);
      return [path, path];
    }

    let string = aStr.slice(0, aEnd);
    let match = string.match(/.+((\s+[-:>\|]+\s+).*?)$/);
    if (match) {
      let index = string.length - match[1].length;
      return [index, index + match[2].length - 1];
    }

    return [-1, -1];
  },

  /**
   * Finds a common suffix (redundancy at the end of) a pair of strings.
   * @param aPathMode true for pathmode, false otherwise
   * @param aStr      a base string to look for a suffix in
   * @param aNextStr  a string that may share a common suffix with aStr
   * @return an index indicating the divergence between the strings
   */
  findCommonSuffix: function RedundancyFinder_findCommonSuffix(aPathMode, aStr,
                                                               aNextStr) {
    let last = this.lastIndexOfSep(aPathMode, aStr)[0];

    // Is there any suffix match?
    if (!aNextStr.endsWith(aStr.slice(last))) {
      return 0;
    }

    // Move backwards on the main string until the suffix diverges
    let oldLast;
    do {
      oldLast = last;
      last = this.lastIndexOfSep(aPathMode, aStr, last - 1)[0];
    } while (last != -1 && aNextStr.endsWith(aStr.slice(last)));

    return aStr.length - oldLast;
  }
};

