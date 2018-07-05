/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");

/* import-globals-from ../../../../toolkit/content/globalOverlay.js */
/* import-globals-from ../../../../toolkit/content/contentAreaUtils.js */
/* import-globals-from ../../../../toolkit/content/treeUtils.js */
/* import-globals-from feeds.js */
/* import-globals-from permissions.js */
/* import-globals-from security.js */

// define a js object to implement nsITreeView
function pageInfoTreeView(treeid, copycol) {
  // copycol is the index number for the column that we want to add to
  // the copy-n-paste buffer when the user hits accel-c
  this.treeid = treeid;
  this.copycol = copycol;
  this.rows = 0;
  this.tree = null;
  this.data = [ ];
  this.selection = null;
  this.sortcol = -1;
  this.sortdir = false;
}

pageInfoTreeView.prototype = {
  set rowCount(c) { throw "rowCount is a readonly property"; },
  get rowCount() { return this.rows; },

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

  setCellValue(row, column, value) {
  },

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
    if (this.tree)
      this.tree.rowCountChanged(0, -this.rows);
    this.rows = 0;
    this.data = [ ];
  },

  handleCopy(row) {
    return (row < 0 || this.copycol < 0) ? "" : (this.data[row][this.copycol] || "");
  },

  performActionOnRow(action, row) {
    if (action == "copy") {
      var data = this.handleCopy(row);
      this.tree.treeBody.parentNode.setAttribute("copybuffer", data);
    }
  },

  onPageMediaSort(columnname) {
    var tree = document.getElementById(this.treeid);
    var treecol = tree.columns.getNamedColumn(columnname);

    this.sortdir =
      gTreeUtils.sort(
        tree,
        this,
        this.data,
        treecol.index,
        function textComparator(a, b) { return (a || "").toLowerCase().localeCompare((b || "").toLowerCase()); },
        this.sortcol,
        this.sortdir
      );

    Array.forEach(tree.columns, function(col) {
      col.element.removeAttribute("sortActive");
      col.element.removeAttribute("sortDirection");
    });
    treecol.element.setAttribute("sortActive", "true");
    treecol.element.setAttribute("sortDirection", this.sortdir ?
                                                  "ascending" : "descending");

    this.sortcol = treecol.index;
  },

  getRowProperties(row) { return ""; },
  getCellProperties(row, column) { return ""; },
  getColumnProperties(column) { return ""; },
  isContainer(index) { return false; },
  isContainerOpen(index) { return false; },
  isSeparator(index) { return false; },
  isSorted() { return this.sortcol > -1; },
  canDrop(index, orientation) { return false; },
  drop(row, orientation) { return false; },
  getParentIndex(index) { return 0; },
  hasNextSibling(index, after) { return false; },
  getLevel(index) { return 0; },
  getImageSrc(row, column) { },
  getCellValue(row, column) { },
  toggleOpenState(index) { },
  cycleHeader(col) { },
  selectionChanged() { },
  cycleCell(row, column) { },
  isEditable(row, column) { return false; },
  isSelectable(row, column) { return false; },
  performAction(action) { },
  performActionOnCell(action, row, column) { }
};

// mmm, yummy. global variables.
var gDocInfo = null;
var gImageElement = null;

// column number to help using the data array
const COL_IMAGE_ADDRESS = 0;
const COL_IMAGE_TYPE    = 1;
const COL_IMAGE_SIZE    = 2;
const COL_IMAGE_ALT     = 3;
const COL_IMAGE_COUNT   = 4;
const COL_IMAGE_NODE    = 5;
const COL_IMAGE_BG      = 6;

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
  if (!checkProtocol(data) ||
      item instanceof HTMLEmbedElement ||
      (item instanceof HTMLObjectElement && !item.type.startsWith("image/")))
    props += "broken";

  if (col.element.id == "image-address")
    props += " ltr";

  return props;
};

