/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("destroy");

loadHelperScript("helper_edits.js");

// Test ProjectEditor image editor functionality
let test = asyncTest(function*() {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  let TEMP_PATH = projecteditor.project.allPaths()[0];

  is (getTempFile("").path, TEMP_PATH, "Temp path is set correctly.");

  ok (projecteditor.currentEditor, "There is an editor for projecteditor");
  let resources = projecteditor.project.allResources();

  let helperImageData = [
    {
      basename: "16x16.png",
      path: "img/icons/16x16.png"
    },
    {
      basename: "32x32.png",
      path: "img/icons/32x32.png"
    },
    {
      basename: "128x128.png",
      path: "img/icons/128x128.png"
    },
  ];

  for (let data of helperImageData) {
    info ("Processing " + data.path);
    let resource = resources.filter(r=>r.basename === data.basename)[0];
    yield selectFile(projecteditor, resource);
    yield testEditor(projecteditor, getTempFile(data.path).path);
  }
});

function testEditor(projecteditor, filePath) {
  info ("Testing file editing for: " + filePath);

  let editor = projecteditor.currentEditor;
  let resource = projecteditor.resourceFor(editor);

  is (resource.path, filePath, "Resource path is set correctly");

  let images = editor.elt.querySelectorAll("image");
  is (images.length, 1, "There is one image inside the editor");
  is (images[0], editor.image, "The image property is set correctly with the DOM");
  is (editor.image.getAttribute("src"), resource.uri, "The image has the resource URL");

  info ("Selecting another resource, then reselecting this one");
  projecteditor.projectTree.selectResource(resource.store.root);
  yield onceEditorActivated(projecteditor);
  projecteditor.projectTree.selectResource(resource);
  yield onceEditorActivated(projecteditor);

  editor = projecteditor.currentEditor;
  images = editor.elt.querySelectorAll("image");
  ok (images.length, 1, "There is one image inside the editor");
  is (images[0], editor.image, "The image property is set correctly with the DOM");
  is (editor.image.getAttribute("src"), resource.uri, "The image has the resource URL");

  info ("Finished checking saving for " + filePath);
}
