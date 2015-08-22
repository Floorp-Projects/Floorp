/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that VariablesView._doSearch() works even without an attached
// VariablesViewController (bug 1196341).

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const DOMParser = Cc["@mozilla.org/xmlextras/domparser;1"]
                    .createInstance(Ci.nsIDOMParser);
const { VariablesView } =
  Cu.import("resource:///modules/devtools/VariablesView.jsm", {});

function run_test() {
  let doc = DOMParser.parseFromString("<div>", "text/html");
  let container = doc.body.firstChild;
  ok(container, "Got a container.");

  let vv = new VariablesView(container, { searchEnabled: true });
  let scope = vv.addScope("Test scope");
  let item1 = scope.addItem("a", { value: "1" });
  let item2 = scope.addItem("b", { value: "2" });

  do_print("Performing a search without a controller.");
  vv._doSearch("a");

  equal(item1.target.hasAttribute("unmatched"), false,
    "First item that matched the filter is visible.");
  equal(item2.target.hasAttribute("unmatched"), true,
    "The second item that did not match the filter is hidden.");
}
