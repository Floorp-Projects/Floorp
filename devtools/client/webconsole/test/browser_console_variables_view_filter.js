/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that variables view filter feature works fine in the console.

function props(view, prefix = "") {
  // First match only the visible one, not hidden by a search
  let visible = [...view].filter(([id, prop]) => prop._isMatch);
  // Then flatten the list into a list of strings
  // being the jsonpath of each attribute being visible in the view
  return visible.reduce((list, [id, prop]) => {
    list.push(prefix + id);
    return list.concat(props(prop, prefix + id + "."));
  }, []);
}

function assertAttrs(view, expected, message) {
  is(props(view).join(","), expected, message);
}

add_task(function* () {
  yield loadTab("data:text/html;charset=utf-8,webconsole-filter");

  let hud = yield openConsole();

  let jsterm = hud.jsterm;

  let fetched = jsterm.once("variablesview-fetched");

  yield jsterm.execute("inspect({ foo: { bar : \"baz\" } })");

  let view = yield fetched;
  let variablesView = view._variablesView;
  let searchbox = variablesView._searchboxNode;

  assertAttrs(view, "foo,__proto__", "To start with, we just see the top level foo attr");

  fetched = jsterm.once("variablesview-fetched");
  searchbox.value = "bar";
  searchbox.doCommand();
  view = yield fetched;

  assertAttrs(view, "", "If we don't manually expand nested attr, we don't see them in search");

  fetched = jsterm.once("variablesview-fetched");
  searchbox.value = "";
  searchbox.doCommand();
  view = yield fetched;

  assertAttrs(view, "foo", "If we reset the search, we get back to original state");

  yield [...view][0][1].expand();

  fetched = jsterm.once("variablesview-fetched");
  searchbox.value = "bar";
  searchbox.doCommand();
  view = yield fetched;

  assertAttrs(view, "foo,foo.bar", "Now if we expand, we see the nested attr");

  fetched = jsterm.once("variablesview-fetched");
  searchbox.value = "baz";
  searchbox.doCommand();
  view = yield fetched;

  assertAttrs(view, "foo,foo.bar", "We can also search for attr values");

  fetched = jsterm.once("variablesview-fetched");
  searchbox.value = "";
  searchbox.doCommand();
  view = yield fetched;

  assertAttrs(view, "foo", "If we reset again, we get back to original state again");
});
