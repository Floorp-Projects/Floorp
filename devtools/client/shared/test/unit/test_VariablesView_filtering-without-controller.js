/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that VariablesView._doSearch() works even without an attached
// VariablesViewController (bug 1196341).
const { VariablesView } =
  ChromeUtils.import("resource://devtools/client/shared/widgets/VariablesView.jsm", {});
const { require } =
  ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const { globals } = require("devtools/shared/builtin-modules");

const DOMParser = new globals.DOMParser();

function run_test() {
  let doc = DOMParser.parseFromString("<div>", "text/html");
  let container = doc.body.firstChild;
  ok(container, "Got a container.");

  let vv = new VariablesView(container, { searchEnabled: true });
  let scope = vv.addScope("Test scope");
  let item1 = scope.addItem("a", { value: "1" });
  let item2 = scope.addItem("b", { value: "2" });

  info("Performing a search without a controller.");
  vv._doSearch("a");

  equal(item1.target.hasAttribute("unmatched"), false,
    "First item that matched the filter is visible.");
  equal(item2.target.hasAttribute("unmatched"), true,
    "The second item that did not match the filter is hidden.");
}
