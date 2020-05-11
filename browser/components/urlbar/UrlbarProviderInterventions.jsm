/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarProviderInterventions", "QueryScorer"];
var gGlobalScope = this;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Log: "resource://gre/modules/Log.jsm",
  NLP: "resource://gre/modules/NLP.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ResetProfile: "resource://gre/modules/ResetProfile.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Provider.Interventions")
);

XPCOMUtils.defineLazyGetter(this, "appUpdater", () => new AppUpdater());

// The possible tips to show.  These names (except NONE) are used in the names
// of keys in the `urlbar.tips` keyed scalar telemetry (see telemetry.rst).
// Don't modify them unless you've considered that.  If you do modify them or
// add new tips, then you are also adding new `urlbar.tips` keys and therefore
// need an expanded data collection review.
const TIPS = {
  NONE: "",
  CLEAR: "intervention_clear",
  REFRESH: "intervention_refresh",

  // There's an update available, but the user's pref says we should ask them to
  // download and apply it.
  UPDATE_ASK: "intervention_update_ask",

  // The updater is currently checking.  We don't actually show a tip for this,
  // but we use it to tell whether we should wait for the check to complete in
  // startQuery.  See startQuery for details.
  UPDATE_CHECKING: "intervention_update_checking",

  // The user's browser is up to date, but they triggered the update
  // intervention. We show this special refresh intervention instead.
  UPDATE_REFRESH: "intervention_update_refresh",

  // There's an update and it's been downloaded and applied. The user needs to
  // restart to finish.
  UPDATE_RESTART: "intervention_update_restart",

  // We can't update the browser or possibly even check for updates for some
  // reason, so the user should download the latest version from the web.
  UPDATE_WEB: "intervention_update_web",
};

const EN_LOCALE_MATCH = /^en(-.*)$/;

// The search "documents" corresponding to each tip type.
const DOCUMENTS = {
  clear: [
    "cache firefox",
    "clear cache firefox",
    "clear cache in firefox",
    "clear cookies firefox",
    "clear firefox cache",
    "clear history firefox",
    "cookies firefox",
    "delete cookies firefox",
    "delete history firefox",
    "firefox cache",
    "firefox clear cache",
    "firefox clear cookies",
    "firefox clear history",
    "firefox cookie",
    "firefox cookies",
    "firefox delete cookies",
    "firefox delete history",
    "firefox history",
    "firefox not loading pages",
    "history firefox",
    "how to clear cache",
    "how to clear history",
  ],
  refresh: [
    "firefox crashing",
    "firefox keeps crashing",
    "firefox not responding",
    "firefox not working",
    "firefox refresh",
    "firefox slow",
    "how to reset firefox",
    "refresh firefox",
    "reset firefox",
  ],
  update: [
    "download firefox",
    "download mozilla",
    "firefox browser",
    "firefox download",
    "firefox for mac",
    "firefox for windows",
    "firefox free download",
    "firefox install",
    "firefox installer",
    "firefox latest version",
    "firefox mac",
    "firefox quantum",
    "firefox update",
    "firefox version",
    "firefox windows",
    "get firefox",
    "how to update firefox",
    "install firefox",
    "mozilla download",
    "mozilla firefox 2019",
    "mozilla firefox 2020",
    "mozilla firefox download",
    "mozilla firefox for mac",
    "mozilla firefox for windows",
    "mozilla firefox free download",
    "mozilla firefox mac",
    "mozilla firefox update",
    "mozilla firefox windows",
    "mozilla update",
    "update firefox",
    "update mozilla",
    "www.firefox.com",
  ],
};

// In order to determine whether we should show an update tip, we check for app
// updates, but only once per this time period.
const UPDATE_CHECK_PERIOD_MS = 12 * 60 * 60 * 1000; // 12 hours

/**
 * A node in the QueryScorer's phrase tree.
 */
class Node {
  constructor(word) {
    this.word = word;
    this.documents = new Set();
    this.childrenByWord = new Map();
  }
}

