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
 *  Simon Bünzli <zeniko@gmail.com>
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
const Cr = Components.results;

const PERMS_FILE    = 0644;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_TRUNCATE = 0x20;

const NS_ERROR_MODULE_DOM = 2152923136;
const NS_ERROR_DOM_BAD_URI = NS_ERROR_MODULE_DOM + 1012;

// How often to check for microsummaries that need updating, in milliseconds.
const CHECK_INTERVAL = 15 * 1000; // 15 seconds

const MICSUM_NS = new Namespace("http://www.mozilla.org/microsummaries/0.1");
const XSLT_NS = new Namespace("http://www.w3.org/1999/XSL/Transform");

#ifdef MOZ_PLACES_BOOKMARKS
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

const MAX_SUMMARY_LENGTH = 4096;

function MicrosummaryService() {}

MicrosummaryService.prototype = {

#ifdef MOZ_PLACES_BOOKMARKS
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

#ifndef MOZ_PLACES_BOOKMARKS
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

  // The update interval as specified by the user (defaults to 30 minutes)
  get _updateInterval() {
    var updateInterval =
      getPref("browser.microsummary.updateInterval", 30);
    // the minimum update interval is 1 minute
    return Math.max(updateInterval, 1) * 60 * 1000;
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
      throw Cr.NS_ERROR_NO_INTERFACE;
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
    var bookmarks = this._getBookmarks();

    var now = Date.now();
    var updateInterval = this._updateInterval;
    for ( var i = 0; i < bookmarks.length; i++ ) {
      var bookmarkID = bookmarks[i];

      // Skip this page if its microsummary hasn't expired yet.
      if (this._hasField(bookmarkID, FIELD_MICSUM_EXPIRATION) &&
          this._getField(bookmarkID, FIELD_MICSUM_EXPIRATION) > now)
        continue;

      // Reset the expiration time immediately, so if the refresh is failing
      // we don't try it every 15 seconds, potentially overloading the server.
      this._setField(bookmarkID, FIELD_MICSUM_EXPIRATION, now + updateInterval);

      // Try to update the microsummary, but trap errors, so an update
      // that throws doesn't prevent us from updating the rest of them.
      try {
        this.refreshMicrosummary(bookmarkID);
      }
      catch(ex) {
        Components.utils.reportError(ex);
      }
    }
  },
  
  _updateMicrosummary: function MSS__updateMicrosummary(bookmarkID, microsummary) {
    this._setField(bookmarkID, FIELD_GENERATED_TITLE, microsummary.content);
    this._setField(bookmarkID, FIELD_MICSUM_EXPIRATION,
                   Date.now() + (microsummary.updateInterval || this._updateInterval));

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

    // Fix the generator's ID if it was installed before we started using URNs
    // to uniquely identify generators.
    // XXX This code can go away after Fx2 beta2, when enough users will have
    // upgraded from earlier versions to make bug 346822 no longer significant.
    this._fixGeneratorID(resource.content, resource.uri);

    var generator = new MicrosummaryGenerator();
    generator.localURI = resource.uri;
    generator.initFromXML(resource.content);

    // Add the generator to the local generators cache.
    // XXX Figure out why Firefox crashes on shutdown if we index generators
    // by uri.spec but doesn't crash if we index by uri.spec.split().join().
    //this._localGenerators[generator.uri.spec] = generator;
    this._localGenerators[generator.uri.spec.split().join()] = generator;

    LOG("loaded local microsummary generator\n" +
        "  file: " + generator.localURI.spec + "\n" +
        "    ID: " + generator.uri.spec);
  },

  /**
   * Fix the ID of a local generator that was installed before we started
   * using URNs to uniquely identify local generators.
   *
   * @param   xmlDefinition
   *          an nsIDOMDocument XML document defining the generator
   * @param   localURI
   *          an nsIURI file URI to the generator's local file
   * 
   */
  _fixGeneratorID: function MSS__fixGeneratorID(xmlDefinition, localURI) {
    var generatorNode = xmlDefinition.getElementsByTagNameNS(MICSUM_NS, "generator")[0];

    if (!generatorNode)
      return;

    // Don't fix generators that have already been fixed or were installed
    // after we switched to identifying generators by URN.
    if (generatorNode.hasAttribute("uri"))
      return;

    // Don't fix generators that don't have any ID at all (we fall back to
    // the local URI in these cases, which is useful for developers during
    // the process of developing generators).
    if (!generatorNode.hasAttribute("sourceURI"))
      return;

    var oldURI = generatorNode.getAttribute("sourceURI");
    var newURI = "urn:source:" + oldURI;

    LOG("fixing generator with old-style ID\n" +
        "  old ID: " + oldURI + "\n" +
        "  new ID: " + newURI);

    // Update the XML definition to reflect the change.
    generatorNode.setAttribute("uri", newURI);

    // Save the updated XML definition to the local file.
    var file = localURI.QueryInterface(Ci.nsIFileURL).file.clone();
    this._saveGeneratorXML(xmlDefinition, file);

    // Update bookmarks in the bookmarks datastore that are using
    // this microsummary generator to reflect the change.
    this._changeField(FIELD_MICSUM_GEN_URI, oldURI, newURI);
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
    rootNode.setAttribute("uri", "urn:source:" + resource.uri.spec);

    this.installGenerator(resource.content);
  },
 
  /**
   * Install a microsummary generator from the given XML definition.
   *
   * @param   xmlDefinition
   *          an nsIDOMDocument XML document defining the generator
   *
   * @returns the newly-installed nsIMicrosummaryGenerator generator
   *
   */
  installGenerator: function MSS_installGenerator(xmlDefinition) {
    var rootNode = xmlDefinition.getElementsByTagNameNS(MICSUM_NS, "generator")[0];
 
    var generatorID = rootNode.getAttribute("uri");
 
    // The existing cache entry for this generator, if it is already installed.
    var generator = this._localGenerators[generatorID];

    var topic;
    var file;
    if (generator) {
      // This generator is already installed.  Save it in the existing file
      // (i.e. update the existing generator with the newly downloaded XML).
      file = generator.localURI.QueryInterface(Ci.nsIFileURL).file.clone();
      topic = "microsummary-generator-updated";
    }
    else {
      // This generator is not already installed.  Save it as a new file.
      var generatorName = rootNode.getAttribute("name");
      var fileName = sanitizeName(generatorName) + ".xml";
      file = this._dirs.get("UsrMicsumGens", Ci.nsIFile);
      file.append(fileName);
      file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
      generator = new MicrosummaryGenerator();
      generator.localURI = this._ios.newFileURI(file);
      this._localGenerators[generatorID] = generator;
      topic = "microsummary-generator-installed";
    }
 
    // Initialize (or reinitialize) the generator from its XML definition,
    // the save the definition to the generator's file.
    generator.initFromXML(xmlDefinition);
    this._saveGeneratorXML(xmlDefinition, file);

    LOG("installed generator " + generatorID);

    this._obs.notifyObservers(generator, topic, null);

    return generator;
  },

  /**
   * Save a generator's XML definition to a local file.
   *
   * @param   xmlDefinition
   *          an nsIDOMDocument XML document defining the generator
   * @param   file
   *          an nsIFile file representing the generator's local file
   * 
   */
  _saveGeneratorXML: function MSS_saveGeneratorXML(xmlDefinition, file) {
    LOG("saving definition to " + file.path);

    // Write the generator XML to the local file.
    var outputStream = Cc["@mozilla.org/network/safe-file-output-stream;1"].
                       createInstance(Ci.nsIFileOutputStream);
    var localFile = file.QueryInterface(Ci.nsILocalFile);
    outputStream.init(localFile, (MODE_WRONLY | MODE_TRUNCATE | MODE_CREATE),
                      PERMS_FILE, 0);
    var serializer = Cc["@mozilla.org/xmlextras/xmlserializer;1"].
                     createInstance(Ci.nsIDOMSerializer);
    serializer.serializeToStream(xmlDefinition, outputStream, null);
    if (outputStream instanceof Ci.nsISafeOutputStream) {
      try       { outputStream.finish() }
      catch (e) { outputStream.close()  }
    }
    else
      outputStream.close();
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
        var microsummary = new Microsummary(pageURI, generator);

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
        if (resource)
          resource.destroy();
        LOG("error downloading page to extract its microsummaries: " + e);
      }
    }

    return microsummaries;
  },

  /**
   * Change all occurrences of a specific value in a given field to a new value.
   *
   * @param   fieldName
   *          the name of the field whose values should be changed
   * @param   oldValue
   *          the value that should be changed
   * @param   newValue
   *          the value to which it should be changed
   *
   */
  _changeField: function MSS__changeField(fieldName, oldValue, newValue) {
    var bookmarks = this._getBookmarks();

    for ( var i = 0; i < bookmarks.length; i++ ) {
      var bookmarkID = bookmarks[i];

      if (this._hasField(bookmarkID, fieldName) &&
          this._getField(bookmarkID, fieldName) == oldValue)
        this._setField(bookmarkID, fieldName, newValue);
    }
  },

