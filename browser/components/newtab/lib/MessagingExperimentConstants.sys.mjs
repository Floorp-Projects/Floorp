/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file is used to define constants related to messaging experiments. It is
 * imported by both ASRouter as well as import-rollouts.js, a node script that
 * imports Nimbus rollouts into tree. It doesn't have access to any Firefox
 * APIs, XPCOM, etc. and should be kept that way.
 */

/**
 * These are the Nimbus feature IDs that correspond to messaging experiments.
 * Other Nimbus features contain specific variables whose keys are enumerated in
 * FeatureManifest.yaml. Conversely, messaging experiment features contain
 * actual messages, with the usual message keys like `template` and `targeting`.
 * @see FeatureManifest.yaml
 */
export const MESSAGING_EXPERIMENTS_DEFAULT_FEATURES = [
  "cfr",
  "fxms-message-1",
  "fxms-message-2",
  "fxms-message-3",
  "fxms-message-4",
  "fxms-message-5",
  "fxms-message-6",
  "fxms-message-7",
  "fxms-message-8",
  "fxms-message-9",
  "fxms-message-10",
  "fxms-message-11",
  "infobar",
  "moments-page",
  "pbNewtab",
  "spotlight",
  "featureCallout",
];