gImageView.getCellText = function(row, column) {
  var value = this.data[row][column.index];
  if (column.index == COL_IMAGE_SIZE) {
    if (value == -1) {
      return gStrings.unknown;
    }
    var kbSize = Number(Math.round(value / 1024 * 100) / 100);
    return gBundle.getFormattedString("mediaFileSize", [kbSize]);
  }
  return value || "";
};

gImageView.onPageMediaSort = function(columnname) {
  var tree = document.getElementById(this.treeid);
  var treecol = tree.columns.getNamedColumn(columnname);

  var comparator;
  var index = treecol.index;
  if (index == COL_IMAGE_SIZE || index == COL_IMAGE_COUNT) {
    comparator = function numComparator(a, b) { return a - b; };
  } else {
    comparator = function textComparator(a, b) { return (a || "").toLowerCase().localeCompare((b || "").toLowerCase()); };
  }

  this.sortdir =
    gTreeUtils.sort(
      tree,
      this,
      this.data,
      index,
      comparator,
      this.sortcol,
      this.sortdir
    );

  Array.forEach(tree.columns, function(col) {
    col.element.removeAttribute("sortActive");
    col.element.removeAttribute("sortDirection");
  });
  treecol.element.setAttribute("sortActive", "true");
  treecol.element.setAttribute("sortDirection", this.sortdir ?
                                                "ascending" : "descending");

  this.sortcol = index;
};

var gImageHash = { };

// localized strings (will be filled in when the document is loaded)
// this isn't all of them, these are just the ones that would otherwise have been loaded inside a loop
var gStrings = { };
var gBundle;

const PERMISSION_CONTRACTID     = "@mozilla.org/permissionmanager;1";
const PREFERENCES_CONTRACTID    = "@mozilla.org/preferences-service;1";

// a number of services I'll need later
// the cache services
const nsICacheStorageService = Ci.nsICacheStorageService;
const nsICacheStorage = Ci.nsICacheStorage;
const cacheService = Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(nsICacheStorageService);

var loadContextInfo = Services.loadContextInfo.fromLoadContext(
  window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsILoadContext), false);
var diskStorage = cacheService.diskCacheStorage(loadContextInfo, false);

const nsICookiePermission  = Ci.nsICookiePermission;
const nsIPermissionManager = Ci.nsIPermissionManager;

const nsICertificateDialogs = Ci.nsICertificateDialogs;
const CERTIFICATEDIALOGS_CONTRACTID = "@mozilla.org/nsCertificateDialogs;1";

// clipboard helper
function getClipboardHelper() {
    try {
        return Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    } catch (e) {
        // do nothing, later code will handle the error
        return null;
    }
}
const gClipboardHelper = getClipboardHelper();

// namespaces, don't need all of these yet...
const XLinkNS  = "http://www.w3.org/1999/xlink";
const XULNS    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XMLNS    = "http://www.w3.org/XML/1998/namespace";
const XHTMLNS  = "http://www.w3.org/1999/xhtml";
const XHTML2NS = "http://www.w3.org/2002/06/xhtml2";

const XHTMLNSre  = "^http\:\/\/www\.w3\.org\/1999\/xhtml$";
const XHTML2NSre = "^http\:\/\/www\.w3\.org\/2002\/06\/xhtml2$";
const XHTMLre = RegExp(XHTMLNSre + "|" + XHTML2NSre, "");

/* Overlays register functions here.
 * These arrays are used to hold callbacks that Page Info will call at
 * various stages. Use them by simply appending a function to them.
 * For example, add a function to onLoadRegistry by invoking
 *   "onLoadRegistry.push(XXXLoadFunc);"
 * The XXXLoadFunc should be unique to the overlay module, and will be
 * invoked as "XXXLoadFunc();"
 */

// These functions are called to build the data displayed in the Page Info window.
var onLoadRegistry = [ ];

// These functions are called to remove old data still displayed in
// the window when the document whose information is displayed
// changes. For example, at this time, the list of images of the Media
// tab is cleared.
var onResetRegistry = [ ];

// These functions are called once when all the elements in all of the target
// document (and all of its subframes, if any) have been processed
var onFinished = [ ];

// These functions are called once when the Page Info window is closed.
var onUnloadRegistry = [ ];

