/**
 * Determine whether or not a given focused DOMWindow is in the content
 * area.
 **/
 
function openNewTabWith(href, linkNode, event, securityCheck, postData)
{
  if (securityCheck)
    urlSecurityCheck(href, document); 

  var prefSvc = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefService);
  prefSvc = prefSvc.getBranch(null);

  // should we open it in a new tab?
  var loadInBackground = true;
  try {
    loadInBackground = prefSvc.getBoolPref("browser.tabs.loadInBackground");
  }
  catch(ex) {
  }

  if (event && event.shiftKey)
    loadInBackground = !loadInBackground;

  // As in openNewWindowWith(), we want to pass the charset of the
  // current document over to a new tab. 
  var wintype = document.firstChild.getAttribute('windowtype');
  var originCharset;
  if (wintype == "navigator:browser") {
    originCharset = window._content.document.characterSet;
  }

  // open link in new tab
  var browser = top.document.getElementById("content");  
  var theTab = browser.addTab(href, getReferrer(document), originCharset, postData);
  if (!loadInBackground)
    browser.selectedTab = theTab;
  
  if (linkNode)
    markLinkVisited(href, linkNode);
}

function openNewWindowWith(href, linkNode, securityCheck, postData) 
{
  if (securityCheck)
    urlSecurityCheck(href, document);

  // if and only if the current window is a browser window and it has a document with a character
  // set, then extract the current charset menu setting from the current document and use it to
  // initialize the new browser window...
  var charsetArg = null;
  var wintype = document.firstChild.getAttribute('windowtype');
  if (wintype == "navigator:browser")
    charsetArg = "charset=" + window._content.document.characterSet;

  var referrer = getReferrer(document);
  window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", href, charsetArg, referrer, postData);
  
  if (linkNode)
    markLinkVisited(href, linkNode);
}

function markLinkVisited(href, linkNode)
{
  var globalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                                .getService(Components.interfaces.nsIGlobalHistory2);

  var uri = makeURL(href);
  if (!globalHistory.isVisited(uri)) {
    globalHistory.addURI(uri, false, true);
    var oldHref = linkNode.getAttribute("href");
    if (typeof oldHref == "string") {
      // Use setAttribute instead of direct assignment.
      // (bug 217195, bug 187195)
      linkNode.setAttribute("href", "");
      linkNode.setAttribute("href", oldHref);
    }
    else {
      // Converting to string implicitly would be a 
      // minor security hole (similar to bug 202994).
    }
  }
}

function urlSecurityCheck(url, doc) 
{
  // URL Loading Security Check
  var focusedWindow = doc.commandDispatcher.focusedWindow;
  var sourceURL = getContentFrameURI(focusedWindow);
  const nsIScriptSecurityManager = Components.interfaces.nsIScriptSecurityManager;
  var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(nsIScriptSecurityManager);
  try {
    secMan.checkLoadURIStr(sourceURL, url, nsIScriptSecurityManager.STANDARD);
  } catch (e) {
    throw "Load of " + url + " denied.";
  }
}

function isContentFrame(aFocusedWindow)
{
  if (!aFocusedWindow)
    return false;

  var focusedTop = Components.lookupMethod(aFocusedWindow, 'top')
                             .call(aFocusedWindow);

  return (focusedTop == window.content);
}

function getContentFrameURI(aFocusedWindow)
{
  var contentFrame = isContentFrame(aFocusedWindow) ? aFocusedWindow : window.content;
  if (contentFrame)
    return Components.lookupMethod(contentFrame, 'location').call(contentFrame).href;
  else
    return null;
}

function getReferrer(doc)
{
  var focusedWindow = doc.commandDispatcher.focusedWindow;
  var sourceURL = getContentFrameURI(focusedWindow);

  if (sourceURL) {
    try {
      return makeURL(sourceURL);
    }
    catch (e) { }
  }
  return null;
}

const kSaveAsType_Complete = 0;   // Save document with attached objects
const kSaveAsType_URL = 1;        // Save document or URL by itself
const kSaveAsType_Text = 2;       // Save document, converting to plain text. 

