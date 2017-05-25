/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This code runs in the sandbox in the content process where the page that
 * loaded the plugin lives. It communicates with the process where the PPAPI
 * implementation lives.
 */
const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                          "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                             "resource://gre/modules/PrivateBrowsingUtils.jsm");

let mm = pluginElement.frameLoader.messageManager;
let containerWindow = pluginElement.ownerGlobal;
// We have to cache the print settings for printing PDF file here, since in
// general we are not able to send XPCOM object across processes.
let printSettings = {};

// Prevent the drag event's default action on the element, to avoid dragging of
// that element while the user selects text.
pluginElement.addEventListener("dragstart",
                               function(event) { event.preventDefault(); });
// For synthetic documents only, prevent the select event's default action on
// the element, to avoid selecting the element and the copying of an empty
// string into the clipboard. Don't do this for non-synthetic documents, as
// users still need to be able to select text outside the plugin.
let isSynthetic = pluginElement.ownerDocument.mozSyntheticDocument;
if (isSynthetic) {
  pluginElement.ownerDocument.body.addEventListener(
    "selectstart", function(event) { event.preventDefault(); });
}

function mapValue(v, instance) {
  return instance.rt.toPP_Var(v, instance);
}

function getFileName(url) {
  let filename = "document.pdf";
  let regex = /[^\/#\?]+\.pdf$/i;

  let result = regex.exec(url.hash) ||
               regex.exec(url.search) ||
               regex.exec(url.pathname);
  if (result) {
    filename = result[0];
  }
  return filename;
}

dump("<>>>>>>>>>>>>>>>>>>>> AHA <<<<<<<<<<<<<<<<<<<<<>\n");
dump(`pluginElement: ${pluginElement.toSource()}\n`);
dump(`pluginElement.frameLoader: ${pluginElement.frameLoader.toSource()}\n`);
dump(`pluginElement.frameLoader.messageManager: ${pluginElement.frameLoader.messageManager.toSource()}\n`);
dump("<>>>>>>>>>>>>>>>>>>>> AHA2 <<<<<<<<<<<<<<<<<<<<<>\n");

mm.addMessageListener("ppapi.js:frameLoaded", ({ target }) => {
  let tagName = pluginElement.nodeName;

  // Getting absolute URL from the EMBED tag
  let url = pluginElement.srcURI.spec;
  let objectParams = new Map();
  for (let i = 0; i < pluginElement.attributes.length; ++i) {
    let paramName = pluginElement.attributes[i].localName;
    objectParams.set(paramName, pluginElement.attributes[i].value);
  }
  if (tagName == "OBJECT") {
    let params = pluginElement.getElementsByTagName("param");
    Array.prototype.forEach.call(params, (p) => {
      var paramName = p.getAttribute("name").toLowerCase();
      objectParams.set(paramName, p.getAttribute("value"));
    });
  }

  let documentURL = pluginElement.ownerDocument.location.href;
  let baseUrl = documentURL;
  if (objectParams.base) {
    try {
      let parsedDocumentUrl = Services.io.newURI(documentURL);
      baseUrl = Services.io.newURI(objectParams.base, null, parsedDocumentUrl).spec;
    } catch (e) { /* ignore */ }
  }

  let info = {
    documentURL: "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai",
    url,
    setupJSInstanceObject: false,
    isFullFrame: false, //pluginElement.ownerDocument.mozSyntheticDocument,
    arguments: {
      keys: ["src", "full-frame", "top-level-url"],
      values: [url, "", documentURL],
    },
  };

  mm.sendAsyncMessage("ppapi.js:createInstance",  { type: "pdf", info },
                      { pluginWindow: containerWindow });

  containerWindow.document.addEventListener("fullscreenchange", () => {
    let fullscreen = (containerWindow.document.fullscreenElement == pluginElement);
    mm.sendAsyncMessage("ppapi.js:fullscreenchange", { fullscreen });
  });

  containerWindow.addEventListener("hashchange", () => {
    let url = containerWindow.location.href;
    mm.sendAsyncMessage("ppapipdf.js:hashchange", { url });
  })
});

mm.addMessageListener("ppapi.js:setFullscreen", ({ data }) => {
  if (data) {
    pluginElement.requestFullscreen();
  } else {
    containerWindow.document.exitFullscreen();
  }
});

mm.addMessageListener("ppapipdf.js:setHash", ({ data }) => {
  if (data) {
    containerWindow.location.hash = data;
  }
});

mm.addMessageListener("ppapipdf.js:getPrintSettings", ({ data }) => {
  // Set the title to pdf file name for the default print to file name.
  let url = new containerWindow.URL(data.url);
  containerWindow.document.title = getFileName(url);

  let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].
              getService(Ci.nsIPrintSettingsService);
  printSettings = PSSVC.globalPrintSettings;
  let webBrowserPrint =
    containerWindow.QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIWebBrowserPrint);
  let PPSVC = Cc["@mozilla.org/embedcomp/printingprompt-service;1"].
              getService(Ci.nsIPrintingPromptService);
  PPSVC.showPrintDialog(containerWindow, webBrowserPrint, printSettings);

  // XPCOM object should not in general be sent across processes. So we have to
  // copy out only the info PDFium needed and send it back to jsplugin process
  // for getting PDF file.
  let trimPrintSettings = {};
  trimPrintSettings.marginTop = printSettings.marginTop;
  trimPrintSettings.marginLeft = printSettings.marginLeft;
  trimPrintSettings.marginBottom = printSettings.marginBottom;
  trimPrintSettings.marginRight = printSettings.marginRight;
  trimPrintSettings.unwriteableMarginLeft =
    printSettings.unwriteableMarginLeft;
  trimPrintSettings.unwriteableMarginTop =
    printSettings.unwriteableMarginTop;
  trimPrintSettings.unwriteableMarginRight =
    printSettings.unwriteableMarginRight;
  trimPrintSettings.unwriteableMarginBottom =
    printSettings.unwriteableMarginBottom;
  trimPrintSettings.paperWidth = printSettings.paperWidth;
  trimPrintSettings.paperHeight = printSettings.paperHeight;
  trimPrintSettings.paperSizeUnit = printSettings.paperSizeUnit;
  trimPrintSettings.orientation = printSettings.orientation;
  trimPrintSettings.shrinkToFit = printSettings.shrinkToFit;
  trimPrintSettings.printInColor = printSettings.printInColor;
  trimPrintSettings.printRange = printSettings.printRange;
  trimPrintSettings.startPageRange = printSettings.startPageRange;
  trimPrintSettings.endPageRange = printSettings.endPageRange;

  mm.sendAsyncMessage("ppapipdf.js:printsettingschanged", {
    trimPrintSettings });
});

