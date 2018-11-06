/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_providers() {
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
