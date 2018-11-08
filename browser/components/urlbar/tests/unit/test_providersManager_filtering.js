/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_filtering() {
  let match = new UrlbarMatch(UrlbarUtils.MATCH_TYPE.TAB_SWITCH,
                              UrlbarUtils.MATCH_SOURCE.TABS,
                              { url: "http://mozilla.org/foo/" });
  registerBasicTestProvider([match]);

  let context = createContext();
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });

  info("Disable the only available source, should get no matches");
  Services.prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
  let promise = Promise.race([
    promiseControllerNotification(controller, "onQueryResults", false),
    promiseControllerNotification(controller, "onQueryFinished"),
  ]);
  await controller.startQuery(context);
  await promise;
  Services.prefs.clearUserPref("browser.urlbar.suggest.openpage");

  let matches = [
    match,
    new UrlbarMatch(UrlbarUtils.MATCH_TYPE.TAB_SWITCH,
                    UrlbarUtils.MATCH_SOURCE.HISTORY,
                    { url: "http://mozilla.org/foo/" }),
  ];
  registerBasicTestProvider(matches);

  info("Disable one of the sources, should get a single match");
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  promise = Promise.all([
    promiseControllerNotification(controller, "onQueryResults"),
    promiseControllerNotification(controller, "onQueryFinished"),
  ]);
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, [match]);
  Services.prefs.clearUserPref("browser.urlbar.suggest.history");

  info("Use a restriction character, should get a single match");
  context = createContext(`foo ${UrlbarTokenizer.RESTRICT.OPENPAGE}`);
  promise = Promise.all([
    promiseControllerNotification(controller, "onQueryResults"),
    promiseControllerNotification(controller, "onQueryFinished"),
  ]);
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, [match]);
});

add_task(async function test_filter_javascript() {
  let context = createContext();
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  let match = new UrlbarMatch(UrlbarUtils.MATCH_TYPE.TAB_SWITCH,
                              UrlbarUtils.MATCH_SOURCE.TABS,
                              { url: "http://mozilla.org/foo/" });
  let jsMatch = new UrlbarMatch(UrlbarUtils.MATCH_TYPE.TAB_SWITCH,
                                UrlbarUtils.MATCH_SOURCE.HISTORY,
                                { url: "javascript:foo" });
  registerBasicTestProvider([match, jsMatch]);

  info("By default javascript should be filtered out");
  let promise = promiseControllerNotification(controller, "onQueryResults");
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, [match]);

  info("Except when the user explicitly starts the search with javascript:");
  context = createContext(`javascript: ${UrlbarTokenizer.RESTRICT.HISTORY}`);
  promise = promiseControllerNotification(controller, "onQueryResults");
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, [jsMatch]);

  info("Disable javascript filtering");
  Services.prefs.setBoolPref("browser.urlbar.filter.javascript", false);
  context = createContext();
  promise = promiseControllerNotification(controller, "onQueryResults");
  await controller.startQuery(context, controller);
  await promise;
  Assert.deepEqual(context.results, [match, jsMatch]);
  Services.prefs.clearUserPref("browser.urlbar.filter.javascript");
});
