/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @flow

/* Checks to see if the root element is available and
 * if the element is visible. We check the width of the element
 * because it is more reliable than either checking a focus state or
 * the visibleState or hidden property.
 */
export function isVisible(): boolean {
  const el = document.querySelector("#mount");
  return !!(el && el.getBoundingClientRect().width > 0);
}

/* Gets the line numbers width in the code editor
 */
export function getLineNumberWidth(editor: Object): number {
  const { gutters } = editor.display;
  const lineNumbers = gutters.querySelector(".CodeMirror-linenumbers");
  return lineNumbers?.clientWidth;
}

/**
 * Forces the breakpoint gutter to be the same size as the line
 * numbers gutter. Editor CSS will absolutely position the gutter
 * beneath the line numbers. This makes it easy to be flexible with
 * how we overlay breakpoints.
 */
export function resizeBreakpointGutter(editor: Object): void {
  const { gutters } = editor.display;
  const breakpoints = gutters.querySelector(".breakpoints");
  if (breakpoints) {
    breakpoints.style.width = `${getLineNumberWidth(editor)}px`;
  }
}

/**
 * Forces the left toggle button in source header to be the same size
 * as the line numbers gutter.
 */
export function resizeToggleButton(editor: Object): void {
  const toggleButton = document.querySelector(
    ".source-header .toggle-button-start"
  );
  if (toggleButton) {
    toggleButton.style.width = `${getLineNumberWidth(editor)}px`;
  }
}
