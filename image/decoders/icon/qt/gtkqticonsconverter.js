/* vim:set ts=2 sw=2 sts=2 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");


function GtkQtIconsConverter() { };
GtkQtIconsConverter.prototype = {
  classID:          Components.ID("{c0783c34-a831-40c6-8c03-98c9f74cca45}"),
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsIGtkQtIconsConverter]),
  convert: function(icon) { return this._gtk_qt_icons_table[icon]; },
  _gtk_qt_icons_table: {
    'about':
    0,
    'add':
    0,
    'apply':
    44, /* QStyle::SP_DialogApplyButton */
    'cancel':
    39, /* QStyle::SP_DialogCancelButton */
    'clear':
    45, /* QStyle::SP_DialogResetButton  */
    'color-picker':
    0,
    'copy':
    0,
    'close':
    43, /* QStyle::SP_DialogCloseButton */
    'cut':
    0,
    'delete':
    0,
    'dialog-error':
    0,
    'dialog-info':
    0,
    'dialog-question':
    12, /* QStyle::SP_MessageBoxQuestion */
    'dialog-warning':
    10, /* QStyle::SP_MessageBoxWarning */
    'directory':
    37, /* QStyle::SP_DirIcon */
    'file':
    24, /* QStyle::SP_FileIcon */
    'find':
    0,
    'go-back-ltr':
    53, /* QStyle::SP_ArrowBack */
    'go-back-rtl':
    53, /* QStyle::SP_ArrowBack */
    'go-back':
    53, /* QStyle::SP_ArrowBack */
    'go-forward-ltr':
    54, /* QStyle::SP_ArrowForward */
    'go-forward-rtl':
    54, /* QStyle::SP_ArrowForward */
    'go-forward':
    54, /* QStyle::SP_ArrowForward */
    'go-up':
    49, /* QStyle::SP_ArrowUp */
    'goto-first':
    0,
    'goto-last':
    0,
    'help':
    7, /* QStyle::SP_TitleBarContextHelpButton */
    'home':
    55, /* QStyle::SP_DirHomeIcon */
    'info':
    9, /* QStyle::SP_MessageBoxInformation */
    'jump-to':
    0,
    'media-pause':
    0,
    'media-play':
    0,
    'network':
    20, /* QStyle::SP_DriveNetIcon */
    'no':
    48, /* QStyle::SP_DialogNoButton */
    'ok':
    38, /* QStyle::SP_DialogOkButton */
    'open':
    21, /* QStyle::SP_DirOpenIcon */
    'orientation-landscape':
    0,
    'orientation-portrait':
    0,
    'paste':
    0,
    'preferences':
    34, /* QStyle::SP_FileDialogContentsView */
    'print-preview':
    0,
    'print':
    0,
    'properties':
    0,
    'quit':
    0,
    'redo':
    0,
    'refresh':
    58, /* QStyle::SP_BrowserReload */
    'remove':
    0,
    'revert-to-saved':
    0,
    'save-as':
    42, /* QStyle::SP_DialogSaveButton */
    'save':
    42, /* QStyle::SP_DialogSaveButton */
    'select-all':
    0,
    'select-font':
    0,
    'stop':
    59, /* QStyle::SP_BrowserStop */
    'undelete':
    0,
    'undo':
    0,
    'yes':
    47, /* QStyle::SP_DialogYesButton */
    'zoom-100':
    0,
    'zoom-in':
    0,
    'zoom-out':
    0
  },
}
var components = [GtkQtIconsConverter];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);

