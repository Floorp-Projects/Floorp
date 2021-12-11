/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../content/contentSearchUI.js */
var input = document.querySelector("input");
var gController = new ContentSearchUIController(
  input,
  input.parentNode,
  "test",
  "test"
);