// Clientelle: (Make sure you don't break any of these)
//  - File    ->  Save Page/Frame As...
//  - Context ->  Save Page/Frame As...
//  - Context ->  Save Link As...
//  - Context ->  Save Image As...
//  - Alt-Click links in web pages
//  - Alt-Click links in the UI
//
// Try saving each of these types:
// - A complete webpage using File->Save Page As, and Context->Save Page As
// - A webpage as HTML only using the above methods
// - A webpage as Text only using the above methods
// - An image with an extension (e.g. .jpg) in its file name, using
//   Context->Save Image As...
// - An image without an extension (e.g. a banner ad on cnn.com) using
//   the above method. 
// - A linked document using Save Link As...
// - A linked document using Alt-click Save Link As...
//
function saveURL(aURL, aFileName, aFilePickerTitleKey, aShouldBypassCache, aSkipPrompt)
{
  saveInternal(aURL, null, aFileName, aFilePickerTitleKey, aShouldBypassCache, aSkipPrompt);
}

function saveDocument(aDocument, aSkipPrompt)
{
  // In both cases here, we want to use cached data because the 
  // document is currently visible. 
  if (aDocument) 
    saveInternal(aDocument.location.href, aDocument, false, aSkipPrompt);
  else
    saveInternal(_content.location.href, null, false, aSkipPrompt);
}

function saveInternal(aURL, aDocument, 
                      aFileName, aFilePickerTitleKey,
                      aShouldBypassCache, aSkipPrompt)
{
  if (aSkipPrompt == undefined)
    aSkipPrompt = false;

  var data = {
    url: aURL,
    fileName: aFileName,
    filePickerTitle: aFilePickerTitleKey,
    document: aDocument,
    bypassCache: aShouldBypassCache,
    window: window
  };
  var sniffer = new nsHeaderSniffer(aURL, foundHeaderInfo, data, aSkipPrompt);
}

function foundHeaderInfo(aSniffer, aData, aSkipPrompt)
{
  var contentType = aSniffer.contentType;
  var contentEncodingType = aSniffer.contentEncodingType;

  var shouldDecode = false;
  // Are we allowed to decode?
  try {
    const helperAppService =
      Components.classes["@mozilla.org/uriloader/external-helper-app-service;1"].
        getService(Components.interfaces.nsIExternalHelperAppService);
    var url = aSniffer.uri.QueryInterface(Components.interfaces.nsIURL);
    var urlExt = url.fileExtension;
    if (helperAppService.applyDecodingForExtension(urlExt,
                                                   contentEncodingType)) {
      shouldDecode = true;
    }
  }
  catch (e) {
  }

  var isDocument = aData.document != null && isDocumentType(contentType);
  if (!isDocument && !shouldDecode && contentEncodingType) {
    // The data is encoded, we are not going to decode it, and this is not a
    // document save so we won't be doing a "save as, complete" (which would
    // break if we reset the type here).  So just set our content type to
    // correspond to the outermost encoding so we get extensions and the like
    // right.
    contentType = contentEncodingType;
  }
  
  var file = null;
  var saveAsType = kSaveAsType_URL;
  try {
    file = aData.fileName.QueryInterface(Components.interfaces.nsILocalFile);
  }
  catch (e) {
    var saveAsTypeResult = { rv: 0 };
    file = getTargetFile(aData, aSniffer, contentType, isDocument, aSkipPrompt, saveAsTypeResult);
    if (!file)
      return;
    saveAsType = saveAsTypeResult.rv;
  }

  // If we're saving a document, and are saving either in complete mode or 
  // as converted text, pass the document to the web browser persist component.
  // If we're just saving the HTML (second option in the list), send only the URI.
  var source = (isDocument && saveAsType != kSaveAsType_URL) ? aData.document : aSniffer.uri;
  var persistArgs = {
    source      : source,
    contentType : (isDocument && saveAsType == kSaveAsType_Text) ? "text/plain" : contentType,
    target      : makeFileURL(file),
    postData    : aData.document ? getPostData() : null,
    bypassCache : aData.bypassCache
  };
  
  var persist = makeWebBrowserPersist();

  // Calculate persist flags.
  const nsIWBP = Components.interfaces.nsIWebBrowserPersist;
  const flags = nsIWBP.PERSIST_FLAGS_NO_CONVERSION | nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES;
  if (aData.bypassCache)
    persist.persistFlags = flags | nsIWBP.PERSIST_FLAGS_BYPASS_CACHE;
  else 
    persist.persistFlags = flags | nsIWBP.PERSIST_FLAGS_FROM_CACHE;

  if (shouldDecode)
    persist.persistFlags &= ~nsIWBP.PERSIST_FLAGS_NO_CONVERSION;
    
  // Create download and initiate it (below)
  var dl = Components.classes["@mozilla.org/download;1"].createInstance(Components.interfaces.nsIDownload);

  if (isDocument && saveAsType != kSaveAsType_URL) {
    // Saving a Document, not a URI:
    var filesFolder = null;
    if (persistArgs.contentType != "text/plain") {
      // Create the local directory into which to save associated files. 
      filesFolder = file.clone();
      
      var nameWithoutExtension = filesFolder.leafName;
      nameWithoutExtension = nameWithoutExtension.substring(0, nameWithoutExtension.lastIndexOf("."));
      var filesFolderLeafName = getStringBundle().formatStringFromName("filesFolder",
                                                                       [nameWithoutExtension],
                                                                       1);

      filesFolder.leafName = filesFolderLeafName;
    }
      
    var encodingFlags = 0;
    if (persistArgs.contentType == "text/plain") {
      encodingFlags |= nsIWBP.ENCODE_FLAGS_FORMATTED;
      encodingFlags |= nsIWBP.ENCODE_FLAGS_ABSOLUTE_LINKS;
      encodingFlags |= nsIWBP.ENCODE_FLAGS_NOFRAMES_CONTENT;        
    }
    
    const kWrapColumn = 80;
    dl.init(aSniffer.uri, persistArgs.target, null, null, null, persist);
    persist.saveDocument(persistArgs.source, persistArgs.target, filesFolder, 
                         persistArgs.contentType, encodingFlags, kWrapColumn);
  } else {
    dl.init(source, persistArgs.target, null, null, null, persist);
    persist.saveURI(source, null, getReferrer(document), persistArgs.postData, null, persistArgs.target);
  }
}

