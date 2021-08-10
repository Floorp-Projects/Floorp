/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
const EXPORTED_SYMBOLS = ["SnapshotSelector"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventEmitter: "resource://gre/modules/EventEmitter.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  Services: "resource://gre/modules/Services.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logConsole", function() {
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
class SnapshotSelector extends EventEmitter {
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
   */
  #context = {
    /**
     * The number of snapshots desired.
     * @type {number}
     */
    count: undefined,
    /**
     * The page the snapshots are for.
     * @type {string | undefined}
     */
    url: undefined,
    /**
     * The type of snapshots desired.
     * @type {PageDataCollector.DATA_TYPE | undefined}
     */
    type: undefined,
  };

  /**
   * A DeferredTask that runs the task to generate snapshots.
   */
  #task = null;

  /**
   * @param {number} count
   *   The maximum number of snapshots we ever need to generate. This should not
   *   affect the actual snapshots generated and their order but may speed up
   *   calculations.
   */
  constructor(count = 5) {
    super();
    this.#task = new DeferredTask(() => this.#buildSnapshots(), 500);
    this.#context.count = count;
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
   * @param {Snapshot[]} snapshots
   */
  #snapshotsGenerated(snapshots) {
    // If this instance has been destroyed then do nothing.
    if (!this.#task) {
      return;
    }

    logConsole.debug(
      "Generated snapshots",
      snapshots.map(s => s.url)
    );
    this.emit("snapshots-updated", snapshots);
  }

  /**
   * Starts the process of building snapshots.
   */
  async #buildSnapshots() {
    // If this instance has been destroyed then do nothing.
    if (!this.#task) {
      return;
    }

    // Task a copy of the context to avoid it changing while we are generating
    // the list.
    let context = { ...this.#context };
    logConsole.debug("Building snapshots", context);

    // Query for one more than we need in case the current url is returned.
    let snapshots = await Snapshots.query({
      limit: context.count + 1,
      type: context.type,
    });

    snapshots = snapshots
      .filter(snapshot => snapshot.url != context.url)
      .slice(0, context.count);

    this.#snapshotsGenerated(snapshots);
  }

  /**
   * Sets the current context's url for this selector.
   *
   * @param {string} url
   */
  setUrl(url) {
    if (this.#context.url == url) {
      return;
    }

    this.#context.url = url;
    this.rebuild();
  }

  /**
   * Like setUrl, but rebuilds immediately instead of after a delay. Useful for
   * startup.
   *
   * @param {string} url
   */
  setUrlAndRebuildNow(url) {
    if (this.#context.url == url) {
      return;
    }

    this.#context.url = url;
    this.#buildSnapshots();
  }

  /**
   * Sets the type of snapshots for this selector.
   *
   * @param {PageDataCollector.DATA_TYPE | undefined} type
   */
  async setType(type) {
    if (this.#context.type === type) {
      return;
    }

    this.#context.type = type;
    this.rebuild();
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
