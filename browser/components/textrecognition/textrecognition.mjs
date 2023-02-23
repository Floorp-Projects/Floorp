/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

window.docShell.chromeEventHandler.classList.add("textRecognitionDialogFrame");

window.addEventListener("DOMContentLoaded", () => {
  // The arguments are passed in as the final parameters to TabDialogBox.prototype.open.
  new TextRecognitionModal(...window.arguments);
});

/**
 * @typedef {Object} TextRecognitionResult
 * @property {number} confidence
 * @property {string} string
 * @property {DOMQuad} quad
 */

class TextRecognitionModal {
  /**
   * @param {Promise<TextRecognitionResult[]>} resultsPromise
   * @param {() => {}} resizeVertically
   * @param {(url: string, where: string, params: Object) => {}} openLinkIn
   */
  constructor(resultsPromise, resizeVertically, openLinkIn) {
    /** @type {HTMLElement} */
    this.textEl = document.querySelector(".textRecognitionText");

    /** @type {NodeListOf<HTMLElement>} */
    this.headerEls = document.querySelectorAll(".textRecognitionHeader");

    /** @type {HTMLAnchorElement} */
    this.linkEl = document.querySelector(
      "#text-recognition-header-no-results a"
    );

    this.resizeVertically = resizeVertically;
    this.openLinkIn = openLinkIn;
    this.setupLink();
    this.setupCloseHandler();

    this.showHeaderByID("text-recognition-header-loading");

    resultsPromise.then(
      ({ results, direction }) => {
        if (results.length === 0) {
          // Update the UI to indicate that there were no results.
          this.showHeaderByID("text-recognition-header-no-results");
          // It's still worth recording telemetry times, as the API was still invoked.
          TelemetryStopwatch.finish(
            "TEXT_RECOGNITION_API_PERFORMANCE",
            resultsPromise
          );
          return;
        }

        // There were results, cluster them into a nice presentation, and present
        // the results to the UI.
        this.runClusteringAndUpdateUI(results, direction);
        this.showHeaderByID("text-recognition-header-results");
        TelemetryStopwatch.finish(
          "TEXT_RECOGNITION_API_PERFORMANCE",
          resultsPromise
        );

        TextRecognitionModal.recordInteractionTime();
      },
      error => {
        // There was an error in the text recognition call. Treat this as the same
        // as if there were no results, but report the error to the console and telemetry.
        this.showHeaderByID("text-recognition-header-no-results");

        console.error(
          "There was an error recognizing the text from an image.",
          error
        );
        Services.telemetry.scalarAdd(
          "browser.ui.interaction.textrecognition_error",
          1
        );
        TelemetryStopwatch.cancel(
          "TEXT_RECOGNITION_API_PERFORMANCE",
          resultsPromise
        );
      }
    );
  }

  /**
   * After the results are shown, measure how long a user interacts with the modal.
   */
  static recordInteractionTime() {
    TelemetryStopwatch.start(
      "TEXT_RECOGNITION_INTERACTION_TIMING",
      // Pass the instance of the window in case multiple tabs are doing text recognition
      // and there is a race condition.
      window
    );

    const finish = () => {
      TelemetryStopwatch.finish("TEXT_RECOGNITION_INTERACTION_TIMING", window);
      window.removeEventListener("blur", finish);
      window.removeEventListener("unload", finish);
    };

    // The user's focus went away, record this as the total interaction, even if they
    // go back and interact with it more. This can be triggered by doing actions like
    // clicking the URL bar, or by switching tabs.
    window.addEventListener("blur", finish);

    // The modal is closed.
    window.addEventListener("unload", finish);
  }

  /**
   * After the results are shown, measure how long a user interacts with the modal.
   * @param {number} length
   */
  static recordTextLengthTelemetry(length) {
    const histogram = Services.telemetry.getHistogramById(
      "TEXT_RECOGNITION_TEXT_LENGTH"
    );
    histogram.add(length);
  }

  setupCloseHandler() {
    document
      .querySelector("#text-recognition-close")
      .addEventListener("click", () => {
        window.close();
      });
  }