/**
 * This class scores a query string against sets of phrases.  To refer to a
 * single set of phrases, we borrow the term "document" from search engine
 * terminology.  To use this class, first add your documents with `addDocument`,
 * and then call `score` with a query string.  `score` returns a sorted array of
 * document-score pairs.
 *
 * The scoring method is fairly simple and is based on Levenshtein edit
 * distance.  Therefore, lower scores indicate a better match than higher
 * scores.  In summary, a query matches a phrase if the query starts with the
 * phrase.  So a query "firefox update foo bar" matches the phrase "firefox
 * update" for example.  A query matches a document if it matches any phrase in
 * the document.  The query and phrases are compared word for word, and we allow
 * fuzzy matching by computing the Levenshtein edit distance in each comparison.
 * The amount of fuzziness allowed is controlled with `distanceThreshold`.  If
 * the distance in a comparison is greater than this threshold, then the phrase
 * does not match the query.  The final score for a document is the minimum edit
 * distance between its phrases and the query.
 *
 * As mentioned, `score` returns a sorted array of document-score pairs.  It's
 * up to you to filter the array to exclude scores above a certain threshold, or
 * to take the top scorer, etc.
 */
class QueryScorer {
  /**
   * @param {number} distanceThreshold
   *   Edit distances no larger than this value are considered matches.
   * @param {Map} variations
   *   For convenience, the scorer can augment documents by replacing certain
   *   words with other words and phrases. This mechanism is called variations.
   *   This keys of this map are words that should be replaced, and the values
   *   are the replacement words or phrases.  For example, if you add a document
   *   whose only phrase is "firefox update", normally the scorer will register
   *   only this single phrase for the document.  However, if you pass the value
   *   `new Map(["firefox", ["fire fox", "fox fire", "foxfire"]])` for this
   *   parameter, it will register 4 total phrases for the document: "fire fox
   *   update", "fox fire update", "foxfire update", and the original "firefox
   *   update".
   */
  constructor({ distanceThreshold = 1, variations = new Map() } = {}) {
    this._distanceThreshold = distanceThreshold;
    this._variations = variations;
    this._documents = new Set();
    this._rootNode = new Node();
  }

  /**
   * Adds a document to the scorer.
   *
   * @param {object} doc
   *   The document.
   * @param {string} doc.id
   *   The document's ID.
   * @param {array} doc.phrases
   *   The set of phrases in the document.  Each phrase should be a string.
   */
  addDocument(doc) {
    this._documents.add(doc);

    for (let phraseStr of doc.phrases) {
      // Split the phrase and lowercase the words.
      let phrase = phraseStr
        .trim()
        .split(/\s+/)
        .map(word => word.toLocaleLowerCase());

      // Build a phrase list that contains the original phrase plus its
      // variations, if any.
      let phrases = [phrase];
      for (let [triggerWord, variations] of this._variations) {
        let index = phrase.indexOf(triggerWord);
        if (index >= 0) {
          for (let variation of variations) {
            let variationPhrase = Array.from(phrase);
            variationPhrase.splice(index, 1, ...variation.split(/\s+/));
            phrases.push(variationPhrase);
          }
        }
      }

      // Finally, add the phrases to the phrase tree.
      for (let completedPhrase of phrases) {
        this._buildPhraseTree(this._rootNode, doc, completedPhrase, 0);
      }
    }
  }

  /**
   * Scores a query string against the documents in the scorer.
   *
   * @param {string} queryString
   *   The query string to score.
   * @returns {array}
   *   An array of objects: { document, score }.  Each element in the array is a
   *   a document and its score against the query string.  The elements are
   *   ordered by score from low to high.  Scores represent edit distance, so
   *   lower scores are better.
   */
  score(queryString) {
    let queryWords = queryString
      .trim()
      .split(/\s+/)
      .map(word => word.toLocaleLowerCase());
    let minDistanceByDoc = this._traverse({ queryWords });
    let results = [];
    for (let doc of this._documents) {
      let distance = minDistanceByDoc.get(doc);
      results.push({
        document: doc,
        score: distance === undefined ? Infinity : distance,
      });
    }
    results.sort((a, b) => a.score - b.score);
    return results;
  }

