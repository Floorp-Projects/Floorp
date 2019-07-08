/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module contains a small element attribute value parser. It's primary
 * goal is to extract link information from attribute values (like the href in
 * <a href="/some/link.html"> for example).
 *
 * There are several types of linkable attribute values:
 * - TYPE_URI: a URI (e.g. <a href="uri">).
 * - TYPE_URI_LIST: a space separated list of URIs (e.g. <a ping="uri1 uri2">).
 * - TYPE_IDREF: a reference to an other element in the same document via its id
 *   (e.g. <label for="input-id"> or <key command="command-id">).
 * - TYPE_IDREF_LIST: a space separated list of IDREFs (e.g.
 *   <output for="id1 id2">).
 * - TYPE_JS_RESOURCE_URI: a URI to a javascript resource that can be opened in
 *   the devtools (e.g. <script src="uri">).
 * - TYPE_CSS_RESOURCE_URI: a URI to a css resource that can be opened in the
 *   devtools (e.g. <link href="uri">).
 *
 * parseAttribute is the parser entry function, exported on this module.
 */

const TYPE_STRING = "string";
const TYPE_URI = "uri";
const TYPE_URI_LIST = "uriList";
const TYPE_IDREF = "idref";
const TYPE_IDREF_LIST = "idrefList";
const TYPE_JS_RESOURCE_URI = "jsresource";
const TYPE_CSS_RESOURCE_URI = "cssresource";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HTML_NS = "http://www.w3.org/1999/xhtml";