  /**
   * Apply the variables for the support.mozilla.org URL.
   */
  setupLink() {
    this.linkEl.href = Services.urlFormatter.formatURL(this.linkEl.href);
    this.linkEl.addEventListener("click", event => {
      event.preventDefault();
      this.openLinkIn(this.linkEl.href, "tab", {
        fromChrome: true,
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    });
  }

  /**
   * A helper to only show the appropriate header.
   *
   * @param {string} id
   */
  showHeaderByID(id) {
    for (const header of this.headerEls) {
      header.style.display = "none";
    }

    document.getElementById(id).style.display = "";
    this.resizeVertically();
  }

  /**
   * @param {string} text
   */
  static copy(text) {
    const clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
      Ci.nsIClipboardHelper
    );
    clipboard.copyString(text);
  }

  /**
   * Cluster the text based on its visual position.
   *
   * @param {TextRecognitionResult[]} results
   * @param {"ltr" | "rtl"} direction
   */
  runClusteringAndUpdateUI(results, direction) {
    /** @type {Vec2[]} */
    const centers = [];

    for (const result of results) {
      const p = result.quad;

      // Pick either the left-most or right-most edge. This optimizes for
      // aligned text over centered text.
      const minOrMax = direction === "ltr" ? Math.min : Math.max;

      centers.push([
        minOrMax(p.p1.x, p.p2.x, p.p3.x, p.p4.x),
        (p.p1.y, p.p2.y, p.p3.y, p.p4.y) / 4,
      ]);
    }

    const distSq = new DistanceSquared(centers);

    // The values are ranged 0 - 1. This value might be able to be determined
    // algorithmically.
    const averageDistance = Math.sqrt(distSq.quantile(0.2));
    const clusters = densityCluster(
      centers,
      // Neighborhood radius:
      averageDistance,
      // Minimum points to form a cluster:
      2
    );

    let text = "";
    for (const cluster of clusters) {
      const pCluster = document.createElement("p");
      pCluster.className = "textRecognitionTextCluster";

      for (let i = 0; i < cluster.length; i++) {
        const index = cluster[i];
        const { string } = results[index];
        if (i + 1 === cluster.length) {
          // Each cluster could be a paragraph, so add newlines to the end
          // for better copying.
          text += string + "\n\n";
          // The paragraph tag automatically uses two newlines.
          pCluster.innerText += string;
        } else {
          // This text is assumed to be a newlines in a paragraph, so only needs
          // to be separated by a space.
          text += string + " ";
          pCluster.innerText += string + " ";
        }
      }
      this.textEl.appendChild(pCluster);
    }

    this.textEl.style.display = "block";

    text = text.trim();
    TextRecognitionModal.copy(text);
    TextRecognitionModal.recordTextLengthTelemetry(text.length);
  }
}

/**
 * A two dimensional vector.
 *
 * @typedef {[number, number]} Vec2
 */

/**
 * @typedef {number} PointIndex
 */

/**
 * An implementation of the DBSCAN clustering algorithm.
 *
 * https://en.wikipedia.org/wiki/DBSCAN#Algorithm
 *
 * @param {Vec2[]} points
 * @param {number} distance
 * @param {number} minPoints
 * @returns {Array<PointIndex[]>}
 */
function densityCluster(points, distance, minPoints) {
  /**
   * A flat of array of labels that match the index of the points array. The values have
   * the following meaning:
   *
   *   undefined := No label has been assigned
   *   "noise"   := Noise is a point that hasn't been clustered.
   *   number    := Cluster index
   *
   * @type {undefined | "noise" | Index}
   */
  const labels = Array(points.length);
  const noiseLabel = "noise";

  let nextClusterIndex = 0;

  // Every point must be visited at least once. Often they will be visited earlier
  // in the interior of the loop.
  for (let pointIndex = 0; pointIndex < points.length; pointIndex++) {
    if (labels[pointIndex] !== undefined) {
      // This point is already labeled from the interior logic.
      continue;
    }

    // Get the neighbors that are within the range of the epsilon value, includes
    // the current point.
    const neighbors = getNeighborsWithinDistance(points, distance, pointIndex);

    if (neighbors.length < minPoints) {
      labels[pointIndex] = noiseLabel;
      continue;
    }

    // Start a new cluster.
    const clusterIndex = nextClusterIndex++;
    labels[pointIndex] = clusterIndex;

    // Fill the cluster. The neighbors array grows.
    for (let i = 0; i < neighbors.length; i++) {
      const nextPointIndex = neighbors[i];
      if (typeof labels[nextPointIndex] === "number") {
        // This point was already claimed, ignore it.
        continue;
      }

      if (labels[nextPointIndex] === noiseLabel) {
        // Claim this point and move on since noise has no neighbors.
        labels[nextPointIndex] = clusterIndex;
        continue;
      }

      // Claim this point as part of this cluster.
      labels[nextPointIndex] = clusterIndex;

      const newNeighbors = getNeighborsWithinDistance(
        points,
        distance,
        nextPointIndex
      );

      if (newNeighbors.length >= minPoints) {
        // Add on to the neighbors if more are found.
        for (const newNeighbor of newNeighbors) {
          if (!neighbors.includes(newNeighbor)) {
            neighbors.push(newNeighbor);
          }
        }
      }
    }
  }

  const clusters = [];

  // Pre-populate the clusters.
  for (let i = 0; i < nextClusterIndex; i++) {
    clusters[i] = [];
  }

  // Turn the labels into clusters, adding the noise to the end.
  for (let pointIndex = 0; pointIndex < labels.length; pointIndex++) {
    const label = labels[pointIndex];
    if (typeof label === "number") {
      clusters[label].push(pointIndex);
    } else if (label === noiseLabel) {
      // Add a single cluster.
      clusters.push([pointIndex]);
    } else {
      throw new Error("Logic error. Expected every point to have a label.");
    }
  }

  clusters.sort((a, b) => points[b[0]][1] - points[a[0]][1]);

  return clusters;
}

/**
 * @param {Vec2[]} points
 * @param {number} distance
 * @param {number} index,
 * @returns {Index[]}
 */
function getNeighborsWithinDistance(points, distance, index) {
  let neighbors = [index];
  // There is no reason to compute the square root here if we square the
  // original distance.
  const distanceSquared = distance * distance;

  for (let otherIndex = 0; otherIndex < points.length; otherIndex++) {
    if (otherIndex === index) {
      continue;
    }
    const a = points[index];
    const b = points[otherIndex];
    const dx = a[0] - b[0];
    const dy = a[1] - b[1];

    if (dx * dx + dy * dy < distanceSquared) {
      neighbors.push(otherIndex);
    }
  }

  return neighbors;
}

/**
 * This class pre-computes the squared distances to allow for efficient distance lookups,
 * and it provides a way to look up a distance quantile.
 */
class DistanceSquared {
  /** @type {Map<number>} */
  #distances = new Map();
  #list;
  #distancesSorted;

