/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// http rather than chrome to improve coverage
const TESTCASE_URI = TEST_BASE_HTTP + "import.html";

function test()
{
  waitForExplicitFinish();

  addTabAndLaunchStyleEditorChromeWhenLoaded(function (aChrome) {
    run(aChrome);
  });

  content.location = TESTCASE_URI;
}

function run(aChrome)
{
  is(aChrome.editors.length, 3,
    "there are 3 stylesheets after loading @imports");

  is(aChrome.editors[0]._styleSheet.href, TEST_BASE_HTTP + "simple.css",
    "stylesheet 1 is simple.css");

  is(aChrome.editors[1]._styleSheet.href, TEST_BASE_HTTP + "import.css",
    "stylesheet 2 is import.css");

  is(aChrome.editors[2]._styleSheet.href, TEST_BASE_HTTP + "import2.css",
    "stylesheet 3 is import2.css");

  finish();
}