/* Called when PageInfo window is loaded.  Arguments are:
 *  window.arguments[0] - (optional) an object consisting of
 *                         - doc: (optional) document to use for source. if not provided,
 *                                the calling window's document will be used
 *                         - initialTab: (optional) id of the inital tab to display
 */
function onLoadPageInfo() {
  gBundle = document.getElementById("pageinfobundle");
  gStrings.unknown = gBundle.getString("unknown");
  gStrings.notSet = gBundle.getString("notset");
  gStrings.mediaImg = gBundle.getString("mediaImg");
  gStrings.mediaBGImg = gBundle.getString("mediaBGImg");
  gStrings.mediaBorderImg = gBundle.getString("mediaBorderImg");
  gStrings.mediaListImg = gBundle.getString("mediaListImg");
  gStrings.mediaCursor = gBundle.getString("mediaCursor");
  gStrings.mediaObject = gBundle.getString("mediaObject");
  gStrings.mediaEmbed = gBundle.getString("mediaEmbed");
  gStrings.mediaLink = gBundle.getString("mediaLink");
  gStrings.mediaInput = gBundle.getString("mediaInput");
  gStrings.mediaVideo = gBundle.getString("mediaVideo");
  gStrings.mediaAudio = gBundle.getString("mediaAudio");

  var args = "arguments" in window &&
             window.arguments.length >= 1 &&
             window.arguments[0];

  // init media view
  var imageTree = document.getElementById("imagetree");
  imageTree.view = gImageView;

  /* Select the requested tab, if the name is specified */
  loadTab(args);
  Services.obs.notifyObservers(window, "page-info-dialog-loaded");
}

function loadPageInfo(frameOuterWindowID, imageElement, browser) {
  browser = browser || window.opener.gBrowser.selectedBrowser;
  let mm = browser.messageManager;

  gStrings["application/rss+xml"]  = gBundle.getString("feedRss");
  gStrings["application/atom+xml"] = gBundle.getString("feedAtom");
  gStrings["text/xml"]             = gBundle.getString("feedXML");
  gStrings["application/xml"]      = gBundle.getString("feedXML");
  gStrings["application/rdf+xml"]  = gBundle.getString("feedXML");

  let imageInfo = imageElement;

  // Look for pageInfoListener in content.js. Sends message to listener with arguments.
  mm.sendAsyncMessage("PageInfo:getData", {strings: gStrings, frameOuterWindowID});

  let pageInfoData;

  // Get initial pageInfoData needed to display the general, feeds, permission and security tabs.
  mm.addMessageListener("PageInfo:data", function onmessage(message) {
    mm.removeMessageListener("PageInfo:data", onmessage);
    pageInfoData = message.data;
    let docInfo = pageInfoData.docInfo;
    let windowInfo = pageInfoData.windowInfo;
    let uri = makeURI(docInfo.documentURIObject.spec);
    let principal = docInfo.principal;
    gDocInfo = docInfo;

    gImageElement = imageInfo;

    var titleFormat = windowInfo.isTopWindow ? "pageInfo.page.title"
                                             : "pageInfo.frame.title";
    document.title = gBundle.getFormattedString(titleFormat, [docInfo.location]);

    document.getElementById("main-window").setAttribute("relatedUrl", docInfo.location);

    makeGeneralTab(pageInfoData.metaViewRows, docInfo);
    initFeedTab(pageInfoData.feeds);
    onLoadPermission(uri, principal);
    securityOnLoad(uri, windowInfo);
  });

  // Get the media elements from content script to setup the media tab.
  mm.addMessageListener("PageInfo:mediaData", function onmessage(message) {
    // Page info window was closed.
    if (window.closed) {
      mm.removeMessageListener("PageInfo:mediaData", onmessage);
      return;
    }

    // The page info media fetching has been completed.
    if (message.data.isComplete) {
      mm.removeMessageListener("PageInfo:mediaData", onmessage);
      onFinished.forEach(function(func) { func(pageInfoData); });
      return;
    }

    for (let item of message.data.mediaItems) {
      addImage(item);
    }

    selectImage();
  });

  /* Call registered overlay init functions */
  onLoadRegistry.forEach(function(func) { func(); });
}

