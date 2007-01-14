/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Microsummarizer.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Myk Melez <myk@mozilla.org> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;

const PERMS_FILE    = 0644;
const MODE_WRONLY   = 0x02;
const MODE_TRUNCATE = 0x20;

const NS_ERROR_MODULE_DOM = 2152923136;
const NS_ERROR_DOM_BAD_URI = NS_ERROR_MODULE_DOM + 1012;

// How often to update microsummaries, in milliseconds.
// XXX Make this a hidden pref so power users can modify it.
const UPDATE_INTERVAL = 30 * 60 * 1000; // 30 minutes

// How often to check for microsummaries that need updating, in milliseconds.
const CHECK_INTERVAL = 15 * 1000; // 15 seconds

const MICSUM_NS = new Namespace("http://www.mozilla.org/microsummaries/0.1");
const XSLT_NS = new Namespace("http://www.w3.org/1999/XSL/Transform");

#ifdef MOZ_PLACES
const FIELD_MICSUM_GEN_URI    = "microsummary/generatorURI";
const FIELD_MICSUM_EXPIRATION = "microsummary/expiration";
const FIELD_GENERATED_TITLE   = "bookmarks/generatedTitle";
const FIELD_CONTENT_TYPE      = "bookmarks/contentType";
#else
const NC_NS                   = "http://home.netscape.com/NC-rdf#";
const RDF_NS                  = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const FIELD_RDF_TYPE          = RDF_NS + "type";
const VALUE_MICSUM_BOOKMARK   = NC_NS + "MicsumBookmark";
const VALUE_NORMAL_BOOKMARK   = NC_NS + "Bookmark";
const FIELD_MICSUM_GEN_URI    = NC_NS + "MicsumGenURI";
const FIELD_MICSUM_EXPIRATION = NC_NS + "MicsumExpiration";
const FIELD_GENERATED_TITLE   = NC_NS + "GeneratedTitle";
const FIELD_CONTENT_TYPE      = NC_NS + "ContentType";
const FIELD_BOOKMARK_URL      = NC_NS + "URL";
#endif

function MicrosummaryService() {}

MicrosummaryService.prototype = {

#ifdef MOZ_PLACES
  // Bookmarks Service
  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  // Annotation Service
  __ans: null,
  get _ans() {
    if (!this.__ans)
      this.__ans = Cc["@mozilla.org/browser/annotation-service;1"].
                   getService(Ci.nsIAnnotationService);
    return this.__ans;
  },
#else
  // RDF Service
  __rdf: null,
  get _rdf() {
    if (!this.__rdf)
      this.__rdf = Cc["@mozilla.org/rdf/rdf-service;1"].
                   getService(Ci.nsIRDFService);
    return this.__rdf;
  },

  // Bookmarks Data Source
  __bmds: null,
  get _bmds() {
    if (!this.__bmds)
      this.__bmds = this._rdf.GetDataSource("rdf:bookmarks");
    return this.__bmds;
  },

  // Old Bookmarks Service
  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = this._bmds.QueryInterface(Ci.nsIBookmarksService);
    return this.__bms;
  },
#endif
 
  // IO Service
  __ios: null,
  get _ios() {
    if (!this.__ios)
      this.__ios = Cc["@mozilla.org/network/io-service;1"].
                   getService(Ci.nsIIOService);
    return this.__ios;
  },

  // Observer Service
  __obs: null,
  get _obs() {
    if (!this.__obs)
      this.__obs = Cc["@mozilla.org/observer-service;1"].
                   getService(Ci.nsIObserverService);
    return this.__obs;
  },

  /**
   * Make a URI from a spec.
   * @param   spec
   *          The string spec of the URI.
   * @returns An nsIURI object.
   */
  _uri: function MSS__uri(spec) {
    return this._ios.newURI(spec, null, null);
  },

#ifndef MOZ_PLACES
  /**
   * Make an RDF resource from a URI spec.
   * @param   uriSpec
   *          The URI spec to convert into a resource.
   * @returns An nsIRDFResource object.
   */
  _resource: function MSS__resource(uriSpec) {
    return this._rdf.GetResource(uriSpec);
  },

  /**
   * Make an RDF literal from a string.
   * @param   str
   *          The string from which to construct the literal.
   * @returns An nsIRDFLiteral object
   */
  _literal: function MSS__literal(str) {
    return this._rdf.GetLiteral(str);
  },
