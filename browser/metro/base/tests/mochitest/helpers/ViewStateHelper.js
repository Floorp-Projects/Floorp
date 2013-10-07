// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const snappedSize = 330;
const portraitSize = 900;

function setSnappedViewstate() {
  ok(isLandscapeMode(), "setSnappedViewstate expects landscape mode to work.");

  let browser = Browser.selectedBrowser;

  // Reduce browser width to simulate small screen size.
  let fullWidth = browser.clientWidth;
  let padding = fullWidth - snappedSize;

  browser.style.borderRight = padding + "px solid gray";

  // Communicate viewstate change
  ContentAreaObserver._updateViewState("snapped");

  // Make sure it renders the new mode properly
  yield waitForMs(0);
}

function setPortraitViewstate() {
  ok(isLandscapeMode(), "setPortraitViewstate expects landscape mode to work.");

  let browser = Browser.selectedBrowser;

  let fullWidth = browser.clientWidth;
  let padding = fullWidth - portraitSize;

  browser.style.borderRight = padding + "px solid gray";

  ContentAreaObserver._updateViewState("portrait");

  // Make sure it renders the new mode properly
  yield waitForMs(0);
}

function restoreViewstate() {
  ContentAreaObserver._updateViewState("landscape");
  ok(isLandscapeMode(), "restoreViewstate should restore landscape mode.");

  Browser.selectedBrowser.style.removeProperty("border-right");

  yield waitForMs(0);
}
