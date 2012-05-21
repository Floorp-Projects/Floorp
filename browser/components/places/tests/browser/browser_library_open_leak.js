/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 474831
 * https://bugzilla.mozilla.org/show_bug.cgi?id=474831
 *
 * Tests for leaks caused by simply opening and closing the Places Library
 * window.  Opens the Places Library window, waits for it to load, closes it,
 * and finishes.
 */

function test() {
  waitForExplicitFinish();
  openLibrary(function (win) {
    ok(true, "Library has been correctly opened");
    win.close();
    finish();
  });
}
