/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * TODO: This is based on what PdfJs was already doing, it would be
 * best to use this over there as well to reduce duplication and
 * inconsistency.
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "DownloadsViewableInternally",
  "PREF_ENABLED_TYPES",
  "PREF_BRANCH_WAS_REGISTERED",
  "PREF_BRANCH_PREVIOUS_ACTION",
  "PREF_BRANCH_PREVIOUS_ASK",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "HandlerService",
  "@mozilla.org/uriloader/handler-service;1",
  "nsIHandlerService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "MIMEService",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);

ChromeUtils.defineModuleGetter(
  this,
  "Integration",
  "resource://gre/modules/Integration.jsm"
);

const PREF_BRANCH = "browser.download.viewableInternally.";
const PREF_ENABLED_TYPES = PREF_BRANCH + "enabledTypes";
const PREF_BRANCH_WAS_REGISTERED = PREF_BRANCH + "typeWasRegistered.";
const PREF_BRANCH_PREVIOUS_ACTION =
  PREF_BRANCH + "previousHandler.preferredAction.";
const PREF_BRANCH_PREVIOUS_ASK =
  PREF_BRANCH + "previousHandler.alwaysAskBeforeHandling.";

let DownloadsViewableInternally = {
  /**
   * Initially add/remove handlers, watch pref, register with Integration.downloads.
   */
  register() {
    // Watch the pref
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_enabledTypes",
      PREF_ENABLED_TYPES,
      "",
      () => this._updateAllHandlers(),
      pref => {
        let itemStr = pref.trim();
        return itemStr ? itemStr.split(",").map(s => s.trim()) : [];
      }
    );

    for (let handlerType of this._downloadTypesViewableInternally) {
      if (handlerType.initAvailable) {
        handlerType.initAvailable();
      }
    }

    // Initially update handlers
    this._updateAllHandlers();

    // Register the check for use in DownloadIntegration
    Integration.downloads.register(base => ({
      shouldViewDownloadInternally: this._shouldViewDownloadInternally.bind(
        this
      ),
    }));
  },

  /**
   * MIME types to handle with an internal viewer, for downloaded files.
   *
   * |extension| is an extenson that will be viewable, as an alternative for
   *   the MIME type itself. It is also used more generally to identify this
   *   type: It is part of a pref name to indicate the handler was set up once,
   *   and it is the string present in |PREF_ENABLED_TYPES| to enable the type.
   *
   * |mimeTypes| are the types that will be viewable. A handler is set up for
   *   the first element in the array.
   *
   * If |managedElsewhere| is falsy, |_updateAllHandlers()| will set
   *   up or remove handlers for the type, and |_shouldViewDownloadInternally()|
   *   will check for it in |PREF_ENABLED_TYPES|.
   *
   * |available| is used to check whether this type should have
   *   handleInternally handlers set up, and if false then
   *   |_shouldViewDownloadInternally()| will also return false for this
   *   type. If |available| would change, |DownloadsViewableInternally._updateHandler()|
   *   should be called for the type.
   *
   * |initAvailable()| is an opportunity to initially set |available|, set up
   *   observers to change it when prefs change, etc.
   *
   */
  _downloadTypesViewableInternally: [
    {
      extension: "xml",
      mimeTypes: ["text/xml", "application/xml"],
      available: true,
    },
    {
      extension: "svg",
      mimeTypes: ["image/svg+xml"],

      initAvailable() {
        XPCOMUtils.defineLazyPreferenceGetter(
          this,
          "available",
          "svg.disabled",
          true,
          () => DownloadsViewableInternally._updateHandler(this),
          // transform disabled to enabled/available
          disabledPref => !disabledPref
        );
      },
      // available getter is set by initAvailable()
    },
    {
      extension: "webp",
      mimeTypes: ["image/webp"],
      initAvailable() {
        XPCOMUtils.defineLazyPreferenceGetter(
          this,
          "available",
          "image.webp.enabled",
          false,
          () => DownloadsViewableInternally._updateHandler(this)
        );
      },
      // available getter is set by initAvailable()
    },
    {
      extension: "avif",
      mimeTypes: ["image/avif"],
      initAvailable() {
        XPCOMUtils.defineLazyPreferenceGetter(
          this,
          "available",
          "image.avif.enabled",
          false,
          () => DownloadsViewableInternally._updateHandler(this)
        );
      },
      // available getter is set by initAvailable()
    },
    {
      extension: "pdf",
      mimeTypes: ["application/pdf"],
      // PDF uses pdfjs.disabled rather than PREF_ENABLED_TYPES.
      // pdfjs.disabled isn't checked here because PdfJs's own _becomeHandler
      // and _unbecomeHandler manage the handler if the pref is set, and there
      // is an explicit check in nsUnknownContentTypeDialog.shouldShowInternalHandlerOption
      available: true,
      managedElsewhere: true,
    },
  ],

  /*
   * Implementation for DownloadIntegration.shouldViewDownloadInternally
   */
  _shouldViewDownloadInternally(aMimeType, aExtension) {
    if (!aMimeType) {
      return false;
    }

    return this._downloadTypesViewableInternally.some(handlerType => {
      if (
        !handlerType.managedElsewhere &&
        !this._enabledTypes.includes(handlerType.extension)
      ) {
        return false;
      }

      return (
        (handlerType.mimeTypes.includes(aMimeType) ||
          handlerType.extension == aExtension?.toLowerCase()) &&
        handlerType.available
      );
    });
  },

  _makeFakeHandler(aMimeType, aExtension) {
    // Based on PdfJs gPdfFakeHandlerInfo.
    return {
      QueryInterface: ChromeUtils.generateQI(["nsIMIMEInfo"]),
      getFileExtensions() {
        return [aExtension];
      },
      possibleApplicationHandlers: Cc["@mozilla.org/array;1"].createInstance(
        Ci.nsIMutableArray
      ),
      extensionExists(ext) {
        return ext == aExtension;
      },
      alwaysAskBeforeHandling: false,
      preferredAction: Ci.nsIHandlerInfo.handleInternally,
      type: aMimeType,
    };
  },

  _saveSettings(handlerInfo, handlerType) {
    Services.prefs.setIntPref(
      PREF_BRANCH_PREVIOUS_ACTION + handlerType.extension,
      handlerInfo.preferredAction
    );
    Services.prefs.setBoolPref(
      PREF_BRANCH_PREVIOUS_ASK + handlerType.extension,
      handlerInfo.alwaysAskBeforeHandling
    );
  },

  _restoreSettings(handlerInfo, handlerType) {
    const prevActionPref = PREF_BRANCH_PREVIOUS_ACTION + handlerType.extension;
    if (Services.prefs.prefHasUserValue(prevActionPref)) {
      handlerInfo.alwaysAskBeforeHandling = Services.prefs.getBoolPref(
        PREF_BRANCH_PREVIOUS_ASK + handlerType.extension
      );
      handlerInfo.preferredAction = Services.prefs.getIntPref(prevActionPref);
      HandlerService.store(handlerInfo);
    } else {
      // Nothing to restore, just remove the handler.
      HandlerService.remove(handlerInfo);
    }
  },

  _clearSavedSettings(extension) {
    Services.prefs.clearUserPref(PREF_BRANCH_PREVIOUS_ACTION + extension);
    Services.prefs.clearUserPref(PREF_BRANCH_PREVIOUS_ASK + extension);
  },

  _updateAllHandlers() {
    // Set up or remove handlers for each type, if not done already
    for (const handlerType of this._downloadTypesViewableInternally) {
      if (!handlerType.managedElsewhere) {
        this._updateHandler(handlerType);
      }
    }
  },

  _updateHandler(handlerType) {
    const wasRegistered = Services.prefs.getBoolPref(
      PREF_BRANCH_WAS_REGISTERED + handlerType.extension,
      false
    );

    const toBeRegistered =
      this._enabledTypes.includes(handlerType.extension) &&
      handlerType.available;

    if (toBeRegistered && !wasRegistered) {
      this._becomeHandler(handlerType);
    } else if (!toBeRegistered && wasRegistered) {
      this._unbecomeHandler(handlerType);
    }
  },

  _becomeHandler(handlerType) {
    // Set up an empty handler with only a preferred action, to avoid
    // having to ask the OS about handlers on startup.
    let fakeHandlerInfo = this._makeFakeHandler(
      handlerType.mimeTypes[0],
      handlerType.extension
    );
    if (!HandlerService.exists(fakeHandlerInfo)) {
      HandlerService.store(fakeHandlerInfo);
    } else {
      const handlerInfo = MIMEService.getFromTypeAndExtension(
        handlerType.mimeTypes[0],
        handlerType.extension
      );

      if (handlerInfo.preferredAction != Ci.nsIHandlerInfo.handleInternally) {
        // Save the previous settings of preferredAction and
        // alwaysAskBeforeHandling in case we need to revert them.
        // Even if we don't force preferredAction here, the user could
        // set handleInternally manually.
        this._saveSettings(handlerInfo, handlerType);
      } else {
        // handleInternally shouldn't already have been set, the best we
        // can do to restore is to remove the handler, so make sure
        // the settings are clear.
        this._clearSavedSettings(handlerType.extension);
      }

      // Replace the preferred action if it didn't indicate an external viewer.
      // Note: This is a point of departure from PdfJs, which always replaces
      // the preferred action.
      if (
        handlerInfo.preferredAction != Ci.nsIHandlerInfo.useHelperApp &&
        handlerInfo.preferredAction != Ci.nsIHandlerInfo.useSystemDefault
      ) {
        handlerInfo.preferredAction = Ci.nsIHandlerInfo.handleInternally;
        handlerInfo.alwaysAskBeforeHandling = false;

        HandlerService.store(handlerInfo);
      }
    }

    // Note that we set up for this type so a) we don't keep replacing the
    // handler and b) so it can be cleared later.
    Services.prefs.setBoolPref(
      PREF_BRANCH_WAS_REGISTERED + handlerType.extension,
      true
    );
  },

  _unbecomeHandler(handlerType) {
    let handlerInfo;
    try {
      handlerInfo = MIMEService.getFromTypeAndExtension(
        handlerType.mimeTypes[0],
        handlerType.extension
      );
    } catch (ex) {
      // Allow the handler lookup to fail.
    }
    // Restore preferred action if it is still handleInternally
    // (possibly just removing the handler if nothing was saved for it).
    if (handlerInfo?.preferredAction == Ci.nsIHandlerInfo.handleInternally) {
      this._restoreSettings(handlerInfo, handlerType);
    }

    // In any case we do not control this handler now.
    this._clearSavedSettings(handlerType.extension);
    Services.prefs.clearUserPref(
      PREF_BRANCH_WAS_REGISTERED + handlerType.extension
    );
  },
};
