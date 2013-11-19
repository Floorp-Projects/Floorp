#!/usr/bin/env node
/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var copy = require('dryice').copy;

const ORION_EDITOR = "org.eclipse.orion.client.editor/web";

var js_src = copy.createDataObject();

copy({
  source: [
    ORION_EDITOR + "/orion/textview/global.js",
    ORION_EDITOR + "/orion/textview/eventTarget.js",
    ORION_EDITOR + "/orion/editor/regex.js",
    ORION_EDITOR + "/orion/textview/keyBinding.js",
    ORION_EDITOR + "/orion/textview/annotations.js",
    ORION_EDITOR + "/orion/textview/rulers.js",
    ORION_EDITOR + "/orion/textview/undoStack.js",
    ORION_EDITOR + "/orion/textview/textModel.js",
    ORION_EDITOR + "/orion/textview/projectionTextModel.js",
    ORION_EDITOR + "/orion/textview/tooltip.js",
    ORION_EDITOR + "/orion/textview/textView.js",
    ORION_EDITOR + "/orion/textview/textDND.js",
    ORION_EDITOR + "/orion/editor/htmlGrammar.js",
    ORION_EDITOR + "/orion/editor/textMateStyler.js",
    ORION_EDITOR + "/examples/textview/textStyler.js",
  ],
  dest: js_src,
});

copy({
    source: js_src,
    dest: "orion.js",
});

var css_src = copy.createDataObject();

copy({
  source: [
    ORION_EDITOR + "/orion/textview/textview.css",
    ORION_EDITOR + "/orion/textview/rulers.css",
    ORION_EDITOR + "/orion/textview/annotations.css",
    ORION_EDITOR + "/examples/textview/textstyler.css",
    ORION_EDITOR + "/examples/editor/htmlStyles.css",
  ],
  dest: css_src,
});

copy({
    source: css_src,
    dest: "orion.css",
});