mm.addMessageListener("ppapipdf.js:printPDF", ({ data }) => {
  let file = new FileUtils.File(data.filePath);
  if (!file.exists()) {
    return;
  }

  let webBrowserPrint =
    containerWindow.QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIWebBrowserPrint);
  if (!webBrowserPrint || !webBrowserPrint.printPDF) {
    file.remove(false);
    return;
  }

  webBrowserPrint.printPDF(file.path, printSettings)
  .then(() => {
    file.remove(false);
  })
  .catch(() => {
    file.remove(false);
  });
});

mm.addMessageListener("ppapipdf.js:openLink", ({data}) => {
  const PDFIUM_WINDOW_OPEN_DISPOSITION = {
    CURRENT_TAB: 1,
    NEW_FOREGROUND_TAB: 3,
    NEW_BACKGROUND_TAB: 4,
    NEW_WINDOW: 6,
  };
  switch(data.disposition) {
    case PDFIUM_WINDOW_OPEN_DISPOSITION.CURRENT_TAB:
      containerWindow.location.href = data.url;
      break;
    // We don't support opening in background tab for now. Just open it in
    // foreground tab.
    case PDFIUM_WINDOW_OPEN_DISPOSITION.NEW_FOREGROUND_TAB:
    case PDFIUM_WINDOW_OPEN_DISPOSITION.NEW_BACKGROUND_TAB:
      containerWindow.open(data.url);
      break;
    case PDFIUM_WINDOW_OPEN_DISPOSITION.NEW_WINDOW:
      containerWindow.open(data.url, "",
        "noopener=1,menubar=1,toolbar=1," +
        "location=1,personalbar=1,status=1,resizable");
      break;
  }
});