  /**
   * Builds the phrase tree based on the current documents.
   *
   * The phrase tree lets us efficiently match queries against phrases.  Each
   * path through the tree starting from the root and ending at a leaf
   * represents a complete phrase in a document (or more than one document, if
   * the same phrase is present in multiple documents).  Each node in the path
   * represents a word in the phrase.  To match a query, we start at the root,
   * and in the root we look up the query's first word.  If the word matches the
   * first word of any phrase, then the root will have a child node representing
   * that word, and we move on to the child node.  Then we look up the query's
   * second word in the child node, and so on, until either a lookup fails or we
   * reach a leaf node.
   *
   * @param {Node} node
   *   The current node being visited.
   * @param {object} doc
   *   The document whose phrases are being added to the tree.
   * @param {array} phrase
   *   The phrase to add to the tree.
   * @param {number} wordIndex
   *   The index in the phrase of the current word.
   */
  _buildPhraseTree(node, doc, phrase, wordIndex) {
    if (phrase.length == wordIndex) {
      // We're done with this phrase.
      return;
    }

    let word = phrase[wordIndex].toLocaleLowerCase();
    let child = node.childrenByWord.get(word);
    if (!child) {
      child = new Node(word);
      node.childrenByWord.set(word, child);
    }
    child.documents.add(doc);

    // Recurse with the next word in the phrase.
    this._buildPhraseTree(child, doc, phrase, wordIndex + 1);
  }

  /**
   * Traverses a path in the phrase tree in order to score a query.  See
   * `_buildPhraseTree` for a description of how this works.
   *
   * @param {array} queryWords
   *   The query being scored, split into words.
   * @param {Node} node
   *   The node currently being visited.
   * @param {Map} minDistanceByDoc
   *   Keeps track of the minimum edit distance for each document as the
   *   traversal continues.
   * @param {number} queryWordsIndex
   *   The current index in the query words array.
   * @param {number} phraseDistance
   *   The total edit distance between the query and the path in the tree that's
   *   been traversed so far.
   * @returns {Map} minDistanceByDoc
   */
  _traverse({
    queryWords,
    node = this._rootNode,
    minDistanceByDoc = new Map(),
    queryWordsIndex = 0,
    phraseDistance = 0,
  } = {}) {
    if (!node.childrenByWord.size) {
      // We reached a leaf node.  The query has matched a phrase.  If the query
      // and the phrase have the same number of words, then queryWordsIndex ==
      // queryWords.length also.  Otherwise the query contains more words than
      // the phrase.  We still count that as a match.
      for (let doc of node.documents) {
        minDistanceByDoc.set(
          doc,
          Math.min(
            phraseDistance,
            minDistanceByDoc.has(doc) ? minDistanceByDoc.get(doc) : Infinity
          )
        );
      }
      return minDistanceByDoc;
    }

    if (queryWordsIndex == queryWords.length) {
      // We exhausted all the words in the query but have not reached a leaf
      // node.  No match; the query has matched a phrase(s) up to this point,
      // but it doesn't have enough words.
      return minDistanceByDoc;
    }

    // Compare each word in the node to the current query word.
    let queryWord = queryWords[queryWordsIndex];
    for (let [childWord, child] of node.childrenByWord) {
      let distance = NLP.levenshtein(queryWord, childWord);
      if (distance <= this._distanceThreshold) {
        // The word represented by this child node matches the current query
        // word.  Recurse into the child node.
        this._traverse({
          node: child,
          queryWords,
          queryWordsIndex: queryWordsIndex + 1,
          phraseDistance: phraseDistance + distance,
          minDistanceByDoc,
        });
      }
      // Else, the path that continues at the child node can't possibly match
      // the query, so don't recurse into it.
    }

    return minDistanceByDoc;
  }
}

/**
 * Gets appropriate l10n values for each tip's payload.
 * @param {string} tip a value from the TIPS enum
 * @returns {object} an Object shaped as { textData, buttonTextData, helpUrl }
 */