#ifdef MOZ_PLACES_BOOKMARKS
  /**
   * Get the set of bookmarks with microsummaries.
   *
   * This is the internal version of this method, which is not accessible
   * via XPCOM but is more performant; inside this component, use this version.
   * Outside the component, use getBookmarks (no underscore prefix) instead.
   *
   * @returns an array of place: uris representing bookmarks items
   *
   */
  _getBookmarks: function MSS__getBookmarks() {
    var bookmarks;

    // This try/catch block is a temporary workaround for bug 336194.
    try {
      bookmarks = this._ans.getPagesWithAnnotation(FIELD_MICSUM_GEN_URI, {});
    }
    catch(e) {
      bookmarks = [];
    }

    return bookmarks;
  },

  _getField: function MSS__getField(aPlaceURI, aFieldName) {
    aPlaceURI.QueryInterface(Ci.nsIURI);
    var fieldValue;

    switch(aFieldName) {
    case FIELD_MICSUM_EXPIRATION:
      fieldValue = this._ans.getAnnotationInt64(aPlaceURI, aFieldName);
      break;
    case FIELD_MICSUM_GEN_URI:
    case FIELD_GENERATED_TITLE:
    case FIELD_CONTENT_TYPE:
    default:
      fieldValue = this._ans.getAnnotationString(aPlaceURI, aFieldName);
      break;
    }
    
    return fieldValue;
  },

  _setField: function MSS__setField(aPlaceURI, aFieldName, aFieldValue) {
    aPlaceURI.QueryInterface(Ci.nsIURI);

    switch(aFieldName) {
    case FIELD_MICSUM_EXPIRATION:
      this._ans.setAnnotationInt64(aPlaceURI,
                                   aFieldName,
                                   aFieldValue,
                                   0,
                                   this._ans.EXPIRE_NEVER);
      break;
    case FIELD_MICSUM_GEN_URI:
    case FIELD_GENERATED_TITLE:
    case FIELD_CONTENT_TYPE:
    default:
      this._ans.setAnnotationString(aPlaceURI,
                                    aFieldName,
                                    aFieldValue,
                                    0,
                                    this._ans.EXPIRE_NEVER);
      break;
    }
  },

  _clearField: function MSS__clearField(aPlaceURI, aFieldName) {
    aPlaceURI.QueryInterface(Ci.nsIURI);
    this._ans.removeAnnotation(aPlaceURI, aFieldName);
  },

  _hasField: function MSS__hasField(aPlaceURI, fieldName) {
    aPlaceURI.QueryInterface(Ci.nsIURI);
    return this._ans.hasAnnotation(aPlaceURI, fieldName);
  },

  _getPageForBookmark: function MSS__getPageForBookmark(aPlaceURI) {
    aPlaceURI.QueryInterface(Ci.nsIURI);

    // Manually get the bookmark identifier out of the place uri until
    // the query system supports parsing this sort of place: uris
    var matches = /moz_bookmarks.id=([0-9]+)/.exec(aPlaceURI.spec);
    if (matches) {
      var bookmarkId = parseInt(matches[1]);
      return this._bms.getBookmarkURI(bookmarkId);
    }

    return null;
  },