function getTargetFile(aData, aSniffer, aContentType, aIsDocument, aSkipPrompt, aSaveAsTypeResult)
{
  aSaveAsTypeResult.rv = kSaveAsType_Complete;
  
  // Determine what the 'default' string to display in the File Picker dialog 
  // should be. 
  var defaultFileName = getDefaultFileName(aData.fileName, 
                                           aSniffer.suggestedFileName, 
                                           aSniffer.uri,
                                           aData.document);

  var defaultExtension = getDefaultExtension(defaultFileName, aSniffer.uri, aContentType);
  var defaultString = getNormalizedLeafName(defaultFileName, defaultExtension);

  const prefSvcContractID = "@mozilla.org/preferences-service;1";
  const prefSvcIID = Components.interfaces.nsIPrefService;                              
  var prefs = Components.classes[prefSvcContractID].getService(prefSvcIID).getBranch("browser.download.");

  const nsILocalFile = Components.interfaces.nsILocalFile;

  // ben 07/31/2003:
  // |browser.download.defaultFolder| holds the default download folder for 
  // all files when the user has elected to have all files automatically
  // download to a folder. The values of |defaultFolder| can be either their
  // desktop, their downloads folder (My Documents\My Downloads) or some other
  // location of their choosing (which is mapped to |browser.download.dir|
  // This pref is _unset_ when the user has elected to be asked about where
  // to place every download - this will force the prompt to ask the user
  // where to put saved files. 
  var dir = null;
  try {
    dir = prefs.getComplexValue("defaultFolder", nsILocalFile);
  }
  catch (e) { }
  
  var file;
  if (!aSkipPrompt || !dir) {
    // If we're asking the user where to save the file, root the Save As...
    // dialog on they place they last picked. 
    try {
      dir = prefs.getComplexValue("lastDir", nsILocalFile);
    }
    catch (e) {
      // No default download location. Default to desktop. 
      var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIProperties);

      function getDesktopKey()
      {      
#ifdef XP_WIN
        return "DeskV";
#endif
#ifdef XP_MACOSX
        return "UsrDsk";
#endif
#ifdef XP_OS2
        return "Desk";
#endif
        return "Home";
      }
      
      dir = fileLocator.get(getDesktopKey(), Components.interfaces.nsILocalFile);
    }


    var fp = makeFilePicker();
    var titleKey = aData.filePickerTitle || "SaveLinkTitle";
    var bundle = getStringBundle();
    fp.init(window, bundle.GetStringFromName(titleKey), 
            Components.interfaces.nsIFilePicker.modeSave);
    
    var urlExt = null;
    try {
      var url = aSniffer.uri.QueryInterface(Components.interfaces.nsIURL);
      urlExt = url.fileExtension;
    }
    catch (e) {
    }
    appendFiltersForContentType(fp, aContentType, urlExt,
                                aIsDocument ? MODE_COMPLETE : MODE_FILEONLY);  
  
    if (dir)
      fp.displayDirectory = dir;
    
    if (aIsDocument) {
      try {
        fp.filterIndex = prefs.getIntPref("save_converter_index");
      }
      catch (e) {
      }
    }
  
    fp.defaultExtension = defaultExtension;
    fp.defaultString = defaultString;
  
    if (fp.show() == Components.interfaces.nsIFilePicker.returnCancel || !fp.file)
      return null;
  
    var useDownloadDir = false;
    try {
      useDownloadDir = prefs.getBoolPref("useDownloadDir");
    }
    catch(ex) {
    }
    
    var directory = fp.file.parent.QueryInterface(nsILocalFile);
    prefs.setComplexValue("lastDir", nsILocalFile, directory);

    fp.file.leafName = validateFileName(fp.file.leafName);
    aSaveAsTypeResult.rv = fp.filterIndex;
    file = fp.file;

    if (aIsDocument) 
      prefs.setIntPref("save_converter_index", aSaveAsTypeResult.rv);
  }
  else {
    // ben 07/31/2003: 
    // We don't nullcheck dir here because dir should never be null if we get here
    // unless something is badly wrong, and if it is, I want to know about it in
    // bugs. 
    dir.append(defaultString);
    file = dir;
    
    // Since we're automatically downloading, we don't get the file picker's 
    // logic to check for existing files, so we need to do that here.
    //
    // Note - this code is identical to that in 
    //   browser/components/downloads/content/nsHelperAppDlg.js. 
    // If you are updating this code, update that code too! We can't share code
    // here since that code is called in a js component. 
    while (file.exists()) {
      var parts = /.+-(\d+)(\..*)?$/.exec(file.leafName);
      if (parts) {
        file.leafName = file.leafName.replace(/((\d+)\.)|((\d+)$)/,
                                              function (str, dot, dotNum, noDot, noDotNum, pos, s) {
                                                return (parseInt(str) + 1) + (dot ? "." : "");
                                              });
      }
      else {
        file.leafName = file.leafName.replace(/\.|$/, "-1$&");
      }
    }
    
  }

  return file;    
}