function getL10nPropertiesForTip(tip) {
  const baseURL = "https://support.mozilla.org/kb/";
  switch (tip) {
    case TIPS.CLEAR:
      return {
        textData: { id: "intervention-clear-data" },
        buttonTextData: { id: "intervention-clear-data-confirm" },
        helpUrl: baseURL + "delete-browsing-search-download-history-firefox",
      };
    case TIPS.REFRESH:
      return {
        textData: { id: "intervention-refresh-profile" },
        buttonTextData: { id: "intervention-refresh-profile-confirm" },
        helpUrl: baseURL + "refresh-firefox-reset-add-ons-and-settings",
      };
    case TIPS.UPDATE_ASK:
      return {
        textData: { id: "intervention-update-ask" },
        buttonTextData: { id: "intervention-update-ask-confirm" },
        helpUrl: baseURL + "update-firefox-latest-release",
      };
    case TIPS.UPDATE_REFRESH:
      return {
        textData: { id: "intervention-update-refresh" },
        buttonTextData: { id: "intervention-update-refresh-confirm" },
        helpUrl: baseURL + "refresh-firefox-reset-add-ons-and-settings",
      };
    case TIPS.UPDATE_RESTART:
      return {
        textData: { id: "intervention-update-restart" },
        buttonTextData: { id: "intervention-update-restart-confirm" },
        helpUrl: baseURL + "update-firefox-latest-release",
      };
    case TIPS.UPDATE_WEB:
      return {
        textData: { id: "intervention-update-web" },
        buttonTextData: { id: "intervention-update-web-confirm" },
        helpUrl: baseURL + "update-firefox-latest-release",
      };
    default:
      throw new Error("Unknown TIP type.");
  }
}

/**
 * A provider that returns actionable tip results when the user is performing
 * a search related to those actions.
 */
class ProviderInterventions extends UrlbarProvider {
  constructor() {
    super();
    // Maps the running queries by queryContext.
    this.queries = new Map();
    // The tip we should currently show.
    this.currentTip = TIPS.NONE;

    this.tipsShownInCurrentEngagement = new Set();

    // This object is used to match the user's queries to tips.
    XPCOMUtils.defineLazyGetter(this, "queryScorer", () => {
      let queryScorer = new QueryScorer({
        variations: new Map([
          // Recognize "fire fox", "fox fire", and "foxfire" as "firefox".
          ["firefox", ["fire fox", "fox fire", "foxfire"]],
          // Recognize "mozila" as "mozilla".  This will catch common mispellings
          // "mozila", "mozzila", and "mozzilla" (among others) due to the edit
          // distance threshold of 1.
          ["mozilla", ["mozila"]],
        ]),
      });
      for (let [id, phrases] of Object.entries(DOCUMENTS)) {
        queryScorer.addDocument({ id, phrases });
      }
      return queryScorer;
    });
  }

  /**
   * Enum of the types of intervention tips.
   */
  get TIP_TYPE() {
    return TIPS;
  }

  /**
   * Unique name for the provider, used by the context to filter on providers.
   */
  get name() {
    return "UrlbarProviderInterventions";
  }

