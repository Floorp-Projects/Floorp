/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../../toolkit/content/globalOverlay.js */
/* import-globals-from ../../../../toolkit/content/contentAreaUtils.js */
/* import-globals-from ../../../../toolkit/content/treeUtils.js */
/* import-globals-from ../utilityOverlay.js */
/* import-globals-from permissions.js */
/* import-globals-from security.js */

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
});

// define a js object to implement nsITreeView
function pageInfoTreeView(treeid, copycol) {
  // copycol is the index number for the column that we want to add to
  // the copy-n-paste buffer when the user hits accel-c
  this.treeid = treeid;
  this.copycol = copycol;
  this.rows = 0;
  this.tree = null;
  this.data = [];
  this.selection = null;
  this.sortcol = -1;
  this.sortdir = false;
}

pageInfoTreeView.prototype = {
  set rowCount(c) {
    throw new Error("rowCount is a readonly property");
  },
  get rowCount() {
    return this.rows;
  },

  setTree(tree) {
    this.tree = tree;
  },

  getCellText(row, column) {
    // row can be null, but js arrays are 0-indexed.
    // colidx cannot be null, but can be larger than the number
    // of columns in the array. In this case it's the fault of
    // whoever typoed while calling this function.
    return this.data[row][column.index] || "";
  },

  setCellValue(row, column, value) {},

  setCellText(row, column, value) {
    this.data[row][column.index] = value;
  },

  addRow(row) {
    this.rows = this.data.push(row);
    this.rowCountChanged(this.rows - 1, 1);
    if (this.selection.count == 0 && this.rowCount && !gImageElement) {
      this.selection.select(0);
    }
  },

  addRows(rows) {
    for (let row of rows) {
      this.addRow(row);
    }
  },

  rowCountChanged(index, count) {
    this.tree.rowCountChanged(index, count);
  },

  invalidate() {
    this.tree.invalidate();
  },

  clear() {
    if (this.tree) {
      this.tree.rowCountChanged(0, -this.rows);
    }
    this.rows = 0;
    this.data = [];
  },

  onPageMediaSort(columnname) {
    var tree = document.getElementById(this.treeid);
    var treecol = tree.columns.getNamedColumn(columnname);

    this.sortdir = gTreeUtils.sort(
      tree,
      this,
      this.data,
      treecol.index,
      function textComparator(a, b) {
        return (a || "").toLowerCase().localeCompare((b || "").toLowerCase());
      },
      this.sortcol,
      this.sortdir
    );

    for (let col of tree.columns) {
      col.element.removeAttribute("sortActive");
      col.element.removeAttribute("sortDirection");
    }
    treecol.element.setAttribute("sortActive", "true");
    treecol.element.setAttribute(
      "sortDirection",
      this.sortdir ? "ascending" : "descending"
    );

    this.sortcol = treecol.index;
  },

  getRowProperties(row) {
    return "";
  },
  getCellProperties(row, column) {
    return "";
  },
  getColumnProperties(column) {
    return "";
  },
  isContainer(index) {
    return false;
  },
  isContainerOpen(index) {
    return false;
  },
  isSeparator(index) {
    return false;
  },
  isSorted() {
    return this.sortcol > -1;
  },
  canDrop(index, orientation) {
    return false;
  },
  drop(row, orientation) {
    return false;
  },
  getParentIndex(index) {
    return 0;
  },
  hasNextSibling(index, after) {
    return false;
  },
  getLevel(index) {
    return 0;
  },
  getImageSrc(row, column) {},
  getCellValue(row, column) {
    let col = column != null ? column : this.copycol;
    return row < 0 || col < 0 ? "" : this.data[row][col] || "";
  },
  toggleOpenState(index) {},
  cycleHeader(col) {},
  selectionChanged() {},
  cycleCell(row, column) {},
  isEditable(row, column) {
    return false;
  },
};

// mmm, yummy. global variables.
var gDocInfo = null;
var gImageElement = null;

// column number to help using the data array
const COL_IMAGE_ADDRESS = 0;
const COL_IMAGE_TYPE = 1;
const COL_IMAGE_SIZE = 2;
const COL_IMAGE_ALT = 3;
const COL_IMAGE_COUNT = 4;
const COL_IMAGE_NODE = 5;
const COL_IMAGE_BG = 6;

