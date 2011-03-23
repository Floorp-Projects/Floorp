/**
 * @fileoverview 
 * Canonicalization functions used in the RTE test suite.
 *
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the 'License')
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an 'AS IS' BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @version 0.1
 * @author rolandsteiner@google.com
 */

/**
 * Canonicalize HTML entities to their actual character
 *
 * @param str {String} the HTML string to be canonicalized
 * @return {String} the canonicalized string
 */

function canonicalizeEntities(str) {
  // TODO(rolandsteiner): this function is very much not optimized, but that shouldn't
  // theoretically matter too much - look into it at some point.
  var match;
  while (match = str.match(/&#x([0-9A-F]+);/i)) {
    str = str.replace('&#x' + match[1] + ';', String.fromCharCode(parseInt(match[1], 16)));
  }
  while (match = str.match(/&#([0-9]+);/)) {
    str = str.replace('&#' + match[1] + ';', String.fromCharCode(Number(match[1])));
  }
  return str;
}

/**
 * Canonicalize the contents of the HTML 'style' attribute. 
 * I.e. sorts the CSS attributes alphabetically and canonicalizes the values
 * CSS attributes where necessary.
 *
 * If this would return an empty string, return null instead to suppress the
 * whole 'style' attribute.
 *
 * Avoid tests that contain {, } or : within CSS values!
 *
 * Note that this function relies on the spaces of the input string already
 * having been normalized by canonicalizeSpaces!
 *
 * FIXME: does not canonicalize the contents of compound attributes
 * (e.g., 'border').
 * 
 * @param str {String} contents of the 'style' attribute
 * @param emitFlags {Object} flags used for this output
 * @return {String/null} canonicalized string, null instead of the empty string
 */
function canonicalizeStyle(str, emitFlags) {
  // Remove any enclosing curly brackets
  str = str.replace(/ ?[\{\}] ?/g, '');

  var attributes = str.split(';');
  var count = attributes.length;
  var resultArr = [];
  
  for (var a = 0; a < count; ++a) {
    // Retrieve "name: value" pair
    // Note: may expectedly fail if the last pair was terminated with ';'
    var avPair = attributes[a].match(/ ?([^ :]+) ?: ?(.+)/);
    if (!avPair)
      continue;

    var name  = avPair[1];
    var value = avPair[2].replace(/ $/, '');  // Remove any trailing space.

    switch (name) {
      case 'color':
      case 'background-color':
      case 'border-color':
        if (emitFlags.canonicalizeUnits) {
          resultArr.push(name + ': #' + new Color(value).toHexString());
        } else {
          resultArr.push(name + ': ' + value);
        }
        break;
      
      case 'font-family':
        if (emitFlags.canonicalizeUnits) {
          resultArr.push(name + ': ' + new FontName(value).toString());
        } else {
          resultArr.push(name + ': ' + value);
        }
        break;
        
      case 'font-size':
        if (emitFlags.canonicalizeUnits) {
          resultArr.push(name + ': ' + new FontSize(value).toString());
        } else {
          resultArr.push(name + ': ' + value);
        }
        break;
        
      default:
        resultArr.push(name + ': ' + value);  
    }
  }
  
  // Sort by name, assuming no duplicate CSS attribute names.
  resultArr.sort();

  return resultArr.join('; ') || null;
} 

/**
 * Canonicalize a single attribute value.
 *
 * Note that this function relies on the spaces of the input string already
 * having been normalized by canonicalizeSpaces!
 *
 * @param elemName {String} the name of the element
 * @param attrName {String} the name of the attribute
 * @param attrValue {String} the value of the attribute
 * @param emitFlags {Object} flags used for this output
 * @return {String/null} the canonicalized value, or null if the attribute should be skipped.
 */
function canonicalizeSingleAttribute(elemName, attrName, attrValue, emitFlags) {
  // We emit attributes as name="value", so change any contained apostrophes
  // to quote marks.
  attrValue = attrValue.replace(/\x22/, '\x27');

  switch (attrName) {
    case 'class':
      return emitFlags.emitClass ? attrValue : null;

    case 'id':
      if (!emitFlags.emitID) {
        return null;
      }
      if (attrValue && attrValue.substr(0, 7) == 'editor-') {
        return null;
      }
      return attrValue;

    // Remove empty style attributes, canonicalize the contents otherwise,
    // provided the test cares for styles.
    case 'style':
      return (emitFlags.emitStyle && attrValue) 
                 ? canonicalizeStyle(attrValue, emitFlags)
                 : null;

    // Never output onload handlers as they are set by the test environment.
    case 'onload':
        return null;

    // Canonicalize colors.
    case 'bgcolor':
    case 'color':
      if (!attrValue) {
        return null;
      }
      return emitFlags.canonicalizeUnits ? new Color(attrValue).toString() : attrValue;

    // Canonicalize font names.
    case 'face':
      return emitFlags.canonicalizeUnits ? new FontName(attrValue).toString() : attrValue;
      
    // Canonicalize font sizes (leave other 'size' attributes as-is).
    case 'size':
      if (!attrValue) {
        return null;
      }
      switch (elemName) {
        case 'basefont':
        case 'font':
          return emitFlags.canonicalizeUnits ? new FontSize(attrValue).toString() : attrValue;
      }
      return attrValue;
      
    // Remove spans with value 1. Retain spans with other values, even if
    // empty or with a value 0, since those indicate a flawed implementation.
    case 'colspan':
    case 'rowspan':
    case 'span':
      return (attrValue == '1' || attrValue === '') ? null : attrValue;
      
    // Boolean attributes: presence equals true. If present, the value must be
    // the empty string or the attribute's canonical name.
    // (http://www.whatwg.org/specs/web-apps/current-work/#boolean-attributes)
    // Below we only normalize empty string to the canonical name for
    // comparison purposes. All other values are not touched and will therefore
    // in all likelihood result in a failed test (even if they may be accepted
    // by the UA).
    case 'async':
    case 'autofocus':
    case 'checked':
    case 'compact':
    case 'declare':
    case 'defer':
    case 'disabled':
    case 'formnovalidate':
    case 'frameborder':
    case 'ismap':
    case 'loop':
    case 'multiple':
    case 'nohref':
    case 'nosize':
    case 'noshade':
    case 'novalidate':
    case 'nowrap':
    case 'open':
    case 'readonly':
    case 'required':
    case 'reversed':
    case 'seamless':
    case 'selected':
      return attrValue ? attrValue : attrName;
      
    default:
      return attrValue;  
  }
}
 
/**
 * Canonicalize the contents of an element tag.
 *
 * I.e. sorts the attributes alphabetically and canonicalizes their
 * values where necessary. Also removes attributes we're not interested in.
 *
 * Note that this function relies on the spaces of the input string already
 * having been normalized by canonicalizeSpaces!
 *
 * @param str {String} the contens of the element tag, excluding < and >.
 * @param emitFlags {Object} flags used for this output
 * @return {String} the canonicalized contents.
 */
function canonicalizeElementTag(str, emitFlags) {
  // FIXME: lowercase only if emitFlags.lowercase is set
  str = str.toLowerCase();

  var pos = str.search(' ');

  // element name only
  if (pos == -1) {
    return str;
  }

  var elemName = str.substr(0, pos);
  str = str.substr(pos + 1);

  // Even if emitFlags.emitAttrs is not set, we must iterate over the
  // attributes to catch the special selection attribute and/or selection
  // markers. :(

  // Iterate over attributes, add them to an array, canonicalize their
  // contents, and finally output the (remaining) attributes in sorted order.
  // Note: We can't do a simple split on space here, because the value of,
  // e.g., 'style' attributes may also contain spaces.
  var attrs = [];
  var selStartInTag = false;
  var selEndInTag = false;

  while (str) {
    var attrName;
    var attrValue = '';
    
    pos = str.search(/[ =]/);
    if (pos >= 0) {
      attrName = str.substr(0, pos);
      if (str.charAt(pos) == ' ') {
        ++pos;
      }
      if (str.charAt(pos) == '=') {
        ++pos;
        if (str.charAt(pos) == ' ') {
          ++pos;
        }
        str = str.substr(pos);
        switch (str.charAt(0)) {
          case '"':
          case "'":
            pos = str.indexOf(str.charAt(0), 1);
            pos = (pos < 0) ? str.length : pos;
            attrValue = str.substring(1, pos);
            ++pos;
            break;

          default:
            pos = str.indexOf(' ', 0);
            pos = (pos < 0) ? str.length : pos;
            attrValue = (pos == -1) ? str : str.substr(0, pos);
            break;
        }
        attrValue = attrValue.replace(/^ /, '');         
        attrValue = attrValue.replace(/ $/, '');
      }
    } else {
      attrName = str;
    }
    str = (pos == -1 || pos >= str.length) ? '' : str.substr(pos + 1);

    // Remove special selection attributes.
    switch (attrName) {
      case ATTRNAME_SEL_START:
        selStartInTag = true;
        continue;
      
      case ATTRNAME_SEL_END:
        selEndInTag = true;
        continue;
    }

    switch (attrName) {
      case '':
      case 'onload':
      case 'xmlns':
        break;
        
      default:
        if (!emitFlags.emitAttrs) {
          break;
        }
        // >>> fall through >>>
        
      case 'contenteditable':
        attrValue = canonicalizeEntities(attrValue);
        attrValue = canonicalizeSingleAttribute(elemName, attrName, attrValue, emitFlags);
        if (attrValue !== null) {
          attrs.push(attrName + '="' + attrValue + '"');
        }
    }
  }

  var result = elemName;

  // Sort alphabetically (on full string rather than just attribute value for
  // simplicity. Also, attribute names will differ when encountering the '=').
  if (attrs.length > 0) {
    attrs.sort();
    result += ' ' + attrs.join(' ');
  }

  // Add intra-tag selection marker(s) or attribute(s), if any, at the end.
  if (selStartInTag && selEndInTag) {
    result += ' |';
  } else if (selStartInTag) {
    result += ' {';
  } else if (selEndInTag) {
    result += ' }';
  }

  return result;
}

/**
 * Canonicalize elements and attributes to facilitate comparison to the
 * expectation string: sort attributes, canonicalize values and remove chaff.
 *
 * Note that this function relies on the spaces of the input string already
 * having been normalized by canonicalizeSpaces!
 *
 * @param str {String} the HTML string to be canonicalized
 * @param emitFlags {Object} flags used for this output
 * @return {String} the canonicalized string
 */
function canonicalizeElementsAndAttributes(str, emitFlags) {
  var tagStart = str.indexOf('<');
  var tagEnd   = 0;
  var result   = '';

  while (tagStart >= 0) {
    ++tagStart;
    if (str.charAt(tagStart) == '/') {
      ++tagStart;
    }
    result = result + canonicalizeEntities(str.substring(tagEnd, tagStart));
    tagEnd = str.indexOf('>', tagStart);
    if (tagEnd < 0) {
      tagEnd = str.length - 1;  
    }
    if (str.charAt(tagEnd - 1) == '/') {
      --tagEnd;
    }
    var elemStr = str.substring(tagStart, tagEnd);
    elemStr = canonicalizeElementTag(elemStr, emitFlags);
    result = result + elemStr;
    tagStart = str.indexOf('<', tagEnd);
  }
  return result + canonicalizeEntities(str.substring(tagEnd));
}

/**
 * Canonicalize an innerHTML string to uniform single whitespaces.
 *
 * FIXME: running this prevents testing for pre-formatted content
 * and the CSS 'white-space' attribute.
 *
 * @param str {String} the HTML string to be canonicalized
 * @return {String} the canonicalized string
 */
function canonicalizeSpaces(str) {
  // Collapse sequential whitespace.
  str = str.replace(/\s+/g, ' ');

  // Remove spaces immediately inside angle brackets <, >, </ and />.
  // While doing this also canonicalize <.../> to <...>.
  str = str.replace(/\< ?/g, '<');
  str = str.replace(/\<\/ ?/g, '</');
  str = str.replace(/ ?\/?\>/g, '>');
  
  return str;
}

/**
 * Canonicalize an innerHTML string to uniform single whitespaces.
 * Also remove comments to retain only embedded selection markers, and
 * remove </br> and </hr> if present.
 *
 * FIXME: running this prevents testing for pre-formatted content
 * and the CSS 'white-space' attribute.
 *
 * @param str {String} the HTML string to be canonicalized
 * @return {String} the canonicalized string
 */
function initialCanonicalizationOf(str) {
  str = canonicalizeSpaces(str);
  str = str.replace(/ ?<!-- ?/g, '');
  str = str.replace(/ ?--> ?/g, '');
  str = str.replace(/<\/[bh]r>/g, '');
  
  return str;
}
