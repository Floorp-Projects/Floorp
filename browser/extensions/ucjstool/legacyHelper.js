"use strict";

var that = this;

this.legacy = class extends ExtensionAPI {
  getAPI(context) {
    const api = {
      legacy: {
        async bootstrap() {
          try {
            const {BootstrapLoader} = ChromeUtils.import('chrome://userchromejs/content/BootstrapLoader.jsm');
            return;
          } catch (e) {}

          if (context.extension.startupReason === "APP_STARTUP") {
            ( function () {
              const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

              // RDFDataSource.jsm
              const RDFDataSource = (function () {
                /* This Source Code Form is subject to the terms of the Mozilla Public
                * License, v. 2.0. If a copy of the MPL was not distributed with this
                * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

                /**
                 * This module creates a new API for accessing and modifying RDF graphs. The
                 * goal is to be able to serialise the graph in a human readable form. Also
                 * if the graph was originally loaded from an RDF/XML the serialisation should
                 * closely match the original with any new data closely following the existing
                 * layout. The output should always be compatible with Mozilla's RDF parser.
                 *
                 * This is all achieved by using a DOM Document to hold the current state of the
                 * graph in XML form. This can be initially loaded and parsed from disk or
                 * a blank document used for an empty graph. As assertions are added to the
                 * graph, appropriate DOM nodes are added to the document to represent them
                 * along with any necessary whitespace to properly layout the XML.
                 *
                 * In general the order of adding assertions to the graph will impact the form
                 * the serialisation takes. If a resource is first added as the object of an
                 * assertion then it will eventually be serialised inside the assertion's
                 * property element. If a resource is first added as the subject of an assertion
                 * then it will be serialised at the top level of the XML.
                 */

                const NS_XML = "http://www.w3.org/XML/1998/namespace";
                const NS_XMLNS = "http://www.w3.org/2000/xmlns/";
                const NS_RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
                const NS_NC = "http://home.netscape.com/NC-rdf#";

                /* eslint prefer-template: 1 */

                XPCOMUtils.defineLazyGlobalGetters(this, ["DOMParser", "Element", "fetch"]);

                ChromeUtils.defineModuleGetter(this, "Services",
                                               "resource://gre/modules/Services.jsm");

                function isElement(obj) {
                  return Element.isInstance(obj);
                }
                function isText(obj) {
                  return obj && typeof obj == "object" && ChromeUtils.getClassName(obj) == "Text";
                }

                /**
                 * Returns either an rdf namespaced attribute or an un-namespaced attribute
                 * value. Returns null if neither exists,
                 */
                function getRDFAttribute(element, name) {
                  if (element.hasAttributeNS(NS_RDF, name))
                    return element.getAttributeNS(NS_RDF, name);
                  if (element.hasAttribute(name))
                    return element.getAttribute(name);
                  return undefined;
                }

                /**
                 * Represents an assertion in the datasource
                 */
                class RDFAssertion {
                  constructor(subject, predicate, object) {
                    // The subject on this assertion, an RDFSubject
                    this._subject = subject;
                    // The predicate, a string
                    this._predicate = predicate;
                    // The object, an RDFNode
                    this._object = object;
                    // The datasource this assertion exists in
                    this._ds = this._subject._ds;
                    // Marks that _DOMnode is the subject's element
                    this._isSubjectElement = false;
                    // The DOM node that represents this assertion. Could be a property element,
                    // a property attribute or the subject's element for rdf:type
                    this._DOMNode = null;
                  }

                  getPredicate() {
                    return this._predicate;
                  }

                  getObject() {
                    return this._object;
                  }
                }

                class RDFNode {
                  equals(rdfnode) {
                    return (rdfnode.constructor === this.constructor &&
                            rdfnode._value == this._value);
                  }
                }

                /**
                 * A simple literal value
                 */
                class RDFLiteral extends RDFNode {
                  constructor(value) {
                    super();
                    this._value = value;
                  }

                  getValue() {
                    return this._value;
                  }
                }

                /**
                 * This is an RDF node that can be a subject so a resource or a blank node
                 */
                class RDFSubject extends RDFNode {
                  constructor(ds) {
                    super();
                    // A lookup of the assertions with this as the subject. Keyed on predicate
                    this._assertions = {};
                    // A lookup of the assertions with this as the object. Keyed on predicate
                    this._backwards = {};
                    // The datasource this subject belongs to
                    this._ds = ds;
                    // The DOM elements in the document that represent this subject. Array of Element
                    this._elements = [];
                  }

                  /**
                   * Parses the given Element from the DOM document
                   */
                  /* eslint-disable complexity */
                  _parseElement(element) {
                    this._elements.push(element);

                    // There might be an inferred rdf:type assertion in the element name
                    if (element.namespaceURI != NS_RDF ||
                        element.localName != "Description") {
                      var assertion = new RDFAssertion(this, RDF_R("type"),
                                                       this._ds.getResource(element.namespaceURI + element.localName));
                      assertion._DOMnode = element;
                      assertion._isSubjectElement = true;
                      this._addAssertion(assertion);
                    }

                    // Certain attributes can be literal properties
                    for (let attr of element.attributes) {
                      if (attr.namespaceURI == NS_XML || attr.namespaceURI == NS_XMLNS ||
                          attr.nodeName == "xmlns")
                        continue;
                      if ((attr.namespaceURI == NS_RDF || !attr.namespaceURI) &&
                          (["nodeID", "about", "resource", "ID", "parseType"].includes(attr.localName)))
                        continue;
                      var object = null;
                      if (attr.namespaceURI == NS_RDF) {
                        if (attr.localName == "type")
                          object = this._ds.getResource(attr.nodeValue);
                      }
                      if (!object)
                        object = new RDFLiteral(attr.nodeValue);
                      assertion = new RDFAssertion(this, attr.namespaceURI + attr.localName, object);
                      assertion._DOMnode = attr;
                      this._addAssertion(assertion);
                    }

                    var child = element.firstChild;
                    element.listCounter = 1;
                    while (child) {
                      if (isElement(child)) {
                        object = null;
                        var predicate = child.namespaceURI + child.localName;
                        if (child.namespaceURI == NS_RDF) {
                          if (child.localName == "li") {
                            predicate = RDF_R(`_${element.listCounter}`);
                            element.listCounter++;
                          }
                        }

                        // Check for and bail out on unknown attributes on the property element
                        for (let attr of child.attributes) {
                          // Ignore XML namespaced attributes
                          if (attr.namespaceURI == NS_XML)
                            continue;
                          // These are reserved by XML for future use
                          if (attr.localName.substring(0, 3).toLowerCase() == "xml")
                            continue;
                          // We can handle these RDF attributes
                          if ((!attr.namespaceURI || attr.namespaceURI == NS_RDF) &&
                              ["resource", "nodeID"].includes(attr.localName))
                            continue;
                          // This is a special attribute we handle for compatibility with Mozilla RDF
                          if (attr.namespaceURI == NS_NC &&
                              attr.localName == "parseType")
                            continue;
                        }

                        var parseType = child.getAttributeNS(NS_NC, "parseType");

                        var resource = getRDFAttribute(child, "resource");
                        var nodeID = getRDFAttribute(child, "nodeID");

                        if (resource !== undefined) {
                          var base = Services.io.newURI(element.baseURI);
                          object = this._ds.getResource(base.resolve(resource));
                        } else if (nodeID !== undefined) {
                          object = this._ds.getBlankNode(nodeID);
                        } else {
                          var hasText = false;
                          var childElement = null;
                          var subchild = child.firstChild;
                          while (subchild) {
                            if (isText(subchild) && /\S/.test(subchild.nodeValue)) {
                              hasText = true;
                            } else if (isElement(subchild)) {
                              childElement = subchild;
                            }
                            subchild = subchild.nextSibling;
                          }

                          if (childElement) {
                            object = this._ds._getSubjectForElement(childElement);
                            object._parseElement(childElement);
                          } else
                            object = new RDFLiteral(child.textContent);
                        }

                        assertion = new RDFAssertion(this, predicate, object);
                        this._addAssertion(assertion);
                        assertion._DOMnode = child;
                      }
                      child = child.nextSibling;
                    }
                  }
                  /* eslint-enable complexity */

                  /**
                   * Adds a new assertion to the internal hashes. Should be called for every
                   * new assertion parsed or created programmatically.
                   */
                  _addAssertion(assertion) {
                    var predicate = assertion.getPredicate();
                    if (predicate in this._assertions)
                      this._assertions[predicate].push(assertion);
                    else
                      this._assertions[predicate] = [ assertion ];

                    var object = assertion.getObject();
                    if (object instanceof RDFSubject) {
                      // Create reverse assertion
                      if (predicate in object._backwards)
                        object._backwards[predicate].push(assertion);
                      else
                        object._backwards[predicate] = [ assertion ];
                    }
                  }

                  /**
                   * Returns all objects in assertions with this subject and the given predicate.
                   */
                  getObjects(predicate) {
                    if (predicate in this._assertions)
                      return Array.from(this._assertions[predicate],
                                        i => i.getObject());

                    return [];
                  }

                  /**
                   * Retrieves the first property value for the given predicate.
                   */
                  getProperty(predicate) {
                    if (predicate in this._assertions)
                      return this._assertions[predicate][0].getObject();
                    return null;
                  }
                }

                /**
                 * Creates a new RDFResource for the datasource. Private.
                 */
                class RDFResource extends RDFSubject {
                  constructor(ds, uri) {
                    super(ds);
                    // This is the uri that the resource represents.
                    this._uri = uri;
                  }
                }

                /**
                 * Creates a new blank node. Private.
                 */
                class RDFBlankNode extends RDFSubject {
                  constructor(ds, nodeID) {
                    super(ds);
                    // The nodeID of this node. May be null if there is no ID.
                    this._nodeID = nodeID;
                  }

                  /**
                   * Sets attributes on the DOM element to mark it as representing this node
                   */
                  _applyToElement(element) {
                    if (!this._nodeID)
                      return;
                    if (USE_RDFNS_ATTR) {
                      var prefix = this._ds._resolvePrefix(element, RDF_R("nodeID"));
                      element.setAttributeNS(prefix.namespaceURI, prefix.qname, this._nodeID);
                    } else {
                      element.setAttribute("nodeID", this._nodeID);
                    }
                  }

                  /**
                   * Creates a new Element in the document for holding assertions about this
                   * subject. The URI controls what tagname to use.
                   */
                  _createNewElement(uri) {
                    // If there are already nodes representing this in the document then we need
                    // a nodeID to match them
                    if (!this._nodeID && this._elements.length > 0) {
                      this._ds._createNodeID(this);
                      for (let element of this._elements)
                        this._applyToElement(element);
                    }

                    return super._createNewElement.call(uri);
                  }

                  /**
                   * Adds a reference to this node to the given property Element.
                   */
                  _addReferenceToElement(element) {
                    if (this._elements.length > 0 && !this._nodeID) {
                      // In document elsewhere already
                      // Create a node ID and update the other nodes referencing
                      this._ds._createNodeID(this);
                      for (let element of this._elements)
                        this._applyToElement(element);
                    }

                    if (this._nodeID) {
                      if (USE_RDFNS_ATTR) {
                        let prefix = this._ds._resolvePrefix(element, RDF_R("nodeID"));
                        element.setAttributeNS(prefix.namespaceURI, prefix.qname, this._nodeID);
                      } else {
                        element.setAttribute("nodeID", this._nodeID);
                      }
                    } else {
                      // Add the empty blank node, this is generally right since further
                      // assertions will be added to fill this out
                      var newelement = this._ds._addElement(element, RDF_R("Description"));
                      newelement.listCounter = 1;
                      this._elements.push(newelement);
                    }
                  }

                    /**
                     * Removes any reference to this node from the given property Element.
                     */
                    _removeReferenceFromElement(element) {
                      if (element.hasAttributeNS(NS_RDF, "nodeID"))
                        element.removeAttributeNS(NS_RDF, "nodeID");
                      if (element.hasAttribute("nodeID"))
                        element.removeAttribute("nodeID");
                    }

                  getNodeID() {
                    return this._nodeID;
                  }
                }

                /**
                 * Creates a new RDFDataSource from the given document. The document will be
                 * changed as assertions are added and removed to the RDF. Pass a null document
                 * to start with an empty graph.
                 */
                class RDFDataSource {
                  constructor(document) {
                    // All known resources, indexed on URI
                    this._resources = {};
                    // All blank nodes
                    this._allBlankNodes = [];

                    // The underlying DOM document for this datasource
                    this._document = document;
                    this._parseDocument();
                  }

                  static loadFromString(text) {
                    let parser = new DOMParser();
                    let document = parser.parseFromString(text, "application/xml");

                    return new this(document);
                  }

                  /**
                   * Returns an rdf subject for the given DOM Element. If the subject has not
                   * been seen before a new one is created.
                   */
                  _getSubjectForElement(element) {
                    var about = getRDFAttribute(element, "about");

                    if (about !== undefined) {
                      let base = Services.io.newURI(element.baseURI);
                      return this.getResource(base.resolve(about));
                    }
                    return this.getBlankNode(null);
                  }

                  /**
                   * Parses the document for subjects at the top level.
                   */
                  _parseDocument() {
                    var domnode = this._document.documentElement.firstChild;
                    while (domnode) {
                      if (isElement(domnode)) {
                        var subject = this._getSubjectForElement(domnode);
                        subject._parseElement(domnode);
                      }
                      domnode = domnode.nextSibling;
                    }
                  }

                  /**
                   * Gets a blank node. nodeID may be null and if so a new blank node is created.
                   * If a nodeID is given then the blank node with that ID is returned or created.
                   */
                  getBlankNode(nodeID) {
                    var rdfnode = new RDFBlankNode(this, nodeID);
                    this._allBlankNodes.push(rdfnode);
                    return rdfnode;
                  }

                  /**
                   * Gets the resource for the URI. The resource is created if it has not been
                   * used already.
                   */
                  getResource(uri) {
                    if (uri in this._resources)
                      return this._resources[uri];

                    var resource = new RDFResource(this, uri);
                    this._resources[uri] = resource;
                    return resource;
                  }
                }

                return RDFDataSource;
              }).call(this);
              // fim RDFDataSource.jsm

              // RDFManifestConverter.jsm
              const RDFURI_INSTALL_MANIFEST_ROOT = "urn:mozilla:install-manifest";

              function EM_R(aProperty) {
                return `http://www.mozilla.org/2004/em-rdf#${aProperty}`;
              }

              function getValue(literal) {
                return literal && literal.getValue();
              }

              function getProperty(resource, property) {
                return getValue(resource.getProperty(EM_R(property)));
              }

              class Manifest {
                constructor(ds) {
                  this.ds = ds;
                }

                static loadFromString(text) {
                  return new this(RDFDataSource.loadFromString(text));
                }
              }

              class InstallRDF extends Manifest {
                _readProps(source, obj, props) {
                  for (let prop of props) {
                    let val = getProperty(source, prop);
                    if (val != null) {
                      obj[prop] = val;
                    }
                  }
                }

                _readArrayProp(source, obj, prop, target, decode = getValue) {
                  let result = Array.from(source.getObjects(EM_R(prop)),
                                          target => decode(target));
                  if (result.length) {
                    obj[target] = result;
                  }
                }

                _readArrayProps(source, obj, props, decode = getValue) {
                  for (let [prop, target] of Object.entries(props)) {
                    this._readArrayProp(source, obj, prop, target, decode);
                  }
                }

                _readLocaleStrings(source, obj) {
                  this._readProps(source, obj, ["name", "description", "creator", "homepageURL"]);
                  this._readArrayProps(source, obj, {
                    locale: "locales",
                    developer: "developers",
                    translator: "translators",
                    contributor: "contributors",
                  });
                }

                decode() {
                  let root = this.ds.getResource(RDFURI_INSTALL_MANIFEST_ROOT);
                  let result = {};

                  let props = ["id", "version", "type", "updateURL", "optionsURL",
                               "optionsType", "aboutURL", "iconURL",
                               "bootstrap", "unpack", "strictCompatibility"];
                  this._readProps(root, result, props);

                  let decodeTargetApplication = source => {
                    let app = {};
                    this._readProps(source, app, ["id", "minVersion", "maxVersion"]);
                    return app;
                  };

                  let decodeLocale = source => {
                    let localized = {};
                    this._readLocaleStrings(source, localized);
                    return localized;
                  };

                  this._readLocaleStrings(root, result);

                  this._readArrayProps(root, result, {"targetPlatform": "targetPlatforms"});
                  this._readArrayProps(root, result, {"targetApplication": "targetApplications"},
                                       decodeTargetApplication);
                  this._readArrayProps(root, result, {"localized": "localized"},
                                       decodeLocale);
                  this._readArrayProps(root, result, {"dependency": "dependencies"},
                                       source => getProperty(source, "id"));

                  return result;
                }
              }
              // fim RDFManifestConverter.jsm

              // BootstrapLoader.jsm
              XPCOMUtils.defineLazyModuleGetters(this, {
                Blocklist: 'resource://gre/modules/Blocklist.jsm',
                ConsoleAPI: 'resource://gre/modules/Console.jsm',
                Services: 'resource://gre/modules/Services.jsm',
              });

              Services.obs.addObserver(doc => {
                if (doc.location.protocol + doc.location.pathname === 'about:addons' ||
                    doc.location.protocol + doc.location.pathname === 'chrome:/content/extensions/aboutaddons.html') {
                  const win = doc.defaultView;
                  let handleEvent_orig = win.customElements.get('addon-card').prototype.handleEvent;
                  win.customElements.get('addon-card').prototype.handleEvent = function (e) {
                    if (e.type === 'click' &&
                        e.target.getAttribute('action') === 'preferences' &&
                        this.addon.optionsType == 1/*AddonManager.OPTIONS_TYPE_DIALOG*/) {
                      var windows = Services.wm.getEnumerator(null);
                      while (windows.hasMoreElements()) {
                        var win2 = windows.getNext();
                        if (win2.closed) {
                          continue;
                        }
                        if (win2.document.documentURI == this.addon.optionsURL) {
                          win2.focus();
                          return;
                        }
                      }
                      var features = 'chrome,titlebar,toolbar,centerscreen';
                      var instantApply = Services.prefs.getBoolPref('browser.preferences.instantApply');
                      features += instantApply ? ',dialog=no' : '';
                      win.docShell.rootTreeItem.domWindow.openDialog(this.addon.optionsURL, this.addon.id, features);
                    } else {
                      handleEvent_orig.apply(this, arguments);
                    }
                  }
                  let update_orig = win.customElements.get('addon-options').prototype.update;
                  win.customElements.get('addon-options').prototype.update = function (card, addon) {
                    update_orig.apply(this, arguments);
                    if (addon.optionsType == 1/*AddonManager.OPTIONS_TYPE_DIALOG*/)
                      this.querySelector('panel-item[data-l10n-id="preferences-addon-button"]').hidden = false;
                  }
                }
              }, 'chrome-document-loaded');

              const {AddonManager} = ChromeUtils.import('resource://gre/modules/AddonManager.jsm');
              const {XPIDatabase, AddonInternal} = ChromeUtils.import('resource://gre/modules/addons/XPIDatabase.jsm');

              const { defineAddonWrapperProperty } = Cu.import('resource://gre/modules/addons/XPIDatabase.jsm');
              defineAddonWrapperProperty('optionsType', function optionsType() {
                if (!this.isActive) {
                  return null;
                }

                let addon = this.__AddonInternal__;
                let hasOptionsURL = !!this.optionsURL;

                if (addon.optionsType) {
                  switch (parseInt(addon.optionsType, 10)) {
                    case 1/*AddonManager.OPTIONS_TYPE_DIALOG*/:
                    case AddonManager.OPTIONS_TYPE_TAB:
                    case AddonManager.OPTIONS_TYPE_INLINE_BROWSER:
                      return hasOptionsURL ? addon.optionsType : null;
                  }
                  return null;
                }

                return null;
              });

              XPIDatabase.isDisabledLegacy = () => false;

              XPCOMUtils.defineLazyGetter(this, 'BOOTSTRAP_REASONS', () => {
                const {XPIProvider} = ChromeUtils.import('resource://gre/modules/addons/XPIProvider.jsm');
                return XPIProvider.BOOTSTRAP_REASONS;
              });

              const {Log} = ChromeUtils.import('resource://gre/modules/Log.jsm');
              var logger = Log.repository.getLogger('addons.bootstrap');

              /**
               * Valid IDs fit this pattern.
               */
              var gIDTest = /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i;

              // Properties that exist in the install manifest
              const PROP_METADATA      = ['id', 'version', 'type', 'internalName', 'updateURL',
                                          'optionsURL', 'optionsType', 'aboutURL', 'iconURL'];
              const PROP_LOCALE_SINGLE = ['name', 'description', 'creator', 'homepageURL'];
              const PROP_LOCALE_MULTI  = ['developers', 'translators', 'contributors'];

              // Map new string type identifiers to old style nsIUpdateItem types.
              // Retired values:
              // 32 = multipackage xpi file
              // 8 = locale
              // 256 = apiextension
              // 128 = experiment
              // theme = 4
              const TYPES = {
                extension: 2,
                dictionary: 64,
              };

              const COMPATIBLE_BY_DEFAULT_TYPES = {
                extension: true,
                dictionary: true,
              };

              const hasOwnProperty = Function.call.bind(Object.prototype.hasOwnProperty);

              function isXPI(filename) {
                let ext = filename.slice(-4).toLowerCase();
                return ext === '.xpi' || ext === '.zip';
              }

              /**
               * Gets an nsIURI for a file within another file, either a directory or an XPI
               * file. If aFile is a directory then this will return a file: URI, if it is an
               * XPI file then it will return a jar: URI.
               *
               * @param {nsIFile} aFile
               *        The file containing the resources, must be either a directory or an
               *        XPI file
               * @param {string} aPath
               *        The path to find the resource at, '/' separated. If aPath is empty
               *        then the uri to the root of the contained files will be returned
               * @returns {nsIURI}
               *        An nsIURI pointing at the resource
               */
              function getURIForResourceInFile(aFile, aPath) {
                if (!isXPI(aFile.leafName)) {
                  let resource = aFile.clone();
                  if (aPath)
                    aPath.split('/').forEach(part => resource.append(part));

                  return Services.io.newFileURI(resource);
                }

                return buildJarURI(aFile, aPath);
              }

              /**
               * Creates a jar: URI for a file inside a ZIP file.
               *
               * @param {nsIFile} aJarfile
               *        The ZIP file as an nsIFile
               * @param {string} aPath
               *        The path inside the ZIP file
               * @returns {nsIURI}
               *        An nsIURI for the file
               */
              function buildJarURI(aJarfile, aPath) {
                let uri = Services.io.newFileURI(aJarfile);
                uri = 'jar:' + uri.spec + '!/' + aPath;
                return Services.io.newURI(uri);
              }

              var BootstrapLoader = {
                name: 'bootstrap',
                manifestFile: 'install.rdf',
                async loadManifest(pkg) {
                  /**
                   * Reads locale properties from either the main install manifest root or
                   * an em:localized section in the install manifest.
                   *
                   * @param {Object} aSource
                   *        The resource to read the properties from.
                   * @param {boolean} isDefault
                   *        True if the locale is to be read from the main install manifest
                   *        root
                   * @param {string[]} aSeenLocales
                   *        An array of locale names already seen for this install manifest.
                   *        Any locale names seen as a part of this function will be added to
                   *        this array
                   * @returns {Object}
                   *        an object containing the locale properties
                   */
                  function readLocale(aSource, isDefault, aSeenLocales) {
                    let locale = {};
                    if (!isDefault) {
                      locale.locales = [];
                      for (let localeName of aSource.locales || []) {
                        if (!localeName) {
                          logger.warn('Ignoring empty locale in localized properties');
                          continue;
                        }
                        if (aSeenLocales.includes(localeName)) {
                          logger.warn('Ignoring duplicate locale in localized properties');
                          continue;
                        }
                        aSeenLocales.push(localeName);
                        locale.locales.push(localeName);
                      }

                      if (locale.locales.length == 0) {
                        logger.warn('Ignoring localized properties with no listed locales');
                        return null;
                      }
                    }

                    for (let prop of [...PROP_LOCALE_SINGLE, ...PROP_LOCALE_MULTI]) {
                      if (hasOwnProperty(aSource, prop)) {
                        locale[prop] = aSource[prop];
                      }
                    }

                    return locale;
                  }

                  let manifestData = await pkg.readString('install.rdf');
                  let manifest = InstallRDF.loadFromString(manifestData).decode();

                  let addon = new AddonInternal();
                  for (let prop of PROP_METADATA) {
                    if (hasOwnProperty(manifest, prop)) {
                      addon[prop] = manifest[prop];
                    }
                  }

                  if (!addon.type) {
                    addon.type = 'extension';
                  } else {
                    let type = addon.type;
                    addon.type = null;
                    for (let name in TYPES) {
                      if (TYPES[name] == type) {
                        addon.type = name;
                        break;
                      }
                    }
                  }

                  if (!(addon.type in TYPES))
                    throw new Error('Install manifest specifies unknown type: ' + addon.type);

                  if (!addon.id)
                    throw new Error('No ID in install manifest');
                  if (!gIDTest.test(addon.id))
                    throw new Error('Illegal add-on ID ' + addon.id);
                  if (!addon.version)
                    throw new Error('No version in install manifest');

                  addon.strictCompatibility = (!(addon.type in COMPATIBLE_BY_DEFAULT_TYPES) ||
                                               manifest.strictCompatibility == 'true');

                  // Only read these properties for extensions.
                  if (addon.type == 'extension') {
                    if (manifest.bootstrap != 'true') {
                      throw new Error('Non-restartless extensions no longer supported');
                    }

                    if (addon.optionsType &&
                        addon.optionsType != 1/*AddonManager.OPTIONS_TYPE_DIALOG*/ &&
                        addon.optionsType != AddonManager.OPTIONS_TYPE_INLINE_BROWSER &&
                        addon.optionsType != AddonManager.OPTIONS_TYPE_TAB) {
                          throw new Error('Install manifest specifies unknown optionsType: ' + addon.optionsType);
                    }

                    if (addon.optionsType)
                      addon.optionsType = parseInt(addon.optionsType);
                  }

                  addon.defaultLocale = readLocale(manifest, true);

                  let seenLocales = [];
                  addon.locales = [];
                  for (let localeData of manifest.localized || []) {
                    let locale = readLocale(localeData, false, seenLocales);
                    if (locale)
                      addon.locales.push(locale);
                  }

                  let dependencies = new Set(manifest.dependencies);
                  addon.dependencies = Object.freeze(Array.from(dependencies));

                  let seenApplications = [];
                  addon.targetApplications = [];
                  for (let targetApp of manifest.targetApplications || []) {
                    if (!targetApp.id || !targetApp.minVersion ||
                        !targetApp.maxVersion) {
                          logger.warn('Ignoring invalid targetApplication entry in install manifest');
                          continue;
                    }
                    if (seenApplications.includes(targetApp.id)) {
                      logger.warn('Ignoring duplicate targetApplication entry for ' + targetApp.id +
                                  ' in install manifest');
                      continue;
                    }
                    seenApplications.push(targetApp.id);
                    addon.targetApplications.push(targetApp);
                  }

                  // Note that we don't need to check for duplicate targetPlatform entries since
                  // the RDF service coalesces them for us.
                  addon.targetPlatforms = [];
                  for (let targetPlatform of manifest.targetPlatforms || []) {
                    let platform = {
                      os: null,
                      abi: null,
                    };

                    let pos = targetPlatform.indexOf('_');
                    if (pos != -1) {
                      platform.os = targetPlatform.substring(0, pos);
                      platform.abi = targetPlatform.substring(pos + 1);
                    } else {
                      platform.os = targetPlatform;
                    }

                    addon.targetPlatforms.push(platform);
                  }

                  addon.userDisabled = false;
                  addon.softDisabled = addon.blocklistState == Blocklist.STATE_SOFTBLOCKED;
                  addon.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;

                  addon.userPermissions = null;

                  addon.icons = {};
                  if (await pkg.hasResource('icon.png')) {
                    addon.icons[32] = 'icon.png';
                    addon.icons[48] = 'icon.png';
                  }

                  if (await pkg.hasResource('icon64.png')) {
                    addon.icons[64] = 'icon64.png';
                  }

                  return addon;
                },

                loadScope(addon) {
                  let file = addon.file || addon._sourceBundle;
                  let uri = getURIForResourceInFile(file, 'bootstrap.js').spec;
                  let principal = Services.scriptSecurityManager.getSystemPrincipal();

                  let sandbox = new Cu.Sandbox(principal, {
                    sandboxName: uri,
                    addonId: addon.id,
                    wantGlobalProperties: ['ChromeUtils'],
                    metadata: { addonID: addon.id, URI: uri },
                  });

                  try {
                    Object.assign(sandbox, BOOTSTRAP_REASONS);

                    XPCOMUtils.defineLazyGetter(sandbox, 'console', () =>
                      new ConsoleAPI({ consoleID: `addon/${addon.id}` }));

                    Services.scriptloader.loadSubScript(uri, sandbox);
                  } catch (e) {
                    logger.warn(`Error loading bootstrap.js for ${addon.id}`, e);
                  }

                  function findMethod(name) {
                    if (sandbox[name]) {
                      return sandbox[name];
                    }

                    try {
                      let method = Cu.evalInSandbox(name, sandbox);
                      return method;
                    } catch (err) { }

                    return () => {
                      logger.warn(`Add-on ${addon.id} is missing bootstrap method ${name}`);
                    };
                  }

                  let install = findMethod('install');
                  let uninstall = findMethod('uninstall');
                  let startup = findMethod('startup');
                  let shutdown = findMethod('shutdown');

                  return {
                    install(...args) {
                      install(...args);
                      // Forget any cached files we might've had from this extension.
                      Services.obs.notifyObservers(null, 'startupcache-invalidate');
                    },

                    uninstall(...args) {
                      uninstall(...args);
                      // Forget any cached files we might've had from this extension.
                      Services.obs.notifyObservers(null, 'startupcache-invalidate');
                    },

                    startup(...args) {
                      if (addon.type == 'extension') {
                        logger.debug(`Registering manifest for ${file.path}\n`);
                        Components.manager.addBootstrappedManifestLocation(file);
                      }
                      return startup(...args);
                    },

                    shutdown(data, reason) {
                      try {
                        return shutdown(data, reason);
                      } catch (err) {
                        throw err;
                      } finally {
                        if (reason != BOOTSTRAP_REASONS.APP_SHUTDOWN) {
                          logger.debug(`Removing manifest for ${file.path}\n`);
                          Components.manager.removeBootstrappedManifestLocation(file);
                        }
                      }
                    },
                  };
                },
              };

              AddonManager.addExternalExtensionLoader(BootstrapLoader);

              AddonManager.getAllAddons().then(addons => {
                addons.forEach(addon => {
                  if (addon.type == 'extension' && !addon.isWebExtension && !addon.userDisabled) {
                    addon.disable();
                    addon.enable();
                  };
                });
              })
              // fim BootstrapLoader.jsm
            }).apply(that);
          }
        },

        close: function () {}
	    }
    };
    context.extension.callOnClose(api.legacy);
    return api;
	}
}