// column number to copy from, second argument to pageInfoTreeView's constructor
const COPYCOL_NONE = -1;
const COPYCOL_META_CONTENT = 1;
const COPYCOL_IMAGE = COL_IMAGE_ADDRESS;

// one nsITreeView for each tree in the window
var gMetaView = new pageInfoTreeView("metatree", COPYCOL_META_CONTENT);
var gImageView = new pageInfoTreeView("imagetree", COPYCOL_IMAGE);

gImageView.getCellProperties = function(row, col) {
  var data = gImageView.data[row];
  var item = gImageView.data[row][COL_IMAGE_NODE];
  var props = "";
  if (
    !checkProtocol(data) ||
    item instanceof HTMLEmbedElement ||
    (item instanceof HTMLObjectElement && !item.type.startsWith("image/"))
  ) {
    props += "broken";
  }

  if (col.element.id == "image-address") {
    props += " ltr";
  }

  return props;
};

gImageView.onPageMediaSort = function(columnname) {
  var tree = document.getElementById(this.treeid);
  var treecol = tree.columns.getNamedColumn(columnname);

  var comparator;
  var index = treecol.index;
  if (index == COL_IMAGE_SIZE || index == COL_IMAGE_COUNT) {
    comparator = function numComparator(a, b) {
      return a - b;
    };
  } else {
    comparator = function textComparator(a, b) {
      return (a || "").toLowerCase().localeCompare((b || "").toLowerCase());
    };
  }

  this.sortdir = gTreeUtils.sort(
    tree,
    this,
    this.data,
    index,
    comparator,
    this.sortcol,
    this.sortdir
  );

  for (let col of tree.columns) {
    col.element.removeAttribute("sortActive");
    col.element.removeAttribute("sortDirection");
  }
  treecol.element.setAttribute("sortActive", "true");
  treecol.element.setAttribute(
    "sortDirection",
    this.sortdir ? "ascending" : "descending"
  );

  this.sortcol = index;
};

var gImageHash = {};

// localized strings (will be filled in when the document is loaded)
const MEDIA_STRINGS = {};
let SIZE_UNKNOWN = "";
let ALT_NOT_SET = "";

// a number of services I'll need later
// the cache services
const nsICacheStorageService = Ci.nsICacheStorageService;
const nsICacheStorage = Ci.nsICacheStorage;
const cacheService = Cc[
  "@mozilla.org/netwerk/cache-storage-service;1"
].getService(nsICacheStorageService);

var loadContextInfo = Services.loadContextInfo.fromLoadContext(
  window.docShell.QueryInterface(Ci.nsILoadContext),
  false
);
var diskStorage = cacheService.diskCacheStorage(loadContextInfo);

const nsICookiePermission = Ci.nsICookiePermission;

const nsICertificateDialogs = Ci.nsICertificateDialogs;
const CERTIFICATEDIALOGS_CONTRACTID = "@mozilla.org/nsCertificateDialogs;1";

// clipboard helper
function getClipboardHelper() {
  try {
    return Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
      Ci.nsIClipboardHelper
    );
  } catch (e) {
    // do nothing, later code will handle the error
    return null;
  }
}
const gClipboardHelper = getClipboardHelper();

/* Called when PageInfo window is loaded.  Arguments are:
 *  window.arguments[0] - (optional) an object consisting of
 *                         - doc: (optional) document to use for source. if not provided,
 *                                the calling window's document will be used
 *                         - initialTab: (optional) id of the inital tab to display
 */
async function onLoadPageInfo() {
  [
    SIZE_UNKNOWN,
    ALT_NOT_SET,
    MEDIA_STRINGS.img,
    MEDIA_STRINGS["bg-img"],
    MEDIA_STRINGS["border-img"],
    MEDIA_STRINGS["list-img"],
    MEDIA_STRINGS.cursor,
    MEDIA_STRINGS.object,
    MEDIA_STRINGS.embed,
    MEDIA_STRINGS.link,
    MEDIA_STRINGS.input,
    MEDIA_STRINGS.video,
    MEDIA_STRINGS.audio,
  ] = await document.l10n.formatValues([
    "image-size-unknown",
    "not-set-alternative-text",
    "media-img",
    "media-bg-img",
    "media-border-img",
    "media-list-img",
    "media-cursor",
    "media-object",
    "media-embed",
    "media-link",
    "media-input",
    "media-video",
    "media-audio",
  ]);

  const args =
    "arguments" in window &&
    window.arguments.length >= 1 &&
    window.arguments[0];

  // Init media view
  document.getElementById("imagetree").view = gImageView;

  // Select the requested tab, if the name is specified
  await loadTab(args);

  // Emit init event for tests
  window.dispatchEvent(new Event("page-info-init"));
}