#endif

  // Directory Locator
  __dirs: null,
  get _dirs() {
    if (!this.__dirs)
      this.__dirs = Cc["@mozilla.org/file/directory_service;1"].
                   getService(Ci.nsIProperties);
    return this.__dirs;
  },

  // A cache of local microsummary generators.  This gets built on startup
  // by the _cacheLocalGenerators() method.
  _localGenerators: {},

  // The timer that periodically checks for microsummaries needing updating.
  _timer: null,

  // Interfaces this component implements.
  interfaces: [Ci.nsIMicrosummaryService, Ci.nsIObserver, Ci.nsISupports],

  // nsISupports

  QueryInterface: function MSS_QueryInterface(iid) {
    //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
    if (!iid.equals(Ci.nsIMicrosummaryService) &&
        !iid.equals(Ci.nsIObserver) &&
        !iid.equals(Ci.nsISupportsWeakReference) &&
        !iid.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  // nsIObserver

  observe: function MSS_observe(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        this._destroy();
        break;
    }
  },

  _init: function MSS__init() {
    this._obs.addObserver(this, "xpcom-shutdown", true);

    // Periodically update microsummaries that need updating.
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    var callback = {
      _svc: this,
      notify: function(timer) { this._svc._updateMicrosummaries() }
    };
    this._timer.initWithCallback(callback,
                                 CHECK_INTERVAL,
                                 this._timer.TYPE_REPEATING_SLACK);

    this._cacheLocalGenerators();
  },
  
  _destroy: function MSS__destroy() {
    this._timer.cancel();
    this._timer = null;
  },

  _updateMicrosummaries: function MSS__updateMicrosummaries() {
#ifdef MOZ_PLACES
    // This try/catch block is a temporary workaround for bug 336194.
    var bookmarks;
    try {
      bookmarks = this._ans.getPagesWithAnnotation(FIELD_MICSUM_GEN_URI, {});
    }
    catch(e) {
      bookmarks = [];
    }
#else
    var bookmarks = [];

    var resources = this._bmds.GetSources(this._resource(FIELD_RDF_TYPE),
                                          this._resource(VALUE_MICSUM_BOOKMARK),
                                          true);
    while (resources.hasMoreElements())
      bookmarks.push(resources.getNext().QueryInterface(Ci.nsIRDFResource));
#endif

    var now = new Date().getTime();

    for ( var i = 0; i < bookmarks.length; i++ ) {
      var bookmarkID = bookmarks[i];

      // Skip this page if its microsummary hasn't expired yet.
      if (this._hasField(bookmarkID, FIELD_MICSUM_EXPIRATION) &&
          this._getField(bookmarkID, FIELD_MICSUM_EXPIRATION) > now)
        continue;

      this.refreshMicrosummary(bookmarkID);
    }
  },
  
  _updateMicrosummary: function MSS__updateMicrosummary(bookmarkID, microsummary) {
    this._setField(bookmarkID, FIELD_GENERATED_TITLE, microsummary.content);

    var now = new Date().getTime();
    this._setField(bookmarkID, FIELD_MICSUM_EXPIRATION, now + UPDATE_INTERVAL);

    LOG("updated microsummary for page " + microsummary.pageURI.spec +
        " to " + microsummary.content);
  },

  /**
   * Load local generators into the cache.
   * 
   */
  _cacheLocalGenerators: function MSS__cacheLocalGenerators() {
    // Load generators from the application directory.
    var appDir = this._dirs.get("MicsumGens", Ci.nsIFile);
    if (appDir.exists())
      this._cacheLocalGeneratorDir(appDir);

    // Load generators from the user's profile.
    var profileDir = this._dirs.get("UsrMicsumGens", Ci.nsIFile);
    if (profileDir.exists())
      this._cacheLocalGeneratorDir(profileDir);
  },

  /**
   * Load local generators from a directory into the cache.
   *
   * @param   dir
   *          nsIFile object pointing to directory containing generator files
   * 
   */
  _cacheLocalGeneratorDir: function MSS__cacheLocalGeneratorDir(dir) {
    var files = dir.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);
    var file = files.nextFile;

    while (file) {
      // Recursively load generators so support packs containing
      // lots of generators can organize them into multiple directories.
      if (file.isDirectory())
        this._cacheLocalGeneratorDir(file);
      else
        this._cacheLocalGeneratorFile(file);

      file = files.nextFile;
    }
    files.close();
  },

  /**
   * Load a local generator from a file into the cache.
   * 
   * @param   file
   *          nsIFile object pointing to file from which to load generator
   * 
   */
  _cacheLocalGeneratorFile: function MSS__cacheLocalGeneratorFile(file) {
    var uri = this._ios.newFileURI(file);

    var t = this;
    var callback =
      function MSS_cacheLocalGeneratorCallback(resource) {
        try     { t._handleLocalGenerator(resource) }
        finally { resource.destroy() }
      };

    var resource = new MicrosummaryResource(uri);
    resource.load(callback);
  },

  _handleLocalGenerator: function MSS__handleLocalGenerator(resource) {
    if (!resource.isXML)
      throw(resource.uri.spec + " microsummary generator loaded, but not XML");
  
    var generator = new MicrosummaryGenerator();
    generator.localURI = resource.uri;
    generator.initFromXML(resource.content);

    // Add the generator to the local generators cache.
    // XXX Figure out why Firefox crashes on shutdown if we index generators
    // by uri.spec but doesn't crash if we index by uri.spec.split().join().
    //this._localGenerators[generator.uri.spec] = generator;
    this._localGenerators[generator.uri.spec.split().join()] = generator;

    LOG("loaded local microsummary generator\n" +
        "   local URI: " + generator.localURI.spec + "\n" +
        "  source URI: " + generator.uri.spec);
  },

  // nsIMicrosummaryService

  /**
   * Install the microsummary generator from the resource at the supplied URI.
   * Callable by content via the addMicrosummaryGenerator() sidebar method.
   *
   * @param   generatorURI
   *          the URI of the resource providing the generator
   *
   */
  addGenerator: function MSS_addGenerator(generatorURI) {
    var t = this;
    var callback =
      function MSS_addGeneratorCallback(resource) {
        try     { t._handleNewGenerator(resource) }
        finally { resource.destroy() }
      };

    var resource = new MicrosummaryResource(generatorURI);
    resource.load(callback);
  },

  _handleNewGenerator: function MSS__handleNewGenerator(resource) {
    if (!resource.isXML)
      throw(resource.uri.spec + " microsummary generator loaded, but not XML");

    // XXX Make sure it's a valid microsummary generator.

    var rootNode = resource.content.documentElement;

    // Add a reference to the URI from which we got this generator so we have
    // a unique identifier for the generator and also so we can check back later
    // for updates.
    rootNode.setAttribute("sourceURI", resource.uri.spec);

    var generatorName = rootNode.getAttribute("name");
    var fileName = sanitizeName(generatorName) + ".xml";
    var file = this._dirs.get("UsrMicsumGens", Ci.nsIFile);
    file.append(fileName);
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);

    var outputStream = Cc["@mozilla.org/network/safe-file-output-stream;1"].
                       createInstance(Ci.nsIFileOutputStream);
    var localFile = file.QueryInterface(Ci.nsILocalFile);
    outputStream.init(localFile, (MODE_WRONLY | MODE_TRUNCATE), PERMS_FILE, 0);
    var serializer = Cc["@mozilla.org/xmlextras/xmlserializer;1"].
                     createInstance(Ci.nsIDOMSerializer);
    serializer.serializeToStream(resource.content, outputStream, null);
    if (outputStream instanceof Ci.nsISafeOutputStream) {
      try       { outputStream.finish() }
      catch (e) { outputStream.close()  }
    }
    else
      outputStream.close();

    // Finally, cache the generator in the local generators cache.
    this._cacheLocalGeneratorFile(file);
  },

  /**
   * Get the set of microsummaries available for a given page.  The set
   * might change after this method returns, since this method will trigger
   * an asynchronous load of the page in question (if it isn't already loaded)
   * to see if it references any page-specific microsummaries.
   *
   * If the caller passes a bookmark ID, and one of the microsummaries
   * is the current one for the bookmark, this method will retrieve content
   * from the datastore for that microsummary, which is useful when callers
   * want to display a list of microsummaries for a page that isn't loaded,
   * and they want to display the actual content of the selected microsummary
   * immediately (rather than after the content is asynchronously loaded).
   *
   * @param   pageURI
   *          the URI of the page for which to retrieve available microsummaries
   *
   * @param   bookmarkID (optional)
   *          the ID of the bookmark for which this method is being called
   *
   * @returns an nsIMicrosummarySet of nsIMicrosummaries for the given page
   *
   */
  getMicrosummaries: function MSS_getMicrosummaries(pageURI, bookmarkID) {
    var microsummaries = new MicrosummarySet();

    // Get microsummaries defined by local generators.
    for (var genURISpec in this._localGenerators) {
      var generator = this._localGenerators[genURISpec];

      if (generator.appliesToURI(pageURI)) {
        var microsummary = new Microsummary(pageURI, generator.uri);
        microsummary.generator = generator;

        // If this is the current microsummary for this bookmark, load the content
        // from the datastore so it shows up immediately in microsummary picking UI.
        if (bookmarkID && this.isMicrosummary(bookmarkID, microsummary))
          microsummary.content = this._getField(bookmarkID, FIELD_GENERATED_TITLE);

        microsummaries.AppendElement(microsummary);
      }
    }

    // Get microsummaries defined by the page.  If we don't have the page,
    // download it asynchronously, and then finish populating the set.
    var resource = getLoadedMicrosummaryResource(pageURI);
    if (resource) {
      try     { microsummaries.extractFromPage(resource) }
      finally { resource.destroy() }
    }
    else {
      // Load the page with a callback that will add the page's microsummaries
      // to the set once the page has loaded.
      var callback = function MSS_extractFromPageCallback(resource) {
        try     { microsummaries.extractFromPage(resource) }
        finally { resource.destroy() }
      };

      try {
        resource = new MicrosummaryResource(pageURI);
        resource.load(callback);
      }
      catch(e) {
        // We don't have to do anything special if the call fails besides
        // destroying the Resource object.  We can just return the list
        // of microsummaries without including page-defined microsummaries.
        resource.destroy();
        LOG("error downloading page to extract its microsummaries: " + e);
      }
    }

    return microsummaries;
  },

