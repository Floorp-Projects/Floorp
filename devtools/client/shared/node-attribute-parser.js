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

const WILDCARD = Symbol();

const ATTRIBUTE_TYPES = new Map([
  ["action", { form: { namespaceURI: HTML_NS, type: TYPE_URI } }],
  [
    "aria-activedescendant",
    { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF } },
  ],
  [
    "aria-controls",
    { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST } },
  ],
  [
    "aria-describedby",
    { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST } },
  ],
  [
    "aria-details",
    { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST } },
  ],
  [
    "aria-errormessage",
    { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF } },
  ],
  [
    "aria-flowto",
    { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST } },
  ],
  [
    "aria-labelledby",
    { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST } },
  ],
  ["aria-owns", { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST } }],
  ["background", { body: { namespaceURI: HTML_NS, type: TYPE_URI } }],
  [
    "cite",
    {
      blockquote: { namespaceURI: HTML_NS, type: TYPE_URI },
      q: { namespaceURI: HTML_NS, type: TYPE_URI },
      del: { namespaceURI: HTML_NS, type: TYPE_URI },
      ins: { namespaceURI: HTML_NS, type: TYPE_URI },
    },
  ],
  ["classid", { object: { namespaceURI: HTML_NS, type: TYPE_URI } }],
  [
    "codebase",
    {
      object: { namespaceURI: HTML_NS, type: TYPE_URI },
      applet: { namespaceURI: HTML_NS, type: TYPE_URI },
    },
  ],
  [
    "command",
    {
      menuitem: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      key: { namespaceURI: XUL_NS, type: TYPE_IDREF },
    },
  ],
  [
    "contextmenu",
    {
      WILDCARD: { namespaceURI: WILDCARD, type: TYPE_IDREF },
    },
  ],
  ["data", { object: { namespaceURI: HTML_NS, type: TYPE_URI } }],
  [
    "for",
    {
      label: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      output: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST },
    },
  ],
  [
    "form",
    {
      button: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      fieldset: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      input: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      keygen: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      label: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      object: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      output: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      select: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      textarea: { namespaceURI: HTML_NS, type: TYPE_IDREF },
    },
  ],
  [
    "formaction",
    {
      button: { namespaceURI: HTML_NS, type: TYPE_URI },
      input: { namespaceURI: HTML_NS, type: TYPE_URI },
    },
  ],
  [
    "headers",
    {
      td: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST },
      th: { namespaceURI: HTML_NS, type: TYPE_IDREF_LIST },
    },
  ],
  [
    "href",
    {
      a: { namespaceURI: HTML_NS, type: TYPE_URI },
      area: { namespaceURI: HTML_NS, type: TYPE_URI },
      link: [
        {
          namespaceURI: WILDCARD,
          type: TYPE_CSS_RESOURCE_URI,
          isValid: attributes => {
            return getAttribute(attributes, "rel") === "stylesheet";
          },
        },
        { namespaceURI: WILDCARD, type: TYPE_URI },
      ],
      base: { namespaceURI: HTML_NS, type: TYPE_URI },
    },
  ],
  [
    "icon",
    {
      menuitem: { namespaceURI: HTML_NS, type: TYPE_URI },
    },
  ],
  ["invoketarget", { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF } }],
  ["list", { input: { namespaceURI: HTML_NS, type: TYPE_IDREF } }],
  [
    "longdesc",
    {
      img: { namespaceURI: HTML_NS, type: TYPE_URI },
      frame: { namespaceURI: HTML_NS, type: TYPE_URI },
      iframe: { namespaceURI: HTML_NS, type: TYPE_URI },
    },
  ],
  ["manifest", { html: { namespaceURI: HTML_NS, type: TYPE_URI } }],
  [
    "menu",
    {
      button: { namespaceURI: HTML_NS, type: TYPE_IDREF },
      WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF },
    },
  ],
  [
    "ping",
    {
      a: { namespaceURI: HTML_NS, type: TYPE_URI_LIST },
      area: { namespaceURI: HTML_NS, type: TYPE_URI_LIST },
    },
  ],
  ["poster", { video: { namespaceURI: HTML_NS, type: TYPE_URI } }],
  ["profile", { head: { namespaceURI: HTML_NS, type: TYPE_URI } }],
  [
    "src",
    {
      script: { namespaceURI: WILDCARD, type: TYPE_JS_RESOURCE_URI },
      input: { namespaceURI: HTML_NS, type: TYPE_URI },
      frame: { namespaceURI: HTML_NS, type: TYPE_URI },
      iframe: { namespaceURI: HTML_NS, type: TYPE_URI },
      img: { namespaceURI: HTML_NS, type: TYPE_URI },
      audio: { namespaceURI: HTML_NS, type: TYPE_URI },
      embed: { namespaceURI: HTML_NS, type: TYPE_URI },
      source: { namespaceURI: HTML_NS, type: TYPE_URI },
      track: { namespaceURI: HTML_NS, type: TYPE_URI },
      video: { namespaceURI: HTML_NS, type: TYPE_URI },
      stringbundle: { namespaceURI: XUL_NS, type: TYPE_URI },
    },
  ],
  [
    "usemap",
    {
      img: { namespaceURI: HTML_NS, type: TYPE_URI },
      input: { namespaceURI: HTML_NS, type: TYPE_URI },
      object: { namespaceURI: HTML_NS, type: TYPE_URI },
    },
  ],
  ["xmlns", { WILDCARD: { namespaceURI: WILDCARD, type: TYPE_URI } }],
  ["containment", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_URI } }],
  ["context", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF } }],
  ["datasources", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_URI_LIST } }],
  ["insertafter", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF } }],
  ["insertbefore", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF } }],
  ["observes", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF } }],
  ["popovertarget", { WILDCARD: { namespaceURI: HTML_NS, type: TYPE_IDREF } }],
  ["popup", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF } }],
  ["ref", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_URI } }],
  ["removeelement", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF } }],
  ["template", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF } }],
  ["tooltip", { WILDCARD: { namespaceURI: XUL_NS, type: TYPE_IDREF } }],
  // SVG links aren't handled yet, see bug 1158831.
  // ["fill", {
  //   WILDCARD: {namespaceURI: SVG_NS, type: },
  // }],
  // ["stroke", {
  //   WILDCARD: {namespaceURI: SVG_NS, type: },
  // }],
  // ["markerstart", {
  //   WILDCARD: {namespaceURI: SVG_NS, type: },
  // }],
  // ["markermid", {
  //   WILDCARD: {namespaceURI: SVG_NS, type: },
  // }],
  // ["markerend", {
  //   WILDCARD: {namespaceURI: SVG_NS, type: },
  // }],
  // ["xlink:href", {
  //   WILDCARD: {namespaceURI: SVG_NS, type: },
  // }],
]);

