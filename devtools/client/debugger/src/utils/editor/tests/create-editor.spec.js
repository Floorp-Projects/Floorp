/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createEditor } from "../create-editor";

import { features } from "../../prefs";

describe("createEditor", () => {
  test("SourceEditor default config", () => {
    const editor = createEditor();
    expect(editor.config).toMatchSnapshot();
    expect(editor.config.gutters).not.toContain("CodeMirror-foldgutter");
  });

  test("Adds codeFolding", () => {
    features.codeFolding = true;
    const editor = createEditor();
    expect(editor.config).toMatchSnapshot();
    expect(editor.config.gutters).toContain("CodeMirror-foldgutter");
  });
});
