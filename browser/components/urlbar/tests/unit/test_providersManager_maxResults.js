/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_maxResults() {
  const MATCHES_LENGTH = 20;
  let matches = [];
  for (let i = 0; i < MATCHES_LENGTH; i++) {
    matches.push(new UrlbarResult(UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
                                  UrlbarUtils.MATCH_SOURCE.TABS,
                                  { url: `http://mozilla.org/foo/${i}` }));
  }
  let providerName = registerBasicTestProvider(matches);
  let context = createContext(undefined, {providers: [providerName]});
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });

  async function test_count(count) {
    let promise = promiseControllerNotification(controller, "onQueryFinished");
    context.maxResults = count;
    await controller.startQuery(context);
    await promise;
    Assert.equal(context.results.length, Math.min(MATCHES_LENGTH, count),
                 "Check count");
    Assert.deepEqual(context.results, matches.slice(0, count), "Check results");
  }
  await test_count(10);
  await test_count(1);
  await test_count(30);
});
