/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { windows, isInteractive, isDocumentLoaded,
        getOuterId, isTopLevel } = require("../window/utils");
const { InputPort } = require("./system");
const { lift, merges, foldp, keepIf, start, Input } = require("../event/utils");
const { patch } = require("diffpatcher/index");
const { on } = require("../event/core");
const { Sequence,  seq, filter, object, pairs } = require("../util/sequence");


// Create lazy iterators from the regular arrays, although
// once https://github.com/mozilla/addon-sdk/pull/1314 lands
// `windows` will be transforme to lazy iterators.
// When iterated over belowe sequences items will represent
// state of windows at the time of iteration.
const opened = seq(function*() {
  const items = windows(null, {includePrivates: true});
  for (let item of items)
    yield [getOuterId(item), item];
});
const interactive = filter(([_, window]) => isInteractive(window), opened);
const loaded = filter(([_, window]) => isDocumentLoaded(window), opened);

// Helper function that converts given argument to a delta.
const Update = window => window && object([getOuterId(window), window]);
const Delete = window => window && object([getOuterId(window), null]);

// Signal represents delta for last opened top level window.
const LastOpened = lift(Update, new InputPort({topic: "domwindowopened"}));
exports.LastOpened = LastOpened;

// Signal represents delta for last top level window close.
const LastClosed = lift(Delete, new InputPort({topic: "domwindowclosed"}));
exports.LastClosed = LastClosed;

const windowFor = document => document && document.defaultView;

// Signal represent delta for last top level window document becoming interactive.
const InteractiveDoc = new InputPort({topic: "chrome-document-interactive"});
const InteractiveWin = lift(windowFor, InteractiveDoc);
const LastInteractive = lift(Update, keepIf(isTopLevel, null, InteractiveWin));
exports.LastInteractive = LastInteractive;

// Signal represent delta for last top level window loaded.
const LoadedDoc = new InputPort({topic: "chrome-document-loaded"});
const LoadedWin = lift(windowFor, LoadedDoc);
const LastLoaded = lift(Update, keepIf(isTopLevel, null, LoadedWin));
exports.LastLoaded = LastLoaded;


const initialize = input => {
  if (!input.initialized) {
    input.value = object(...input.value);
    Input.start(input);
    input.initialized = true;
  }
};

// Signal represents set of currently opened top level windows, updated
// to new set any time window is opened or closed.
const Opened = foldp(patch, opened, merges([LastOpened, LastClosed]));
Opened[start] = initialize;
exports.Opened = Opened;

// Signal represents set of top level interactive windows, updated any
// time new window becomes interactive or one get's closed.
const Interactive = foldp(patch, interactive, merges([LastInteractive,
                                                      LastClosed]));
Interactive[start] = initialize;
exports.Interactive = Interactive;

// Signal represents set of top level loaded window, updated any time
// new window becomes interactive or one get's closed.
const Loaded = foldp(patch, loaded, merges([LastLoaded, LastClosed]));
Loaded[start] = initialize;
exports.Loaded = Loaded;