mm.addMessageListener("ppapipdf.js:save", ({ data }) => {
  let url = new containerWindow.URL(data.url);
  let filename = getFileName(url);
  let originalUri = NetUtil.newURI(url.href);
  let extHelperAppSvc =
        Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
           getService(Ci.nsIExternalHelperAppService);

  let docIsPrivate =
    PrivateBrowsingUtils.isContentWindowPrivate(containerWindow);
  let netChannel = NetUtil.newChannel({
    uri: originalUri,
    loadUsingSystemPrincipal: true,
  });

  if ("nsIPrivateBrowsingChannel" in Ci &&
      netChannel instanceof Ci.nsIPrivateBrowsingChannel) {
    netChannel.setPrivate(docIsPrivate);
  }
  NetUtil.asyncFetch(netChannel, function(aInputStream, aResult) {
    if (!Components.isSuccessCode(aResult)) {
      return;
    }
    // Create a nsIInputStreamChannel so we can set the url on the channel
    // so the filename will be correct.
    let channel = Cc["@mozilla.org/network/input-stream-channel;1"].
                     createInstance(Ci.nsIInputStreamChannel);
    channel.QueryInterface(Ci.nsIChannel);
    channel.contentDisposition = Ci.nsIChannel.DISPOSITION_ATTACHMENT;
    channel.contentDispositionFilename = filename;
    channel.setURI(originalUri);
    channel.loadInfo = netChannel.loadInfo;
    channel.contentStream = aInputStream;
    if ("nsIPrivateBrowsingChannel" in Ci &&
        channel instanceof Ci.nsIPrivateBrowsingChannel) {
      channel.setPrivate(docIsPrivate);
    }

    let listener = {
      extListener: null,
      onStartRequest(aRequest, aContext) {
        var loadContext = containerWindow
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsILoadContext);
        this.extListener = extHelperAppSvc.doContent(
          "application/pdf", aRequest, loadContext, false);
        this.extListener.onStartRequest(aRequest, aContext);
      },
      onStopRequest(aRequest, aContext, aStatusCode) {
        if (this.extListener) {
          this.extListener.onStopRequest(aRequest, aContext, aStatusCode);
        }
      },
      onDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount) {
        this.extListener
          .onDataAvailable(aRequest, aContext, aInputStream, aOffset, aCount);
      }
    };

    channel.asyncOpen2(listener);
  });
});

// This class is created to transfer global XUL commands event we needed.
// The main reason we need to transfer it from sandbox side is that commands
// triggered from menu bar targets only the outmost iframe (which is sandbox
// itself) so we need to propagate it manually into the plugin's iframe.
class CommandController {
  constructor() {
    this.SUPPORTED_COMMANDS = ['cmd_copy', 'cmd_selectAll'];
    containerWindow.controllers.insertControllerAt(0, this);
    containerWindow.addEventListener('unload', this.terminate.bind(this));
  }

  terminate() {
    containerWindow.controllers.removeController(this);
  }

  supportsCommand(cmd) {
    return this.SUPPORTED_COMMANDS.includes(cmd);
  }

  isCommandEnabled(cmd) {
    return this.SUPPORTED_COMMANDS.includes(cmd);
  }

  doCommand(cmd) {
    mm.sendAsyncMessage("ppapipdf.js:oncommand", {name: cmd});
  }

  onEvent(evt) {}
};
var commandController = new CommandController();

mm.loadFrameScript("resource://ppapi.js/ppapi-instance.js", true);
