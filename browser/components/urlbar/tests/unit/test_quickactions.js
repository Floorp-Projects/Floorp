/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
});

let expectedMatch = (key, inputLength) => ({
  type: UrlbarUtils.RESULT_TYPE.DYNAMIC,
  source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
  heuristic: false,
  payload: {
    results: [{ key }],
    dynamicType: "quickactions",
    helpUrl: UrlbarProviderQuickActions.helpUrl,
    inputLength,
  },
});

add_task(async function init() {
  UrlbarPrefs.set("suggest.quickactions", true);
  // Install a default test engine.
  let engine = await addTestSuggestionsEngine();
  await Services.search.setDefault(engine);

  UrlbarProviderQuickActions.addAction("newaction", {
    commands: ["newaction"],
  });

  registerCleanupFunction(async () => {
    UrlbarPrefs.clear("suggest.quickactions");
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
  UrlbarPrefs.set("suggest.quickactions", false);
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
  UrlbarPrefs.set("suggest.quickactions", true);
  let context = createContext("new", {
    providers: [UrlbarProviderQuickActions.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [expectedMatch("newaction", 3)],
  });
});

add_task(async function duplicate_matches() {
  UrlbarProviderQuickActions.addAction("testaction", {
    commands: ["testaction", "test"],
  });

  let context = createContext("testaction", {
    providers: [UrlbarProviderQuickActions.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [expectedMatch("testaction", 10)],
  });

  UrlbarProviderQuickActions.removeAction("testaction");
});

add_task(async function remove_action() {
  UrlbarProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
  });
  UrlbarProviderQuickActions.removeAction("testaction");

  let context = createContext("test", {
    providers: [UrlbarProviderQuickActions.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [],
  });
});