#ifdef MOZ_PLACES
  _getField: function MSS__getField(bookmarkID, fieldName) {
    var pageURI = bookmarkID.QueryInterface(Ci.nsIURI);
    var fieldValue;

    switch(fieldName) {
    case FIELD_MICSUM_EXPIRATION:
      fieldValue = this._ans.getAnnotationInt64(pageURI, fieldName);
      break;
    case FIELD_MICSUM_GEN_URI:
    case FIELD_GENERATED_TITLE:
    case FIELD_CONTENT_TYPE:
    default:
      fieldValue = this._ans.getAnnotationString(pageURI, fieldName);
      break;
    }
    
    return fieldValue;
  },

  _setField: function MSS__setField(bookmarkID, fieldName, fieldValue) {
    var pageURI = bookmarkID.QueryInterface(Ci.nsIURI);

    switch(fieldName) {
    case FIELD_MICSUM_EXPIRATION:
      this._ans.setAnnotationInt64(pageURI,
                                   fieldName,
                                   fieldValue,
                                   0,
                                   this._ans.EXPIRE_NEVER);
      break;
    case FIELD_MICSUM_GEN_URI:
    case FIELD_GENERATED_TITLE:
    case FIELD_CONTENT_TYPE:
    default:
      this._ans.setAnnotationString(pageURI,
                                    fieldName,
                                    fieldValue,
                                    0,
                                    this._ans.EXPIRE_NEVER);
      break;
    }
  },

  _clearField: function MSS__clearField(bookmarkID, fieldName) {
    var pageURI = bookmarkID.QueryInterface(Ci.nsIURI);

    this._ans.removeAnnotation(pageURI, fieldName);
  },

  _hasField: function MSS__hasField(bookmarkID, fieldName) {
    var pageURI = bookmarkID.QueryInterface(Ci.nsIURI);
    return this._ans.hasAnnotation(pageURI, fieldName);
  },

  _getPageForBookmark: function MSS__getPageForBookmark(bookmarkID) {
    var pageURI = bookmarkID.QueryInterface(Ci.nsIURI);

    return pageURI;
  },

#else
  _getField: function MSS__getField(bookmarkID, fieldName) {
    var bookmarkResource = bookmarkID.QueryInterface(Ci.nsIRDFResource);
    var fieldValue;

    var node = this._bmds.GetTarget(bookmarkResource,
                                    this._resource(fieldName),
                                    true);
    if (node) {
      if (fieldName == FIELD_RDF_TYPE)
        fieldValue = node.QueryInterface(Ci.nsIRDFResource).Value;
      else
        fieldValue = node.QueryInterface(Ci.nsIRDFLiteral).Value;
    }
    else
      fieldValue = null;

    return fieldValue;
  },

  _setField: function MSS__setField(bookmarkID, fieldName, fieldValue) {
    var bookmarkResource = bookmarkID.QueryInterface(Ci.nsIRDFResource);

    if (this._hasField(bookmarkID, fieldName)) {
      var oldValue = this._getField(bookmarkID, fieldName);
      this._bmds.Change(bookmarkResource,
                        this._resource(fieldName),
                        this._literal(oldValue),
                        this._literal(fieldValue));
    }
    else {
      this._bmds.Assert(bookmarkResource,
                        this._resource(fieldName),
                        this._literal(fieldValue),
                        true);
    }

    // If we're setting the generator URI field, make sure the bookmark's
    // RDF type is set to the microsummary bookmark type.
    if (fieldName == FIELD_MICSUM_GEN_URI &&
        this._getField(bookmarkID, FIELD_RDF_TYPE) != VALUE_MICSUM_BOOKMARK) {
      if (this._hasField(bookmarkID, FIELD_RDF_TYPE)) {
        oldValue = this._getField(bookmarkID, FIELD_RDF_TYPE);
        this._bmds.Change(bookmarkResource,
                          this._resource(FIELD_RDF_TYPE),
                          this._resource(oldValue),
                          this._resource(VALUE_MICSUM_BOOKMARK));
      }
      else {
        this._bmds.Assert(bookmarkResource,
                          this._resource(FIELD_RDF_TYPE),
                          this._resource(VALUE_MICSUM_BOOKMARK),
                          true);
      }
    }

    // If we're setting a field that could affect this bookmark's label,
    // then force all bookmark trees to rebuild from scratch.
    if (fieldName == FIELD_MICSUM_GEN_URI || fieldName == FIELD_GENERATED_TITLE)
      this._forceBookmarkTreesRebuild();
  },

  _clearField: function MSS__clearField(bookmarkID, fieldName) {
    var bookmarkResource = bookmarkID.QueryInterface(Ci.nsIRDFResource);

    var node = this._bmds.GetTarget(bookmarkResource,
                                    this._resource(fieldName),
                                    true);
    if (node) {
      this._bmds.Unassert(bookmarkResource,
                          this._resource(fieldName),
                          node);
    }

    // If we're clearing the generator URI field, set the bookmark's RDF type
    // back to the normal bookmark type.
    if (fieldName == FIELD_MICSUM_GEN_URI &&
        this._getField(bookmarkID, FIELD_RDF_TYPE) == VALUE_MICSUM_BOOKMARK)
      this._bmds.Change(bookmarkResource,
                        this._resource(FIELD_RDF_TYPE),
                        this._resource(VALUE_MICSUM_BOOKMARK),
                        this._resource(VALUE_NORMAL_BOOKMARK));

    // If we're clearing a field that could affect this bookmark's label,
    // then force all bookmark trees to rebuild from scratch.
    if (fieldName == FIELD_MICSUM_GEN_URI || fieldName == FIELD_GENERATED_TITLE)
      this._forceBookmarkTreesRebuild();
  },

  _hasField: function MSS__hasField(bookmarkID, fieldName) {
    var bookmarkResource = bookmarkID.QueryInterface(Ci.nsIRDFResource);

    var node = this._bmds.GetTarget(bookmarkResource,
                                    this._resource(fieldName),
                                    true);
    return node ? true : false;
  },

  /**
   * Oy vey, a hack!  Force the bookmark trees to rebuild, since they don't
   * seem to be able to do it correctly on their own right after we twiddle
   * something microsummaryish (but they rebuild fine otherwise, incorporating
   * all the microsummary changes upon next full rebuild, f.e. if you open
   * a new window or shut down and restart your browser).
   *
   */
  _forceBookmarkTreesRebuild: function MSS__forceBookmarkTreesRebuild() {
    var mediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                   getService(Ci.nsIWindowMediator);
    var windows = mediator.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      var win = windows.getNext();
      
      // rebuild the bookmarks toolbar
      var bookmarksToolbar = win.document.getElementById("bookmarks-ptf");
      if (bookmarksToolbar)
        bookmarksToolbar.builder.rebuild();
      
      // rebuild the bookmarks sidebar
      var sidebar = win.document.getElementById("sidebar");
      if (sidebar.contentWindow && sidebar.contentWindow.location ==
          "chrome://browser/content/bookmarks/bookmarksPanel.xul") {
        var treeElement = sidebar.contentDocument.getElementById("bookmarks-view");
        treeElement.tree.builder.rebuild();
      }
    }
    
    windows = mediator.getEnumerator("bookmarks:manager");
    while (windows.hasMoreElements()) {
      win = windows.getNext();
      // rebuild the Bookmarks Manager's view
      treeElement = win.document.getElementById("bookmarks-view");
      treeElement.tree.builder.rebuild();
    }
  },

  /**
   * Get the URI of the page to which a given bookmark refers.
   *
   * @param   bookmarkResource
   *          an nsIResource uniquely identifying the bookmark
   *
   * @returns an nsIURI object representing the bookmark's page,
   *          or null if the bookmark doesn't exist
   *
   */
  _getPageForBookmark: function MSS__getPageForBookmark(bookmarkID) {
    var bookmarkResource = bookmarkID.QueryInterface(Ci.nsIRDFResource);

    var node = this._bmds.GetTarget(bookmarkResource,
                                    this._resource(NC_NS + "URL"),
                                    true);

    if (!node)
      return null;

    var pageSpec = node.QueryInterface(Ci.nsIRDFLiteral).Value;
    var pageURI = this._uri(pageSpec);
    return pageURI;
  },
