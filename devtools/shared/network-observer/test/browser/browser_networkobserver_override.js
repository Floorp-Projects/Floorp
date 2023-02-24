/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = URL_ROOT + "doc_network-observer.html";
const REQUEST_URL =
  URL_ROOT + `sjs_network-observer-test-server.sjs?sts=200&fmt=html`;
const GZIPPED_REQUEST_URL = URL_ROOT + `gzipped.sjs`;
const OVERRIDE_FILENAME = "override.js";
const OVERRIDE_HTML_FILENAME = "override.html";

add_task(async function testLocalOverride() {
  await addTab(TEST_URL);

  let eventsCount = 0;
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== REQUEST_URL,
    onNetworkEvent: event => {
      info("received a network event");
      eventsCount++;
      return createNetworkEventOwner(event);
    },
  });

  const overrideFile = getChromeDir(getResolvedURI(gTestPath));
  overrideFile.append(OVERRIDE_FILENAME);
  info(" override " + REQUEST_URL + " to " + overrideFile.path + "\n");
  networkObserver.override(REQUEST_URL, overrideFile.path);

  info("Assert that request and cached request are overriden");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL],
    async _url => {
      const request = await content.wrappedJSObject.fetch(_url);
      const requestcontent = await request.text();
      is(
        requestcontent,
        `"use strict";\ndocument.title = "evaluated";\n`,
        "the request content has been overriden"
      );
      const secondRequest = await content.wrappedJSObject.fetch(_url);
      const secondRequestcontent = await secondRequest.text();
      is(
        secondRequestcontent,
        `"use strict";\ndocument.title = "evaluated";\n`,
        "the cached request content has been overriden"
      );
    }
  );

  info("Assert that JS scripts can be overriden");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL],
    async _url => {
      const script = await content.document.createElement("script");
      const onLoad = new Promise(resolve =>
        script.addEventListener("load", resolve, { once: true })
      );
      script.src = _url;
      content.document.body.appendChild(script);
      await onLoad;
      is(
        content.document.title,
        "evaluated",
        "The <script> tag content has been overriden and correctly evaluated"
      );
    }
  );

  await BrowserTestUtils.waitForCondition(() => eventsCount >= 1);

  networkObserver.destroy();
});

add_task(async function testHtmlFileOverride() {
  let eventsCount = 0;
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== TEST_URL,
    onNetworkEvent: event => {
      info("received a network event");
      eventsCount++;
      return createNetworkEventOwner(event);
    },
  });

  const overrideFile = getChromeDir(getResolvedURI(gTestPath));
  overrideFile.append(OVERRIDE_HTML_FILENAME);
  info(" override " + TEST_URL + " to " + overrideFile.path + "\n");
  networkObserver.override(TEST_URL, overrideFile.path);

  await addTab(TEST_URL);
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [TEST_URL],
    async pageUrl => {
      is(
        content.document.documentElement.outerHTML,
        "<html><head></head><body>Overriden!\n</body></html>",
        "The content of the HTML has been overriden"
      );
      // For now, all overriden request have their location changed to an internal data: URI
      // Bug xxx aims at keeping the original URI.
      todo_is(
        content.location.href,
        pageUrl,
        "The location of the page is still the original one"
      );
    }
  );
  await BrowserTestUtils.waitForCondition(() => eventsCount >= 1);
  networkObserver.destroy();
});

// Exact same test, but with a gzipped request, which requires very special treatment
add_task(async function testLocalOverrideGzipped() {
  await addTab(TEST_URL);

  let eventsCount = 0;
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== GZIPPED_REQUEST_URL,
    onNetworkEvent: event => {
      info("received a network event");
      eventsCount++;
      return createNetworkEventOwner(event);
    },
  });

  const overrideFile = getChromeDir(getResolvedURI(gTestPath));
  overrideFile.append(OVERRIDE_FILENAME);
  info(" override " + GZIPPED_REQUEST_URL + " to " + overrideFile.path + "\n");
  networkObserver.override(GZIPPED_REQUEST_URL, overrideFile.path);

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [GZIPPED_REQUEST_URL],
    async _url => {
      const request = await content.wrappedJSObject.fetch(_url);
      const requestcontent = await request.text();
      is(
        requestcontent,
        `"use strict";\ndocument.title = "evaluated";\n`,
        "the request content has been overriden"
      );
      const secondRequest = await content.wrappedJSObject.fetch(_url);
      const secondRequestcontent = await secondRequest.text();
      is(
        secondRequestcontent,
        `"use strict";\ndocument.title = "evaluated";\n`,
        "the cached request content has been overriden"
      );
    }
  );

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [GZIPPED_REQUEST_URL],
    async _url => {
      const script = await content.document.createElement("script");
      const onLoad = new Promise(resolve =>
        script.addEventListener("load", resolve, { once: true })
      );
      script.src = _url;
      content.document.body.appendChild(script);
      await onLoad;
      is(
        content.document.title,
        "evaluated",
        "The <script> tag content has been overriden and correctly evaluated"
      );
    }
  );

  await BrowserTestUtils.waitForCondition(() => eventsCount >= 1);

  networkObserver.destroy();
});
