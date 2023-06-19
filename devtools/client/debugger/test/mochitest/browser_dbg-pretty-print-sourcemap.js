/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This tests pretty-printing for sourcemapped files.

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerContentType("html", "text/html");
httpServer.registerContentType("js", "application/javascript");

httpServer.registerPathHandler(
  "/doc-prettyprint-sourcemap.html",
  (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`<!DOCTYPE html>
    <html>
      <head>
        <script type="text/javascript" src="/js1.min.js"></script>
        <script type="text/javascript" src="/js2.min.js"></script>
      </head>
      <script>
        const a = 3;
        console.log(a);
      </script>
    </html>
  `);
  }
);

function sourceHandler(request, response) {
  response.setHeader("Content-Type", "text/javascript");
  console.log(request.path.substring(1));
  response.write(`function add(a,b,k){var result=a+b;return k(result)}function sub(a,b,k){var result=a-b;return k(result)}function mul(a,b,k){var result=a*b;return k(result)}function div(a,b,k){var result=a/b;return k(result)}function arithmetic(){add(4,4,function(a){sub(a,2,function(b){mul(b,3,function(c){div(c,2,function(d){console.log("arithmetic",d)})})})})};
//# sourceMappingURL=${request.path.substring(1)}.map`);
}

httpServer.registerPathHandler("/js1.min.js", sourceHandler);
httpServer.registerPathHandler("/js2.min.js", sourceHandler);

// This returns no sourcemap
httpServer.registerPathHandler("/js1.min.js.map", (request, response) => {
  response.setHeader("Content-Type", "application/javascript");
  response.setStatusLine(request.httpVersion, 404, "Not found");
  response.write(`console.log("http error")`);
});

// This returns a sourcemap without any original source
httpServer.registerPathHandler("/js2.min.js.map", (request, response) => {
  response.setHeader("Content-Type", "application/javascript");
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(
    `{"version":3,"sources":[],"names":["message","document","getElementById","logMessage","console","log","value","addEventListener","logMessageButton"],"mappings":"AAAA,GAAIA,SAAUC,SAASC,eAAe,UAEtC,SAASC,cACPC,QAAQC,IAAI,eAAiBL,QAAQM,OAGvCN,QAAQO,iBAAiB,QAAS,WAChCP,QAAQM,MAAQ,IACf,MAEH,IAAIE,kBAAmBP,SAASC,eAAe,cAE/CM,kBAAiBD,iBAAiB,QAAS,WACzCJ,cACC"}`
  );
});

const BASE_URL = `http://localhost:${httpServer.identity.primaryPort}/`;

// Tests that the pretty-print icon is visible for source files with a sourceMappingURL
// but the linked sourcemap is not found or no original files exist.
add_task(async () => {
  const dbg = await initDebuggerWithAbsoluteURL(
    `${BASE_URL}doc-prettyprint-sourcemap.html`,
    "doc-prettyprint-sourcemap.html",
    "js1.min.js",
    "js2.min.js"
  );

  info(" - Test HTML source");
  const htmlSource = findSource(dbg, "doc-prettyprint-sourcemap.html");
  await selectSource(dbg, htmlSource);
  assertPrettyPrintButton(
    dbg,
    DEBUGGER_L10N.getStr("sourceTabs.prettyPrint"),
    false
  );

  info(" - Test source with sourceMappingURL but sourcemap does not exist");
  const source1 = findSource(dbg, "js1.min.js");
  await selectSource(dbg, source1);

  // The source may be reported as pretty printable while we are fetching the sourcemap,
  // but once the sourcemap is reported to be failing, we no longer report to be pretty printable.
  await waitFor(
    () => !dbg.selectors.isSourceWithMap(source1.id),
    "Wait for the selector to report the source to be source-map less"
  );

  assertPrettyPrintButton(
    dbg,
    DEBUGGER_L10N.getStr("sourceTabs.prettyPrint"),
    false
  );

  info(
    " - Test source with sourceMappingURL and sourcemap but no original files"
  );
  const source2 = findSource(dbg, "js2.min.js");
  await selectSource(dbg, source2);

  assertPrettyPrintButton(
    dbg,
    DEBUGGER_L10N.getStr("sourceTabs.prettyPrint"),
    false
  );

  info(" - Test an already pretty-printed source");
  info("Pretty print the script");
  clickElement(dbg, "prettyPrintButton");

  await waitForSelectedSource(dbg, "js2.min.js:formatted");
  assertPrettyPrintButton(
    dbg,
    DEBUGGER_L10N.getStr("sourceFooter.prettyPrint.isPrettyPrintedMessage"),
    true
  );
});

add_task(async () => {
  const dbg = await initDebugger(
    "doc-sourcemaps2.html",
    "main.min.js",
    "main.js"
  );

  info(
    " - Test source with sourceMappingURL, sourcemap and original files exist"
  );
  const source = findSource(dbg, "main.min.js");
  await selectSource(dbg, source);

  assertPrettyPrintButton(
    dbg,
    DEBUGGER_L10N.getStr("sourceFooter.prettyPrint.hasSourceMapMessage"),
    true
  );

  info(" - Test an original source");
  const originalSource = findSource(dbg, "main.js");
  await selectSource(dbg, originalSource);

  assertPrettyPrintButton(
    dbg,
    DEBUGGER_L10N.getStr("sourceFooter.prettyPrint.isOriginalMessage"),
    true
  );
});

function assertPrettyPrintButton(dbg, expectedTitle, shouldBeDisabled) {
  const prettyPrintButton = findElement(dbg, "prettyPrintButton");
  ok(prettyPrintButton, "Pretty Print Button is visible");
  is(
    prettyPrintButton.title,
    expectedTitle,
    "The pretty print button title should be correct"
  );
  ok(
    shouldBeDisabled ? prettyPrintButton.disabled : !prettyPrintButton.disabled,
    `The pretty print button should be ${
      shouldBeDisabled ? "disabled" : "enabled"
    }`
  );
}
