/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  SnapshotScorer: "resource:///modules/SnapshotScorer.sys.mjs",
  Snapshots: "resource:///modules/Snapshots.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  FilterAdult: "resource://activity-stream/lib/FilterAdult.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logConsole", function() {
  return console.createInstance({
    prefix: "SnapshotSelector",
    maxLogLevel: Services.prefs.getBoolPref(
      "browser.places.interactions.log",
      false
    )
      ? "Debug"
      : "Warn",
  });
});

/**
 * @typedef {object} SelectionContext
 *   The necessary context for selecting, filtering and scoring snapshot
 *   recommendations. This context is expected to be specific to what the user
 *   is doing.
 * @property {number} count
 *   The maximum number of recommendations to generate.
 * @property {boolean} filterAdult
 *   Whether to filter out adult sites.
 * @property {Map<string, number> | null} sourceWeights
 *   Weights for the different recommendation sources. May be null in the
 *   event that the new recommendations are disabled.
 * @property {string | undefined} url
 *   The page the snapshots are for.
 * @property {PageDataCollector.DATA_TYPE | undefined} type
 *   The type of snapshots desired.
 * @property {function} getCurrentSessionUrls
 *   A function that returns a Set containing the urls for the current session.
 */

/**
 * @typedef {object} Recommendation
 *   The details of a specific recommendation for a snapshot.
 * @property {Snapshot} snapshot
 *   The snapshot this recommendation relates to.
 * @property {number} score
 *   The score for the snapshot.
 * @property {string | undefined} source
 *   The source that provided the largest score for this snapshot.
 */

/**
 * A snapshot selector is responsible for generating a list of snapshots based
 * on the current context. The context initially is just the url of the page
 * being viewed but will evolve to include things like the search terms that
 * brought the user to that page etc.
 *
 * Individual snapshots can be told to rebuild their set of snapshots and a
 * global function is provided that triggers all current selectors to rebuild.
 *
 * The selector is an event emitter that will emit a "snapshots-updated" event
 * when a new list is generated.
 *
 * This component is intentionally decoupled from where the context comes from
 * so it can be unit tested.
 */
export class SnapshotSelector extends EventEmitter {
  /**
   * All of the active selectors.
   */
  static #selectors = new Set();

  /**
   * Triggers a rebuild of all selectors.
   */
  static rebuildAll() {
    for (let selector of SnapshotSelector.#selectors) {
      selector.rebuild();
    }
  }

