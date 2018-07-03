"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.formatTree = formatTree;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function formatTree(tree, depth = 0, str = "") {
  const whitespace = new Array(depth * 2).join(" ");

  if (tree.type === "directory") {
    str += `${whitespace} - ${tree.name} path=${tree.path} \n`;
    tree.contents.forEach(t => {
      str = formatTree(t, depth + 1, str);
    });
  } else {
    const id = tree.contents.id;
    str += `${whitespace} - ${tree.name} path=${tree.path} source_id=${id} \n`;
  }

  return str;
}