function nsHeaderSniffer(aURL, aCallback, aData, aSkipPrompt)
{
  this.mCallback = aCallback;
  this.mData = aData;
  this.mSkipPrompt = aSkipPrompt;
  
  this.uri = makeURL(aURL);
  
  this.linkChecker = Components.classes["@mozilla.org/network/urichecker;1"]
    .createInstance(Components.interfaces.nsIURIChecker);
  this.linkChecker.init(this.uri);

  var flags;
  if (aData.bypassCache) {
    flags = Components.interfaces.nsIRequest.LOAD_BYPASS_CACHE;
  } else {
    flags = Components.interfaces.nsIRequest.LOAD_FROM_CACHE;
  }
  this.linkChecker.loadFlags = flags;

  this.linkChecker.asyncCheck(this, null);
}

nsHeaderSniffer.prototype = {

  // ---------- nsISupports methods ----------
  QueryInterface: function (iid) {
    if (!iid.equals(Components.interfaces.nsIRequestObserver) &&
        !iid.equals(Components.interfaces.nsISupports) &&
        !iid.equals(Components.interfaces.nsIInterfaceRequestor)) {
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
  },

  // ---------- nsIInterfaceRequestor methods ----------
  getInterface : function(iid) {
    if (iid.equals(Components.interfaces.nsIAuthPrompt)) {
      // use the window watcher service to get a nsIAuthPrompt impl
      var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(Components.interfaces.nsIWindowWatcher);
      return ww.getNewAuthPrompter(window);
    }
    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },

  // ---------- nsIRequestObserver methods ----------
  onStartRequest: function (aRequest, aContext) { },
  
  onStopRequest: function (aRequest, aContext, aStatus) {
    try {
      if (aStatus == 0) { // NS_BINDING_SUCCEEDED, so there's something there
        var linkChecker = aRequest.QueryInterface(Components.interfaces.nsIURIChecker);
        var channel = linkChecker.baseChannel;
        this.contentType = channel.contentType;
        try {
          var httpChannel = channel.QueryInterface(Components.interfaces.nsIHttpChannel);
          var encodedChannel = channel.QueryInterface(Components.interfaces.nsIEncodedChannel);
          this.contentEncodingType = null;
          // There may be content-encodings on the channel.  Multiple content
          // encodings are allowed, eg "Content-Encoding: gzip, uuencode".  This
          // header would mean that the content was first gzipped and then
          // uuencoded.  The encoding enumerator returns MIME types
          // corresponding to each encoding starting from the end, so the first
          // thing it returns corresponds to the outermost encoding.
          var encodingEnumerator = encodedChannel.contentEncodings;
          if (encodingEnumerator && encodingEnumerator.hasMore()) {
            try {
              this.contentEncodingType = encodingEnumerator.getNext();
            } catch (e) {
            }
          }
          this.mContentDisposition = httpChannel.getResponseHeader("content-disposition");
        }
        catch (e) {
        }
        if (!this.contentType || this.contentType == "application/x-unknown-content-type") {
          // We didn't get a type from the server.  Fall back on other type detection mechanisms
          throw "Unknown Type";
        }
      }
      else {
        dump("Error saving link aStatus = 0x" + aStatus.toString(16) + "\n");
        var bundle = getStringBundle();
        var errorTitle = bundle.GetStringFromName("saveLinkErrorTitle");
        var errorMsg = bundle.GetStringFromName("saveLinkErrorMsg");
        const promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
        promptService.alert(this.mData.window, errorTitle, errorMsg);
        return;
      }
    }
    catch (e) {
      if (this.mData.document) {
        this.contentType = this.mData.document.contentType;
      } else {
        var type = getMIMETypeForURI(this.uri);
        if (type)
          this.contentType = type;
      }
    }
    this.mCallback(this, this.mData, this.mSkipPrompt);
  },

  // ------------------------------------------------

  get promptService()
  {
    var promptSvc;
    try {
      promptSvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
      promptSvc = promptSvc.QueryInterface(Components.interfaces.nsIPromptService);
    }
    catch (e) {}
    return promptSvc;
  },

  get suggestedFileName()
  {
    var fileName = "";

    if (this.mContentDisposition) {
      const mhpContractID = "@mozilla.org/network/mime-hdrparam;1"
      const mhpIID = Components.interfaces.nsIMIMEHeaderParam;
      const mhp = Components.classes[mhpContractID].getService(mhpIID);
      var dummy = { value: null }; // To make JS engine happy.
      var charset = getCharsetforSave(null);

      try {
        fileName = mhp.getParameter(this.mContentDisposition, "filename", charset, true, dummy);
      } 
      catch (e) {
        try {
          fileName = mhp.getParameter(this.mContentDisposition, "name", charset, true, dummy);
        }
        catch (e) {
        }
      }
    }
    fileName = fileName.replace(/^"|"$/g, "");
    return fileName;
  }
};

const MODE_COMPLETE = 0;
const MODE_FILEONLY = 1;

function appendFiltersForContentType(aFilePicker, aContentType, aFileExtension, aSaveMode)
{
  var bundle = getStringBundle();
    
  switch (aContentType) {
  case "text/html":
    if (aSaveMode == MODE_COMPLETE)
      aFilePicker.appendFilter(bundle.GetStringFromName("WebPageCompleteFilter"), "*.htm; *.html");
    aFilePicker.appendFilter(bundle.GetStringFromName("WebPageHTMLOnlyFilter"), "*.htm; *.html");
    if (aSaveMode == MODE_COMPLETE)
      aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterText);
    break;
  default:
    var mimeInfo = getMIMEInfoForType(aContentType, aFileExtension);
    if (mimeInfo) {

      var extEnumerator = mimeInfo.getFileExtensions();

      var extString = "";
      var defaultDesc = "";
      var plural = false;
      while (extEnumerator.hasMore()) {
        if (defaultDesc) {
          defaultDesc += ", ";
          plural = true;
        }
        var extension = extEnumerator.getNext();
        if (extString)
          extString += "; ";    // If adding more than one extension,
                                // separate by semi-colon
        extString += "*." + extension;
        defaultDesc += extension.toUpperCase();
      }

      if (extString) {
        var desc = mimeInfo.Description;
        if (!desc) { 
          var key = plural ? "unknownDescriptionFilesPluralFilter" : 
                             "unknownDescriptionFilesFilter";
          desc = getStringBundle().formatStringFromName(key, [defaultDesc], 1);
        }
        aFilePicker.appendFilter(desc, extString);
      } else {
        aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
      }        
    }
    else
      aFilePicker.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
    break;
  }
} 

