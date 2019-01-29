/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createEditor } from "../create-editor";
import SourceEditor from "../source-editor";

import { features } from "../../prefs";

describe("createEditor", () => {
  test("Returns a SourceEditor", () => {
    const editor = createEditor();
    expect(editor).toBeInstanceOf(SourceEditor);
    expect(editor.opts).toMatchSnapshot();
    expect(editor.opts.gutters).not.toContain("CodeMirror-foldgutter");
  });

  test("Adds codeFolding", () => {
    features.codeFolding = true;
    const editor = createEditor();
    expect(editor).toBeInstanceOf(SourceEditor);
    expect(editor.opts).toMatchSnapshot();
    expect(editor.opts.gutters).toContain("CodeMirror-foldgutter");
  });
});
