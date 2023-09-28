"use strict";

const TEST_ROOT =
  "http://mochi.test:8888/browser/browser/base/content/test/favicons/";
const TEST_URL = TEST_ROOT + "file_favicon_change_not_in_document.html";

// Runs the given task in the document of the browser.
function runInDoc(browser, task) {
  return ContentTask.spawn(browser, `(${task.toString()})();`, scriptStr => {
    let script = content.document.createElement("script");
    script.textContent = scriptStr;
    content.document.body.appendChild(script);

    // Link events are dispatched asynchronously so allow the event loop to run
    // to ensure that any events are actually dispatched before returning.
    return new Promise(resolve => content.setTimeout(resolve, 0));
  });
}

/*
 * This test tests a link element won't fire DOMLinkChanged/DOMLinkAdded unless
 * it is added to the DOM. See more details in bug 1083895.
 *
 * Note that there is debounce logic in FaviconLoader.sys.mjs, adding a new
 * icon link after the icon parsing timeout will trigger a new icon extraction
 * cycle. Hence, there should be two favicons loads in this test as it appends
 * a new link to the DOM in the timeout callback defined in the test HTML page.
 * However, the not-yet-added link element with href as "http://example.org/other-icon"
 * should not fire the DOMLinkAdded event, nor should it fire the DOMLinkChanged
 * event after its href gets updated later.
 */
add_task(async function () {
  let extraTab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_ROOT
  ));
  let domLinkAddedFired = 0;
  let domLinkChangedFired = 0;
  const linkAddedHandler = event => domLinkAddedFired++;
  const linkChangedhandler = event => domLinkChangedFired++;
  BrowserTestUtils.addContentEventListener(
    gBrowser.selectedBrowser,
    "DOMLinkAdded",
    linkAddedHandler
  );
  BrowserTestUtils.addContentEventListener(
    gBrowser.selectedBrowser,
    "DOMLinkChanged",
    linkChangedhandler
  );

  let expectedFavicon = TEST_ROOT + "file_generic_favicon.ico";
  let faviconPromise = waitForFavicon(extraTab.linkedBrowser, expectedFavicon);

  BrowserTestUtils.startLoadingURIString(extraTab.linkedBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(extraTab.linkedBrowser);

  await faviconPromise;

  is(
    domLinkAddedFired,
    2,
    "Should fire the correct number of DOMLinkAdded event."
  );
  is(domLinkChangedFired, 0, "Should not fire any DOMLinkChanged event.");

  gBrowser.removeTab(extraTab);
});

/*
 * This verifies that creating and manipulating link elements inside document
 * fragments doesn't trigger the link events.
 */
add_task(async function () {
  let extraTab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    TEST_ROOT
  ));
  let browser = extraTab.linkedBrowser;

  let domLinkAddedFired = 0;
  let domLinkChangedFired = 0;
  const linkAddedHandler = event => domLinkAddedFired++;
  const linkChangedhandler = event => domLinkChangedFired++;
  BrowserTestUtils.addContentEventListener(
    browser,
    "DOMLinkAdded",
    linkAddedHandler
  );
  BrowserTestUtils.addContentEventListener(
    browser,
    "DOMLinkChanged",
    linkChangedhandler
  );

  BrowserTestUtils.startLoadingURIString(browser, TEST_ROOT + "blank.html");
  await BrowserTestUtils.browserLoaded(browser);

  is(domLinkAddedFired, 0, "Should have been no link add events");
  is(domLinkChangedFired, 0, "Should have been no link change events");

  await runInDoc(browser, () => {
    let fragment = document
      .createRange()
      .createContextualFragment(
        '<link type="image/ico" href="file_generic_favicon.ico" rel="icon">'
      );
    fragment.firstElementChild.setAttribute("type", "image/png");
  });

  is(domLinkAddedFired, 0, "Should have been no link add events");
  is(domLinkChangedFired, 0, "Should have been no link change events");

  await runInDoc(browser, () => {
    let fragment = document.createDocumentFragment();
    let link = document.createElement("link");
    link.setAttribute("href", "file_generic_favicon.ico");
    link.setAttribute("rel", "icon");
    link.setAttribute("type", "image/ico");

    fragment.appendChild(link);
    link.setAttribute("type", "image/png");
  });

  is(domLinkAddedFired, 0, "Should have been no link add events");
  is(domLinkChangedFired, 0, "Should have been no link change events");

  let expectedFavicon = TEST_ROOT + "file_generic_favicon.ico";
  let faviconPromise = waitForFavicon(browser, expectedFavicon);

  // Moving an element from the fragment into the DOM should trigger the add
  // events and start favicon loading.
  await runInDoc(browser, () => {
    let fragment = document
      .createRange()
      .createContextualFragment(
        '<link type="image/ico" href="file_generic_favicon.ico" rel="icon">'
      );
    document.head.appendChild(fragment);
  });

  is(domLinkAddedFired, 1, "Should have been one link add events");
  is(domLinkChangedFired, 0, "Should have been no link change events");

  await faviconPromise;

  gBrowser.removeTab(extraTab);
});