#endif

  /**
   * Get the current microsummary for the given bookmark.
   *
   * @param   bookmarkID
   *          the bookmark for which to get the current microsummary
   *
   * @returns the current microsummary for the bookmark, or null
   *          if the bookmark does not have a current microsummary
   *
   */
  getMicrosummary: function MSS_getMicrosummary(bookmarkID) {
    if (!this.hasMicrosummary(bookmarkID))
      return null;

    var pageURI = this._getPageForBookmark(bookmarkID);
    var generatorURI = this._uri(this._getField(bookmarkID, FIELD_MICSUM_GEN_URI));
    
    var microsummary = new Microsummary(pageURI, generatorURI);
    if (this._localGenerators[generatorURI.spec])
      microsummary.generator = this._localGenerators[generatorURI.spec];

    return microsummary;
  },

  /**
   * Set the current microsummary for the given bookmark.
   *
   * @param   bookmarkID
   *          the bookmark for which to set the current microsummary
   *
   * @param   microsummary
   *          the microsummary to set as the current one
   *
   */
  setMicrosummary: function MSS_setMicrosummary(bookmarkID, microsummary) {
    this._setField(bookmarkID, FIELD_MICSUM_GEN_URI, microsummary.generatorURI.spec);
    if (microsummary.content)
      this._updateMicrosummary(bookmarkID, microsummary);
    else
      this.refreshMicrosummary(bookmarkID);
  },

  /**
   * Remove the current microsummary for the given bookmark.
   *
   * @param   bookmarkID
   *          the bookmark for which to remove the current microsummary
   *
   */
  removeMicrosummary: function MSS_removeMicrosummary(bookmarkID) {
    var fields = [FIELD_MICSUM_GEN_URI,
                  FIELD_MICSUM_EXPIRATION,
                  FIELD_GENERATED_TITLE,
                  FIELD_CONTENT_TYPE];

    for ( var i = 0; i < fields.length; i++ ) {
      var field = fields[i];
      if (this._hasField(bookmarkID, field))
        this._clearField(bookmarkID, field);
    }
  },

  /**
   * Whether or not the given bookmark has a current microsummary.
   *
   * @param   bookmarkID
   *          the bookmark for which to set the current microsummary
   *
   * @returns a boolean representing whether or not the given bookmark
   *          has a current microsummary
   *
   */
  hasMicrosummary: function MSS_hasMicrosummary(bookmarkID) {
    return this._hasField(bookmarkID, FIELD_MICSUM_GEN_URI);
  },

  /**
   * Whether or not the given microsummary is the current microsummary
   * for the given bookmark.
   *
   * @param   bookmarkID
   *          the bookmark to check
   *
   * @param   microsummary
   *          the microsummary to check
   *
   * @returns whether or not the microsummary is the current one
   *          for the bookmark
   *
   */
  isMicrosummary: function MSS_isMicrosummary(bookmarkID, microsummary) {
    if (!this.hasMicrosummary(bookmarkID))
      return false;

    var currentGen = this._getField(bookmarkID, FIELD_MICSUM_GEN_URI);

    if (microsummary.generatorURI.equals(this._uri(currentGen)))
      return true;

    return false
  },

  /**
   * Refresh a microsummary, updating its value in the datastore and UI.
   * If this method can refresh the microsummary instantly, it will.
   * Otherwise, it'll asynchronously download the necessary information
   * (the generator and/or page) before refreshing the microsummary.
   *
   * Callers should check the "content" property of the returned microsummary
   * object to distinguish between sync and async refreshes.  If its value
   * is "null", then it's an async refresh, and the caller should register
   * itself as an nsIMicrosummaryObserver via nsIMicrosummary.addObserver()
   * to find out when the refresh completes.
   *
   * @param   bookmarkID
   *          the bookmark whose microsummary is being refreshed
   *
   * @returns the microsummary being refreshed
   *
   */
  refreshMicrosummary: function MSS_refreshMicrosummary(bookmarkID) {
    if (!this.hasMicrosummary(bookmarkID))
      throw "bookmark " + bookmarkID + " does not have a microsummary";

    var pageURI = this._getPageForBookmark(bookmarkID);
    var generatorURI = this._uri(this._getField(bookmarkID, FIELD_MICSUM_GEN_URI));
    var microsummary = new Microsummary(pageURI, generatorURI);
    if (this._localGenerators[generatorURI.spec])
      microsummary.generator = this._localGenerators[generatorURI.spec];

    // A microsummary observer that calls the microsummary service
    // to update the datastore when the microsummary finishes loading.
    var observer = {
      _svc: this,
      _bookmarkID: bookmarkID,
      onContentLoaded: function MSS_observer_onContentLoaded(microsummary) {
        try {
          this._svc._updateMicrosummary(this._bookmarkID, microsummary);
        }
        finally {
          this._svc = null;
          this._bookmarkID = null;
          microsummary.removeObserver(this);
        }
      }
    };

    // Register the observer with the microsummary and trigger the microsummary
    // to update itself.
    microsummary.addObserver(observer);
    microsummary.update();
    
    return microsummary;
  }
};





