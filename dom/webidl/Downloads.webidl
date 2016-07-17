/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Represents the state of a download.
// "downloading": The resource is actively transfering.
// "stopped"    : No network tranfer is happening.
// "succeeded"  : The resource has been downloaded successfully.
// "finalized"  : We won't try to download this resource, but the DOM
//                object is still alive.
enum DownloadState {
  "downloading",
  "stopped",
  "succeeded",
  "finalized"
};

[NoInterfaceObject,
 NavigatorProperty="mozDownloadManager",
 JSImplementation="@mozilla.org/downloads/manager;1",
 Pref="dom.mozDownloads.enabled",
 ChromeOnly]
interface DOMDownloadManager : EventTarget {
  // This promise returns an array of downloads with all the current
  // download objects.
  Promise<sequence<DOMDownload>> getDownloads();

  // Removes one download from the downloads set. Returns a promise resolved
  // with the finalized download.
  [UnsafeInPrerendering]
  Promise<DOMDownload> remove(DOMDownload download);

  // Removes all completed downloads.  This kicks off an asynchronous process
  // that will eventually complete, but will not have completed by the time this
  // method returns.  If you care about the side-effects of this method, know
  // that each existing download will have its onstatechange method invoked and
  // will have a new state of "finalized".  (After the download is finalized, no
  // further events will be generated on it.)
  [UnsafeInPrerendering]
  void clearAllDone();

  // Add completed downloads from applications that must perform the download
  // process themselves. For example, email.  The method is resolved with a
  // fully populated DOMDownload instance on success, or rejected in the
  // event all required options were not provided.
  //
  // The adopted download will also be reported via the ondownloadstart event
  // handler.
  //
  // Applications must currently be certified to use this, but it could be
  // widened at a later time.
  //
  // Note that "download" is not actually optional, but WebIDL requires that it
  // be marked as such because it is not followed by a required argument.  The
  // promise will be rejected if the dictionary is omitted or the specified
  // file does not exist on disk.
  Promise<DOMDownload> adoptDownload(optional AdoptDownloadDict download);

  // Fires when a new download starts.
  attribute EventHandler ondownloadstart;
};

[JSImplementation="@mozilla.org/downloads/download;1",
 Pref="dom.mozDownloads.enabled",
 ChromeOnly]
interface DOMDownload : EventTarget {
  // The full size of the resource.
  readonly attribute long long totalBytes;

  // The number of bytes that we have currently downloaded.
  readonly attribute long long currentBytes;

  // The url of the resource.
  readonly attribute DOMString url;

  // The full path in local storage where the file will end up once the download
  // is complete. This is equivalent to the concatenation of the 'storagePath'
  // to the 'mountPoint' of the nsIVolume associated with the 'storageName'
  // (with delimiter).
  readonly attribute DOMString path;

  // The DeviceStorage volume name on which the file is being downloaded.
  readonly attribute DOMString storageName;

  // The DeviceStorage path on the volume with 'storageName' of the file being
  // downloaded.
  readonly attribute DOMString storagePath;

  // The state of the download.  One of: downloading, stopped, succeeded, or
  // finalized.  A finalized download is a download that has been removed /
  // cleared and is no longer tracked by the download manager and will not
  // receive any further onstatechange updates.
  readonly attribute DownloadState state;

  // The mime type for this resource.
  readonly attribute DOMString contentType;

  // The timestamp this download started.
  readonly attribute Date startTime;

  // An opaque identifier for this download. All instances of the same
  // download (eg. in different windows) will have the same id.
  readonly attribute DOMString id;

  // The manifestURL of the application that added this download. Only used for
  // downloads added via the adoptDownload API call.
  readonly attribute DOMString? sourceAppManifestURL;

  // A DOM error object, that will be not null when a download is stopped
  // because something failed.
  readonly attribute DOMError? error;

  // Pauses the download.
  [UnsafeInPrerendering]
  Promise<DOMDownload> pause();

  // Resumes the download. This resolves only once the download has
  // succeeded.
  [UnsafeInPrerendering]
  Promise<DOMDownload> resume();

  // This event is triggered anytime a property of the object changes:
  // - when the transfer progresses, updating currentBytes.
  // - when the state and/or error attributes change.
  attribute EventHandler onstatechange;
};

// Used to initialize the DOMDownload object for adopted downloads.
// fields directly maps to the DOMDownload fields.
dictionary AdoptDownloadDict {
  // The URL of this resource if there is one available. An empty string if
  // the download is not accessible via URL. An empty string is chosen over
  // null so that existinc code does not need to null-check but the value is
  // still falsey.  (Note: If you do have a usable URL, you should probably not
  // be using the adoptDownload API and instead be initiating downloads the
  // normal way.)
  DOMString url;

  // The storageName of the DeviceStorage instance the file was saved to.
  // Required but marked as optional so the bindings don't auto-coerce the value
  // null to "null".
  DOMString? storageName;
  // The path of the file within the DeviceStorage instance named by
  // 'storageName'.  This is used to automatically compute the 'path' of the
  // download.  Note that when DeviceStorage gives you a path to a file, the
  // first path segment is the name of the specific device storage and you do
  // *not* want to include this.  For example, if DeviceStorage tells you the
  // file has a path of '/sdcard1/actual/path/file.ext', then the storageName
  // should be 'sdcard1' and the storagePath should be 'actual/path/file.ext'.
  //
  // The existence of the file will be validated will be validated with stat()
  // and the size the file-system tells us will be what we use.
  //
  // Required but marked as optional so the bindings don't auto-coerce the value
  // null to "null".
  DOMString? storagePath;

  // The mime type for this resource.  Required, but marked as optional because
  // WebIDL otherwise auto-coerces the value null to "null".
  DOMString? contentType;

  // The time the download was started. If omitted, the current time is used.
  Date? startTime;
};