async function loadPageInfo(browsingContext, imageElement, browser) {
  browser = browser || window.opener.gBrowser.selectedBrowser;
  browsingContext = browsingContext || browser.browsingContext;

  let actor = browsingContext.currentWindowGlobal.getActor("PageInfo");

  let result = await actor.sendQuery("PageInfo:getData");
  await onNonMediaPageInfoLoad(browser, result, imageElement);

  // Here, we are walking the frame tree via BrowsingContexts to collect all of the
  // media information for each frame
  let contextsToVisit = [browsingContext];
  while (contextsToVisit.length) {
    let currContext = contextsToVisit.pop();
    let global = currContext.currentWindowGlobal;

    if (!global) {
      continue;
    }

    let subframeActor = global.getActor("PageInfo");
    let mediaResult = await subframeActor.sendQuery("PageInfo:getMediaData");
    for (let item of mediaResult.mediaItems) {
      addImage(item);
    }
    selectImage();
    contextsToVisit.push(...currContext.children);
  }
}

/**
 * onNonMediaPageInfoLoad is responsible for populating the page info
 * UI other than the media tab. This includes general, permissions, and security.
 */
async function onNonMediaPageInfoLoad(browser, pageInfoData, imageInfo) {
  const { docInfo, windowInfo } = pageInfoData;
  let uri = Services.io.newURI(docInfo.documentURIObject.spec);
  let principal = docInfo.principal;
  gDocInfo = docInfo;

  gImageElement = imageInfo;
  var titleFormat = windowInfo.isTopWindow
    ? "page-info-page"
    : "page-info-frame";
  document.l10n.setAttributes(document.documentElement, titleFormat, {
    website: docInfo.location,
  });

  document
    .getElementById("main-window")
    .setAttribute("relatedUrl", docInfo.location);

  await makeGeneralTab(pageInfoData.metaViewRows, docInfo);
  if (
    uri.spec.startsWith("about:neterror") ||
    uri.spec.startsWith("about:certerror") ||
    uri.spec.startsWith("about:httpsonlyerror")
  ) {
    uri = browser.currentURI;
    principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      browser.contentPrincipal.originAttributes
    );
  }
  onLoadPermission(uri, principal);
  securityOnLoad(uri, windowInfo);
}

function resetPageInfo(args) {
  /* Reset Meta tags part */
  gMetaView.clear();

  /* Reset Media tab */
  var mediaTab = document.getElementById("mediaTab");
  if (!mediaTab.hidden) {
    mediaTab.hidden = true;
  }
  gImageView.clear();
  gImageHash = {};

  /* Rebuild the data */
  loadTab(args);
}

function doHelpButton() {
  const helpTopics = {
    generalPanel: "pageinfo_general",
    mediaPanel: "pageinfo_media",
    permPanel: "pageinfo_permissions",
    securityPanel: "pageinfo_security",
  };

  var deck = document.getElementById("mainDeck");
  var helpdoc = helpTopics[deck.selectedPanel.id] || "pageinfo_general";
  openHelpLink(helpdoc);
}

function showTab(id) {
  var deck = document.getElementById("mainDeck");
  var pagel = document.getElementById(id + "Panel");
  deck.selectedPanel = pagel;
}

async function loadTab(args) {
  // If the "View Image Info" context menu item was used, the related image
  // element is provided as an argument. This can't be a background image.
  let imageElement = args?.imageElement;
  let browsingContext = args?.browsingContext;
  let browser = args?.browser;

  /* Load the page info */
  await loadPageInfo(browsingContext, imageElement, browser);

  var initialTab = args?.initialTab || "generalTab";
  var radioGroup = document.getElementById("viewGroup");
  initialTab =
    document.getElementById(initialTab) ||
    document.getElementById("generalTab");
  radioGroup.selectedItem = initialTab;
  radioGroup.selectedItem.doCommand();
  radioGroup.focus();
}