function resetPageInfo(args) {
  /* Reset Meta tags part */
  gMetaView.clear();

  /* Reset Media tab */
  var mediaTab = document.getElementById("mediaTab");
  if (!mediaTab.hidden) {
    Services.obs.removeObserver(imagePermissionObserver, "perm-changed");
    mediaTab.hidden = true;
  }
  gImageView.clear();
  gImageHash = {};

  /* Reset Feeds Tab */
  var feedListbox = document.getElementById("feedListbox");
  while (feedListbox.firstChild)
    feedListbox.firstChild.remove();

  /* Call registered overlay reset functions */
  onResetRegistry.forEach(function(func) { func(); });

  /* Rebuild the data */
  loadTab(args);
}

function onUnloadPageInfo() {
  // Remove the observer, only if there is at least 1 image.
  if (!document.getElementById("mediaTab").hidden) {
    Services.obs.removeObserver(imagePermissionObserver, "perm-changed");
  }

  /* Call registered overlay unload functions */
  onUnloadRegistry.forEach(function(func) { func(); });
}

function doHelpButton() {
  const helpTopics = {
    "generalPanel":  "pageinfo_general",
    "mediaPanel":    "pageinfo_media",
    "feedPanel":     "pageinfo_feed",
    "permPanel":     "pageinfo_permissions",
    "securityPanel": "pageinfo_security"
  };

  var deck  = document.getElementById("mainDeck");
  var helpdoc = helpTopics[deck.selectedPanel.id] || "pageinfo_general";
  openHelpLink(helpdoc);
}

function showTab(id) {
  var deck  = document.getElementById("mainDeck");
  var pagel = document.getElementById(id + "Panel");
  deck.selectedPanel = pagel;
}

function loadTab(args) {
  // If the "View Image Info" context menu item was used, the related image
  // element is provided as an argument. This can't be a background image.
  let imageElement = args && args.imageElement;
  let frameOuterWindowID = args && args.frameOuterWindowID;
  let browser = args && args.browser;

  /* Load the page info */
  loadPageInfo(frameOuterWindowID, imageElement, browser);

  var initialTab = (args && args.initialTab) || "generalTab";
  var radioGroup = document.getElementById("viewGroup");
  initialTab = document.getElementById(initialTab) || document.getElementById("generalTab");
  radioGroup.selectedItem = initialTab;
  radioGroup.selectedItem.doCommand();
  radioGroup.focus();
}

function openCacheEntry(key, cb) {
  var checkCacheListener = {
    onCacheEntryCheck(entry, appCache) {
      return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
    },
    onCacheEntryAvailable(entry, isNew, appCache, status) {
      cb(entry);
    }
  };
  diskStorage.asyncOpenURI(Services.io.newURI(key), "", nsICacheStorage.OPEN_READONLY, checkCacheListener);
}