  /**
   * The type of the provider, must be one of UrlbarUtils.PROVIDER_TYPE.
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    if (
      !UrlbarPrefs.get("update1.interventions") ||
      !queryContext.searchString ||
      !EN_LOCALE_MATCH.test(Services.locale.appLocaleAsBCP47) ||
      !Services.policies.isAllowed("urlbarinterventions")
    ) {
      return false;
    }

    this.currentTip = TIPS.NONE;

    // Get the scores and the top score.
    let docScores = this.queryScorer.score(queryContext.searchString);
    let topDocScore = docScores[0];

    // Multiple docs may have the top score, so collect them all.
    let topDocIDs = new Set();
    if (topDocScore.score != Infinity) {
      for (let { score, document } of docScores) {
        if (score != topDocScore.score) {
          break;
        }
        topDocIDs.add(document.id);
      }
    }

    // Determine the tip to show, if any. If there are multiple top-score docs,
    // prefer them in the following order.
    if (topDocIDs.has("update")) {
      this._setCurrentTipFromAppUpdaterStatus();
    } else if (topDocIDs.has("clear")) {
      let window = BrowserWindowTracker.getTopWindow();
      if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
        this.currentTip = TIPS.CLEAR;
      }
    } else if (topDocIDs.has("refresh")) {
      // Note that the "update" case can set currentTip to TIPS.REFRESH too.
      this.currentTip = TIPS.REFRESH;
    }

    return (
      this.currentTip != TIPS.NONE &&
      (this.currentTip != TIPS.REFRESH ||
        Services.policies.isAllowed("profileRefresh"))
    );
  }

  async _setCurrentTipFromAppUpdaterStatus(waitForCheck) {
    // The update tips depend on the app's update status, so check for updates
    // now (if we haven't already checked within the update-check period).  If
    // we're running in an xpcshell test, then checkForBrowserUpdate's attempt
    // to use appUpdater will throw an exception because it won't be available.
    // In that case, return false to disable the provider.
    //
    // This causes synchronous IO within the updater the first time it's called
    // (at least) so be careful not to do it the first time the urlbar is used.
    try {
      this.checkForBrowserUpdate();
    } catch (ex) {
      return;
    }

    // There are several update tips. Figure out which one to show.
    switch (appUpdater.status) {
      case AppUpdater.STATUS.READY_FOR_RESTART:
        // Prompt the user to restart.
        this.currentTip = TIPS.UPDATE_RESTART;
        break;
      case AppUpdater.STATUS.DOWNLOAD_AND_INSTALL:
        // There's an update available, but the user's pref says we should ask
        // them to download and apply it.
        this.currentTip = TIPS.UPDATE_ASK;
        break;
      case AppUpdater.STATUS.NO_UPDATES_FOUND:
        // We show a special refresh tip when the browser is up to date.
        this.currentTip = TIPS.UPDATE_REFRESH;
        break;
      case AppUpdater.STATUS.CHECKING:
        // This will be the case the first time we check.  See startQuery for
        // how this special tip is handled.
        this.currentTip = TIPS.UPDATE_CHECKING;
        break;
      default:
        // Give up and ask the user to download the latest version from the
        // web. We default to this case when the update is still downloading
        // because an update doesn't actually occur if the user were to
        // restart the browser. See bug 1625241.
        this.currentTip = TIPS.UPDATE_WEB;
        break;
    }
  }

  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   */
  async startQuery(queryContext, addCallback) {
    let instance = {};
    this.queries.set(queryContext, instance);

    // TIPS.UPDATE_CHECKING is special, and we never actually show a tip that
    // reflects a "checking" status.  Instead it's handled like this.  We call
    // appUpdater.check() to start an update check.  If we haven't called it
    // before, then when it returns, appUpdater.status will be
    // AppUpdater.STATUS.CHECKING, and it will remain CHECKING until the check
    // finishes.  We can add a listener to appUpdater to be notified when the
    // check finishes.  We don't want to wait for it to finish in isActive
    // because that would block other providers from adding their results, so
    // instead we wait here in startQuery.  The results from other providers
    // will be added while we're waiting.  When the check finishes, we call
    // addCallback and add our result.  It doesn't matter how long the check
    // takes because if another query starts, the view is closed, or the user
    // changes the selection, the query will be canceled.
    if (this.currentTip == TIPS.UPDATE_CHECKING) {
      // First check the status because it may have changed between the time
      // isActive was called and now.
      this._setCurrentTipFromAppUpdaterStatus();
      if (this.currentTip == TIPS.UPDATE_CHECKING) {
        // The updater is still checking, so wait for it to finish.
        await new Promise(resolve => {
          this._appUpdaterListener = () => {
            appUpdater.removeListener(this._appUpdaterListener);
            delete this._appUpdaterListener;
            resolve();
          };
          appUpdater.addListener(this._appUpdaterListener);
        });
        if (!this.queries.has(queryContext)) {
          // The query was canceled before the check finished.
          return;
        }
        // Finally, set the tip from the updater status.  The updater should no
        // longer be checking, but guard against it just in case by returning
        // early.
        this._setCurrentTipFromAppUpdaterStatus();
        if (this.currentTip == TIPS.UPDATE_CHECKING) {
          this.queries.delete(queryContext);
          return;
        }
      }
    }
    // At this point, this.currentTip != TIPS.UPDATE_CHECKING because we
    // returned early above if it was.

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        type: this.currentTip,
      }
    );

    result.suggestedIndex = 1;

    Object.assign(result.payload, getL10nPropertiesForTip(this.currentTip));

    if (!this.queries.has(queryContext)) {
      return;
    }

    this.tipsShownInCurrentEngagement.add(this.currentTip);

    addCallback(this, result);
    this.queries.delete(queryContext);
  }

  /**
   * Cancels a running query,
   * @param {UrlbarQueryContext} queryContext the query context object to cancel
   *        query for.
   */
  cancelQuery(queryContext) {
    logger.info(`Canceling query for ${queryContext.searchString}`);
    this.queries.delete(queryContext);

    // If we're waiting for appUpdater to finish its update check,
    // this._appUpdaterListener will be defined.  We can stop listening now.
    if (this._appUpdaterListener) {
      appUpdater.removeListener(this._appUpdaterListener);
      delete this._appUpdaterListener;
    }
  }

  /**
   * Called when a result from the provider without a URL is picked, but
   * currently only for tip results.  The provider should handle the pick.
   * @param {UrlbarResult} result
   *   The result that was picked.
   */
  pickResult(result) {
    let tip = result.payload.type;

    // Do the tip action.
    switch (tip) {
      case TIPS.CLEAR:
        openClearHistoryDialog();
        break;
      case TIPS.REFRESH:
      case TIPS.UPDATE_REFRESH:
        resetBrowser();
        break;
      case TIPS.UPDATE_ASK:
        installBrowserUpdateAndRestart();
        break;
      case TIPS.UPDATE_RESTART:
        restartBrowser();
        break;
      case TIPS.UPDATE_WEB:
        let window = BrowserWindowTracker.getTopWindow();
        window.gBrowser.selectedTab = window.gBrowser.addWebTab(
          "https://www.mozilla.org/firefox/new/"
        );
        break;
    }
  }

  onEngagement(isPrivate, state) {
    if (["engagement", "abandonment"].includes(state)) {
      for (let tip of this.tipsShownInCurrentEngagement) {
        Services.telemetry.keyedScalarAdd("urlbar.tips", `${tip}-shown`, 1);
      }
    }
    this.tipsShownInCurrentEngagement.clear();
  }

  /**
   * Checks for app updates.
   *
   * @param {boolean} force If false, this only checks for updates if we haven't
   *        already checked within the update-check period.  If true, we check
   *        regardless.
   */
  checkForBrowserUpdate(force = false) {
    if (
      force ||
      !this._lastUpdateCheckTime ||
      Date.now() - this._lastUpdateCheckTime >= UPDATE_CHECK_PERIOD_MS
    ) {
      this._lastUpdateCheckTime = Date.now();
      appUpdater.check();
    }
  }

  /**
   * Resets the provider's app updater state by making a new app updater.  This
   * is intended to be used by tests.
   */
  resetAppUpdater() {
    // Reset only if the object has already been initialized.
    if (!Object.getOwnPropertyDescriptor(gGlobalScope, "appUpdater").get) {
      appUpdater = new AppUpdater();
    }
  }
}