#else
  /**
   * Get the set of bookmarks with microsummaries.
   *
   * This is the internal version of this method, which is not accessible
   * via XPCOM but is more performant; inside this component, use this version.
   * Outside the component, use getBookmarks (no underscore prefix) instead.
   *
   * @returns an array of bookmark IDs
   *
   */
  _getBookmarks: function MSS__getBookmarks() {
    var bookmarks = [];

    var resources = this._bmds.GetSources(this._resource(FIELD_RDF_TYPE),
                                          this._resource(VALUE_MICSUM_BOOKMARK),
                                          true);
    while (resources.hasMoreElements()) {
      var resource = resources.getNext().QueryInterface(Ci.nsIRDFResource);

      // When a bookmark gets deleted or cut, most of its arcs get removed
      // from the data source, but a few of them remain, in particular its RDF
      // type arc.  So just because this resource has a MicsumBookmark type,
      // that doesn't mean it's a real bookmark!  We need to check.
      if (!this._bms.isBookmarkedResource(resource))
        continue;

      bookmarks.push(resource);
    }

    return bookmarks;
  },

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
  },

  _hasField: function MSS__hasField(bookmarkID, fieldName) {
    var bookmarkResource = bookmarkID.QueryInterface(Ci.nsIRDFResource);

    var node = this._bmds.GetTarget(bookmarkResource,
                                    this._resource(fieldName),
                                    true);
    return node ? true : false;
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
   * Get the set of bookmarks with microsummaries.
   *
   * Bookmark IDs are nsIRDFResource objects on builds with old RDF-based
   * bookmarks and nsIURI objects on builds with new Places-based bookmarks.
   *
   * This is the external version of this method and is accessible via XPCOM.
   * Use it outside this component. Inside the component, use _getBookmarks
   * (with underscore prefix) instead for performance.
   *
   * @returns an nsISimpleEnumerator enumeration of bookmark IDs
   *
   */
  getBookmarks: function MSS_getBookmarks() {
    return new ArrayEnumerator(this._getBookmarks());
  },

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
    
    var localGenerator = this._localGenerators[generatorURI.spec];

    var microsummary = new Microsummary(pageURI, localGenerator);
    if (!localGenerator)
      microsummary.generator.uri = generatorURI;

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
#ifndef MOZ_PLACES_BOOKMARKS
    // Make sure that the bookmark is of type MicsumBookmark
    // because that's what the template rules are matching
    if (this._getField(bookmarkID, FIELD_RDF_TYPE) != VALUE_MICSUM_BOOKMARK) {
      // Force the bookmark trees to rebuild, since they don't seem
      // to be rebuilding on their own (bug 348928).
      this._bmds.beginUpdateBatch();

      var bookmarkResource = bookmarkID.QueryInterface(Ci.nsIRDFResource);
      if (this._hasField(bookmarkID, FIELD_RDF_TYPE)) {
        var oldValue = this._getField(bookmarkID, FIELD_RDF_TYPE);
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

      this._bmds.endUpdateBatch();
    }
#endif
    this._setField(bookmarkID, FIELD_MICSUM_GEN_URI, microsummary.generator.uri.spec);

    if (microsummary.content) {
      this._updateMicrosummary(bookmarkID, microsummary);
    }
    else {
      // Display a static title initially (unless there's already one set)
      if (!this._hasField(bookmarkID, FIELD_GENERATED_TITLE))
        this._setField(bookmarkID, FIELD_GENERATED_TITLE,
                       microsummary.generator.name || microsummary.generator.uri.spec);

      this.refreshMicrosummary(bookmarkID);
    }
  },

  /**
   * Remove the current microsummary for the given bookmark.
   *
   * @param   bookmarkID
   *          the bookmark for which to remove the current microsummary
   *
   */
  removeMicrosummary: function MSS_removeMicrosummary(bookmarkID) {
#ifndef MOZ_PLACES_BOOKMARKS
    // Set the bookmark's RDF type back to the normal bookmark type
    if (this._getField(bookmarkID, FIELD_RDF_TYPE) == VALUE_MICSUM_BOOKMARK) {
      // Force the bookmark trees to rebuild, since they don't seem
      // to be rebuilding on their own (bug 348928).
      this._bmds.beginUpdateBatch();

      var bookmarkResource = bookmarkID.QueryInterface(Ci.nsIRDFResource);
      this._bmds.Change(bookmarkResource,
                        this._resource(FIELD_RDF_TYPE),
                        this._resource(VALUE_MICSUM_BOOKMARK),
                        this._resource(VALUE_NORMAL_BOOKMARK));

      this._bmds.endUpdateBatch();
    }
#endif

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

    if (microsummary.generator.uri.equals(this._uri(currentGen)))
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
    if (!pageURI)
      throw("can't get URL for bookmark with ID " + bookmarkID);
    var generatorURI = this._uri(this._getField(bookmarkID, FIELD_MICSUM_GEN_URI));

    var localGenerator = this._localGenerators[generatorURI.spec];

    var microsummary = new Microsummary(pageURI, localGenerator);
    if (!localGenerator)
      microsummary.generator.uri = generatorURI;

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





function Microsummary(pageURI, generator) {
  this._observers = [];
  this.pageURI = pageURI;
  this.generator = generator ? generator : new MicrosummaryGenerator();
}

Microsummary.prototype = {
  // The microsummary service.
  __mss: null,
  get _mss() {
    if (!this.__mss)
      this.__mss = Cc["@mozilla.org/microsummary/service;1"].
                   getService(Ci.nsIMicrosummaryService);
    return this.__mss;
  },

  // IO Service
  __ios: null,
  get _ios() {
    if (!this.__ios)
      this.__ios = Cc["@mozilla.org/network/io-service;1"].
                   getService(Ci.nsIIOService);
    return this.__ios;
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

  interfaces: [Ci.nsIMicrosummary, Ci.nsISupports],

  // nsISupports

  QueryInterface: function (iid) {
    //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
    if (!iid.equals(Ci.nsIMicrosummary) &&
        !iid.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },

  // nsIMicrosummary

  _content: null,
  get content() {
    // If we have everything we need to generate the content, generate it.
    if (this._content == null &&
        this.generator.loaded &&
        (this.pageContent || !this.generator.needsPageContent)) {
      this._content = this.generator.generateMicrosummary(this.pageContent);
      this.updateInterval = this.generator.calculateUpdateInterval(this.pageContent);
    }

    // Note: we return "null" if the content wasn't already generated and we
    // couldn't retrieve it from the generated title annotation or generate it
    // ourselves.  So callers shouldn't count on getting content; instead,
    // they should call update if the return value of this getter is "null",
    // setting an observer to tell them when content generation is done.
    return this._content;
  },
  set content(newValue) { this._content = newValue },

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

  _updateInterval: null,
  get updateInterval()         { return this._updateInterval; },
  set updateInterval(newValue) { this._updateInterval = newValue; },

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
        "\nwith generator:\n  " + this.generator.uri.spec);

    var t = this;

    // If we don't have the generator, download it now.  After it downloads,
    // we'll re-call this method to continue updating the microsummary.
    if (!this.generator.loaded) {
      // If this generator is identified by a URN, then it's a local generator
      // that should have been cached on application start, so it's missing.
      if (this.generator.uri.scheme == "urn") {
        // If it was installed via nsSidebar::addMicrosummaryGenerator (i.e. it
        // has a URN that identifies the source URL from which we installed it),
        // try to reinstall it (once).
        if (/^source:/.test(this.generator.uri.path)) {
          this._reinstallMissingGenerator();
          return;
        }
        else
          throw "missing local generator: " + this.generator.uri.spec;
      }

      LOG("generator not yet loaded; downloading it");
      var generatorCallback =
        function MS_generatorCallback(resource) {
          try     { t._handleGeneratorLoad(resource) }
          finally { resource.destroy() }
        };
      var resource = new MicrosummaryResource(this.generator.uri);
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
    this.updateInterval = this.generator.calculateUpdateInterval(this.pageContent);
    this.pageContent = null;
    for ( var i = 0; i < this._observers.length; i++ )
      this._observers[i].onContentLoaded(this);

    LOG("generated microsummary: " + this.content);
  },

  _handleGeneratorLoad: function MS__handleGeneratorLoad(resource) {
    LOG(this.generator.uri.spec + " microsummary generator downloaded");
    if (resource.isXML)
      this.generator.initFromXML(resource.content);
    else if (resource.contentType == "text/plain")
      this.generator.initFromText(resource.content);
    else if (resource.contentType == "text/html")
      this.generator.initFromText(resource.content.body.textContent);
    else
      throw("generator is neither XML nor plain text");

    // Only trigger a [content] update if we were able to init the generator. 
    if (this.generator.loaded)
      this.update();
  },

  _handlePageLoad: function MS__handlePageLoad(resource) {
    if (!resource.isXML && resource.contentType != "text/html")
      throw("page is neither HTML nor XML");

    this.pageContent = resource.content;
    this.update();
  },

  /**
   * Try to reinstall a missing local generator that was originally installed
   * from a URL using nsSidebar::addMicrosumaryGenerator.
   *
   */
  _reinstallMissingGenerator: function MS__reinstallMissingGenerator() {
    LOG("attempting to reinstall missing generator " + this.generator.uri.spec);

    var t = this;

    var loadCallback =
      function MS_missingGeneratorLoadCallback(resource) {
        try     { t._handleMissingGeneratorLoad(resource) }
        finally { resource.destroy() }
      };

    var errorCallback =
      function MS_missingGeneratorErrorCallback(resource) {
        try     { t._handleMissingGeneratorError(resource) }
        finally { resource.destroy() }
      };

    try {
      // Extract the URI from which the generator was originally installed.
      var sourceURL = this.generator.uri.path.replace(/^source:/, "");
      var sourceURI = this._uri(sourceURL);

      var resource = new MicrosummaryResource(sourceURI);
      resource.load(loadCallback, errorCallback);
    }
    catch(ex) {
      Components.utils.reportError(ex);
      this._handleMissingGeneratorError();
    }
  },

  /**
   * Handle a load event for a missing local generator by trying to reinstall
   * the generator.  If this fails, call _handleMissingGeneratorError to unset
   * microsummaries for bookmarks using this generator so we don't repeatedly
   * try to reinstall the generator, creating too much traffic to the website
   * from which we downloaded it.
   *
   * @param resource
   *        the nsIMicrosummaryResource representing the downloaded generator
   *
   */
  _handleMissingGeneratorLoad: function MS__handleMissingGeneratorLoad(resource) {
    try {
      // Make sure the generator is XML, since local generators have to be.
      if (!resource.isXML)
        throw("downloaded, but not XML " + this.generator.uri.spec);

      // Store the generator's ID in its XML definition.
      var generatorID = this.generator.uri.spec;
      resource.content.documentElement.setAttribute("uri", generatorID);

      // Reinstall the generator and replace our placeholder generator object
      // with the newly installed generator.
      this.generator = this._mss.installGenerator(resource.content);

      // A reinstalled generator should always be loaded.  But just in case
      // it isn't, throw an error so we don't get into an infinite loop
      // (otherwise this._update would try again to reinstall it).
      if (!this.generator.loaded)
        throw("supposedly installed, but not in cache " + this.generator.uri.spec);
    }
    catch(ex) {
      Components.utils.reportError(ex);
      this._handleMissingGeneratorError(resource);
      return;
    }
  
    LOG("reinstall succeeded; resuming update " + this.generator.uri.spec);
    this.update();
  },

  /**
   * Handle an error event for a missing local generator load by unsetting
   * the microsummaries for bookmarks using this generator so we don't
   * repeatedly try to reinstall the generator, creating too much traffic
   * to the website from which we downloaded it.
   *
   * @param resource
   *        the nsIMicrosummaryResource representing the downloaded generator
   *
   */
  _handleMissingGeneratorError: function MS__handleMissingGeneratorError(resource) {
    LOG("reinstall failed; removing microsummaries " + this.generator.uri.spec);
    var bookmarks = this._mss.getBookmarks();
    while (bookmarks.hasMoreElements()) {
      var bookmarkID = bookmarks.getNext();
      var microsummary = this._mss.getMicrosummary(bookmarkID);
      if (microsummary.generator.uri.equals(this.generator.uri)) {
        LOG("removing microsummary for " + microsummary.pageURI.spec);
        this._mss.removeMicrosummary(bookmarkID);
      }
    }
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
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },

  // nsIMicrosummaryGenerator

  // Normally this is just the URL from which we download the generator,
  // but for generators stored in the app or profile generators directory
  // it's the value of the generator tag's "uri" attribute (or its local URI
  // should the "uri" attribute be missing).
  _uri: null,
  get uri() { return this._uri || this.localURI },
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

  _loaded: false,
  get loaded() { return this._loaded },
  set loaded(newValue) { this._loaded = newValue },

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
    this.loaded = true;
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
    if (!generatorNode)
      throw Cr.NS_ERROR_FAILURE;

    this.name = generatorNode.getAttribute("name");

    // If this is a local generator (i.e. it has a local URI), then we have
    // to retrieve its URI from the "uri" attribute of its generator tag.
    if (this.localURI && generatorNode.hasAttribute("uri")) {
      this.uri = this._ios.newURI(generatorNode.getAttribute("uri"), null, null);
    }

    function getFirstChildByTagName(tagName, parentNode, namespace) {
      var nodeList = parentNode.getElementsByTagNameNS(namespace, tagName);
      for (var i = 0; i < nodeList.length; i++) {
        // Make sure that the node is a direct descendent of the generator node
        if (nodeList[i].parentNode == parentNode)
          return nodeList[i];
      }
      return null;
    }

    // Slurp the include/exclude rules that determine the pages to which
    // this generator applies.  Order is important, so we add the rules
    // in the order in which they appear in the XML.
    this._rules = [];
    var pages = getFirstChildByTagName("pages", generatorNode, MICSUM_NS);
    if (pages) {
      // XXX Make sure the pages tag exists.
      for ( var i = 0; i < pages.childNodes.length ; i++ ) {
        var node = pages.childNodes[i];
        if (node.nodeType != node.ELEMENT_NODE ||
            node.namespaceURI != MICSUM_NS ||
            (node.nodeName != "include" && node.nodeName != "exclude"))
          continue;
        var urlRegexp = node.textContent.replace(/^\s+|\s+$/g, "");
        this._rules.push({ type: node.nodeName, regexp: new RegExp(urlRegexp) });
      }
    }

    // allow the generators to set individual update values (even varying
    // depending on certain XPath expressions)
    var update = getFirstChildByTagName("update", generatorNode, MICSUM_NS);
    if (update) {
      function _parseInterval(string) {
        // convert from minute fractions to milliseconds
        // and ensure a minimum value of 1 minute
        return Math.round(Math.max(parseFloat(string) || 0, 1) * 60 * 1000);
      }

      this._unconditionalUpdateInterval =
        update.hasAttribute("interval") ?
        _parseInterval(update.getAttribute("interval")) : null;

      // collect the <condition expression="XPath Expression" interval="time"/> clauses
      this._updateIntervals = new Array();
      for (i = 0; i < update.childNodes.length; i++) {
        node = update.childNodes[i];
        if (node.nodeType != node.ELEMENT_NODE || node.namespaceURI != MICSUM_NS ||
            node.nodeName != "condition")
          continue;
        if (!node.getAttribute("expression") || !node.getAttribute("interval")) {
          LOG("ignoring incomplete conditional update interval for " + this.uri.spec);
          continue;
        }
        this._updateIntervals.push({
          expression: node.getAttribute("expression"),
          interval: _parseInterval(node.getAttribute("interval"))
        });
      }
    }

    var templateNode = getFirstChildByTagName("template", generatorNode, MICSUM_NS);
    if (templateNode) {
      this.template = getFirstChildByTagName("transform", templateNode, XSLT_NS) ||
                      getFirstChildByTagName("stylesheet", templateNode, XSLT_NS);
    }
    // XXX Make sure the template is a valid XSL transform sheet.

    this.loaded = true;
  },

  generateMicrosummary: function MSD_generateMicrosummary(pageContent) {

    var content;

    if (this.content) {
      content = this.content;
    } else if (this.template) {
      content = this._processTemplate(pageContent);
    } else
      throw("generateMicrosummary called on uninitialized microsummary generator");

    // Clean up the output
    content = content.replace(/^\s+|\s+$/g, "");
    if (content.length > MAX_SUMMARY_LENGTH) 
      content = content.substring(0, MAX_SUMMARY_LENGTH);

    return content;
  },

  calculateUpdateInterval: function MSD_calculateUpdateInterval(doc) {
    if (this.content || !this._updateIntervals || !doc)
      return null;

    for (var i = 0; i < this._updateIntervals.length; i++) {
      try {
        if (doc.evaluate(this._updateIntervals[i].expression, doc, null,
                         Ci.nsIDOMXPathResult.BOOLEAN_TYPE, null).booleanValue)
          return this._updateIntervals[i].interval;
      }
      catch (ex) {
        Components.utils.reportError(ex);
        // remove the offending conditional update interval
        this._updateIntervals.splice(i--, 1);
      }
    }

    return this._unconditionalUpdateInterval;
  },

  _processTemplate: function MSD__processTemplate(doc) {
    LOG("processing template " + this.template + " against document " + doc);

    // XXX Should we just have one global instance of the processor?
    var processor = Cc["@mozilla.org/document-transformer;1?type=xslt"].
                    createInstance(Ci.nsIXSLTProcessor);

    // Turn off document loading of all kinds (document(), <include>, <import>)
    // for security (otherwise local generators would be able to load local files).
    processor.flags |= Ci.nsIXSLTProcessorPrivate.DISABLE_ALL_LOADS;

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
      throw Cr.NS_ERROR_NO_INTERFACE;
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

    var links = resource.content.getElementsByTagName("link");
    for ( var i = 0; i < links.length; i++ ) {
      var link = links[i];

      if(!link.hasAttribute("rel"))
        continue;

      var relAttr = link.getAttribute("rel");

      // The attribute's value can be a space-separated list of link types,
      // check to see if "microsummary" is one of them.
      var linkTypes = relAttr.split(/\s+/);
      if (!linkTypes.some( function(v) { return v.toLowerCase() == "microsummary"; }))
        continue;


      // Look for a TITLE attribute to give the generator a nice name in the UI.
      var linkTitle = link.getAttribute("title");


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

      var microsummary = new Microsummary(resource.uri, null);
      microsummary.generator.name = linkTitle;
      microsummary.generator.uri  = generatorURI;
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
      throw Cr.NS_ERROR_NO_INTERFACE;
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





/**
 * Outputs aText to the JavaScript console as well as to stdout if the
 * microsummary logging pref (browser.microsummary.log) is set to true.
 * 
 * @param aText
 *        the text to log
 */
function LOG(aText) {
  if (getPref("browser.microsummary.log", false)) {
    dump("*** Microsummaries: " +  aText + "\n");
    var consoleService = Cc["@mozilla.org/consoleservice;1"].
                         getService(Ci.nsIConsoleService);
    consoleService.logStringMessage(aText);
  }
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
  _loadCallback: null,

  // A function to call if we get an error while loading/parsing the resource.
  _errorCallback: null,

  // A hidden iframe to parse HTML content.
  _iframe: null,

  // Implement notification callback interfaces so we can suppress UI
  // and abort loads for bad SSL certs and HTTP authorization requests.
  
  // Interfaces this component implements.
  interfaces: [Ci.nsIBadCertListener,
               Ci.nsIAuthPromptProvider,
               Ci.nsIAuthPrompt,
               Ci.nsIPrompt,
               Ci.nsIProgressEventSink,
               Ci.nsIInterfaceRequestor,
               Ci.nsISupports],

  // nsISupports

  QueryInterface: function MSR_QueryInterface(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;

    // nsIAuthPrompt and nsIPrompt need separate implementations because
    // their method signatures conflict.  The other interfaces we implement
    // within MicrosummaryResource itself.
    switch(iid) {
    case Ci.nsIAuthPrompt:
      return this.authPrompt;
    case Ci.nsIPrompt:
      return this.prompt;
    default:
      return this;
    }
  },

  // nsIInterfaceRequestor
  
  getInterface: function MSR_getInterface(iid) {
    return this.QueryInterface(iid);
  },

  // nsIBadCertListener

  // Suppress UI and abort secure loads from servers with bad SSL certificates.
  
  confirmUnknownIssuer: function MSR_confirmUnknownIssuer(socketInfo, cert, certAddType) {
    return false;
  },

  confirmMismatchDomain: function MSR_confirmMismatchDomain(socketInfo, targetURL, cert) {
    return false;
  },

  confirmCertExpired: function MSR_confirmCertExpired(socketInfo, cert) {
    return false;
  },

  notifyCrlNextupdate: function MSR_notifyCrlNextupdate(socketInfo, targetURL, cert) {
  },

  // Suppress UI and abort loads for files secured by authentication.

  // Auth requests appear to succeed when we cancel them (since the server
  // redirects us to a "you're not authorized" page), so we have to set a flag
  // to let the load handler know to treat the load as a failure.
  __authFailed: false,
  get _authFailed()         { return this.__authFailed },
  set _authFailed(newValue) { this.__authFailed = newValue },

  // nsIAuthPromptProvider
  
  getAuthPrompt: function(aPromptReason, aIID) {
    this._authFailed = true;
    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  // HTTP always requests nsIAuthPromptProvider first, so it never needs
  // nsIAuthPrompt, but not all channels use nsIAuthPromptProvider, so we
  // implement nsIAuthPrompt too.

  // nsIAuthPrompt

  get authPrompt() {
    var resource = this;
    return {
      interfaces: [Ci.nsIPrompt, Ci.nsISupports],
      QueryInterface: function(iid) {
        if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
          throw Cr.NS_ERROR_NO_INTERFACE;
        return this;
      },
      prompt: function(dialogTitle, text, passwordRealm, savePassword, defaultText, result) {
        resource._authFailed = true;
        return false;
      },
      promptUsernameAndPassword: function(dialogTitle, text, passwordRealm, savePassword, user, pwd) {
        resource._authFailed = true;
        return false;
      },
      promptPassword: function(dialogTitle, text, passwordRealm, savePassword, pwd) {
        resource._authFailed = true;
        return false;
      }
    };
  },

  // nsIPrompt

  get prompt() {
    var resource = this;
    return {
      interfaces: [Ci.nsIPrompt, Ci.nsISupports],
      QueryInterface: function(iid) {
        if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
          throw Cr.NS_ERROR_NO_INTERFACE;
        return this;
      },
      alert: function(dialogTitle, text) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      alertCheck: function(dialogTitle, text, checkMessage, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirm: function(dialogTitle, text) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirmCheck: function(dialogTitle, text, checkMessage, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirmEx: function(dialogTitle, text, buttonFlags, button0Title, button1Title, button2Title, checkMsg, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      prompt: function(dialogTitle, text, value, checkMsg, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      promptPassword: function(dialogTitle, text, password, checkMsg, checkValue) {
        resource._authFailed = true;
        return false;
      },
      promptUsernameAndPassword: function(dialogTitle, text, username, password, checkMsg, checkValue) {
        resource._authFailed = true;
        return false;
      },
      select: function(dialogTitle, text, count, selectList, outSelection) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      }
    };
  },

  // XXX We implement nsIProgressEventSink because otherwise bug 253127
  // would cause too many extraneous errors to get reported to the console.
  // Fortunately this doesn't screw up XMLHttpRequest, because it ensures
  // that its implementation of nsIProgressEventSink will always get called
  // in addition to whatever notification callbacks we set on the channel.

  // nsIProgressEventSink

  onProgress: function(aRequest, aContext, aProgress, aProgressMax) {},
  onStatus: function(aRequest, aContext, aStatus, aStatusArg) {},

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
    this._loadCallback = null;
    this._errorCallback = null;
    this._loadTimer = null;
    this._authFailed = false;
    if (this._iframe) {
      if (this._iframe && this._iframe.parentNode)
        this._iframe.parentNode.removeChild(this._iframe);
      this._iframe = null;
    }
  },

  /**
   * Load the resource.
   * 
   * @param   loadCallback
   *          a function to invoke when the resource finishes loading
   * @param   errorCallback
   *          a function to invoke when an error occurs during the load
   *
   */
  load: function MSR_load(loadCallback, errorCallback) {
    LOG(this.uri.spec + " loading");
  
    this._loadCallback = loadCallback;
    this._errorCallback = errorCallback;

    var request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance();
  
    var loadHandler = {
      _self: this,
      handleEvent: function MSR_loadHandler_handleEvent(event) {
        if (this._self._loadTimer)
          this._self._loadTimer.cancel();

        if (this._self._authFailed) {
          // Technically the request succeeded, but we treat it as a failure,
          // since we aren't able to handle HTTP authentication.
          LOG(this._self.uri.spec + " load failed; HTTP auth required");
          try     { this._self._handleError(event) }
          finally { this._self = null }
        }
        else if (event.target.channel.contentType == "multipart/x-mixed-replace") {
          // Technically the request succeeded, but we treat it as a failure,
          // since we aren't able to handle multipart content.
          LOG(this._self.uri.spec + " load failed; contains multipart content");
          try     { this._self._handleError(event) }
          finally { this._self = null }
        }
        else {
          LOG(this._self.uri.spec + " load succeeded; invoking callback");
          try     { this._self._handleLoad(event) }
          finally { this._self = null }
        }
      }
    };

    var errorHandler = {
      _self: this,
      handleEvent: function MSR_errorHandler_handleEvent(event) {
        if (this._self._loadTimer)
          this._self._loadTimer.cancel();

        LOG(this._self.uri.spec + " load failed");
        try     { this._self._handleError(event) }
        finally { this._self = null }
      }
    };

    // cancel loads that take too long
    // timeout specified in seconds at browser.microsummary.requestTimeout,
    // or 300 seconds (five minutes)
    var timeout = getPref("browser.microsummary.requestTimeout", 300) * 1000;
    var timerObserver = {
      _self: this,
      observe: function MSR_timerObserver_observe() {
        LOG("timeout loading microsummary resource " + this._self.uri.spec + ", aborting request");
        request.abort();
        try     { this._self.destroy() }
        finally { this._self = null }
      }
    };
    this._loadTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._loadTimer.init(timerObserver, timeout, Ci.nsITimer.TYPE_ONE_SHOT);

    request = request.QueryInterface(Ci.nsIDOMEventTarget);
    request.addEventListener("load", loadHandler, false);
    request.addEventListener("error", errorHandler, false);
    
    request = request.QueryInterface(Ci.nsIXMLHttpRequest);
    request.open("GET", this.uri.spec, true);
    request.setRequestHeader("X-Moz", "microsummary");

    // Register ourselves as a listener for notification callbacks so we
    // can handle authorization requests and SSL issues like cert mismatches.
    // XMLHttpRequest will handle the notifications we don't handle.
    request.channel.notificationCallbacks = this;

    // If this is a bookmarked resource, and the bookmarks service recorded
    // its charset in the bookmarks datastore the last time the user visited it,
    // then specify the charset in the channel so XMLHttpRequest loads
    // the resource correctly.
    try {
      var resolver = Cc["@mozilla.org/embeddor.implemented/bookmark-charset-resolver;1"].
                     getService(Ci.nsICharsetResolver);
      if (resolver) {
        var charset = resolver.requestCharset(null, request.channel, {}, {});
        if (charset != "");
          request.channel.contentCharset = charset;
      }
    }
    catch(ex) {}

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
      this._loadCallback(this);
    }

    else if (request.channel.contentType == "text/html") {
      this._parse(request.responseText);
    }

    else {
      // This catches text/plain as well as any other content types
      // not accounted for by the content type-specific code above.
      this._content = request.responseText;
      this._contentType = request.channel.contentType;
      this._loadCallback(this);
    }
  },
  
  _handleError: function MSR__handleError(event) {
    // Call the error callback, then destroy ourselves to prevent memory leaks.
    try     { if (this._errorCallback) this._errorCallback() }
    finally { this.destroy() }
  },

  /**
   * Parse a string of HTML text.  Used by _load() when it retrieves HTML.
   * We do this via hidden XUL iframes, which according to bz is the best way
   * to do it currently, since bug 102699 is hard to fix.
   * 
   * @param   htmlText
   *          a string containing the HTML content
   *
   */
  _parse: function MSR__parse(htmlText) {
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
      throw(this._uri.spec + " can't parse; no browser window");
    var document = window.document;
    var rootElement = document.documentElement;
  
    // Create an iframe, make it hidden, and secure it against untrusted content.
    this._iframe = document.createElement('iframe');
    this._iframe.setAttribute("collapsed", true);
    this._iframe.setAttribute("type", "content");
  
    // Insert the iframe into the window, creating the doc shell.
    rootElement.appendChild(this._iframe);

    // When we insert the iframe into the window, it immediately starts loading
    // about:blank, which we don't need and could even hurt us (for example
    // by triggering bugs like bug 344305), so cancel that load.
    var webNav = this._iframe.docShell.QueryInterface(Ci.nsIWebNavigation);
    webNav.stop(Ci.nsIWebNavigation.STOP_NETWORK);

    // Turn off JavaScript and auth dialogs for security and other things
    // to reduce network load.
    // XXX We should also turn off CSS.
    this._iframe.docShell.allowJavascript = false;
    this._iframe.docShell.allowAuth = false;
    this._iframe.docShell.allowPlugins = false;
    this._iframe.docShell.allowMetaRedirects = false;
    this._iframe.docShell.allowSubframes = false;
    this._iframe.docShell.allowImages = false;
  
    var parseHandler = {
      _self: this,
      handleEvent: function MSR_parseHandler_handleEvent(event) {
        event.target.removeEventListener("DOMContentLoaded", this, false);
        try     { this._self._handleParse(event) }
        finally { this._self = null }
      }
    };
 
    // Convert the HTML text into an input stream.
    var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    var stream = converter.convertToInputStream(htmlText);

    // Set up a channel to load the input stream.
    var channel = Cc["@mozilla.org/network/input-stream-channel;1"].
                  createInstance(Ci.nsIInputStreamChannel);
    channel.setURI(this._uri);
    channel.contentStream = stream;

    // Load in the background so we don't trigger web progress listeners.
    var request = channel.QueryInterface(Ci.nsIRequest);
    request.loadFlags |= Ci.nsIRequest.LOAD_BACKGROUND;

    // Specify the content type since we're not loading content from a server,
    // so it won't get specified for us, and if we don't specify it ourselves,
    // then Firefox will prompt the user to download content of "unknown type".
    var baseChannel = channel.QueryInterface(Ci.nsIChannel);
    baseChannel.contentType = "text/html";

    // Load as UTF-8, which it'll always be, because XMLHttpRequest converts
    // the text (i.e. XMLHTTPRequest.responseText) from its original charset
    // to UTF-16, then the string input stream component converts it to UTF-8.
    baseChannel.contentCharset = "UTF-8";

    // Register the parse handler as a load event listener and start the load.
    // Listen for "DOMContentLoaded" instead of "load" because background loads
    // don't fire "load" events.
    this._iframe.addEventListener("DOMContentLoaded", parseHandler, true);
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
    this._loadCallback(this);
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

/**
 * Get a value from a pref or a default value if the pref doesn't exist.
 *
 * @param   prefName
 * @param   defaultValue
 * @returns the pref's value or the default (if it is missing)
 */
function getPref(prefName, defaultValue) {
  try {
    var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
    switch (prefBranch.getPrefType(prefName)) {
    case prefBranch.PREF_BOOL:
      return prefBranch.getBoolPref(prefName);
    case prefBranch.PREF_INT:
      return prefBranch.getIntPref(prefName);
    }
  }
  catch (ex) { /* return the default value */ }
  
  return defaultValue;
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
  const maxLength = 60;

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

  if (name.length > maxLength)
    name = name.substring(0, maxLength);

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
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  
    for (var key in this._objects) {
      if (cid.equals(this._objects[key].CID))
      return this._objects[key].factory;
    }
    
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  
  _objects: {
    service: {
      CID        : Components.ID("{460a9792-b154-4f26-a922-0f653e2c8f91}"),
      contractID : "@mozilla.org/microsummary/service;1",
      className  : "Microsummary Service",
      factory    : MicrosummaryServiceFactory = {
                     createInstance: function(aOuter, aIID) {
                       if (aOuter != null)
                         throw Cr.NS_ERROR_NO_AGGREGATION;
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
