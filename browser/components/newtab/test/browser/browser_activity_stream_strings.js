XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const LOCALE_PATH = "resource://activity-stream/prerendered/";

/**
 * Test current locale strings can be fetched the way activity stream does it.
 */
add_task(async function test_activity_stream_fetch_strings() {
  const file = `${LOCALE_PATH}${aboutNewTabService.activityStreamLocale}/activity-stream-strings.js`;
  const strings = JSON.parse((await (await fetch(file)).text()).match(/{[^]*}/)[0]); // eslint-disable-line fetch-options/no-fetch-credentials
  const ids = Object.keys(strings);

  info(`Got string ids: ${ids}`);
  Assert.ok(ids.length, "Localized strings are available");
});