function Microsummary(pageURI, generatorURI) {
  this._observers = [];
  this.pageURI = pageURI;
  this.generatorURI = generatorURI;
}

Microsummary.prototype = {
  interfaces: [Ci.nsIMicrosummary, Ci.nsISupports],

  // nsISupports

  QueryInterface: function (iid) {
    //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
    if (!iid.equals(Ci.nsIMicrosummary) &&
        !iid.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  // nsIMicrosummary

  _content: null,
  get content() {
    // If we have everything we need to generate the content, generate it.
    if (this._content == null &&
        this.generator &&
        (this.pageContent || !this.generator.needsPageContent))
      this._content = this.generator.generateMicrosummary(this.pageContent);
  
    // Note: we return "null" if the content wasn't already generated and we
    // couldn't retrieve it from the generated title annotation or generate it
    // ourselves.  So callers shouldn't count on getting content; instead,
    // they should call update if the return value of this getter is "null",
    // setting an observer to tell them when content generation is done.
    return this._content;
  },
  set content(newValue) { this._content = newValue },

  _generatorURI: null,
  get generatorURI()         { return this._generatorURI },
  set generatorURI(newValue) { this._generatorURI = newValue },

  _generator: null,
  get generator()            { return this._generator },
  set generator(newValue)    { this._generator = newValue },

  _pageURI: null,
  get pageURI()              { return this._pageURI },
  set pageURI(newValue)      { this._pageURI = newValue },

  _pageContent: null,
  get pageContent() {
    if (!this._pageContent) {
      // If the page is currently loaded into a browser window, use that.
      var resource = getLoadedMicrosummaryResource(this.pageURI);
      if (resource) {
        this._pageContent = resource.content;
        resource.destroy();
      }
    }

    return this._pageContent;
  },
  set pageContent(newValue) { this._pageContent = newValue },

  // nsIMicrosummary

  _observers: null,

  addObserver: function MS_addObserver(observer) {
    // Register the observer, but only if it isn't already registered,
    // so that we don't call the same observer twice for any given change.
    if (this._observers.indexOf(observer) == -1)
      this._observers.push(observer);
  },
  
  removeObserver: function MS_removeObserver(observer) {
    //NS_ASSERT(this._observers.indexOf(observer) != -1,
    //          "can't remove microsummary observer " + observer + ": not registered");
  
    //this._observers =
    //  this._observers.filter(function(i) { observer != i });
    if (this._observers.indexOf(observer) != -1)
      this._observers.splice(this._observers.indexOf(observer), 1);
  },

  /**
   * Regenerates the microsummary, asynchronously downloading its generator
   * and content as needed.
   *
   */
  update: function MS_update() {
    LOG("microsummary.update called for page:\n  " + this.pageURI.spec +
        "\nwith generator:\n  " + this.generatorURI.spec);

    var t = this;

    // If we don't have the generator, download it now.  After it downloads,
    // we'll re-call this method to continue updating the microsummary.
    if (!this.generator) {
      LOG("generator not yet loaded; downloading it");
      var generatorCallback =
        function MS_generatorCallback(resource) {
          try     { t._handleGeneratorLoad(resource) }
          finally { resource.destroy() }
        };
      var resource = new MicrosummaryResource(this.generatorURI);
      resource.load(generatorCallback);
      return;
    }

    // If we need the page content, and we don't have it, download it now.
    // Afterwards we'll re-call this method to continue updating the microsummary.
    if (this.generator.needsPageContent && !this.pageContent) {
      LOG("page content not yet loaded; downloading it");
      var pageCallback =
        function MS_pageCallback(resource) {
          try     { t._handlePageLoad(resource) }
          finally { resource.destroy() }
        };
      var resource = new MicrosummaryResource(this.pageURI);
      resource.load(pageCallback);
      return;
    }

    LOG("generator (and page, if needed) both loaded; generating microsummary");

    // Now that we have both the generator and (if needed) the page content,
    // generate the microsummary, then let the observers know about it.
    this.content = this.generator.generateMicrosummary(this.pageContent);
    this.pageContent = null;
    for ( var i = 0; i < this._observers.length; i++ )
      this._observers[i].onContentLoaded(this);

    LOG("generated microsummary: " + this.content);
  },

  _handleGeneratorLoad: function MS__handleGeneratorLoad(resource) {
    var generator = new MicrosummaryGenerator();
    generator.uri = resource.uri;
    LOG(generator.uri.spec + " microsummary generator downloaded");
    if (resource.isXML)
      generator.initFromXML(resource.content);
    else if (resource.contentType == "text/plain")
      generator.initFromText(resource.content);
    else
      throw("generator is neither XML nor plain text");

    this.generator = generator;
    this.update();
  },

  _handlePageLoad: function MS__handlePageLoad(resource) {
    if (!resource.isXML && !resource.contentType == "text/html")
      throw("page is neither HTML nor XML");

    this.pageContent = resource.content;
    this.update();
  }

};





function MicrosummaryGenerator() {}

MicrosummaryGenerator.prototype = {

  // IO Service
  __ios: null,
  get _ios() {
    if (!this.__ios)
      this.__ios = Cc["@mozilla.org/network/io-service;1"].
                   getService(Ci.nsIIOService);
    return this.__ios;
  },

  interfaces: [Ci.nsIMicrosummaryGenerator, Ci.nsISupports],

  // nsISupports

  QueryInterface: function (iid) {
    //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
    if (!iid.equals(Ci.nsIMicrosummaryGenerator) &&
        !iid.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  // nsIMicrosummaryGenerator

  // Normally this is just the URL from which we download the generator,
  // but for generators stored in the app or profile generators directory
  // it's the value of the generator tag's sourceURI attribute (or the
  // generator's local URI should the sourceURI be missing).
  _uri: null,
  get uri() { return this._uri },
  set uri(newValue) { this._uri = newValue },

  // For generators bundled with the browser or installed by the user,
  // the local URI is the URI of the local file containing the generator XML.
  _localURI: null,
  get localURI() { return this._localURI },
  set localURI(newValue) { this._localURI = newValue },

  _name: null,
  get name() { return this._name },
  set name(newValue) { this._name = newValue },

  _template: null,
  get template() { return this._template },
  set template(newValue) { this._template = newValue },

  _content: null,
  get content() { return this._content },
  set content(newValue) { this._content = newValue },

  _rules: null,

  /**
   * Determines whether or not the generator applies to a given URI.
   * By default, the generator does not apply to any URI.  In order for it
   * to apply to a URI, the URI must match one or more of the generator's
   * "include" rules and not match any of the generator's "exclude" rules.
   *
   * @param   uri
   *          the URI to test to see if this generator applies to it
   *
   * @returns boolean
   *          whether or not the generator applies to the given URI
   *
   */
  appliesToURI: function(uri) {
    var applies = false;

    for ( var i = 0 ; i < this._rules.length ; i++ ) {
      var rule = this._rules[i];

      switch (rule.type) {
      case "include":
        if (rule.regexp.test(uri.spec))
          applies = true;
        break;
      case "exclude":
        if (rule.regexp.test(uri.spec))
          return false;
        break;
      }
    }

    return applies;
  },

  get needsPageContent() {
    if (this.template)
      return true;
    else if (this.content)
      return false;
    else
      throw("needsPageContent called on uninitialized microsummary generator");
  },

  /**
   * Initializes a generator from text content.  Generators initialized
   * from text content merely return that content when their generate() method
   * gets called.
   *
   * @param   text
   *          the text content
   *
   */
  initFromText: function(text) {
    this.content = text;
  },

  /**
   * Initializes a generator from an XML description of it.
   * 
   * @param   xmlDocument
   *          An XMLDocument object describing a microsummary generator.
   *
   */
  initFromXML: function(xmlDocument) {
    // XXX Make sure the argument is a valid generator XML document.

    // XXX I would have wanted to retrieve the info from the XML via E4X,
    // but we'll need to pass the XSLT transform sheet to the XSLT processor,
    // and the processor can't deal with an E4X-wrapped template node.

    // XXX Right now the code retrieves the first "generator" element
    // in the microsummaries namespace, regardless of whether or not
    // it's the root element.  Should it matter?
    var generatorNode = xmlDocument.getElementsByTagNameNS(MICSUM_NS, "generator")[0];
    // XXX We should throw instead of returning if the file doesn't contain
    // a generator.
    if (!generatorNode)
      return;

    this.name = generatorNode.getAttribute("name");

    // Only set the source URI from the XML if we have a local URI, i.e.
    // if this is a locally-installed generator, since for remote generators
    // the source URI of the generator is the URI from which we downloaded it.
    if (this.localURI) {
      this.uri = generatorNode.hasAttribute("sourceURI") ?
                 this._ios.newURI(generatorNode.getAttribute("sourceURI"), null, null) :
                 this.localURI; // locally created generator without sourceURI
    }

    // Slurp the include/exclude rules that determine the pages to which
    // this generator applies.  Order is important, so we add the rules
    // in the order in which they appear in the XML.
    this._rules = [];
    var pages = generatorNode.getElementsByTagNameNS(MICSUM_NS, "pages")[0];
    if (pages) {
      // XXX Make sure the pages tag exists.
      for ( var i = 0; i < pages.childNodes.length ; i++ ) {
        var node = pages.childNodes[i];
        if (node.nodeType != node.ELEMENT_NODE ||
            node.namespaceURI != MICSUM_NS ||
            (node.nodeName != "include" && node.nodeName != "exclude"))
          continue;
        this._rules.push({ type: node.nodeName, regexp: new RegExp(node.textContent) });
      }
    }

    var templateNode = generatorNode.getElementsByTagNameNS(MICSUM_NS, "template")[0];
    if (templateNode) {
      this.template = templateNode.getElementsByTagNameNS(XSLT_NS, "transform")[0]
                      || templateNode.getElementsByTagNameNS(XSLT_NS, "stylesheet")[0];
    }
      // XXX Make sure the template is a valid XSL transform sheet.
  },

  generateMicrosummary: function MSD_generateMicrosummary(pageContent) {
    if (this.content)
      return this.content.replace(/^\s+|\s+$/g, "");
    else if (this.template)
      return this._processTemplate(pageContent);
    else
      throw("generateMicrosummary called on uninitialized microsummary generator");
  },
  
  _processTemplate: function MSD__processTemplate(doc) {
    LOG("processing template " + this.template + " against document " + doc);

    // XXX Should we just have one global instance of the processor?
    var processor = Cc["@mozilla.org/document-transformer;1?type=xslt"].
                    createInstance(Ci.nsIXSLTProcessor);
    processor.importStylesheet(this.template);
    var fragment = processor.transformToFragment(doc, doc);

    LOG("template processing result: " + fragment.textContent);

    // XXX When we support HTML microsummaries we'll need to do something
    // more sophisticated than just returning the text content of the fragment.
    return fragment.textContent;
  }
};





// Microsummary sets are collections of microsummaries.  They allow callers
// to register themselves as observers of the set, and when any microsummary
// in the set changes, the observers get notified.  Thus a caller can observe
// the set instead of each individual microsummary.

function MicrosummarySet() {
  this._observers = [];
  this._elements = [];
}

MicrosummarySet.prototype = {

  // IO Service
  __ios: null,
  get _ios() {
    if (!this.__ios)
      this.__ios = Cc["@mozilla.org/network/io-service;1"].
                   getService(Ci.nsIIOService);
    return this.__ios;
  },

  interfaces: [Ci.nsIMicrosummarySet,
               Ci.nsIMicrosummaryObserver,
               Ci.nsISupports],

  QueryInterface: function (iid) {
    //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
    if (!iid.equals(Ci.nsIMicrosummarySet) &&
        !iid.equals(Ci.nsIMicrosummaryObserver) &&
        !iid.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  _observers: null,
  _elements: null,

  // nsIMicrosummaryObserver

  onContentLoaded: function MSSet_onContentLoaded(microsummary) {
    for ( var i = 0; i < this._observers.length; i++ )
      this._observers[i].onContentLoaded(microsummary);
  },

  // nsIMicrosummarySet

  addObserver: function MSSet_addObserver(observer) {
    if (this._observers.length == 0) {
      for ( var i = 0 ; i < this._elements.length ; i++ )
        this._elements[i].addObserver(this);
    }

    // Register the observer, but only if it isn't already registered,
    // so that we don't call the same observer twice for any given change.
    if (this._observers.indexOf(observer) == -1)
      this._observers.push(observer);
  },
  
  removeObserver: function MSSet_removeObserver(observer) {
    //NS_ASSERT(this._observers.indexOf(observer) != -1,
    //          "can't remove microsummary observer " + observer + ": not registered");
  
    //this._observers =
    //  this._observers.filter(function(i) { observer != i });
    if (this._observers.indexOf(observer) != -1)
      this._observers.splice(this._observers.indexOf(observer), 1);
    
    if (this._observers.length == 0) {
      for ( var i = 0 ; i < this._elements.length ; i++ )
        this._elements[i].removeObserver(this);
    }
  },

  extractFromPage: function MSSet_extractFromPage(resource) {
    if (!resource.isXML && resource.contentType != "text/html")
      throw("page is neither HTML nor XML");

    // XXX Handle XML documents, whose microsummaries are specified
    // via processing instructions.

    var links = resource.content.getElementsByTagName("LINK");
    for ( var i = 0; i < links.length; i++ ) {
      var link = links[i];

      if (link.getAttribute("rel") != "microsummary")
        continue;

      // Unlike the "href" attribute, the "href" property contains
      // an absolute URI spec, so we use it here to create the URI.
      var generatorURI = this._ios.newURI(link.href,
                                          resource.content.characterSet,
                                          null);

      try {
        const securityManager = Cc["@mozilla.org/scriptsecuritymanager;1"].
                                getService(Ci.nsIScriptSecurityManager);
        securityManager.checkLoadURI(resource.uri,
                                     generatorURI,
                                     Ci.nsIScriptSecurityManager.DISALLOW_SCRIPT);
      }
      catch(e) {
        LOG("can't load generator " + generatorURI.spec + " from page " +
            resource.uri.spec + ": " + e);
        continue;
      }

      var microsummary = new Microsummary(resource.uri, generatorURI);
      this.AppendElement(microsummary);
    }
  },

  // XXX Turn this into a complete implementation of nsICollection?
  AppendElement: function MSSet_AppendElement(element) {
    // Query the element to a microsummary.
    // XXX Should we NS_ASSERT if this fails?
    element = element.QueryInterface(Ci.nsIMicrosummary);

    if (this._elements.indexOf(element) == -1) {
      this._elements.push(element);
      element.addObserver(this);
    }

    // Notify observers that an element has been appended.
    for ( var i = 0; i < this._observers.length; i++ )
      this._observers[i].onElementAppended(element);
  },

  Enumerate: function MSSet_Enumerate() {
    return new ArrayEnumerator(this._elements);
  }
};





/**
 * An enumeration of items in a JS array.
 * @constructor
 */
function ArrayEnumerator(aItems) {
  this._index = 0;
  if (aItems) {
    for (var i = 0; i < aItems.length; ++i) {
      if (!aItems[i])
        aItems.splice(i, 1);      
    }
  }
  this._contents = aItems;
}

ArrayEnumerator.prototype = {
  interfaces: [Ci.nsISimpleEnumerator, Ci.nsISupports],

  QueryInterface: function (iid) {
    //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
    if (!iid.equals(Ci.nsISimpleEnumerator) &&
        !iid.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  _index: 0,
  _contents: [],
  
  hasMoreElements: function() {
    return this._index < this._contents.length;
  },
  
  getNext: function() {
    return this._contents[this._index++];      
  }
};





function LOG(str) {
  dump(str + "\n");
  //var css = Cc['@mozilla.org/consoleservice;1'].
  //          getService(Ci.nsIConsoleService);
  //css.logStringMessage("MsS: " + str)
}





/**
 * A resource (page, microsummary, generator, etc.) identifiable by URI.
 * This object abstracts away much of the code for loading resources
 * and parsing their content if they are XML or HTML.
 * 
 * @constructor
 * 
 * @param   uri
 *          the location of the resource
 *
 */
function MicrosummaryResource(uri) {
  // Make sure we're not loading javascript: or data: URLs, which could
  // take advantage of the load to run code with chrome: privileges.
  // XXX Perhaps use nsIScriptSecurityManager.checkLoadURI instead.
  if (uri.scheme != "http" && uri.scheme != "https" && uri.scheme != "file")
    throw NS_ERROR_DOM_BAD_URI;

  this._uri = uri;
}

MicrosummaryResource.prototype = {
  // IO Service
  __ios: null,
  get _ios() {
    if (!this.__ios)
      this.__ios = Cc["@mozilla.org/network/io-service;1"].
                   getService(Ci.nsIIOService);
    return this.__ios;
  },

  _uri: null,
  get uri() {
    return this._uri;
  },

  _content: null,
  get content() {
    return this._content;
  },

  _contentType: null,
  get contentType() {
    return this._contentType;
  },

  _isXML: false,
  get isXML() {
    return this._isXML;
  },

  // A function to call when we finish loading/parsing the resource.
  _callback: null,

  // A hidden iframe to parse HTML content.
  _iframe: null,

  /**
   * Initialize the resource from an existing DOM document object.
   * 
   * @param   document
   *          a DOM document object
   *
   */
  initFromDocument: function MSR_initFromDocument(document) {
    this._content = document;
    this._contentType = document.contentType;

    // Normally we set this property based on whether or not
    // XMLHttpRequest parsed the content into an XML document object,
    // but since we already have the content, we have to analyze
    // its content type ourselves to see if it is XML.
    this._isXML = (this.contentType == "text/xml" ||
                   this.contentType == "application/xml" ||
                   /^.+\/.+\+xml$/.test(this.contentType));
  },

  /**
   * Destroy references to avoid leak-causing cycles.  Instantiators must call
   * this method on all instances they instantiate once they're done with them.
   *
   */
  destroy: function MSR_destroy() {
    this._uri = null;
    this._content = null;
    this._callback = null;
    if (this._iframe) {
      if (this._iframe && this._iframe.parentNode)
        this._iframe.parentNode.removeChild(this._iframe);
      this._iframe = null;
    }
  },

  /**
   * Load the resource.
   * 
   * @param   callback
   *          a function to invoke when the resource finishes loading
   *
   */
  load: function MSR_load(callback) {
    LOG(this.uri.spec + " loading");
  
    this._callback = callback;

    var request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance();
  
    var loadHandler = {
      _self: this,
      handleEvent: function MSR_loadHandler_handleEvent(event) {
        event.target.removeEventListener("load", this, false);
        try     { this._self._handleLoad(event) }
        finally { this._self = null }
      }
    };

    request = request.QueryInterface(Ci.nsIDOMEventTarget);
    request.addEventListener("load", loadHandler, false);
    
    request = request.QueryInterface(Ci.nsIXMLHttpRequest);
    request.open("GET", this.uri.spec, true);
    request.send(null);
  },

  _handleLoad: function MSR__handleLoad(event) {
    var request = event.target;

    if (request.responseXML) {
      this._isXML = true;
      // XXX Figure out the parsererror format and log a specific error.
      if (request.responseXML.documentElement.nodeName == "parsererror")
        throw(request.channel.originalURI.spec + " contains invalid XML");
      this._content = request.responseXML;
      this._contentType = request.channel.contentType;
      this._callback(this);
    }

    else if (request.channel.contentType == "text/html") {
      // request.responseText itself will always be UTF-16 because it is
      // a DOMString (XMLHttpRequest will convert it to that when it was
      // something else).  When we pass this to newURI(), it becomes UTF-8.
      // So the charset in the data: URL should always be UTF-8.
      var dataURISpec = "data:text/html;charset=UTF-8," + request.responseText;
      var dataURI = this._ios.newURI(dataURISpec, null, null);
      this._parse(dataURI);
    }

    else {
      // This catches text/plain as well as any other content types
      // not accounted for by the content type-specific code above.
      this._content = request.responseText;
      this._contentType = request.channel.contentType;
      this._callback(this);
    }
  },
  
  /**
   * Parse content encapsulated in a data URI.  Used by _load() to parse HTML.
   * We do this via hidden XUL iframes, which according to bz is the best way
   * to do it currently, since bug 102699 is hard to fix.
   * 
   * @param   dataURI
   *          the data URI containing the encapsulated content
   *
   */
  _parse: function MSR__parse(dataURI) {
    if (dataURI.scheme != "data")
      throw NS_ERROR_DOM_BAD_URI;
  
    // Find a window to stick our hidden iframe into.
    var windowMediator = Cc['@mozilla.org/appshell/window-mediator;1'].
                         getService(Ci.nsIWindowMediator);
    var window = windowMediator.getMostRecentWindow("navigator:browser");
    // XXX We can use other windows, too, so perhaps we should try to get
    // some other window if there's no browser window open.  Perhaps we should
    // even prefer other windows, since there's less chance of any browser
    // window machinery like throbbers treating our load like one initiated
    // by the user.
    if (!window)
      throw(dataURI.spec + " can't parse; no browser window");
    var document = window.document;
    var rootElement = document.documentElement;
  
    // Create an iframe, make it hidden, and secure it against untrusted content.
    this._iframe = document.createElement('iframe');
    this._iframe.setAttribute("collapsed", true);
    this._iframe.setAttribute("type", "content");
  
    // Insert the iframe into the window, creating the doc shell, then turn off
    // JavaScript and auth dialogs for security and turn off other things
    // to reduce network load.
    // XXX We should also turn off CSS.
    rootElement.appendChild(this._iframe);
    this._iframe.docShell.allowJavascript = false;
    this._iframe.docShell.allowAuth = false;
    this._iframe.docShell.allowPlugins = false;
    this._iframe.docShell.allowMetaRedirects = false;
    this._iframe.docShell.allowSubframes = false;
    this._iframe.docShell.allowImages = false;
  
    var parseHandler = {
      _self: this,
      handleEvent: function MSR_parseHandler_handleEvent(event) {
        // Appending a new iframe to a XUL window makes it immediately start
        // loading "about:blank", and we'll probably get notified about that
        // first, so make sure we check for that and drop it on the floor.
        if (event.originalTarget.location == "about:blank")
          return;

        event.target.removeEventListener("DOMContentLoaded", this, false);
        try     { this._self._handleParse(event) }
        finally { this._self = null }
      }
    };

    // Register the parse handler as a load event listener and kick off the load.
    // We use the URI loader instead of the iframe's src attribute so we can set
    // the LOAD_BACKGROUND flag.
    this._iframe.addEventListener("DOMContentLoaded", parseHandler, true);
    var channel = this._ios.newChannelFromURI(dataURI);
    channel.loadFlags |= Ci.nsIRequest.LOAD_BACKGROUND;
    var uriLoader = Cc["@mozilla.org/uriloader;1"].getService(Ci.nsIURILoader);
    uriLoader.openURI(channel, true, this._iframe.docShell);
  },

  /**
   * Handle a load event for the iframe-based parser.
   * 
   * @param   event
   *          the event object representing the load event
   *
   */
  _handleParse: function MSR__handleParse(event) {
    // XXX Make sure the parse was successful?

    this._content = this._iframe.contentDocument;
    this._contentType = this._iframe.contentDocument.contentType;
    this._callback(this);
  }

};

/**
 * Get a resource currently loaded into a browser window.  Checks windows
 * one at a time, starting with the frontmost (a.k.a. most recent) one.
 * 
 * @param   uri
 *          the URI of the resource
 *
 * @returns a Resource object, if the resource is currently loaded
 *          into a browser window; otherwise null
 *
 */
function getLoadedMicrosummaryResource(uri) {
  var mediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                 getService(Ci.nsIWindowMediator);

  // Apparently the Z order enumerator is broken on Linux per bug 156333.
  //var windows = mediator.getZOrderDOMWindowEnumerator("navigator:browser", true);
  var windows = mediator.getEnumerator("navigator:browser");

  while (windows.hasMoreElements()) {
    var win = windows.getNext();
    var tabBrowser = win.document.getElementById("content");
    for ( var i = 0; i < tabBrowser.browsers.length; i++ ) {
      var browser = tabBrowser.browsers[i];
      if (uri.equals(browser.currentURI)) {
        var resource = new MicrosummaryResource(uri);
        resource.initFromDocument(browser.contentDocument);
        return resource;
      }
    }
  }

  return null;
}





// From http://lxr.mozilla.org/mozilla/source/browser/components/search/nsSearchService.js

/**
 * Removes all characters not in the "chars" string from aName.
 *
 * @returns a sanitized name to be used as a filename, or a random name
 *          if a sanitized name cannot be obtained (if aName contains
 *          no valid characters).
 */
function sanitizeName(aName) {
  const chars = "-abcdefghijklmnopqrstuvwxyz0123456789";

  var name = aName.toLowerCase();
  name = name.replace(/ /g, "-");
  //name = name.split("").filter(function (el) {
  //                               return chars.indexOf(el) != -1;
  //                             }).join("");
  var filteredName = "";
  for ( var i = 0 ; i < name.length ; i++ )
    if (chars.indexOf(name[i]) != -1)
      filteredName += name[i];
  name = filteredName;

  if (!name) {
    // Our input had no valid characters - use a random name
    for (var i = 0; i < 8; ++i)
      name += chars.charAt(Math.round(Math.random() * (chars.length - 1)));
  }

  return name;
}





var gModule = {
  registerSelf: function(componentManager, fileSpec, location, type) {
    componentManager = componentManager.QueryInterface(Ci.nsIComponentRegistrar);
    
    for (var key in this._objects) {
      var obj = this._objects[key];
      componentManager.registerFactoryLocation(obj.CID,
                                               obj.className,
                                               obj.contractID,
                                               fileSpec,
                                               location,
                                               type);
    }
  },
  
  unregisterSelf: function(componentManager, fileSpec, location) {},

  getClassObject: function(componentManager, cid, iid) {
    if (!iid.equals(Components.interfaces.nsIFactory))
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  
    for (var key in this._objects) {
      if (cid.equals(this._objects[key].CID))
      return this._objects[key].factory;
    }
    
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  
  _objects: {
    service: {
      CID        : Components.ID("{460a9792-b154-4f26-a922-0f653e2c8f91}"),
      contractID : "@mozilla.org/microsummary/service;1",
      className  : "Microsummary Service",
      factory    : MicrosummaryServiceFactory = {
                     createInstance: function(aOuter, aIID) {
                       if (aOuter != null)
                         throw Components.results.NS_ERROR_NO_AGGREGATION;
                       var svc = new MicrosummaryService();
                       svc._init();
                      return svc.QueryInterface(aIID);
                     }
                   }
    }
  },
  
  canUnload: function(componentManager) {
    return true;
  }
};

function NSGetModule(compMgr, fileSpec) {
  return gModule;
}