function makeGeneralTab(metaViewRows, docInfo) {
  var title = (docInfo.title) ? docInfo.title : gBundle.getString("noPageTitle");
  document.getElementById("titletext").value = title;

  var url = docInfo.location;
  setItemValue("urltext", url);

  var referrer = ("referrer" in docInfo && docInfo.referrer);
  setItemValue("refertext", referrer);

  var mode = ("compatMode" in docInfo && docInfo.compatMode == "BackCompat") ? "generalQuirksMode" : "generalStrictMode";
  document.getElementById("modetext").value = gBundle.getString(mode);

  // find out the mime type
  var mimeType = docInfo.contentType;
  setItemValue("typetext", mimeType);

  // get the document characterset
  var encoding = docInfo.characterSet;
  document.getElementById("encodingtext").value = encoding;

  let length = metaViewRows.length;

  var metaGroup = document.getElementById("metaTags");
  if (!length)
    metaGroup.style.visibility = "hidden";
  else {
    var metaTagsCaption = document.getElementById("metaTagsCaption");
    if (length == 1)
      metaTagsCaption.value = gBundle.getString("generalMetaTag");
    else
      metaTagsCaption.value = gBundle.getFormattedString("generalMetaTags", [length]);
    var metaTree = document.getElementById("metatree");
    metaTree.view = gMetaView;

    // Add the metaViewRows onto the general tab's meta info tree.
    gMetaView.addRows(metaViewRows);

    metaGroup.style.removeProperty("visibility");
  }

  // get the date of last modification
  var modifiedText = formatDate(docInfo.lastModified, gStrings.notSet);
  document.getElementById("modifiedtext").value = modifiedText;

  // get cache info
  var cacheKey = url.replace(/#.*$/, "");
  openCacheEntry(cacheKey, function(cacheEntry) {
    var sizeText;
    if (cacheEntry) {
      var pageSize = cacheEntry.dataSize;
      var kbSize = formatNumber(Math.round(pageSize / 1024 * 100) / 100);
      sizeText = gBundle.getFormattedString("generalSize", [kbSize, formatNumber(pageSize)]);
    }
    setItemValue("sizetext", sizeText);
  });
}

function addImage(imageViewRow) {
  let [url, type, alt, elem, isBg] = imageViewRow;

  if (!url)
    return;

  if (!gImageHash.hasOwnProperty(url))
    gImageHash[url] = { };
  if (!gImageHash[url].hasOwnProperty(type))
    gImageHash[url][type] = { };
  if (!gImageHash[url][type].hasOwnProperty(alt)) {
    gImageHash[url][type][alt] = gImageView.data.length;
    var row = [url, type, -1, alt, 1, elem, isBg];
    gImageView.addRow(row);

    // Fill in cache data asynchronously
    openCacheEntry(url, function(cacheEntry) {
      // The data at row[2] corresponds to the data size.
      if (cacheEntry) {
        row[2] = cacheEntry.dataSize;
        // Invalidate the row to trigger a repaint.
        gImageView.tree.invalidateRow(gImageView.data.indexOf(row));
      }
    });

    // Add the observer, only once.
    if (gImageView.data.length == 1) {
      document.getElementById("mediaTab").hidden = false;
      Services.obs.addObserver(imagePermissionObserver, "perm-changed");
    }
  } else {
    var i = gImageHash[url][type][alt];
    gImageView.data[i][COL_IMAGE_COUNT]++;
    // The same image can occur several times on the page at different sizes.
    // If the "View Image Info" context menu item was used, ensure we select
    // the correct element.
    if (!gImageView.data[i][COL_IMAGE_BG] &&
        gImageElement && url == gImageElement.currentSrc &&
        gImageElement.width == elem.width &&
        gImageElement.height == elem.height &&
        gImageElement.imageText == elem.imageText) {
      gImageView.data[i][COL_IMAGE_NODE] = elem;
    }
  }
}

// Link Stuff
function openURL(target) {
  var url = target.parentNode.childNodes[2].value;
  window.open(url, "_blank", "chrome");
}

function onBeginLinkDrag(event, urlField, descField) {
  if (event.originalTarget.localName != "treechildren")
    return;

  var tree = event.target;
  if (!("treeBoxObject" in tree))
    tree = tree.parentNode;

  var row = tree.treeBoxObject.getRowAt(event.clientX, event.clientY);
  if (row == -1)
    return;

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
  var start = { };
  var end   = { };
  var numRanges = tree.view.selection.getRangeCount();

  var rowArray = [ ];
  for (var t = 0; t < numRanges; t++) {
    tree.view.selection.getRangeAt(t, start, end);
    for (var v = start.value; v <= end.value; v++)
      rowArray.push(v);
  }

  return rowArray;
}

function getSelectedRow(tree) {
  var rows = getSelectedRows(tree);
  return (rows.length == 1) ? rows[0] : -1;
}

function selectSaveFolder(aCallback) {
  const nsIFile = Ci.nsIFile;
  const nsIFilePicker = Ci.nsIFilePicker;
  let titleText = gBundle.getString("mediaSelectFolder");
  let fp = Cc["@mozilla.org/filepicker;1"].
           createInstance(nsIFilePicker);
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
    let initialDir = Services.prefs.getComplexValue("browser.download.dir", nsIFile);
    if (initialDir) {
      fp.displayDirectory = initialDir;
    }
  } catch (ex) {
  }
  fp.open(fpCallback);
}

