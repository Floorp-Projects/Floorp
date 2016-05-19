/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var helperEditData = [
  {
    basename: "styles.css",
    path: "css/styles.css",
    newContent: "body,html { color: orange; }"
  },
  {
    basename: "index.html",
    path: "index.html",
    newContent: "<h1>Changed Content Again</h1>"
  },
  {
    basename: "LICENSE",
    path: "LICENSE",
    newContent: "My new license"
  },
  {
    basename: "README.md",
    path: "README.md",
    newContent: "My awesome readme"
  },
  {
    basename: "script.js",
    path: "js/script.js",
    newContent: "alert('hi')"
  },
  {
    basename: "vector.svg",
    path: "img/icons/vector.svg",
    newContent: "<svg></svg>"
  },
];

function* selectFile(projecteditor, resource) {
  ok(resource && resource.path, "A valid resource has been passed in for selection " + (resource && resource.path));
  projecteditor.projectTree.selectResource(resource);

  if (resource.isDir) {
    return;
  }

  let [editorActivated] = yield promise.all([
    onceEditorActivated(projecteditor)
  ]);

  is(editorActivated, projecteditor.currentEditor, "Editor has been activated for " + resource.path);
}
