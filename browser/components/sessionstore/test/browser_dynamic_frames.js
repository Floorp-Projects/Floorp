/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that static frames of framesets are serialized but dynamically
 * inserted iframes are ignored.
 */
add_task(function () {
  // This URL has the following frames:
  //  + about:mozilla (static)
  //  + about:robots (static)
  //  + about:rights (dynamic iframe)
  const URL = "data:text/html;charset=utf-8," +
              "<frameset cols=50%25,50%25><frame src=about%3Amozilla>" +
              "<frame src=about%3Arobots></frameset>" +
              "<script>var i=document.createElement('iframe');" +
              "i.setAttribute('src', 'about%3Arights');" +
              "document.body.appendChild(i);</script>";

  // Add a new tab with two "static" and one "dynamic" frame.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));

  // Check URLs.
  ok(entries[0].url.startsWith("data:text/html"), "correct root url");
  is(entries[0].children[0].url, "about:mozilla", "correct url for 1st frame");
  is(entries[0].children[1].url, "about:robots", "correct url for 2nd frame");

  // Check the number of children.
  is(entries.length, 1, "there is one root entry ...");
  is(entries[0].children.length, 2, "... with two child entries");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * Ensure that iframes created by the network parser are serialized but
 * dynamically inserted iframes are ignored. Navigating a subframe should
 * create a second root entry that doesn't contain any dynamic children either.
 */
add_task(function () {
  // This URL has the following frames:
  //  + about:mozilla (static iframe)
  //  + about:rights (dynamic iframe)
  const URL = "data:text/html;charset=utf-8," +
              "<iframe name=t src=about%3Amozilla></iframe>" +
              "<a id=lnk href=about%3Arobots target=t>clickme</a>" +
              "<script>var i=document.createElement('iframe');" +
              "i.setAttribute('src', 'about%3Arights');" +
              "document.body.appendChild(i);</script>";

  // Add a new tab with one "static" and one "dynamic" frame.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));

  // Check URLs.
  ok(entries[0].url.startsWith("data:text/html"), "correct root url");
  is(entries[0].children[0].url, "about:mozilla", "correct url for static frame");

  // Check the number of children.
  is(entries.length, 1, "there is one root entry ...");
  is(entries[0].children.length, 1, "... with a single child entry");

  // Navigate the subframe.
  browser.messageManager.sendAsyncMessage("ss-test:click", {id: "lnk"});
  yield promiseBrowserLoaded(browser, false /* don't ignore subframes */);

  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));

  // Check URLs.
  ok(entries[0].url.startsWith("data:text/html"), "correct 1st root url");
  ok(entries[1].url.startsWith("data:text/html"), "correct 2nd root url");
  is(entries[0].children[0].url, "about:mozilla", "correct url for 1st static frame");
  is(entries[1].children[0].url, "about:robots", "correct url for 2ns static frame");

  // Check the number of children.
  is(entries.length, 2, "there are two root entries ...");
  is(entries[0].children.length, 1, "... with a single child entry ...");
  is(entries[1].children.length, 1, "... each");

  // Cleanup.
  gBrowser.removeTab(tab);
});
