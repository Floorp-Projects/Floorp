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

var EXPORTED_SYMBOLS = ["RDFLiteral", "RDFBlankNode", "RDFResource", "RDFDataSource"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

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
