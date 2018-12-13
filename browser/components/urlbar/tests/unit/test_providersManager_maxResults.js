/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_maxResults() {
  let matches = [];
  for (let i = 0; i < 20; i++) {
    matches.push(new UrlbarMatch(UrlbarUtils.MATCH_TYPE.TAB_SWITCH,
                                 UrlbarUtils.MATCH_SOURCE.TABS,
                                 { url: `http://mozilla.org/foo/${i}` }));
  }
  registerBasicTestProvider(matches);

  let context = createContext();
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });

  let promise = promiseControllerNotification(controller, "onQueryFinished");
  context.maxResults = 10;
  await controller.startQuery(context);
  await promise;
  Assert.deepEqual(context.results, matches.slice(0, 10), "Check results");

  promise = promiseControllerNotification(controller, "onQueryFinished");
  context.maxResults = 1;
  await controller.startQuery(context);
  await promise;
  Assert.deepEqual(context.results, matches.slice(0, 1), "Check results");

  promise = promiseControllerNotification(controller, "onQueryFinished");
  context.maxResults = 30; // More than available.
  await controller.startQuery(context);
  await promise;
  Assert.deepEqual(context.results, matches.slice(0, 20), "Check results");
});