var parsers = {
  [TYPE_URI](attributeValue) {
    return [
      {
        type: TYPE_URI,
        value: attributeValue,
      },
    ];
  },
  [TYPE_URI_LIST](attributeValue) {
    const data = splitBy(attributeValue, " ");
    for (const token of data) {
      if (!token.type) {
        token.type = TYPE_URI;
      }
    }
    return data;
  },
  [TYPE_JS_RESOURCE_URI](attributeValue) {
    return [
      {
        type: TYPE_JS_RESOURCE_URI,
        value: attributeValue,
      },
    ];
  },
  [TYPE_CSS_RESOURCE_URI](attributeValue) {
    return [
      {
        type: TYPE_CSS_RESOURCE_URI,
        value: attributeValue,
      },
    ];
  },
  [TYPE_IDREF](attributeValue) {
    return [
      {
        type: TYPE_IDREF,
        value: attributeValue,
      },
    ];
  },
  [TYPE_IDREF_LIST](attributeValue) {
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
 * @param {String} attributeValue The value of the attribute to parse.
 * @return {Array} An array of tokens that represents the value. Each token is
 * an object {type: [string|uri|jsresource|cssresource|idref], value}.
 * For instance parsing the ping attribute in <a ping="uri1 uri2"> returns:
 * [
 *   {type: "uri", value: "uri2"},
 *   {type: "string", value: " "},
 *   {type: "uri", value: "uri1"}
 * ]
 */
function parseAttribute(
  namespaceURI,
  tagName,
  attributes,
  attributeName,
  attributeValue
) {
  const type = getType(namespaceURI, tagName, attributes, attributeName);
  if (!type) {
    return [
      {
        type: TYPE_STRING,
        value: attributeValue,
      },
    ];
  }

  return parsers[type](attributeValue);
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
  const attributeType = ATTRIBUTE_TYPES.get(attributeName);
  if (!attributeType) {
    return null;
  }

  const lcTagName = tagName.toLowerCase();
  const typeData = attributeType[lcTagName] || attributeType.WILDCARD;

  if (!typeData) {
    return null;
  }

  if (Array.isArray(typeData)) {
    for (const data of typeData) {
      const hasNamespace =
        data.namespaceURI === WILDCARD || data.namespaceURI === namespaceURI;
      const isValid = data.isValid ? data.isValid(attributes) : true;

      if (hasNamespace && isValid) {
        return data.type;
      }
    }

    return null;
  } else if (
    typeData.namespaceURI === WILDCARD ||
    typeData.namespaceURI === namespaceURI
  ) {
    return typeData.type;
  }

  return null;
}

function getAttribute(attributes, attributeName) {
  const attribute = attributes.find(x => x.name === attributeName);
  return attribute ? attribute.value : null;
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
exports.ATTRIBUTE_TYPES = {
  TYPE_STRING,
  TYPE_URI,
  TYPE_URI_LIST,
  TYPE_IDREF,
  TYPE_IDREF_LIST,
  TYPE_JS_RESOURCE_URI,
  TYPE_CSS_RESOURCE_URI,
};

// Exported for testing only.
exports.splitBy = splitBy;
