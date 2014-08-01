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

//
// XXXTODO: When we have a generic way to do feature detection in marketplace
//          we will *STOP* using the pref and use CheckPermissions like 
//          DOMDownload and DownloadEvent.
//
[NoInterfaceObject,
 NavigatorProperty="mozDownloadManager",
 JSImplementation="@mozilla.org/downloads/manager;1",
 Pref="dom.mozDownloads.enabled"]
interface DOMDownloadManager : EventTarget {
  // This promise returns an array of downloads with all the current
  // download objects.
  Promise<sequence<DOMDownload>> getDownloads();

  // Removes one download from the downloads set. Returns a promise resolved
  // with the finalized download.
  Promise<DOMDownload> remove(DOMDownload download);

  // Removes all the completed downloads from the set.  Returns an
  // array of the completed downloads that were removed.
  Promise<sequence<DOMDownload>> clearAllDone();

  // Fires when a new download starts.
  attribute EventHandler ondownloadstart;
};

[JSImplementation="@mozilla.org/downloads/download;1",
 Pref="dom.mozDownloads.enabled",
 CheckPermissions="downloads"]
interface DOMDownload : EventTarget {
  // The full size of the resource.
  readonly attribute long long totalBytes;

  // The number of bytes that we have currently downloaded.
  readonly attribute long long currentBytes;

  // The url of the resource.
  readonly attribute DOMString url;

  // The full path in local storage where the file will end up once the download
  // is complete.
  readonly attribute DOMString path;

  // The DeviceStorage volume name on which the file is being downloaded.
  readonly attribute DOMString storageName;

  // The DeviceStorage path on the volume with 'storageName' of the file being
  // downloaded.
  readonly attribute DOMString storagePath;

  // The state of the download.
  readonly attribute DownloadState state;

  // The mime type for this resource.
  readonly attribute DOMString contentType;

  // The timestamp this download started.
  readonly attribute Date startTime;

  // An opaque identifier for this download. All instances of the same
  // download (eg. in different windows) will have the same id.
  readonly attribute DOMString id;

  // A DOM error object, that will be not null when a download is stopped
  // because something failed.
  readonly attribute DOMError? error;

  // Pauses the download.
  Promise<DOMDownload> pause();

  // Resumes the download. This resolves only once the download has
  // succeeded.
  Promise<DOMDownload> resume();

  // This event is triggered anytime a property of the object changes:
  // - when the transfer progresses, updating currentBytes.
  // - when the state and/or error attributes change.
  attribute EventHandler onstatechange;
};
