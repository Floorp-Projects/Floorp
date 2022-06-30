/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This test focus on asserting the source content displayed in CodeMirror
 * when we open a source from the SourceTree (or by any other means).
 *
 * The source content is being fetched from the server only on-demand.
 * The main shortcoming is about sources being GC-ed. This only happens
 * when we open the debugger on an already loaded page.
 * When we (re)load a page while the debugger is opened, sources are never GC-ed.
 * There are also specifics related to HTML page having inline scripts.
 * Also, as this data is fetched on-demand, there is a loading prompt
 * being displayed while the source is being fetched from the server.
 */

"use strict";

const httpServer = createTestHTTPServer();
const BASE_URL = `http://localhost:${httpServer.identity.primaryPort}/`;
const loadCounts = {};

/**
 * Simple tests, asserting that we correctly display source text content in CodeMirror
 */
const NAMED_EVAL_CONTENT = `function namedEval() {}; console.log('eval script'); //# sourceURL=named-eval.js`;
const INDEX_PAGE_CONTENT = `<!DOCTYPE html>
    <html>
      <head>
        <script type="text/javascript" src="/normal-script.js"></script>
        <script type="text/javascript" src="/slow-loading-script.js"></script>
        <script type="text/javascript" src="/http-error-script.js"></script>
        <script type="text/javascript" src="/same-url.js"></script>
        <script>
          console.log("inline script");
        </script>
        <script>
          console.log("second inline script");
          this.evaled1 = eval("${NAMED_EVAL_CONTENT}");

          // Load same-url.js in various different ways
          this.evaled2 = eval("function sameUrlEval() {}; //# sourceURL=same-url.js");

          const script = document.createElement("script");
          script.src = "same-url.js";
          document.documentElement.append(script);

          // Ensure loading the same-url.js file *after*
          // the iframe is done loading. So that we have a deterministic load order.
          window.onload = () => {
            this.worker = new Worker("same-url.js");
            this.worker.postMessage("foo");
          };
        </script>
      </head>
      <body>
        <iframe src="iframe.html"></iframe>
      </body>
    </html>`;
const IFRAME_CONTENT = `<!DOCTYPE html>
    <html>
      <head>
        <script type="text/javascript" src="/same-url.js"></script>
      </head>
      <body>
        <script>
          console.log("First inline script");
        </script>
        <script>
          console.log("Second inline script");
        </script>
      </body>
    </html>`;

httpServer.registerPathHandler("/index.html", (request, response) => {
  loadCounts[request.path] = (loadCounts[request.path] || 0) + 1;
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(INDEX_PAGE_CONTENT);
});