var UrlbarProviderInterventions = new ProviderInterventions();

/**
 * Tip callbacks follow.
 */

function installBrowserUpdateAndRestart() {
  if (appUpdater.status != AppUpdater.STATUS.DOWNLOAD_AND_INSTALL) {
    return Promise.resolve();
  }
  return new Promise(resolve => {
    let listener = () => {
      // Once we call startDownload, there are two possible end
      // states: DOWNLOAD_FAILED and READY_FOR_RESTART.
      if (
        appUpdater.status != AppUpdater.STATUS.READY_FOR_RESTART &&
        appUpdater.status != AppUpdater.STATUS.DOWNLOAD_FAILED
      ) {
        return;
      }
      appUpdater.removeListener(listener);
      if (appUpdater.status == AppUpdater.STATUS.READY_FOR_RESTART) {
        restartBrowser();
      }
      resolve();
    };
    appUpdater.addListener(listener);
    appUpdater.startDownload();
  });
}

function openClearHistoryDialog() {
  let window = BrowserWindowTracker.getTopWindow();
  // The behaviour of the Clear Recent History dialog in PBM does
  // not have the expected effect (bug 463607).
  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    return;
  }
  Sanitizer.showUI(window);
}

function restartBrowser() {
  // Notify all windows that an application quit has been requested.
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
    Ci.nsISupportsPRBool
  );
  Services.obs.notifyObservers(
    cancelQuit,
    "quit-application-requested",
    "restart"
  );
  // Something aborted the quit process.
  if (cancelQuit.data) {
    return;
  }
  // If already in safe mode restart in safe mode.
  if (Services.appinfo.inSafeMode) {
    Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
  } else {
    Services.startup.quit(
      Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
    );
  }
}

function resetBrowser() {
  if (!ResetProfile.resetSupported()) {
    return;
  }
  let window = BrowserWindowTracker.getTopWindow();
  ResetProfile.openConfirmationDialog(window);
}
