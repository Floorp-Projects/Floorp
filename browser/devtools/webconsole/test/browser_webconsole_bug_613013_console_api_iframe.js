/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-bug-613013-console-api-iframe.html";

let test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  BrowserReload();

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foobarBug613013",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });
});
