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
    inQuickActionsSearchMode: false,
    helpUrl: UrlbarProviderQuickActions.helpUrl,
    inputLength,
  },
});

testEngine_setup();

add_setup(async () => {
  UrlbarPrefs.set("quickactions.enabled", true);
  UrlbarPrefs.set("suggest.quickactions", true);

  UrlbarProviderQuickActions.addAction("newaction", {
    commands: ["newaction"],
  });

  registerCleanupFunction(async () => {
    UrlbarPrefs.clear("quickactions.enabled");
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

add_task(async function minimum_search_string() {
  let searchString = "newa";
  for (let minimumSearchString of [0, 3]) {
    UrlbarPrefs.set("quickactions.minimumSearchString", minimumSearchString);
    for (let i = 1; i < 4; i++) {
      let context = createContext(searchString.substring(0, i), {
        providers: [UrlbarProviderQuickActions.name],
        isPrivate: false,
      });
      let matches =
        i >= minimumSearchString ? [expectedMatch("newaction", i)] : [];
      await check_results({ context, matches });
    }
  }
  UrlbarPrefs.clear("quickactions.minimumSearchString");
});
