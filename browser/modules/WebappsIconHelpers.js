/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This function receives a list of icon sizes
 * and URLs and returns the url string for the biggest icon.
 *
 * @param aIcons An object where the keys are the icon sizes
 *               and the values are URL strings. E.g.:
 *               aIcons = {
 *                 "16": "http://www.example.org/icon16.png",
 *                 "32": "http://www.example.org/icon32.png"
 *               };
 *
 * @returns the URL string for the largest specified icon
 */
function getBiggestIconURL(aIcons) {
  if (!aIcons) {
    return "chrome://browser/skin/webapps-64.png";
  }

  let iconSizes = Object.keys(aIcons);
  if (iconSizes.length == 0) {
    return "chrome://browser/skin/webapps-64.png";
  }
  iconSizes.sort(function(a, b) a - b);
  return aIcons[iconSizes.pop()];
}

/**
 * This function retrieves the icon for an app as specified
 * in the iconURI on the shell object.
 * Upon completion it will call aShell.processIcon()
 *
 * @param aShell The shell that specifies the properties
 *               of the native app. Three properties from this
 *               shell will be used in this function:
 *                 - iconURI
 *                 - useTmpForIcon
 *                 - processIcon()
 */
function getIconForApp(aShell, callback) {
  let iconURI = aShell.iconURI;
  let mimeService = Cc["@mozilla.org/mime;1"]
                      .getService(Ci.nsIMIMEService);

  let mimeType;
  try {
    let tIndex = iconURI.path.indexOf(";");
    if("data" == iconURI.scheme && tIndex != -1) {
      mimeType = iconURI.path.substring(0, tIndex);
    } else {
      mimeType = mimeService.getTypeFromURI(iconURI);
    }
  } catch(e) {
    throw("getIconFromURI - Failed to determine MIME type");
  }

  try {
    let listener;
    if(aShell.useTmpForIcon) {
      let downloadObserver = {
        onDownloadComplete: function(downloader, request, cx, aStatus, file) {
          // pass downloader just to keep reference around
          onIconDownloaded(aShell, mimeType, aStatus, file, callback, downloader);
        }
      };

      let tmpIcon = Services.dirsvc.get("TmpD", Ci.nsIFile);
      tmpIcon.append("tmpicon." + mimeService.getPrimaryExtension(mimeType, ""));
      tmpIcon.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

      listener = Cc["@mozilla.org/network/downloader;1"]
                   .createInstance(Ci.nsIDownloader);
      listener.init(downloadObserver, tmpIcon);
    } else {
      let pipe = Cc["@mozilla.org/pipe;1"]
                   .createInstance(Ci.nsIPipe);
      pipe.init(true, true, 0, 0xffffffff, null);

      listener = Cc["@mozilla.org/network/simple-stream-listener;1"]
                   .createInstance(Ci.nsISimpleStreamListener);
      listener.init(pipe.outputStream, {
          onStartRequest: function() {},
          onStopRequest: function(aRequest, aContext, aStatusCode) {
            pipe.outputStream.close();
            onIconDownloaded(aShell, mimeType, aStatusCode, pipe.inputStream, callback);
         }
      });
    }

    let channel = NetUtil.newChannel(iconURI);
    let CertUtils = { };
    Cu.import("resource://gre/modules/CertUtils.jsm", CertUtils);
    // Pass true to avoid optional redirect-cert-checking behavior.
    channel.notificationCallbacks = new CertUtils.BadCertHandler(true);

    channel.asyncOpen(listener, null);
  } catch(e) {
    throw("getIconFromURI - Failure getting icon (" + e + ")");
  }
}

function onIconDownloaded(aShell, aMimeType, aStatusCode, aIcon, aCallback) {
  if (Components.isSuccessCode(aStatusCode)) {
    aShell.processIcon(aMimeType, aIcon, aCallback);
  } else {
    aCallback.call(aShell);
  }
}
