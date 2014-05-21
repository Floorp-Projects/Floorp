/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains helper functions for showing OS-specific
 * file and folder pickers.
 */

const { Cu, Cc, Ci } = require("chrome");
const { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const promise = require("projecteditor/helpers/promise");
const { merge } = require("sdk/util/object");
const { getLocalizedString } = require("projecteditor/helpers/l10n");

/**
 * Show a file / folder picker.
 * https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIFilePicker
 *
 * @param object options
 *        Additional options for setting the source. Supported options:
 *          - directory: string, The path to default opening
 *          - defaultName: string, The filename including extension that
 *                         should be suggested to the user as a default
 *          - window: DOMWindow, The filename including extension that
 *                         should be suggested to the user as a default
 *          - title: string, The filename including extension that
 *                         should be suggested to the user as a default
 *          - mode: int, The type of picker to open.
 *
 * @return promise
 *         A promise that is resolved with the full path
 *         after the file has been picked.
 */
function showPicker(options) {
  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  if (options.directory) {
    try {
      fp.displayDirectory = FileUtils.File(options.directory);
    } catch(ex) {
      console.warn(ex);
    }
  }

  if (options.defaultName) {
    fp.defaultString = options.defaultName;
  }

  fp.init(options.window, options.title, options.mode);
  let deferred = promise.defer();
  fp.open({
    done: function(res) {
      if (res === Ci.nsIFilePicker.returnOK || res === Ci.nsIFilePicker.returnReplace) {
        deferred.resolve(fp.file.path);
      } else {
        deferred.reject();
      }
    }
  });
  return deferred.promise;
}
exports.showPicker = showPicker;

/**
 * Show a save dialog
 * https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIFilePicker
 *
 * @param object options
 *        Additional options as specified in showPicker
 *
 * @return promise
 *         A promise that is resolved when the save dialog has closed
 */
function showSave(options) {
  return showPicker(merge({
    title: getLocalizedString("projecteditor.selectFileLabel"),
    mode: Ci.nsIFilePicker.modeSave
  }, options));
}
exports.showSave = showSave;

/**
 * Show a file open dialog
 *
 * @param object options
 *        Additional options as specified in showPicker
 *
 * @return promise
 *         A promise that is resolved when the file has been opened
 */
function showOpen(options) {
  return showPicker(merge({
    title: getLocalizedString("projecteditor.openFileLabel"),
    mode: Ci.nsIFilePicker.modeOpen
  }, options));
}
exports.showOpen = showOpen;

/**
 * Show a folder open dialog
 *
 * @param object options
 *        Additional options as specified in showPicker
 *
 * @return promise
 *         A promise that is resolved when the folder has been opened
 */
function showOpenFolder(options) {
  return showPicker(merge({
    title: getLocalizedString("projecteditor.openFolderLabel"),
    mode: Ci.nsIFilePicker.modeGetFolder
  }, options));
}
exports.showOpenFolder = showOpenFolder;
