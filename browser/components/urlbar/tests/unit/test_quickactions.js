/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.jsm",
});

const EXPECTED_MATCH = {
  type: UrlbarUtils.RESULT_TYPE.DYNAMIC,
  source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
  heuristic: false,
  payload: { results: [{ key: "newaction" }], dynamicType: "quickactions" },
};

add_task(async function init() {
  UrlbarPrefs.set("quickactions.enabled", true);
  // Install a default test engine.
  let engine = await addTestSuggestionsEngine();
  await Services.search.setDefault(engine);

  UrlbarProviderQuickActions.addAction("newaction", {
    commands: ["newaction"],
  });

  registerCleanupFunction(async () => {
    UrlbarPrefs.clear("quickactions.enabled");
    UrlbarProviderQuickActions.removeAction("newaction");
  });
});

add_task(async function nomatch() {
  let context = createContext("this doesnt match", {
    providers: [UrlbarProviderQuickActions.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });
});

add_task(async function quickactions_disabled() {
  UrlbarPrefs.set("quickactions.enabled", false);
  let context = createContext("new", {
    providers: [UrlbarProviderQuickActions.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });
});

add_task(async function quickactions_match() {
  UrlbarPrefs.set("quickactions.enabled", true);
  let context = createContext("new", {
    providers: [UrlbarProviderQuickActions.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MATCH],
  });
});