function getPostData()
{
  try {
    var sessionHistory = getWebNavigation().sessionHistory;
    entry = sessionHistory.getEntryAtIndex(sessionHistory.index, false);
    entry = entry.QueryInterface(Components.interfaces.nsISHEntry);
    return entry.postData;
  }
  catch (e) {
  }
  return null;
}

//XXXPch: that that be removed.
function getStringBundle()
{
  const bundleURL = "chrome://browser/locale/contentAreaCommands.properties";
  
  const sbsContractID = "@mozilla.org/intl/stringbundle;1";
  const sbsIID = Components.interfaces.nsIStringBundleService;
  const sbs = Components.classes[sbsContractID].getService(sbsIID);
  
  const lsContractID = "@mozilla.org/intl/nslocaleservice;1";
  const lsIID = Components.interfaces.nsILocaleService;
  const ls = Components.classes[lsContractID].getService(lsIID);
  var appLocale = ls.getApplicationLocale();
  return sbs.createBundle(bundleURL, appLocale);    
}

function makeWebBrowserPersist()
{
  const persistContractID = "@mozilla.org/embedding/browser/nsWebBrowserPersist;1";
  const persistIID = Components.interfaces.nsIWebBrowserPersist;
  return Components.classes[persistContractID].createInstance(persistIID);
}

