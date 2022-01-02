/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// /////////////////////////////////////////////////////////////////////////
//
// Utilities for navigation tests
//
// /////////////////////////////////////////////////////////////////////////

var body = "This frame was navigated.";
var target_url = "navigation_target_url.html";

var popup_body = "This is a popup";
var target_popup_url = "navigation_target_popup_url.html";

// /////////////////////////////////////////////////////////////////////////
// Functions that navigate frames
// /////////////////////////////////////////////////////////////////////////

function navigateByLocation(wnd) {
  try {
    wnd.location = target_url;
  } catch (ex) {
    // We need to keep our finished frames count consistent.
    // Oddly, this ends up simulating the behavior of IE7.
    window.open(target_url, "_blank", "width=10,height=10");
  }
}

function navigateByOpen(name) {
  window.open(target_url, name, "width=10,height=10");
}

function navigateByForm(name) {
  var form = document.createElement("form");
  form.action = target_url;
  form.method = "POST";
  form.target = name;
  document.body.appendChild(form);
  form.submit();
}

var hyperlink_count = 0;

function navigateByHyperlink(name) {
  var link = document.createElement("a");
  link.href = target_url;
  link.target = name;
  link.id = "navigation_hyperlink_" + hyperlink_count++;
  document.body.appendChild(link);
  sendMouseEvent({ type: "click" }, link.id);
}

// /////////////////////////////////////////////////////////////////////////
// Functions that call into Mochitest framework
// /////////////////////////////////////////////////////////////////////////

async function isNavigated(wnd, message) {
  var result = null;
  try {
    result = await SpecialPowers.spawn(wnd, [], () =>
      this.content.document.body.innerHTML.trim()
    );
  } catch (ex) {
    result = ex;
  }
  is(result, body, message);
}

function isBlank(wnd, message) {
  var result = null;
  try {
    result = wnd.document.body.innerHTML.trim();
  } catch (ex) {
    result = ex;
  }
  is(result, "This is a blank document.", message);
}

function isAccessible(wnd, message) {
  try {
    wnd.document.body.innerHTML;
    ok(true, message);
  } catch (ex) {
    ok(false, message);
  }
}

function isInaccessible(wnd, message) {
  try {
    wnd.document.body.innerHTML;
    ok(false, message);
  } catch (ex) {
    ok(true, message);
  }
}

function delay(msec) {
  return new Promise(resolve => setTimeout(resolve, msec));
}

// /////////////////////////////////////////////////////////////////////////
// Functions that uses SpecialPowers.spawn
// /////////////////////////////////////////////////////////////////////////

async function waitForFinishedFrames(numFrames) {
  SimpleTest.requestFlakyTimeout("Polling");

  var finishedWindows = new Set();

  async function searchForFinishedFrames(win) {
    try {
      let { href, bodyText, readyState } = await SpecialPowers.spawn(
        win,
        [],
        () => {
          return {
            href: this.content.location.href,
            bodyText:
              this.content.document.body &&
              this.content.document.body.textContent.trim(),
            readyState: this.content.document.readyState,
          };
        }
      );

      if (
        (href.endsWith(target_url) || href.endsWith(target_popup_url)) &&
        (bodyText == body || bodyText == popup_body) &&
        readyState == "complete"
      ) {
        finishedWindows.add(SpecialPowers.getBrowsingContextID(win));
      }
    } catch (e) {
      // This may throw if a frame is not fully initialized, in which
      // case we'll handle it in a later iteration.
    }

    for (let i = 0; i < win.frames.length; i++) {
      await searchForFinishedFrames(win.frames[i]);
    }
  }

  while (finishedWindows.size < numFrames) {
    await delay(500);

    for (let win of SpecialPowers.getGroupTopLevelWindows(window)) {
      win = SpecialPowers.unwrap(win);
      await searchForFinishedFrames(win);
    }
  }

  if (finishedWindows.size > numFrames) {
    throw new Error("Too many frames loaded.");
  }
}

async function getFramesByName(name) {
  let results = [];
  for (let win of SpecialPowers.getGroupTopLevelWindows(window)) {
    win = SpecialPowers.unwrap(win);
    if (
      (await SpecialPowers.spawn(win, [], () => this.content.name)) === name
    ) {
      results.push(win);
    }
  }

  return results;
}

async function cleanupWindows() {
  for (let win of SpecialPowers.getGroupTopLevelWindows(window)) {
    win = SpecialPowers.unwrap(win);
    if (win.closed) {
      continue;
    }

    let href = "";
    try {
      href = await SpecialPowers.spawn(
        win,
        [],
        () =>
          this.content && this.content.location && this.content.location.href
      );
    } catch (error) {
      // SpecialPowers.spawn(win, ...) throws if win is closed. We did
      // our best to not call it on a closed window, but races happen.
      if (!win.closed) {
        throw error;
      }
    }

    if (
      href &&
      (href.endsWith(target_url) || href.endsWith(target_popup_url))
    ) {
      win.close();
    }
  }
}
