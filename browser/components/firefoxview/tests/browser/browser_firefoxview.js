/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function about_firefoxview_smoke_test() {
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    // sanity check the important regions exist on this page
    ok(
      document.querySelector("fxview-category-navigation"),
      "fxview-category-navigation element exists"
    );
    ok(document.querySelector("named-deck"), "named-deck element exists");
  });
});
