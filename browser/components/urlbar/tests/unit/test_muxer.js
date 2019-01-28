/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_muxer() {
  Assert.throws(() => UrlbarProvidersManager.registerMuxer(),
                /invalid muxer/,
                "Should throw with no arguments");
  Assert.throws(() => UrlbarProvidersManager.registerMuxer({}),
                /invalid muxer/,
                "Should throw with empty object");
  Assert.throws(() => UrlbarProvidersManager.registerMuxer({
                  name: "",
                }),
                /invalid muxer/,
                "Should throw with empty name");
  Assert.throws(() => UrlbarProvidersManager.registerMuxer({
                  name: "test",
                  sort: "no",
                }),
                /invalid muxer/,
                "Should throw with invalid sort");

  let matches = [
    new UrlbarResult(UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
                     UrlbarUtils.MATCH_SOURCE.TABS,
                     { url: "http://mozilla.org/tab/" }),
    new UrlbarResult(UrlbarUtils.RESULT_TYPE.URL,
                     UrlbarUtils.MATCH_SOURCE.BOOKMARKS,
                     { url: "http://mozilla.org/bookmark/" }),
    new UrlbarResult(UrlbarUtils.RESULT_TYPE.URL,
                     UrlbarUtils.MATCH_SOURCE.HISTORY,
                     { url: "http://mozilla.org/history/" }),
  ];

  let providerName = registerBasicTestProvider(matches);
  let context = createContext(undefined, {providers: [providerName]});
  let controller = new UrlbarController({
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  /**
   * A test muxer.
   */
  class TestMuxer extends UrlbarMuxer {
    get name() {
      return "TestMuxer";
    }
    sort(queryContext) {
      queryContext.results.sort((a, b) => {
        if (b.source == UrlbarUtils.MATCH_SOURCE.TABS) {
          return -1;
        }
        if (b.source == UrlbarUtils.MATCH_SOURCE.BOOKMARKS) {
          return 1;
        }
        return a.source == UrlbarUtils.MATCH_SOURCE.BOOKMARKS ? -1 : 1;
      });
    }
  }
  let muxer = new TestMuxer();

  UrlbarProvidersManager.registerMuxer(muxer);
  context.muxer = "TestMuxer";

  info("Check results, the order should be: bookmark, history, tab");
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, [
    matches[1],
    matches[2],
    matches[0],
  ]);

  // Sanity check, should not throw.
  UrlbarProvidersManager.unregisterMuxer(muxer);
  UrlbarProvidersManager.unregisterMuxer("TestMuxer"); // no-op.
});