httpServer.registerPathHandler("/normal-script.js", (request, response) => {
  loadCounts[request.path] = (loadCounts[request.path] || 0) + 1;
  response.setHeader("Content-Type", "application/javascript");
  response.write(`console.log("normal script")`);
});
httpServer.registerPathHandler(
  "/slow-loading-script.js",
  (request, response) => {
    loadCounts[request.path] = (loadCounts[request.path] || 0) + 1;
    response.processAsync();
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(function() {
      response.setHeader("Content-Type", "application/javascript");
      response.write(`console.log("slow loading script")`);
      response.finish();
    }, 1000);
  }
);
httpServer.registerPathHandler("/http-error-script.js", (request, response) => {
  loadCounts[request.path] = (loadCounts[request.path] || 0) + 1;
  response.setStatusLine(request.httpVersion, 404, "Not found");
  response.write(`console.log("http error")`);
});
httpServer.registerPathHandler("/same-url.js", (request, response) => {
  loadCounts[request.path] = (loadCounts[request.path] || 0) + 1;
  const sameUrlLoadCount = loadCounts[request.path];
  // Prevents gecko from cache this request in order to force fetching
  // a new, distinct content for each usage of this URL
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("Content-Type", "application/javascript");
  response.write(`console.log("same url #${sameUrlLoadCount}")`);
});
httpServer.registerPathHandler("/iframe.html", (request, response) => {
  loadCounts[request.path] = (loadCounts[request.path] || 0) + 1;
  response.setHeader("Content-Type", "text/html");
  response.write(IFRAME_CONTENT);
});
add_task(async function testSourceTextContent() {
  const dbg = await initDebuggerWithAbsoluteURL("about:blank");

  // Load the document *once* the debugger is opened
  // in order to avoid having any source being GC-ed.
  await navigateToAbsoluteURL(
    dbg,
    BASE_URL + "index.html",
    "index.html",
    "normal-script.js",
    "slow-loading-script.js",
    "same-url.js"
  );

  await selectSource(dbg, "normal-script.js");
  is(getCM(dbg).getValue(), `console.log("normal script")`);

  await selectSource(dbg, "slow-loading-script.js");
  is(getCM(dbg).getValue(), `console.log("slow loading script")`);

  await selectSource(dbg, "index.html");
  is(getCM(dbg).getValue(), INDEX_PAGE_CONTENT);

  await selectSource(dbg, "named-eval.js");
  is(getCM(dbg).getValue(), NAMED_EVAL_CONTENT);

  const mainThreadSameUrlSource = findSourceInThread(
    dbg,
    "same-url.js",
    "Main Thread"
  );
  await selectSource(dbg, mainThreadSameUrlSource);
  is(
    getCM(dbg).getValue(),
    `console.log("same url #1")`,
    "We get an arbitrary content for same-url, the first loaded one"
  );
  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    is(
      dbg.selectors.getSourceActorsForSource(mainThreadSameUrlSource.id).length,
      3,
      "same-url.js is loaded 3 times in the main thread"
    );

    const iframeSameUrlSource = findSourceInThread(
      dbg,
      "same-url.js",
      BASE_URL + "iframe.html"
    );
    await selectSource(dbg, iframeSameUrlSource);
    is(
      getCM(dbg).getValue(),
      `console.log("same url #3")`,
      "We get the expected content for same-url.js in the iframe"
    );
    is(
      dbg.selectors.getSourceActorsForSource(iframeSameUrlSource.id).length,
      1,
      "same-url.js is loaded one time in the iframe thread"
    );
  } else {
    // There is no iframe thread when fission is off
    is(
      dbg.selectors.getSourceActorsForSource(mainThreadSameUrlSource.id).length,
      4,
      "same-url.js is loaded 4 times in the main thread without fission"
    );
  }

  const workerSameUrlSource = findSourceInThread(
    dbg,
    "same-url.js",
    BASE_URL + "same-url.js"
  );
  await selectSource(dbg, workerSameUrlSource);
  is(
    getCM(dbg).getValue(),
    `console.log("same url #4")`,
    "We get the expected content for same-url.js worker"
  );
  is(
    dbg.selectors.getSourceActorsForSource(workerSameUrlSource.id).length,
    1,
    "same-url.js is loaded one time in the worker thread"
  );

  await selectSource(dbg, "iframe.html");
  is(getCM(dbg).getValue(), IFRAME_CONTENT);

  ok(
    !sourceExists(dbg, "http-error-script.js"),
    "scripts with HTTP error code do not appear in the source list"
  );

  // As we are loading the page while the debugger is already opened,
  // none of the resources are loaded twice.
  is(loadCounts["/index.html"], 1, "We loaded index.html only once");
  is(
    loadCounts["/normal-script.js"],
    1,
    "We loaded normal-script.js only once"
  );
  is(
    loadCounts["/slow-loading-script.js"],
    1,
    "We loaded slow-loading-script.js only once"
  );
  is(
    loadCounts["/same-url.js"],
    4,
    "We loaded same-url.js in 4 distinct ways (the named eval doesn't count)"
  );
  // For some reason external to the debugger, we issue two requests to scripts having http error codes.
  // These two requests are done before opening the debugger.
  is(
    loadCounts["/http-error-script.js"],
    2,
    "We loaded http-error-script.js twice, only before the debugger is opened"
  );
});

/**
 * In this test, we force a GC before loading DevTools.
 * So that Spidermonkey will no longer have access to the sources
 * and another request should be issues to load the source text content.
 */
const GARBAGED_PAGE_CONTENT = `<!DOCTYPE html>
    <html>
      <head>
        <script type="text/javascript" src="/garbaged-script.js"></script>
        <script>
          console.log("garbaged inline script");
        </script>
      </head>
    </html>`;

httpServer.registerPathHandler(
  "/garbaged-collected.html",
  (request, response) => {
    loadCounts[request.path] = (loadCounts[request.path] || 0) + 1;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(GARBAGED_PAGE_CONTENT);
  }
);

