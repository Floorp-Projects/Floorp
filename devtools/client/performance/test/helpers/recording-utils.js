/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * These utilities provide a functional interface for accessing the particulars
 * about the recording's details.
 */

/**
 * Access the selected view from the panel's recording list.
 *
 * @param {object} panel - The current panel.
 * @return {object} The recording model.
 */
exports.getSelectedRecording = function (panel) {
  const view = panel.panelWin.RecordingsView;
  return view.selected;
};

/**
 * Set the selected index of the recording via the panel.
 *
 * @param {object} panel - The current panel.
 * @return {number} index
 */
exports.setSelectedRecording = function (panel, index) {
  const view = panel.panelWin.RecordingsView;
  view.setSelectedByIndex(index);
  return index;
};

/**
 * Access the selected view from the panel's recording list.
 *
 * @param {object} panel - The current panel.
 * @return {number} index
 */
exports.getSelectedRecordingIndex = function (panel) {
  const view = panel.panelWin.RecordingsView;
  return view.getSelectedIndex();
};

exports.getDurationLabelText = function (panel, elementIndex) {
  const { $$ } = panel.panelWin;
  const elements = $$(".recording-list-item-duration", panel.panelWin.document);
  return elements[elementIndex].innerHTML;
};

exports.getRecordingsCount = function (panel) {
  const { $$ } = panel.panelWin;
  return $$(".recording-list-item", panel.panelWin.document).length;
};
