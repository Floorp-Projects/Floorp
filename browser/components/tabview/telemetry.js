/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Collects telemetry data for Tabview.
 */
let Telemetry = {
  TOPIC_GATHER_TELEMETRY: "gather-telemetry",

  /**
   * Initializes the object.
   */
  init: function Telemetry_init() {
    Services.obs.addObserver(this, this.TOPIC_GATHER_TELEMETRY, false);
  }, 

  /**
   * Uninitializes the object.
   */
  uninit: function Telemetry_uninit() {
    Services.obs.removeObserver(this, this.TOPIC_GATHER_TELEMETRY);
  },

  /**
   * Adds telemetry values to gather usage statistics.
   */
  _collect: function Telemetry_collect() {
    let stackedGroupsCount = 0;
    let childCounts = [];

    GroupItems.groupItems.forEach(function (groupItem) {
      if (!groupItem.isEmpty()) {
        childCounts.push(groupItem.getChildren().length);

        if (groupItem.isStacked())
          stackedGroupsCount++;
      }
    });

    function addTelemetryValue(aId, aValue) {
      Services.telemetry.getHistogramById("PANORAMA_" + aId).add(aValue);
    }
    function median(aChildCounts) {
      aChildCounts.sort(function(x, y) { return x - y; });
      let middle = Math.floor(aChildCounts.length / 2);
      return aChildCounts[middle];
    }

    addTelemetryValue("GROUPS_COUNT", GroupItems.groupItems.length);
    addTelemetryValue("STACKED_GROUPS_COUNT", stackedGroupsCount);
    addTelemetryValue("MEDIAN_TABS_IN_GROUPS_COUNT", median(childCounts));
  },

  /**
   * Observes for gather telemetry topic.
   */
  observe: function Telemetry_observe(aSubject, aTopic, aData) {
    if (!gWindow.PrivateBrowsingUtils.isWindowPrivate(gWindow))
      this._collect();
  }
}

