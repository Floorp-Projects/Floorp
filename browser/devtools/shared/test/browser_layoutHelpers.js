/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that scrollIntoViewIfNeeded works properly.

let imported = {};
Components.utils.import("resource:///modules/devtools/LayoutHelpers.jsm",
    imported);
registerCleanupFunction(function () {
  imported = {};
});

let LayoutHelpers = imported.LayoutHelpers;

const TEST_URI = "http://example.com/browser/browser/devtools/shared/test/browser_layoutHelpers.html";

function test() {
  addTab(TEST_URI, function(browser, tab) {
    info("Starting browser_layoutHelpers.js");
    let doc = browser.contentDocument;
    runTest(doc.defaultView, doc.getElementById('some'));
    gBrowser.removeCurrentTab();
    finish();
  });
}

function runTest(win, some) {
  some.style.top = win.innerHeight + 'px';
  some.style.left = win.innerWidth + 'px';
  // The tests start with a black 2x2 pixels square below bottom right.
  // Do not resize the window during the tests.

  win.scroll(win.innerWidth / 2, win.innerHeight + 2);  // Above the viewport.
  LayoutHelpers.scrollIntoViewIfNeeded(some);
  is(win.scrollY, Math.floor(win.innerHeight / 2) + 1,
     'Element completely hidden above should appear centered.');

  win.scroll(win.innerWidth / 2, win.innerHeight + 1);  // On the top edge.
  LayoutHelpers.scrollIntoViewIfNeeded(some);
  is(win.scrollY, win.innerHeight,
     'Element partially visible above should appear above.');

  win.scroll(win.innerWidth / 2, 0);  // Just below the viewport.
  LayoutHelpers.scrollIntoViewIfNeeded(some);
  is(win.scrollY, Math.floor(win.innerHeight / 2) + 1,
     'Element completely hidden below should appear centered.');

  win.scroll(win.innerWidth / 2, 1);  // On the bottom edge.
  LayoutHelpers.scrollIntoViewIfNeeded(some);
  is(win.scrollY, 2,
     'Element partially visible below should appear below.');


  win.scroll(win.innerWidth / 2, win.innerHeight + 2);  // Above the viewport.
  LayoutHelpers.scrollIntoViewIfNeeded(some, false);
  is(win.scrollY, win.innerHeight,
     'Element completely hidden above should appear above ' +
     'if parameter is false.');

  win.scroll(win.innerWidth / 2, win.innerHeight + 1);  // On the top edge.
  LayoutHelpers.scrollIntoViewIfNeeded(some, false);
  is(win.scrollY, win.innerHeight,
     'Element partially visible above should appear above ' +
     'if parameter is false.');

  win.scroll(win.innerWidth / 2, 0);  // Below the viewport.
  LayoutHelpers.scrollIntoViewIfNeeded(some, false);
  is(win.scrollY, 2,
     'Element completely hidden below should appear below ' +
     'if parameter is false.');

  win.scroll(win.innerWidth / 2, 1);  // On the bottom edge.
  LayoutHelpers.scrollIntoViewIfNeeded(some, false);
  is(win.scrollY, 2,
     'Element partially visible below should appear below ' +
     'if parameter is false.');

  // The case of iframes.
  win.scroll(0, 0);

  let frame = win.document.getElementById('frame');
  let fwin = frame.contentWindow;

  frame.style.top = win.innerHeight + 'px';
  frame.style.left = win.innerWidth + 'px';

  fwin.addEventListener('load', function frameLoad() {
    let some = fwin.document.getElementById('some');
    LayoutHelpers.scrollIntoViewIfNeeded(some);
    is(win.scrollX, Math.floor(win.innerWidth / 2) + 20,
       'Scrolling from an iframe should center the iframe vertically.');
    is(win.scrollY, Math.floor(win.innerHeight / 2) + 20,
       'Scrolling from an iframe should center the iframe horizontally.');
    is(fwin.scrollX, Math.floor(fwin.innerWidth / 2) + 1,
       'Scrolling from an iframe should center the element vertically.');
    is(fwin.scrollY, Math.floor(fwin.innerHeight / 2) + 1,
       'Scrolling from an iframe should center the element horizontally.');
  }, false);
}
