/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = (URL_ROOT + "service-workers/simple-unicode.html")
  .replace("example.com", "xn--hxajbheg2az3al.xn--jxalpdlp");

/**
 * Check that the application panel displays filenames and URL's in human-readable,
 * Unicode characters, and not encoded URI's or punycode.
 */

add_task(async function() {
  await enableApplicationPanel();

  let { panel, target } = await openNewTabAndApplicationPanel(TAB_URL);
  let doc = panel.panelWin.document;

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 1);

  let workerContainer = getWorkerContainers(doc)[0];

  let scopeEl = workerContainer.querySelector(".service-worker-scope");
  ok(
    scopeEl.textContent.startsWith(
      "\u03C0\u03B1\u03C1\u03AC\u03B4\u03B5\u03B9\u03B3\u03BC\u03B1." +
      "\u03B4\u03BF\u03BA\u03B9\u03BC\u03AE"),
    "Service worker has the expected Unicode scope"
  );
  let urlEl = workerContainer.querySelector(".js-source-url");
  ok(
    urlEl.textContent.endsWith("\u65E5\u672C"),
    "Service worker has the expected Unicode url"
  );

  await unregisterAllWorkers(target.client);
});