const ATTRIBUTE_TYPES = new Map([
  ["action", [{ namespaceURI: HTML_NS, tagName: "form", type: TYPE_URI }]],
  ["background", [{ namespaceURI: HTML_NS, tagName: "body", type: TYPE_URI }]],
  [
    "cite",
    [
      { namespaceURI: HTML_NS, tagName: "blockquote", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "q", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "del", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "ins", type: TYPE_URI },
    ],
  ],
  ["classid", [{ namespaceURI: HTML_NS, tagName: "object", type: TYPE_URI }]],
  [
    "codebase",
    [
      { namespaceURI: HTML_NS, tagName: "object", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "applet", type: TYPE_URI },
    ],
  ],
  [
    "command",
    [
      { namespaceURI: HTML_NS, tagName: "menuitem", type: TYPE_IDREF },
      { namespaceURI: XUL_NS, tagName: "key", type: TYPE_IDREF },
    ],
  ],
  ["contextmenu", [{ namespaceURI: "*", tagName: "*", type: TYPE_IDREF }]],
  ["data", [{ namespaceURI: HTML_NS, tagName: "object", type: TYPE_URI }]],
  [
    "for",
    [
      { namespaceURI: HTML_NS, tagName: "label", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "output", type: TYPE_IDREF_LIST },
    ],
  ],
  [
    "form",
    [
      { namespaceURI: HTML_NS, tagName: "button", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "fieldset", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "input", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "keygen", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "label", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "object", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "output", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "select", type: TYPE_IDREF },
      { namespaceURI: HTML_NS, tagName: "textarea", type: TYPE_IDREF },
    ],
  ],
  [
    "formaction",
    [
      { namespaceURI: HTML_NS, tagName: "button", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "input", type: TYPE_URI },
    ],
  ],
  [
    "headers",
    [
      { namespaceURI: HTML_NS, tagName: "td", type: TYPE_IDREF_LIST },
      { namespaceURI: HTML_NS, tagName: "th", type: TYPE_IDREF_LIST },
    ],
  ],
  [
    "href",
    [
      { namespaceURI: HTML_NS, tagName: "a", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "area", type: TYPE_URI },
      {
        namespaceURI: "*",
        tagName: "link",
        type: TYPE_CSS_RESOURCE_URI,
        isValid: (namespaceURI, tagName, attributes) => {
          return getAttribute(attributes, "rel") === "stylesheet";
        },
      },
      { namespaceURI: "*", tagName: "link", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "base", type: TYPE_URI },
    ],
  ],
  ["icon", [{ namespaceURI: HTML_NS, tagName: "menuitem", type: TYPE_URI }]],
  ["list", [{ namespaceURI: HTML_NS, tagName: "input", type: TYPE_IDREF }]],
  [
    "longdesc",
    [
      { namespaceURI: HTML_NS, tagName: "img", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "frame", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "iframe", type: TYPE_URI },
    ],
  ],
  ["manifest", [{ namespaceURI: HTML_NS, tagName: "html", type: TYPE_URI }]],
  [
    "menu",
    [
      { namespaceURI: HTML_NS, tagName: "button", type: TYPE_IDREF },
      { namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF },
    ],
  ],
  [
    "ping",
    [
      { namespaceURI: HTML_NS, tagName: "a", type: TYPE_URI_LIST },
      { namespaceURI: HTML_NS, tagName: "area", type: TYPE_URI_LIST },
    ],
  ],
  ["poster", [{ namespaceURI: HTML_NS, tagName: "video", type: TYPE_URI }]],
  ["profile", [{ namespaceURI: HTML_NS, tagName: "head", type: TYPE_URI }]],
  [
    "src",
    [
      { namespaceURI: "*", tagName: "script", type: TYPE_JS_RESOURCE_URI },
      { namespaceURI: HTML_NS, tagName: "input", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "frame", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "iframe", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "img", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "audio", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "embed", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "source", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "track", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "video", type: TYPE_URI },
      { namespaceURI: XUL_NS, tagName: "stringbundle", type: TYPE_URI },
    ],
  ],
  [
    "usemap",
    [
      { namespaceURI: HTML_NS, tagName: "img", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "input", type: TYPE_URI },
      { namespaceURI: HTML_NS, tagName: "object", type: TYPE_URI },
    ],
  ],
  ["xmlns", [{ namespaceURI: "*", tagName: "*", type: TYPE_URI }]],
  ["containment", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_URI }]],
  ["context", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF }]],
  [
    "datasources",
    [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_URI_LIST }],
  ],
  ["insertafter", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF }]],
  ["insertbefore", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF }]],
  ["observes", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF }]],
  ["popup", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF }]],
  ["ref", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_URI }]],
  ["removeelement", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF }]],
  ["template", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF }]],
  ["tooltip", [{ namespaceURI: XUL_NS, tagName: "*", type: TYPE_IDREF }]],
  // SVG links aren't handled yet, see bug 1158831.
  // ["fill", [
  //   {namespaceURI: SVG_NS, tagName: "*", type: },
  // ]],
  // ["stroke", [
  //   {namespaceURI: SVG_NS, tagName: "*", type: },
  // ]],
  // ["markerstart", [
  //   {namespaceURI: SVG_NS, tagName: "*", type: },
  // ]],
  // ["markermid", [
  //   {namespaceURI: SVG_NS, tagName: "*", type: },
  // ]],
  // ["markerend", [
  //   {namespaceURI: SVG_NS, tagName: "*", type: },
  // ]],
  // ["xlink:href", [
  //   {namespaceURI: SVG_NS, tagName: "*", type: },
  // ]],
]);

var parsers = {
  [TYPE_URI]: function(attributeValue) {
    return [
      {
        type: TYPE_URI,
        value: attributeValue,
      },
    ];
  },
  [TYPE_URI_LIST]: function(attributeValue) {
    const data = splitBy(attributeValue, " ");
    for (const token of data) {
      if (!token.type) {
        token.type = TYPE_URI;
      }
    }
    return data;
  },
  [TYPE_JS_RESOURCE_URI]: function(attributeValue) {
    return [
      {
        type: TYPE_JS_RESOURCE_URI,
        value: attributeValue,
      },
    ];
  },
  [TYPE_CSS_RESOURCE_URI]: function(attributeValue) {
    return [
      {
        type: TYPE_CSS_RESOURCE_URI,
        value: attributeValue,
      },
    ];
  },
  [TYPE_IDREF]: function(attributeValue) {
    return [
      {
        type: TYPE_IDREF,
        value: attributeValue,
      },
    ];
  },
  [TYPE_IDREF_LIST]: function(attributeValue) {
    const data = splitBy(attributeValue, " ");
    for (const token of data) {
      if (!token.type) {
        token.type = TYPE_IDREF;
      }
    }
    return data;
  },
};

