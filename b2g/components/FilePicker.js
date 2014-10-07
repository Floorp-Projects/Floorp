/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * No magic constructor behaviour, as is de rigeur for XPCOM.
 * If you must perform some initialization, and it could possibly fail (even
 * due to an out-of-memory condition), you should use an Init method, which
 * can convey failure appropriately (thrown exception in JS,
 * NS_FAILED(nsresult) return in C++).
 *
 * In JS, you can actually cheat, because a thrown exception will cause the
 * CreateInstance call to fail in turn, but not all languages are so lucky.
 * (Though ANSI C++ provides exceptions, they are verboten in Mozilla code
 * for portability reasons -- and even when you're building completely
 * platform-specific code, you can't throw across an XPCOM method boundary.)
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

// FIXME: improve this list of filters.
const IMAGE_FILTERS = ['image/gif', 'image/jpeg', 'image/pjpeg',
                       'image/png', 'image/svg+xml', 'image/tiff',
                       'image/vnd.microsoft.icon'];
const VIDEO_FILTERS = ['video/mpeg', 'video/mp4', 'video/ogg',
                       'video/quicktime', 'video/webm', 'video/x-matroska',
                       'video/x-ms-wmv', 'video/x-flv'];
const AUDIO_FILTERS = ['audio/basic', 'audio/L24', 'audio/mp4',
                       'audio/mpeg', 'audio/ogg', 'audio/vorbis',
                       'audio/vnd.rn-realaudio', 'audio/vnd.wave',
                       'audio/webm'];

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyServiceGetter(this, 'cpmm',
                                   '@mozilla.org/childprocessmessagemanager;1',
                                   'nsIMessageSender');

function FilePicker() {
}

FilePicker.prototype = {
  classID: Components.ID('{436ff8f9-0acc-4b11-8ec7-e293efba3141}'),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFilePicker]),

  /* members */

  mParent: undefined,
  mExtraProps: undefined,
  mFilterTypes: undefined,
  mFileEnumerator: undefined,
  mFilePickerShownCallback: undefined,

  /* methods */

  init: function(parent, title, mode) {
    this.mParent = parent;
    this.mExtraProps = {};
    this.mFilterTypes = [];
    this.mMode = mode;

    if (mode != Ci.nsIFilePicker.modeOpen &&
        mode != Ci.nsIFilePicker.modeOpenMultiple) {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  /* readonly attribute nsILocalFile file - not implemented; */
  /* readonly attribute nsISimpleEnumerator files - not implemented; */
  /* readonly attribute nsIURI fileURL - not implemented; */

  get domfiles() {
    return this.mFilesEnumerator;
  },

  get domfile() {
    return this.mFilesEnumerator ? this.mFilesEnumerator.mFiles[0] : null;
  },

  get mode() {
    return this.mMode;
  },

  appendFilters: function(filterMask) {
    // Ci.nsIFilePicker.filterHTML is not supported
    // Ci.nsIFilePicker.filterText is not supported

    if (filterMask & Ci.nsIFilePicker.filterImages) {
      this.mFilterTypes = this.mFilterTypes.concat(IMAGE_FILTERS);
      // This property is needed for the gallery app pick activity.
      this.mExtraProps['nocrop'] = true;
    }

    // Ci.nsIFilePicker.filterXML is not supported
    // Ci.nsIFilePicker.filterXUL is not supported
    // Ci.nsIFilePicker.filterApps is not supported
    // Ci.nsIFilePicker.filterAllowURLs is not supported

    if (filterMask & Ci.nsIFilePicker.filterVideo) {
      this.mFilterTypes = this.mFilterTypes.concat(VIDEO_FILTERS);
    }

    if (filterMask & Ci.nsIFilePicker.filterAudio) {
      this.mFilterTypes = this.mFilterTypes.concat(AUDIO_FILTERS);
    }

    if (filterMask & Ci.nsIFilePicker.filterAll) {
      // This property is needed for the gallery app pick activity.
      this.mExtraProps['nocrop'] = true;
    }
  },

  appendFilter: function(title, extensions) {
    // pick activity doesn't support extensions
  },

  open: function(aFilePickerShownCallback) {
    this.mFilePickerShownCallback = aFilePickerShownCallback;

    cpmm.addMessageListener('file-picked', this);

    let detail = {};
    if (this.mFilterTypes) {
       detail.type = this.mFilterTypes;
    }

    for (let prop in this.mExtraProps) {
      if (!(prop in detail)) {
        detail[prop] = this.mExtraProps[prop];
      }
    }

    cpmm.sendAsyncMessage('file-picker', detail);
  },

  fireSuccess: function(file) {
    this.mFilesEnumerator = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsISimpleEnumerator]),

      mFiles: [file],
      mIndex: 0,

      hasMoreElements: function() {
        return (this.mIndex < this.mFiles.length);
      },

      getNext: function() {
        if (this.mIndex >= this.mFiles.length) {
          throw Components.results.NS_ERROR_FAILURE;
        }
        return this.mFiles[this.mIndex++];
      }
    };

    if (this.mFilePickerShownCallback) {
      this.mFilePickerShownCallback.done(Ci.nsIFilePicker.returnOK);
      this.mFilePickerShownCallback = null;
    }
  },

  fireError: function() {
    if (this.mFilePickerShownCallback) {
      this.mFilePickerShownCallback.done(Ci.nsIFilePicker.returnCancel);
      this.mFilePickerShownCallback = null;
    }
  },

  receiveMessage: function(message) {
    if (message.name !== 'file-picked') {
      return;
    }

    cpmm.removeMessageListener('file-picked', this);

    let data = message.data;
    if (!data.success || !data.result.blob) {
      this.fireError();
      return;
    }

    // The name to be shown can be part of the message, or can be taken from
    // the DOMFile (if the blob is a DOMFile).
    let name = data.result.name;
    if (!name &&
        (data.result.blob instanceof this.mParent.File) &&
        data.result.blob.name) {
      name = data.result.blob.name;
    }

    // Let's try to remove the full path and take just the filename.
    if (name) {
      let names = OS.Path.split(name);
      name = names.components[names.components.length - 1];
    }

    // the fallback is a filename composed by 'blob' + extension.
    if (!name) {
      name = 'blob';
      if (data.result.blob.type) {
        let mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
        let mimeInfo = mimeSvc.getFromTypeAndExtension(data.result.blob.type, '');
        if (mimeInfo) {
          name += '.' + mimeInfo.primaryExtension;
        }
      }
    }

    let file = new this.mParent.File([data.result.blob],
                                     name,
                                     { type: data.result.blob.type });

    if (file) {
      this.fireSuccess(file);
    } else {
      this.fireError();
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FilePicker]);
