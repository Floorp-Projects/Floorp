/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function() {
  const { front, client } = await initPerfFront();

  info("Get the supported features from the perf actor.");
  const features = await front.getSupportedFeatures();

  ok(Array.isArray(features), "The features are an array.");
  ok(!!features.length, "There are many features supported.");
  ok(
    features.includes("js"),
    "All platforms support the js feature, and it's in this list."
  );

  // clean up
  await front.stopProfilerAndDiscardProfile();
  await front.destroy();
  await client.close();
});