  /**
   * @param {Vec2[]} list
   */
  constructor(list) {
    this.#list = list;
    for (let aIndex = 0; aIndex < list.length; aIndex++) {
      for (let bIndex = aIndex + 1; bIndex < list.length; bIndex++) {
        const id = this.#getTupleID(aIndex, bIndex);
        const a = this.#list[aIndex];
        const b = this.#list[bIndex];
        const dx = a[0] - b[0];
        const dy = a[1] - b[1];
        this.#distances.set(id, dx * dx + dy * dy);
      }
    }
  }

  /**
   * Returns a unique tuple ID to identify the relationship between two vectors.
   */
  #getTupleID(aIndex, bIndex) {
    return aIndex < bIndex
      ? aIndex * this.#list.length + bIndex
      : bIndex * this.#list.length + aIndex;
  }

  /**
   * Returns the distance squared between two vectors.
   *
   * @param {Index} aIndex
   * @param {Index} bIndex
   * @returns {number} The distance squared
   */
  get(aIndex, bIndex) {
    return this.#distances.get(this.#getTupleID(aIndex, bIndex));
  }

  /**
   * Returns the quantile squared.
   *
   * @param {number} percentile - Ranged between 0 - 1
   * @returns {number}
   */
  quantile(percentile) {
    if (!this.#distancesSorted) {
      this.#distancesSorted = [...this.#distances.values()].sort(
        (a, b) => a - b
      );
    }
    const index = Math.max(
      0,
      Math.min(
        this.#distancesSorted.length - 1,
        Math.round(this.#distancesSorted.length * percentile)
      )
    );
    return this.#distancesSorted[index];
  }
}
