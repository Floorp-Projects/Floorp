/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests basic pretty-printing functionality.

"use strict";

requestLongerTimeout(2);

add_task(async function () {
  const dbg = await initDebugger("doc-minified.html", "math.min.js");

  await selectSource(dbg, "math.min.js", 2);
  clickElement(dbg, "prettyPrintButton");

  await waitForSelectedSource(dbg, "math.min.js:formatted");
  ok(
    !findElement(dbg, "mappedSourceLink"),
    "When we are on the pretty printed source, we don't show the link to the minified source"
  );
  const ppSrc = findSource(dbg, "math.min.js:formatted");

  ok(ppSrc, "Pretty-printed source exists");

  // this is not implemented yet
  // assertHighlightLocation(dbg, "math.min.js:formatted", 18);
  // await selectSource(dbg, "math.min.js")
  await addBreakpoint(dbg, ppSrc, 18);

  invokeInTab("arithmetic");
  await waitForPaused(dbg);

  assertPausedAtSourceAndLine(dbg, ppSrc.id, 18);

  await stepOver(dbg);

  assertPausedAtSourceAndLine(dbg, ppSrc.id, 39);

  await resume(dbg);

  // The pretty-print button should be disabled in the pretty-printed source.
  ok(
    findElement(dbg, "prettyPrintButton").disabled,
    "Pretty Print Button should be disabled"
  );

  await selectSource(dbg, "math.min.js");
  await waitForSelectedSource(dbg, "math.min.js");
  ok(
    !findElement(dbg, "mappedSourceLink"),
    "When we are on the minified source,  we don't show the link to the pretty printed source"
  );

  ok(
    !findElement(dbg, "prettyPrintButton").disabled,
    "Pretty Print Button should be enabled"
  );
});

add_task(async function testPrivateFields() {
  // Create a source containing a class with private fields
  const httpServer = createTestHTTPServer();
  httpServer.registerContentType("html", "text/html");
  httpServer.registerContentType("js", "application/javascript");

  httpServer.registerPathHandler(`/`, function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`
      <html>
          Test pretty-printing class with private fields
          <script type="text/javascript" src="class-with-private-fields.js"></script>
      </html>`);
  });

  httpServer.registerPathHandler(
    "/class-with-private-fields.js",
    function (request, response) {
      response.setHeader("Content-Type", "application/javascript");
      response.write(`
      class MyClass {
        constructor(a) {
          this.#a = a;this.#b = Math.random();this.ab = this.#getAB();
        }
        #a
        #b = "default value"
        static #someStaticPrivate
        #getA() {
          return this.#a;
        }
        #getAB() {
          return this.#getA()+this.
            #b
        }
      }
  `);
    }
  );
  const port = httpServer.identity.primaryPort;
  const TEST_URL = `http://localhost:${port}/`;

  info("Open toolbox");
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL);

  info("Select script with private fields");
  await selectSource(dbg, "class-with-private-fields.js", 2);

  info("Pretty print the script");
  clickElement(dbg, "prettyPrintButton");

  info("Wait until the script is pretty-printed");
  await waitForSelectedSource(dbg, "class-with-private-fields.js:formatted");

  info("Check that the script was pretty-printed as expected");
  const prettyPrintedSource = await findSourceContent(
    dbg,
    "class-with-private-fields.js:formatted"
  );

  is(
    prettyPrintedSource.value.trim(),
    `
class MyClass {
  constructor(a) {
    this.#a = a;
    this.#b = Math.random();
    this.ab = this.#getAB();
  }
  #a
  #b = 'default value'
  static #someStaticPrivate
  #getA() {
    return this.#a;
  }
  #getAB() {
    return this.#getA() + this.#b
  }
}
  `.trim(),
    "script was pretty printed as expected"
  );
});