  /**
   * The context should be thought of as the current state for this specific
   * selector. Global state that impacts all selectors should not be kept here.
   *
   * @type {SelectionContext}
   */
  #context = {
    count: undefined,
    filterAdult: false,
    sourceWeights: null,
    url: undefined,
    time: Date.now(),
    type: undefined,
    getCurrentSessionUrls: undefined,
  };

  /**
   * A DeferredTask that runs the task to generate snapshots.
   */
  #task = null;

  /**
   * @param {object} options
   * @param {number} [options.count]
   *   The maximum number of snapshots we ever need to generate. This should not
   *   affect the actual snapshots generated and their order but may speed up
   *   calculations.
   * @param {boolean} [options.filterAdult]
   *   Whether adult sites should be filtered from the snapshots.
   * @param {object | undefined} [options.sourceWeights]
   *   Overrides for the weights of different recommendation sources.
   * @param {function} [options.getCurrentSessionUrls]
   *   A function that returns a Set containing the urls for the current session.
   */
  constructor({
    count = 5,
    filterAdult = false,
    sourceWeights = undefined,
    getCurrentSessionUrls = () => new Set(),
  }) {
    super();
    this.#task = new lazy.DeferredTask(
      () => this.#buildSnapshots().catch(console.error),
      500
    );
    this.#context.count = count;
    this.#context.filterAdult = filterAdult;

    if (
      sourceWeights ||
      Services.prefs.getBoolPref(
        "browser.pinebuild.snapshots.relevancy.enabled",
        false
      )
    ) {
      // Fetch the defaults
      let branch = Services.prefs.getBranch("browser.snapshots.source.");
      let weights = Object.fromEntries(
        branch.getChildList("").map(name => [name, branch.getIntPref(name, 0)])
      );

      // Apply overrides
      Object.assign(weights, sourceWeights ?? {});

      this.#context.sourceWeights = new Map(Object.entries(weights));
    }

    this.#context.getCurrentSessionUrls = getCurrentSessionUrls;
    SnapshotSelector.#selectors.add(this);
  }

  /**
   * Call to destroy the selector.
   */
  destroy() {
    this.#task.disarm();
    this.#task.finalize();
    this.#task = null;
    SnapshotSelector.#selectors.delete(this);
  }

  rebuild() {
    // If this instance has been destroyed then do nothing.
    if (!this.#task) {
      return;
    }

    this.#task.arm();
  }

  /**
   * Called internally when the set of snapshots has been generated.
   *
   * @param {Recommendation[]} recommendations
   */
  #snapshotsGenerated(recommendations) {
    // If this instance has been destroyed then do nothing.
    if (!this.#task) {
      return;
    }

    lazy.logConsole.debug(
      "Generated recommendations",
      recommendations.map(s => s.snapshot.url)
    );
    this.emit("snapshots-updated", recommendations);
  }

  /**
   * Starts the process of building snapshots.
   */
  async #buildSnapshots() {
    if (this.#context.sourceWeights) {
      await this.#buildRelevancySnapshots();
      return;
    }

    // If this instance has been destroyed then do nothing.
    if (!this.#task) {
      return;
    }

    // Take a copy of the context to avoid it changing while we are generating
    // the list.
    let context = { ...this.#context };
    lazy.logConsole.debug("Building snapshots", context);

    // We query for more snapshots than we need so that we can account for
    // deduplicating and filtering out adult sites. This may not catch all
    // cases, but saves the complexity of repeated queries.
    let snapshots = await lazy.Snapshots.query({
      limit: context.count * 4,
      type: context.type,
    });

    snapshots = snapshots.filter(snapshot => {
      if (snapshot.url == context.url) {
        return false;
      }
      return !context.filterAdult || !lazy.FilterAdult.isAdultUrl(snapshot.url);
    });

    let recommendations = snapshots.map((snapshot, index) => ({
      source: "recent",
      score: snapshots.length - index,
      snapshot,
    }));

    recommendations = lazy.SnapshotScorer.dedupeSnapshots(
      recommendations
    ).slice(0, context.count);

    lazy.PlacesUIUtils.insertTitleStartDiffs(
      recommendations.map(s => s.snapshot)
    );

    this.#snapshotsGenerated(recommendations);
  }

  /**
   * Build snapshots based on relevancy heuristsics.
   *  These include overlapping visits and common referrer, defined by the context options.
   *
   */
  async #buildRelevancySnapshots() {
    // If this instance has been destroyed then do nothing.
    if (!this.#task) {
      return;
    }

    // Take a copy of the context to avoid it changing while we are generating
    // the list.
    let context = { ...this.#context };
    lazy.logConsole.debug("Building relevant snapshots", context);

    let recommendationGroups = await Promise.all(
      Object.entries(lazy.Snapshots.recommendationSources).map(
        async ([key, source]) => {
          let weight = context.sourceWeights.get(key) ?? 0;
          if (weight == 0) {
            return { recommendations: [], weight };
          }

          let recommendations = await source(context);

          lazy.logConsole.debug(
            `Found ${key} recommendations:`,
            recommendations.map(
              r =>
                `${r.snapshot.url} (score: ${r.score}${
                  r.data ? ", data: " + JSON.stringify(r.data) : ""
                })`
            )
          );

          return { source: key, recommendations, weight };
        }
      )
    );

    let recommendations = lazy.SnapshotScorer.combineAndScore(
      context,
      ...recommendationGroups
    );

    recommendations = recommendations.slice(0, context.count);

    lazy.PlacesUIUtils.insertTitleStartDiffs(
      recommendations.map(r => r.snapshot)
    );

    this.#snapshotsGenerated(recommendations);
  }

  /**
   * Update context details and start a rebuild.
   * Undefined properties are ignored, thus pass null to nullify a property.
   * @param {string} [url]
   *  The url of the current context.
   * @param {number} [time]
   *  The time, in milliseconds since the Unix epoch.
   * @param {PageDataSchema.DATA_TYPE} [type]
   *  The type of snapshots for this selector.
   * @param {number} [sessionStartTime]
   *  The start time of the session, in milliseconds since the Unix epoch.
   * @param {string} [rebuildImmediately] (default: false)
   *  Whether to rebuild immediately instead of waiting some delay. Useful on
   *  startup.
   */
  updateDetailsAndRebuild({
    url,
    time,
    type,
    sessionStartTime,
    rebuildImmediately = false,
  }) {
    let rebuild = false;
    if (url !== undefined) {
      url = lazy.Snapshots.stripFragments(url);
      if (url != this.#context.url) {
        this.#context.url = url;
        rebuild = true;
      }
    }
    if (time !== undefined && time != this.#context.time) {
      this.#context.time = time;
      rebuild = true;
    }
    if (type !== undefined && type != this.#context.type) {
      this.#context.type = type;
      rebuild = true;
    }
    if (
      sessionStartTime != undefined &&
      sessionStartTime != this.#context.sessionStartTime
    ) {
      this.#context.sessionStartTime = sessionStartTime;
      rebuild = true;
    }

    if (rebuild) {
      if (rebuildImmediately) {
        this.#buildSnapshots();
      } else {
        this.rebuild();
      }
    }
  }
}

// Listen for global events that may affect the snapshots generated.
Services.obs.addObserver(SnapshotSelector.rebuildAll, "places-snapshots-added");
Services.obs.addObserver(
  SnapshotSelector.rebuildAll,
  "places-snapshots-deleted"
);
Services.obs.addObserver(
  SnapshotSelector.rebuildAll,
  "places-metadata-updated"
);