function saveMedia() {
  var tree = document.getElementById("imagetree");
  var rowArray = getSelectedRows(tree);
  if (rowArray.length == 1) {
    let row = rowArray[0];
    let item = gImageView.data[row][COL_IMAGE_NODE];
    let url = gImageView.data[row][COL_IMAGE_ADDRESS];

    if (url) {
      var titleKey = "SaveImageTitle";

      if (item instanceof HTMLVideoElement)
        titleKey = "SaveVideoTitle";
      else if (item instanceof HTMLAudioElement)
        titleKey = "SaveAudioTitle";

      saveURL(url, null, titleKey, false, false, makeURI(item.baseURI),
              null, gDocInfo.isContentWindowPrivate, gDocInfo.principal);
    }
  } else {
    selectSaveFolder(function(aDirectory) {
      if (aDirectory) {
        var saveAnImage = function(aURIString, aChosenData, aBaseURI) {
          uniqueFile(aChosenData.file);
          internalSave(aURIString, null, null, null, null, false, "SaveImageTitle",
                       aChosenData, aBaseURI, null, false, null,
                       gDocInfo.isContentWindowPrivate, gDocInfo.principal);
        };

        for (var i = 0; i < rowArray.length; i++) {
          let v = rowArray[i];
          let dir = aDirectory.clone();
          let item = gImageView.data[v][COL_IMAGE_NODE];
          let uriString = gImageView.data[v][COL_IMAGE_ADDRESS];
          let uri = makeURI(uriString);

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
            saveAnImage(uriString, new AutoChosen(dir, uri), makeURI(item.baseURI));
          } else {
            // This delay is a hack which prevents the download manager
            // from opening many times. See bug 377339.
            setTimeout(saveAnImage, 200, uriString, new AutoChosen(dir, uri),
                       makeURI(item.baseURI));
          }
        }
      }
    });
  }
}

function onBlockImage() {
  var permissionManager = Cc[PERMISSION_CONTRACTID]
                            .getService(nsIPermissionManager);

  var checkbox = document.getElementById("blockImage");
  var uri = makeURI(document.getElementById("imageurltext").value);
  if (checkbox.checked)
    permissionManager.add(uri, "image", nsIPermissionManager.DENY_ACTION);
  else
    permissionManager.remove(uri, "image");
}