function makeURL(aURL)
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);
  return ioService.newURI(aURL, null, null);
}

function makeFileURL(aFile)
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
  return ioService.newFileURI(aFile);
}

function makeFilePicker()
{
  const fpContractID = "@mozilla.org/filepicker;1";
  const fpIID = Components.interfaces.nsIFilePicker;
  return Components.classes[fpContractID].createInstance(fpIID);
}

function getMIMEService()
{
  const mimeSvcContractID = "@mozilla.org/mime;1";
  const mimeSvcIID = Components.interfaces.nsIMIMEService;
  const mimeSvc = Components.classes[mimeSvcContractID].getService(mimeSvcIID);
  return mimeSvc;
}

function getMIMETypeForURI(aURI)
{
  try {  
    return getMIMEService().getTypeFromURI(aURI);
  }
  catch (e) {
  }
  return null;
}

function getMIMEInfoForType(aMIMEType, aExtension)
{
  try {  
    return getMIMEService().getFromTypeAndExtension(aMIMEType, aExtension);
  }
  catch (e) {
  }
  return null;
}

function getDefaultFileName(aDefaultFileName, aNameFromHeaders, aDocumentURI, aDocument)
{
  if (aNameFromHeaders)
    // 1) Use the name suggested by the HTTP headers
    return validateFileName(aNameFromHeaders);

  try {
    var url = aDocumentURI.QueryInterface(Components.interfaces.nsIURL);
    if (url.fileName != "") {
      // 2) Use the actual file name, if present
      return validateFileName(decodeURIComponent(url.fileName));
    }
  } catch (e) {
    try {
      // the file name might be non ASCII
      // try unescape again with a characterSet
      var textToSubURI = Components.classes["@mozilla.org/intl/texttosuburi;1"]
                                   .getService(Components.interfaces.nsITextToSubURI);
      var charset = getCharsetforSave(aDocument);
      return validateFileName(textToSubURI.unEscapeURIForUI(charset, url.fileName));
    } catch (e) {
      // This is something like a wyciwyg:, data:, and so forth
      // URI... no usable filename here.
    }
  }
  
  if (aDocument) {
    var docTitle = validateFileName(aDocument.title).replace(/^\s+|\s+$/g, "");

    if (docTitle != "") {
      // 3) Use the document title
      return docTitle;
    }
  }
  
  if (aDefaultFileName)
    // 4) Use the caller-provided name, if any
    return validateFileName(aDefaultFileName);

  // 5) If this is a directory, use the last directory name
  var re = /\/([^\/]+)\/$/;
  var path = aDocumentURI.path.match(re);
  if (path && path.length > 1) {
      return validateFileName(path[1]);
  }
  
  try {
    if (aDocumentURI.host)
      // 6) Use the host.
      return aDocumentURI.host;
  } catch (e) {
    // Some files have no information at all, like Javascript generated pages
  }
  try {
    // 7) Use the default file name
    return getStringBundle().GetStringFromName("DefaultSaveFileName");
  } catch (e) {
    //in case localized string cannot be found
  }
  // 8) If all else fails, use "index"
  return "index";
}