httpServer.registerPathHandler("/garbaged-script.js", (request, response) => {
  loadCounts[request.path] = (loadCounts[request.path] || 0) + 1;
  response.setHeader("Content-Type", "application/javascript");
  response.write(`console.log("garbaged script ${loadCounts[request.path]}")`);
});
add_task(async function testGarbageCollectedSourceTextContent() {
  const tab = await addTab(BASE_URL + "garbaged-collected.html");
  is(
    loadCounts["/garbaged-collected.html"],
    1,
    "The HTML page is loaded once before opening the DevTools"
  );
  is(
    loadCounts["/garbaged-script.js"],
    1,
    "The script is loaded once before opening the DevTools"
  );

  // Force freeing both the HTML page and script in memory
  // so that the debugger has to fetch source content from http cache.
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    Cu.forceGC();
  });

  const toolbox = await openToolboxForTab(tab, "jsdebugger");
  const dbg = createDebuggerContext(toolbox);
  await waitForSources(dbg, "garbaged-collected.html", "garbaged-script.js");

  await selectSource(dbg, "garbaged-script.js");
  // XXX Bug 1758454 - Source content of GC-ed script can be wrong!
  // Even if we have to issue a new HTTP request for this source,
  // we should be using HTTP cache and retrieve the first served version which
  // is the one that actually runs in the page!
  // We should be displaying `console.log("garbaged script 1")`,
  // but instead, a new HTTP request is dispatched and we get a new content.
  is(getCM(dbg).getValue(), `console.log("garbaged script 2")`);

  await selectSource(dbg, "garbaged-collected.html");
  is(getCM(dbg).getValue(), GARBAGED_PAGE_CONTENT);

  is(
    loadCounts["/garbaged-collected.html"],
    2,
    "We loaded the html page once as we haven't tried to display it in the debugger (2)"
  );
  is(
    loadCounts["/garbaged-script.js"],
    2,
    "We loaded the garbaged script twice as we lost its content"
  );
});

/**
 * Test failures when trying to open the source text content.
 *
 * In this test we load an html page
 * - with inline source (so that it shows up in the debugger)
 * - it first loads fine so that it shows up
 * - initDebuggerWithAbsoluteURL will first load the document before the debugger
 * - so the debugger will have to fetch the html page content via a network request
 * - the test page will return a 404 error code on the second load attempt
 */
let loadCount = 0;
httpServer.registerPathHandler(
  "/200-then-404-page.html",
  (request, response) => {
    loadCount++;
    if (loadCount > 1) {
      response.setStatusLine(request.httpVersion, 404, "Not found");
      return;
    }
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`<!DOCTYPE html><script>console.log("200 page");</script>`);
  }
);
add_task(async function testFailingHtmlSource() {
  info("Test failure in retrieving html page sources");

  // initDebuggerWithAbsoluteURL will first load the document once before the debugger,
  // then the debugger will have to fetch the html page content via a network request
  // therefore the test page will return a 404 error code on the second load attempt
  const dbg = await initDebuggerWithAbsoluteURL(
    BASE_URL + "200-then-404-page.html",
    "200-then-404-page.html"
  );

  // We can't select the HTML page as its source content isn't fetched
  // (waitForSelectedSource doesn't resolve)
  // Note that it is important to load the page *before* opening the page
  // so that the thread actor has to request the page content and will fail
  const source = findSource(dbg, "200-then-404-page.html");
  await dbg.actions.selectLocation(
    getContext(dbg),
    { sourceId: source.id },
    { keepContext: false }
  );
  is(getCM(dbg).getValue(), `Error loading this URI: Unknown source`);
});

/**
 * In this test we try to reproduce the "Loading..." message.
 * This may happen when opening an HTML source that was loaded *before*
 * opening DevTools. The thread actor will have to issue a new HTTP request
 * to load the source content.
 */
let loadCount2 = 0;
let slowLoadingPageResolution = null;
httpServer.registerPathHandler(
  "/slow-loading-page.html",
  (request, response) => {
    loadCount2++;
    if (loadCount2 > 1) {
      response.processAsync();
      slowLoadingPageResolution = function() {
        response.write(
          `<!DOCTYPE html><script>console.log("slow-loading-page:second-load");</script>`
        );
        response.finish();
      };
      return;
    }
    response.write(
      `<!DOCTYPE html><script>console.log("slow-loading-page:first-load");</script>`
    );
  }
);
add_task(async function testLoadingHtmlSource() {
  info("Test loading progress of html page sources");
  const dbg = await initDebuggerWithAbsoluteURL(
    BASE_URL + "slow-loading-page.html",
    "slow-loading-page.html"
  );

  const onSelected = selectSource(dbg, "slow-loading-page.html");
  await waitFor(
    () => getCM(dbg).getValue() == `Loading…`,
    "Wait for the source to be displayed as loading"
  );

  info("Wait for a second HTTP request to be made for the html page");
  await waitFor(
    () => slowLoadingPageResolution,
    "Wait for the html page to be queried a second time"
  );
  is(
    getCM(dbg).getValue(),
    `Loading…`,
    "The source is still loading until we release the network request"
  );

  slowLoadingPageResolution();
  info("Wait for the source to be fully selected and loaded");
  await onSelected;

  // Note that, even if the thread actor triggers a new HTTP request,
  // it will use the HTTP cache and retrieve the first request content.
  // This is actually relevant as that's the source that actually runs in the page!
  //
  // XXX Bug 1758458 - the source content is wrong.
  // We should be seeing the whole HTML page content,
  // whereas we only see the inline source text content.
  is(getCM(dbg).getValue(), `console.log("slow-loading-page:first-load");`);
});