function onImageSelect() {
  var previewBox   = document.getElementById("mediaPreviewBox");
  var mediaSaveBox = document.getElementById("mediaSaveBox");
  var splitter     = document.getElementById("mediaSplitter");
  var tree = document.getElementById("imagetree");
  var count = tree.view.selection.count;
  if (count == 0) {
    previewBox.collapsed   = true;
    mediaSaveBox.collapsed = true;
    splitter.collapsed     = true;
    tree.flex = 1;
  } else if (count > 1) {
    splitter.collapsed     = true;
    previewBox.collapsed   = true;
    mediaSaveBox.collapsed = false;
    tree.flex = 1;
  } else {
    mediaSaveBox.collapsed = true;
    splitter.collapsed     = false;
    previewBox.collapsed   = false;
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
    var sizeText;
    if (cacheEntry) {
      let imageSize = cacheEntry.dataSize;
      var kbSize = Math.round(imageSize / 1024 * 100) / 100;
      sizeText = gBundle.getFormattedString("generalSize",
                                            [formatNumber(kbSize), formatNumber(imageSize)]);
    } else
      sizeText = gBundle.getString("mediaUnknownNotCached");
    setItemValue("imagesizetext", sizeText);

    var mimeType = item.mimeType || this.getContentTypeFromHeaders(cacheEntry);
    var numFrames = item.numFrames;

    var imageType;
    if (mimeType) {
      // We found the type, try to display it nicely
      let imageMimeType = /^image\/(.*)/i.exec(mimeType);
      if (imageMimeType) {
        imageType = imageMimeType[1].toUpperCase();
        if (numFrames > 1)
          imageType = gBundle.getFormattedString("mediaAnimatedImageType",
                                                 [imageType, numFrames]);
        else
          imageType = gBundle.getFormattedString("mediaImageType", [imageType]);
      } else {
        // the MIME type doesn't begin with image/, display the raw type
        imageType = mimeType;
      }
    } else {
      // We couldn't find the type, fall back to the value in the treeview
      imageType = gImageView.data[row][COL_IMAGE_TYPE];
    }
    setItemValue("imagetypetext", imageType);

    var imageContainer = document.getElementById("theimagecontainer");
    var oldImage = document.getElementById("thepreviewimage");

    var isProtocolAllowed = checkProtocol(gImageView.data[row]);

    var newImage = new Image;
    newImage.id = "thepreviewimage";
    var physWidth = 0, physHeight = 0;
    var width = 0, height = 0;

    let serial = Cc["@mozilla.org/network/serialization-helper;1"]
                   .getService(Ci.nsISerializationHelper);
    let triggeringPrinStr = serial.serializeToString(gDocInfo.principal);
    if ((item.HTMLLinkElement || item.HTMLInputElement ||
         item.HTMLImageElement || item.SVGImageElement ||
         (item.HTMLObjectElement && mimeType && mimeType.startsWith("image/")) ||
         isBG) && isProtocolAllowed) {
      // We need to wait for the image to finish loading before using width & height
      newImage.addEventListener("loadend", function() {
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
          newImage.width = ("width" in item && item.width) || newImage.naturalWidth;
          newImage.height = ("height" in item && item.height) || newImage.naturalHeight;
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

        let imageSize = "";
        if (url) {
          if (width != physWidth || height != physHeight) {
            imageSize = gBundle.getFormattedString("mediaDimensionsScaled",
                                                   [formatNumber(physWidth),
                                                    formatNumber(physHeight),
                                                    formatNumber(width),
                                                    formatNumber(height)]);
          } else {
            imageSize = gBundle.getFormattedString("mediaDimensions",
                                                   [formatNumber(width),
                                                    formatNumber(height)]);
          }
        }
        setItemValue("imagedimensiontext", imageSize);
      }, {once: true});

      newImage.setAttribute("triggeringprincipal", triggeringPrinStr);
      newImage.setAttribute("src", url);
    } else {
      // Handle the case where newImage is not used for width & height
      if (item.HTMLVideoElement && isProtocolAllowed) {
        newImage = document.createElementNS("http://www.w3.org/1999/xhtml", "video");
        newImage.id = "thepreviewimage";
        newImage.setAttribute("triggeringprincipal", triggeringPrinStr);
        newImage.src = url;
        newImage.controls = true;
        width = physWidth = item.videoWidth;
        height = physHeight = item.videoHeight;

        document.getElementById("theimagecontainer").collapsed = false;
        document.getElementById("brokenimagecontainer").collapsed = true;
      } else if (item.HTMLAudioElement && isProtocolAllowed) {
        newImage = new Audio;
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

      let imageSize = "";
      if (url && !isAudio) {
        imageSize = gBundle.getFormattedString("mediaDimensions",
                                               [formatNumber(width),
                                                formatNumber(height)]);
      }
      setItemValue("imagedimensiontext", imageSize);
    }

    makeBlockImage(url);

    imageContainer.removeChild(oldImage);
    imageContainer.appendChild(newImage);
  });
}