function validateFileName(aFileName)
{
  var re = /[\/]+/g;
  if (navigator.appVersion.indexOf("Windows") != -1) {
    re = /[\\\/\|]+/g;
    aFileName = aFileName.replace(/[\"]+/g, "'");
    aFileName = aFileName.replace(/[\*\:\?]+/g, " ");
    aFileName = aFileName.replace(/[\<]+/g, "(");
    aFileName = aFileName.replace(/[\>]+/g, ")");
  }
  else if (navigator.appVersion.indexOf("Macintosh") != -1)
    re = /[\:\/]+/g;
  
  return aFileName.replace(re, "_");
}

function getNormalizedLeafName(aFile, aDefaultExtension)
{
  if (!aDefaultExtension)
    return aFile;
  
  // Fix up the file name we're saving to to include the default extension
  const stdURLContractID = "@mozilla.org/network/standard-url;1";
  const stdURLIID = Components.interfaces.nsIURL;
  var url = Components.classes[stdURLContractID].createInstance(stdURLIID);
  url.filePath = aFile;
  
  if (url.fileExtension != aDefaultExtension) {
    return aFile + "." + aDefaultExtension;
  }

  return aFile;
}

function getDefaultExtension(aFilename, aURI, aContentType)
{
  if (aContentType == "text/plain" || aContentType == "application/octet-stream" || aURI.scheme == "ftp")
    return "";   // temporary fix for bug 120327

  // First try the extension from the filename
  const stdURLContractID = "@mozilla.org/network/standard-url;1";
  const stdURLIID = Components.interfaces.nsIURL;
  var url = Components.classes[stdURLContractID].createInstance(stdURLIID);
  url.filePath = aFilename;

  var ext = url.fileExtension;

  // This mirrors some code in nsExternalHelperAppService::DoContent
  // Use the filename first and then the URI if that fails
  
  var mimeInfo = getMIMEInfoForType(aContentType, ext);

  if (ext && mimeInfo && mimeInfo.extensionExists(ext)) {
    return ext;
  }
  
  // Well, that failed.  Now try the extension from the URI
  var urlext;
  try {
    url = aURI.QueryInterface(Components.interfaces.nsIURL);
    urlext = url.fileExtension;
  } catch (e) {
  }

  if (urlext && mimeInfo && mimeInfo.extensionExists(urlext)) {
    return urlext;
  }
  else {
    try {
      return mimeInfo.primaryExtension;
    }
    catch (e) {
      // Fall back on the extensions in the filename and URI for lack
      // of anything better.
      return ext || urlext;
    }
  }
}

function isDocumentType(aContentType)
{
  switch (aContentType) {
  case "text/html":
    return true;
  case "text/xml":
  case "application/xhtml+xml":
  case "application/xml":
    return false; // XXX Disables Save As Complete until it works for XML
  }
  return false;
}

function getCharsetforSave(aDocument)
{
  if (aDocument)
    return aDocument.characterSet;

  if (document.commandDispatcher.focusedWindow)
    return document.commandDispatcher.focusedWindow.document.characterSet;

  return  window._content.document.characterSet;
  return false;
}

# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is 
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@netscape.com> (Save File)
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