function openCacheEntry(key, cb) {
  var checkCacheListener = {
    onCacheEntryCheck(entry) {
      return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
    },
    onCacheEntryAvailable(entry, isNew, status) {
      cb(entry);
    },
  };
  diskStorage.asyncOpenURI(
    Services.io.newURI(key),
    "",
    nsICacheStorage.OPEN_READONLY,
    checkCacheListener
  );
}

async function makeGeneralTab(metaViewRows, docInfo) {
  // Sets Title in the General Tab, set to "Untitled Page" if no title found
  if (docInfo.title) {
    document.getElementById("titletext").value = docInfo.title;
  } else {
    document.l10n.setAttributes(
      document.getElementById("titletext"),
      "no-page-title"
    );
  }

  var url = docInfo.location;
  setItemValue("urltext", url);

  var referrer = "referrer" in docInfo && docInfo.referrer;
  setItemValue("refertext", referrer);

  var mode =
    "compatMode" in docInfo && docInfo.compatMode == "BackCompat"
      ? "general-quirks-mode"
      : "general-strict-mode";
  document.l10n.setAttributes(document.getElementById("modetext"), mode);

  // find out the mime type
  setItemValue("typetext", docInfo.contentType);

  // get the document characterset
  var encoding = docInfo.characterSet;
  document.getElementById("encodingtext").value = encoding;

  let length = metaViewRows.length;

  var metaGroup = document.getElementById("metaTags");
  if (!length) {
    metaGroup.style.visibility = "hidden";
  } else {
    document.l10n.setAttributes(
      document.getElementById("metaTagsCaption"),
      "general-meta-tags",
      { tags: length }
    );

    document.getElementById("metatree").view = gMetaView;

    // Add the metaViewRows onto the general tab's meta info tree.
    gMetaView.addRows(metaViewRows);

    metaGroup.style.removeProperty("visibility");
  }

  var modifiedText = formatDate(
    docInfo.lastModified,
    await document.l10n.formatValue("not-set-date")
  );
  document.getElementById("modifiedtext").value = modifiedText;

  // get cache info
  var cacheKey = url.replace(/#.*$/, "");
  openCacheEntry(cacheKey, function(cacheEntry) {
    if (cacheEntry) {
      var pageSize = cacheEntry.dataSize;
      var kbSize = formatNumber(Math.round((pageSize / 1024) * 100) / 100);
      document.l10n.setAttributes(
        document.getElementById("sizetext"),
        "properties-general-size",
        { kb: kbSize, bytes: formatNumber(pageSize) }
      );
    } else {
      setItemValue("sizetext", null);
    }
  });
}

async function addImage({ url, type, alt, altNotProvided, element, isBg }) {
  if (!url) {
    return;
  }

  if (altNotProvided) {
    alt = ALT_NOT_SET;
  }

  if (!gImageHash.hasOwnProperty(url)) {
    gImageHash[url] = {};
  }
  if (!gImageHash[url].hasOwnProperty(type)) {
    gImageHash[url][type] = {};
  }
  if (!gImageHash[url][type].hasOwnProperty(alt)) {
    gImageHash[url][type][alt] = gImageView.data.length;
    var row = [url, MEDIA_STRINGS[type], SIZE_UNKNOWN, alt, 1, element, isBg];
    gImageView.addRow(row);

    // Fill in cache data asynchronously
    openCacheEntry(url, function(cacheEntry) {
      // The data at row[2] corresponds to the data size.
      if (cacheEntry) {
        let value = cacheEntry.dataSize;
        // If value is not -1 then replace with actual value, else keep as "unknown"
        if (value != -1) {
          let kbSize = Number(Math.round((value / 1024) * 100) / 100);
          document.l10n
            .formatValue("media-file-size", { size: kbSize })
            .then(function(response) {
              row[2] = response;
              // Invalidate the row to trigger a repaint.
              gImageView.tree.invalidateRow(gImageView.data.indexOf(row));
            });
        }
      }
    });

    if (gImageView.data.length == 1) {
      document.getElementById("mediaTab").hidden = false;
    }
  } else {
    var i = gImageHash[url][type][alt];
    gImageView.data[i][COL_IMAGE_COUNT]++;
    // The same image can occur several times on the page at different sizes.
    // If the "View Image Info" context menu item was used, ensure we select
    // the correct element.
    if (
      !gImageView.data[i][COL_IMAGE_BG] &&
      gImageElement &&
      url == gImageElement.currentSrc &&
      gImageElement.width == element.width &&
      gImageElement.height == element.height &&
      gImageElement.imageText == element.imageText
    ) {
      gImageView.data[i][COL_IMAGE_NODE] = element;
    }
  }
}

// Link Stuff
function onBeginLinkDrag(event, urlField, descField) {
  if (event.originalTarget.localName != "treechildren") {
    return;
  }

  var tree = event.target;
  if (tree.localName != "tree") {
    tree = tree.parentNode;
  }

  var row = tree.getRowAt(event.clientX, event.clientY);
  if (row == -1) {
    return;
  }

  // Adding URL flavor
  var col = tree.columns[urlField];
  var url = tree.view.getCellText(row, col);
  col = tree.columns[descField];
  var desc = tree.view.getCellText(row, col);

  var dt = event.dataTransfer;
  dt.setData("text/x-moz-url", url + "\n" + desc);
  dt.setData("text/url-list", url);
  dt.setData("text/plain", url);
}

// Image Stuff
function getSelectedRows(tree) {
  var start = {};
  var end = {};
  var numRanges = tree.view.selection.getRangeCount();

  var rowArray = [];
  for (var t = 0; t < numRanges; t++) {
    tree.view.selection.getRangeAt(t, start, end);
    for (var v = start.value; v <= end.value; v++) {
      rowArray.push(v);
    }
  }

  return rowArray;
}

function getSelectedRow(tree) {
  var rows = getSelectedRows(tree);
  return rows.length == 1 ? rows[0] : -1;
}

async function selectSaveFolder(aCallback) {
  const { nsIFile, nsIFilePicker } = Ci;
  let titleText = await document.l10n.formatValue("media-select-folder");
  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  let fpCallback = function fpCallback_done(aResult) {
    if (aResult == nsIFilePicker.returnOK) {
      aCallback(fp.file.QueryInterface(nsIFile));
    } else {
      aCallback(null);
    }
  };

  fp.init(window, titleText, nsIFilePicker.modeGetFolder);
  fp.appendFilters(nsIFilePicker.filterAll);
  try {
    let initialDir = Services.prefs.getComplexValue(
      "browser.download.dir",
      nsIFile
    );
    if (initialDir) {
      fp.displayDirectory = initialDir;
    }
  } catch (ex) {}
  fp.open(fpCallback);
}

function saveMedia() {
  var tree = document.getElementById("imagetree");
  var rowArray = getSelectedRows(tree);
  let ReferrerInfo = Components.Constructor(
    "@mozilla.org/referrer-info;1",
    "nsIReferrerInfo",
    "init"
  );

  if (rowArray.length == 1) {
    let row = rowArray[0];
    let item = gImageView.data[row][COL_IMAGE_NODE];
    let url = gImageView.data[row][COL_IMAGE_ADDRESS];

    if (url) {
      var titleKey = "SaveImageTitle";

      if (item instanceof HTMLVideoElement) {
        titleKey = "SaveVideoTitle";
      } else if (item instanceof HTMLAudioElement) {
        titleKey = "SaveAudioTitle";
      }

      // Bug 1565216 to evaluate passing referrer as item.baseURL
      let referrerInfo = new ReferrerInfo(
        Ci.nsIReferrerInfo.EMPTY,
        true,
        Services.io.newURI(item.baseURI)
      );
      let cookieJarSettings = E10SUtils.deserializeCookieJarSettings(
        gDocInfo.cookieJarSettings
      );
      saveURL(
        url,
        null,
        titleKey,
        false,
        false,
        referrerInfo,
        cookieJarSettings,
        null,
        gDocInfo.isContentWindowPrivate,
        gDocInfo.principal
      );
    }
  } else {
    selectSaveFolder(function(aDirectory) {
      if (aDirectory) {
        var saveAnImage = function(aURIString, aChosenData, aBaseURI) {
          uniqueFile(aChosenData.file);

          let referrerInfo = new ReferrerInfo(
            Ci.nsIReferrerInfo.EMPTY,
            true,
            aBaseURI
          );
          let cookieJarSettings = E10SUtils.deserializeCookieJarSettings(
            gDocInfo.cookieJarSettings
          );
          internalSave(
            aURIString,
            null,
            null,
            null,
            null,
            false,
            "SaveImageTitle",
            aChosenData,
            referrerInfo,
            cookieJarSettings,
            null,
            false,
            null,
            gDocInfo.isContentWindowPrivate,
            gDocInfo.principal
          );
        };

        for (var i = 0; i < rowArray.length; i++) {
          let v = rowArray[i];
          let dir = aDirectory.clone();
          let item = gImageView.data[v][COL_IMAGE_NODE];
          let uriString = gImageView.data[v][COL_IMAGE_ADDRESS];
          let uri = Services.io.newURI(uriString);

          try {
            uri.QueryInterface(Ci.nsIURL);
            dir.append(decodeURIComponent(uri.fileName));
          } catch (ex) {
            // data:/blob: uris
            // Supply a dummy filename, otherwise Download Manager
            // will try to delete the base directory on failure.
            dir.append(gImageView.data[v][COL_IMAGE_TYPE]);
          }

          if (i == 0) {
            saveAnImage(
              uriString,
              new AutoChosen(dir, uri),
              Services.io.newURI(item.baseURI)
            );
          } else {
            // This delay is a hack which prevents the download manager
            // from opening many times. See bug 377339.
            setTimeout(
              saveAnImage,
              200,
              uriString,
              new AutoChosen(dir, uri),
              Services.io.newURI(item.baseURI)
            );
          }
        }
      }
    });
  }
}

function onImageSelect() {
  var previewBox = document.getElementById("mediaPreviewBox");
  var mediaSaveBox = document.getElementById("mediaSaveBox");
  var splitter = document.getElementById("mediaSplitter");
  var tree = document.getElementById("imagetree");
  var count = tree.view.selection.count;
  if (count == 0) {
    previewBox.collapsed = true;
    mediaSaveBox.collapsed = true;
    splitter.collapsed = true;
    tree.flex = 1;
  } else if (count > 1) {
    splitter.collapsed = true;
    previewBox.collapsed = true;
    mediaSaveBox.collapsed = false;
    tree.flex = 1;
  } else {
    mediaSaveBox.collapsed = true;
    splitter.collapsed = false;
    previewBox.collapsed = false;
    tree.flex = 0;
    makePreview(getSelectedRows(tree)[0]);
  }
}

// Makes the media preview (image, video, etc) for the selected row on the media tab.
function makePreview(row) {
  var item = gImageView.data[row][COL_IMAGE_NODE];
  var url = gImageView.data[row][COL_IMAGE_ADDRESS];
  var isBG = gImageView.data[row][COL_IMAGE_BG];
  var isAudio = false;

  setItemValue("imageurltext", url);
  setItemValue("imagetext", item.imageText);
  setItemValue("imagelongdesctext", item.longDesc);

  // get cache info
  var cacheKey = url.replace(/#.*$/, "");
  openCacheEntry(cacheKey, function(cacheEntry) {
    // find out the file size
    if (cacheEntry) {
      let imageSize = cacheEntry.dataSize;
      var kbSize = Math.round((imageSize / 1024) * 100) / 100;
      document.l10n.setAttributes(
        document.getElementById("imagesizetext"),
        "properties-general-size",
        { kb: formatNumber(kbSize), bytes: formatNumber(imageSize) }
      );
    } else {
      document.l10n.setAttributes(
        document.getElementById("imagesizetext"),
        "media-unknown-not-cached"
      );
    }

    var mimeType = item.mimeType || this.getContentTypeFromHeaders(cacheEntry);
    var numFrames = item.numFrames;

    let element = document.getElementById("imagetypetext");
    var imageType;
    if (mimeType) {
      // We found the type, try to display it nicely
      let imageMimeType = /^image\/(.*)/i.exec(mimeType);
      if (imageMimeType) {
        imageType = imageMimeType[1].toUpperCase();
        if (numFrames > 1) {
          document.l10n.setAttributes(element, "media-animated-image-type", {
            type: imageType,
            frames: numFrames,
          });
        } else {
          document.l10n.setAttributes(element, "media-image-type", {
            type: imageType,
          });
        }
      } else {
        // the MIME type doesn't begin with image/, display the raw type
        element.setAttribute("value", mimeType);
        element.removeAttribute("data-l10n-id");
      }
    } else {
      // We couldn't find the type, fall back to the value in the treeview
      element.setAttribute("value", gImageView.data[row][COL_IMAGE_TYPE]);
      element.removeAttribute("data-l10n-id");
    }

    var imageContainer = document.getElementById("theimagecontainer");
    var oldImage = document.getElementById("thepreviewimage");

    var isProtocolAllowed = checkProtocol(gImageView.data[row]);

    var newImage = new Image();
    newImage.id = "thepreviewimage";
    var physWidth = 0,
      physHeight = 0;
    var width = 0,
      height = 0;

    let triggeringPrinStr = E10SUtils.serializePrincipal(gDocInfo.principal);
    if (
      (item.HTMLLinkElement ||
        item.HTMLInputElement ||
        item.HTMLImageElement ||
        item.SVGImageElement ||
        (item.HTMLObjectElement && mimeType && mimeType.startsWith("image/")) ||
        isBG) &&
      isProtocolAllowed
    ) {
      // We need to wait for the image to finish loading before using width & height
      newImage.addEventListener(
        "loadend",
        function() {
          physWidth = newImage.width || 0;
          physHeight = newImage.height || 0;

          // "width" and "height" attributes must be set to newImage,
          // even if there is no "width" or "height attribute in item;
          // otherwise, the preview image cannot be displayed correctly.
          // Since the image might have been loaded out-of-process, we expect
          // the item to tell us its width / height dimensions. Failing that
          // the item should tell us the natural dimensions of the image. Finally
          // failing that, we'll assume that the image was never loaded in the
          // other process (this can be true for favicons, for example), and so
          // we'll assume that we can use the natural dimensions of the newImage
          // we just created. If the natural dimensions of newImage are not known
          // then the image is probably broken.
          if (!isBG) {
            newImage.width =
              ("width" in item && item.width) || newImage.naturalWidth;
            newImage.height =
              ("height" in item && item.height) || newImage.naturalHeight;
          } else {
            // the Width and Height of an HTML tag should not be used for its background image
            // (for example, "table" can have "width" or "height" attributes)
            newImage.width = item.naturalWidth || newImage.naturalWidth;
            newImage.height = item.naturalHeight || newImage.naturalHeight;
          }

          if (item.SVGImageElement) {
            newImage.width = item.SVGImageElementWidth;
            newImage.height = item.SVGImageElementHeight;
          }

          width = newImage.width;
          height = newImage.height;

          document.getElementById("theimagecontainer").collapsed = false;
          document.getElementById("brokenimagecontainer").collapsed = true;

          if (url) {
            if (width != physWidth || height != physHeight) {
              document.l10n.setAttributes(
                document.getElementById("imagedimensiontext"),
                "media-dimensions-scaled",
                {
                  dimx: formatNumber(physWidth),
                  dimy: formatNumber(physHeight),
                  scaledx: formatNumber(width),
                  scaledy: formatNumber(height),
                }
              );
            } else {
              document.l10n.setAttributes(
                document.getElementById("imagedimensiontext"),
                "media-dimensions",
                { dimx: formatNumber(width), dimy: formatNumber(height) }
              );
            }
          }
        },
        { once: true }
      );

      newImage.setAttribute("triggeringprincipal", triggeringPrinStr);
      newImage.setAttribute("src", url);
    } else {
      // Handle the case where newImage is not used for width & height
      if (item.HTMLVideoElement && isProtocolAllowed) {
        newImage = document.createElement("video");
        newImage.id = "thepreviewimage";
        newImage.setAttribute("triggeringprincipal", triggeringPrinStr);
        newImage.src = url;
        newImage.controls = true;
        width = physWidth = item.videoWidth;
        height = physHeight = item.videoHeight;

        document.getElementById("theimagecontainer").collapsed = false;
        document.getElementById("brokenimagecontainer").collapsed = true;
      } else if (item.HTMLAudioElement && isProtocolAllowed) {
        newImage = new Audio();
        newImage.id = "thepreviewimage";
        newImage.setAttribute("triggeringprincipal", triggeringPrinStr);
        newImage.src = url;
        newImage.controls = true;
        isAudio = true;

        document.getElementById("theimagecontainer").collapsed = false;
        document.getElementById("brokenimagecontainer").collapsed = true;
      } else {
        // fallback image for protocols not allowed (e.g., javascript:)
        // or elements not [yet] handled (e.g., object, embed).
        document.getElementById("brokenimagecontainer").collapsed = false;
        document.getElementById("theimagecontainer").collapsed = true;
      }

      if (url && !isAudio) {
        document.l10n.setAttributes(
          document.getElementById("imagedimensiontext"),
          "media-dimensions",
          { dimx: formatNumber(width), dimy: formatNumber(height) }
        );
      }
    }

    imageContainer.removeChild(oldImage);
    imageContainer.appendChild(newImage);
  });
}

function getContentTypeFromHeaders(cacheEntryDescriptor) {
  if (!cacheEntryDescriptor) {
    return null;
  }

  let headers = cacheEntryDescriptor.getMetaDataElement("response-head");
  let type = /^Content-Type:\s*(.*?)\s*(?:\;|$)/im.exec(headers);
  return type && type[1];
}

function setItemValue(id, value) {
  var item = document.getElementById(id);
  item.closest("tr").hidden = !value;
  if (value) {
    item.value = value;
  }
}

function formatNumber(number) {
  return (+number).toLocaleString(); // coerce number to a numeric value before calling toLocaleString()
}

function formatDate(datestr, unknown) {
  var date = new Date(datestr);
  if (!date.valueOf()) {
    return unknown;
  }

  const dateTimeFormatter = new Services.intl.DateTimeFormat(undefined, {
    dateStyle: "long",
    timeStyle: "long",
  });
  return dateTimeFormatter.format(date);
}

function doCopy() {
  if (!gClipboardHelper) {
    return;
  }

  var elem = document.commandDispatcher.focusedElement;

  if (elem && elem.localName == "tree") {
    var view = elem.view;
    var selection = view.selection;
    var text = [],
      tmp = "";
    var min = {},
      max = {};

    var count = selection.getRangeCount();

    for (var i = 0; i < count; i++) {
      selection.getRangeAt(i, min, max);

      for (var row = min.value; row <= max.value; row++) {
        tmp = view.getCellValue(row, null);
        if (tmp) {
          text.push(tmp);
        }
      }
    }
    gClipboardHelper.copyString(text.join("\n"));
  }
}

function doSelectAllMedia() {
  var tree = document.getElementById("imagetree");

  if (tree) {
    tree.view.selection.selectAll();
  }
}

function doSelectAll() {
  var elem = document.commandDispatcher.focusedElement;

  if (elem && elem.localName == "tree") {
    elem.view.selection.selectAll();
  }
}

function selectImage() {
  if (!gImageElement) {
    return;
  }

  var tree = document.getElementById("imagetree");
  for (var i = 0; i < tree.view.rowCount; i++) {
    // If the image row element is the image selected from the "View Image Info" context menu item.
    let image = gImageView.data[i][COL_IMAGE_NODE];
    if (
      !gImageView.data[i][COL_IMAGE_BG] &&
      gImageElement.currentSrc == gImageView.data[i][COL_IMAGE_ADDRESS] &&
      gImageElement.width == image.width &&
      gImageElement.height == image.height &&
      gImageElement.imageText == image.imageText
    ) {
      tree.view.selection.select(i);
      tree.ensureRowIsVisible(i);
      tree.focus();
      return;
    }
  }
}

function checkProtocol(img) {
  var url = img[COL_IMAGE_ADDRESS];
  return (
    /^data:image\//i.test(url) ||
    /^(https?|ftp|file|about|chrome|resource):/.test(url)
  );
}