function makeBlockImage(url) {
  var permissionManager = Cc[PERMISSION_CONTRACTID]
                            .getService(nsIPermissionManager);

  var checkbox = document.getElementById("blockImage");
  var imagePref = Services.prefs.getIntPref("permissions.default.image");
  if (!(/^https?:/.test(url)) || imagePref == 2)
    // We can't block the images from this host because either is is not
    // for http(s) or we don't load images at all
    checkbox.hidden = true;
  else {
    var uri = makeURI(url);
    if (uri.host) {
      checkbox.hidden = false;
      checkbox.label = gBundle.getFormattedString("mediaBlockImage", [uri.host]);
      var perm = permissionManager.testPermission(uri, "image");
      checkbox.checked = perm == nsIPermissionManager.DENY_ACTION;
    } else
      checkbox.hidden = true;
  }
}

var imagePermissionObserver = {
  observe(aSubject, aTopic, aData) {
    if (document.getElementById("mediaPreviewBox").collapsed)
      return;

    if (aTopic == "perm-changed") {
      var permission = aSubject.QueryInterface(Ci.nsIPermission);
      if (permission.type == "image") {
        var imageTree = document.getElementById("imagetree");
        var row = getSelectedRow(imageTree);
        var url = gImageView.data[row][COL_IMAGE_ADDRESS];
        if (permission.matchesURI(makeURI(url), true)) {
          makeBlockImage(url);
        }
      }
    }
  }
};

function getContentTypeFromHeaders(cacheEntryDescriptor) {
  if (!cacheEntryDescriptor)
    return null;

  let headers = cacheEntryDescriptor.getMetaDataElement("response-head");
  let type = /^Content-Type:\s*(.*?)\s*(?:\;|$)/mi.exec(headers);
  return type && type[1];
}

function setItemValue(id, value) {
  var item = document.getElementById(id);
  if (value) {
    item.parentNode.collapsed = false;
    item.value = value;
  } else
    item.parentNode.collapsed = true;
}

function formatNumber(number) {
  return (+number).toLocaleString(); // coerce number to a numeric value before calling toLocaleString()
}

function formatDate(datestr, unknown) {
  var date = new Date(datestr);
  if (!date.valueOf())
    return unknown;

  const dateTimeFormatter = new Services.intl.DateTimeFormat(undefined, {
    dateStyle: "long", timeStyle: "long"
  });
  return dateTimeFormatter.format(date);
}

function doCopy() {
  if (!gClipboardHelper)
    return;

  var elem = document.commandDispatcher.focusedElement;

  if (elem && "treeBoxObject" in elem) {
    var view = elem.view;
    var selection = view.selection;
    var text = [], tmp = "";
    var min = {}, max = {};

    var count = selection.getRangeCount();

    for (var i = 0; i < count; i++) {
      selection.getRangeAt(i, min, max);

      for (var row = min.value; row <= max.value; row++) {
        view.performActionOnRow("copy", row);

        tmp = elem.getAttribute("copybuffer");
        if (tmp)
          text.push(tmp);
        elem.removeAttribute("copybuffer");
      }
    }
    gClipboardHelper.copyString(text.join("\n"));
  }
}

function doSelectAllMedia() {
  var tree = document.getElementById("imagetree");

  if (tree)
    tree.view.selection.selectAll();
}

function doSelectAll() {
  var elem = document.commandDispatcher.focusedElement;

  if (elem && "treeBoxObject" in elem)
    elem.view.selection.selectAll();
}

function selectImage() {
  if (!gImageElement)
    return;

  var tree = document.getElementById("imagetree");
  for (var i = 0; i < tree.view.rowCount; i++) {
    // If the image row element is the image selected from the "View Image Info" context menu item.
    let image = gImageView.data[i][COL_IMAGE_NODE];
    if (!gImageView.data[i][COL_IMAGE_BG] &&
        gImageElement.currentSrc == gImageView.data[i][COL_IMAGE_ADDRESS] &&
        gImageElement.width == image.width &&
        gImageElement.height == image.height &&
        gImageElement.imageText == image.imageText) {
      tree.view.selection.select(i);
      tree.treeBoxObject.ensureRowIsVisible(i);
      tree.focus();
      return;
    }
  }
}

function checkProtocol(img) {
  var url = img[COL_IMAGE_ADDRESS];
  return /^data:image\//i.test(url) ||
    /^(https?|ftp|file|about|chrome|resource):/.test(url);
}