/**
 * Parse an attribute value.
 * @param {String} namespaceURI The namespaceURI of the node that has the
 * attribute.
 * @param {String} tagName The tagName of the node that has the attribute.
 * @param {Array} attributes The list of all attributes of the node. This should
 * be an array of {name, value} objects.
 * @param {String} attributeName The name of the attribute to parse.
 * @return {Array} An array of tokens that represents the value. Each token is
 * an object {type: [string|uri|jsresource|cssresource|idref], value}.
 * For instance parsing the ping attribute in <a ping="uri1 uri2"> returns:
 * [
 *   {type: "uri", value: "uri2"},
 *   {type: "string", value: " "},
 *   {type: "uri", value: "uri1"}
 * ]
 */
function parseAttribute(namespaceURI, tagName, attributes, attributeName) {
  if (!hasAttribute(attributes, attributeName)) {
    throw new Error(
      `Attribute ${attributeName} isn't part of the ` + "provided attributes"
    );
  }

  const type = getType(namespaceURI, tagName, attributes, attributeName);
  if (!type) {
    return [
      {
        type: TYPE_STRING,
        value: getAttribute(attributes, attributeName),
      },
    ];
  }

  return parsers[type](getAttribute(attributes, attributeName));
}

/**
 * Get the type for links in this attribute if any.
 * @param {String} namespaceURI The node's namespaceURI.
 * @param {String} tagName The node's tagName.
 * @param {Array} attributes The node's attributes, as a list of {name, value}
 * objects.
 * @param {String} attributeName The name of the attribute to get the type for.
 * @return {Object} null if no type exist for this attribute on this node, the
 * type object otherwise.
 */
function getType(namespaceURI, tagName, attributes, attributeName) {
  if (!ATTRIBUTE_TYPES.get(attributeName)) {
    return null;
  }

  for (const typeData of ATTRIBUTE_TYPES.get(attributeName)) {
    const hasNamespace =
      namespaceURI === typeData.namespaceURI || typeData.namespaceURI === "*";
    const hasTagName =
      tagName.toLowerCase() === typeData.tagName || typeData.tagName === "*";
    const isValid = typeData.isValid
      ? typeData.isValid(namespaceURI, tagName, attributes, attributeName)
      : true;

    if (hasNamespace && hasTagName && isValid) {
      return typeData.type;
    }
  }

  return null;
}

function getAttribute(attributes, attributeName) {
  for (const { name, value } of attributes) {
    if (name === attributeName) {
      return value;
    }
  }
  return null;
}

function hasAttribute(attributes, attributeName) {
  for (const { name } of attributes) {
    if (name === attributeName) {
      return true;
    }
  }
  return false;
}

/**
 * Split a string by a given character and return an array of objects parts.
 * The array will contain objects for the split character too, marked with
 * TYPE_STRING type.
 * @param {String} value The string to parse.
 * @param {String} splitChar A 1 length split character.
 * @return {Array}
 */
function splitBy(value, splitChar) {
  const data = [];

  let i = 0,
    buffer = "";
  while (i <= value.length) {
    if (i === value.length && buffer) {
      data.push({ value: buffer });
    }
    if (value[i] === splitChar) {
      if (buffer) {
        data.push({ value: buffer });
      }
      data.push({
        type: TYPE_STRING,
        value: splitChar,
      });
      buffer = "";
    } else {
      buffer += value[i];
    }

    i++;
  }
  return data;
}

exports.parseAttribute = parseAttribute;
// Exported for testing only.
exports.splitBy = splitBy;
