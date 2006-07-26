# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Browser Search Service.
#
# The Initial Developer of the Original Code is
# Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005-2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <beng@google.com> (Original author)
#   Gavin Sharp <gavin@gavinsharp.com>
#   Joe Hughes  <joe@retrovirus.com>
#   Pamela Greene <pamg.bugs@gmail.com>
#   Michael Kaply
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# This code is a subset of nsSearchService.js from the current trunk
# It is used to retrieve the name from a Sherlock SRC file

const PERMS_FILE                 = 0644;
const MODE_RDONLY                = 0x01;
const SEARCH_DATA_XML            = 1;
const SEARCH_DATA_TEXT           = 2;
const SEARCH_TYPE_SHERLOCK       = 2;
const NEW_LINES = /(\r\n|\r|\n)/;

// Returns false for whitespace-only or commented out lines in a
// Sherlock file, true otherwise.
function isUsefulLine(aLine) {
  return !(/^\s*($|#)/i.test(aLine));
}

function getSearchEngineName(searchenginepath)
{
  var data;
  var type;
  var dataType = SEARCH_DATA_TEXT;
  var name;
  
  var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                             .createInstance(Components.interfaces.nsILocalFile);
  sourcefile.initWithPath(searchenginepath);

  var fileInStream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                               .createInstance(Components.interfaces.nsIFileInputStream);
  fileInStream.init(sourcefile, MODE_RDONLY, PERMS_FILE, false);

  switch (dataType) {
    case SEARCH_DATA_XML:

      var domParser = Components.classes["@mozilla.org/xmlextras/domparser;1"]
                                .createInstance(Components.interfaces.nsIDOMParser);
      var doc = domParser.parseFromStream(fileInStream, "UTF-8",
                                          sourcefile.fileSize,
                                          "text/xml");
      data = doc.documentElement;

//      if (checkNameSpace(data, [MOZSEARCH_LOCALNAME], [MOZSEARCH_NS_10])) {
//        type = SEARCH_TYPE_MOZSEARCH;
//          parseAsMozSearch();
//      } else if (checkNameSpace(data, [OPENSEARCH_LOCALNAME], OPENSEARCH_NAMESPACES)) {
//        type = SEARCH_TYPE_OPENSEARCH;
//          parseAsOpenSearch();
//      }
      break;
    case SEARCH_DATA_TEXT:
      var binaryInStream = Components.classes["@mozilla.org/binaryinputstream;1"]
                                     .createInstance(Components.interfaces.nsIBinaryInputStream);
      binaryInStream.setInputStream(fileInStream);

      data = binaryInStream.readByteArray(binaryInStream.available());
      type = SEARCH_TYPE_SHERLOCK;
      name = parseAsSherlock(data);
      break;
  }
  fileInStream.close();
  return name;
}

function fileCharsetFromCode(aCode) {
  const codes = [
    "x-mac-roman",           // 0
    "Shift_JIS",             // 1
    "Big5",                  // 2
    "EUC-KR",                // 3
    "X-MAC-ARABIC",          // 4
    "X-MAC-HEBREW",          // 5
    "X-MAC-GREEK",           // 6
    "X-MAC-CYRILLIC",        // 7
    "X-MAC-DEVANAGARI" ,     // 9
    "X-MAC-GURMUKHI",        // 10
    "X-MAC-GUJARATI",        // 11
    "X-MAC-ORIYA",           // 12
    "X-MAC-BENGALI",         // 13
    "X-MAC-TAMIL",           // 14
    "X-MAC-TELUGU",          // 15
    "X-MAC-KANNADA",         // 16
    "X-MAC-MALAYALAM",       // 17
    "X-MAC-SINHALESE",       // 18
    "X-MAC-BURMESE",         // 19
    "X-MAC-KHMER",           // 20
    "X-MAC-THAI",            // 21
    "X-MAC-LAOTIAN",         // 22
    "X-MAC-GEORGIAN",        // 23
    "X-MAC-ARMENIAN",        // 24
    "GB2312",                // 25
    "X-MAC-TIBETAN",         // 26
    "X-MAC-MONGOLIAN",       // 27
    "X-MAC-ETHIOPIC",        // 28
    "X-MAC-CENTRALEURROMAN", // 29
    "X-MAC-VIETNAMESE",      // 30
    "X-MAC-EXTARABIC"        // 31
  ];
  // Sherlock files have always defaulted to x-mac-roman, so do that here too
  return codes[aCode] || codes[0];
}

/**
 * Returns a string interpretation of aBytes using aCharset, or null on
 * failure.
 */
function bytesToString(aBytes, aCharset) {
  var converter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
                            .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);

  try {
    converter.charset = aCharset;
    return converter.convertFromByteArray(aBytes, aBytes.length);
  } catch (ex) {}

  return null;
}

/**
 * Converts an array of bytes representing a Sherlock file into an array of
 * lines representing the useful data from the file.
 *
 * @param aBytes
 *        The array of bytes representing the Sherlock file.
 * @param aCharsetCode
 *        An integer value representing a character set code to be passed to
 *        fileCharsetFromCode, or null for the default Sherlock encoding.
 */
function sherlockBytesToLines(aBytes, aCharsetCode) {
  // fileCharsetFromCode returns the default encoding if aCharsetCode is null
  var charset = fileCharsetFromCode(aCharsetCode);

  var dataString = bytesToString(aBytes, charset);

  // Split the string into lines, and filter out comments and
  // whitespace-only lines
  return dataString.split(NEW_LINES).filter(isUsefulLine);
}

/**
 * Extract search engine information from the collected data to initialize
 * the engine object.
 */
function parseAsSherlock(data) {
  /**
   * Trims leading and trailing whitespace from aStr.
   */
  function sTrim(aStr) {
    return aStr.replace(/^\s+/g, "").replace(/\s+$/g, "");
  }

  /**
   * Extracts one Sherlock "section" from aSource. A section is essentially
   * an HTML element with attributes, but each attribute must be on a new
   * line, by definition.
   *
   * @param aLines
   *        An array of lines from the sherlock file.
   * @param aSection
   *        The name of the section (e.g. "search" or "browser"). This value
   *        is not case sensitive.
   * @returns an object whose properties correspond to the section's
   *          attributes.
   */
  function getSection(aLines, aSection) {
    var lines = aLines;
    var startMark = new RegExp("^\\s*<" + aSection.toLowerCase() + "\\s*",
                               "gi");
    var endMark   = /\s*>\s*$/gi;

    var foundStart = false;
    var startLine, numberOfLines;
    // Find the beginning and end of the section
    for (var i = 0; i < lines.length; i++) {
      if (foundStart) {
        if (endMark.test(lines[i])) {
          numberOfLines = i - startLine;
          // Remove the end marker
          lines[i] = lines[i].replace(endMark, "");
          // If the endmarker was not the only thing on the line, include
          // this line in the results
          if (lines[i])
            numberOfLines++;
          break;
        }
      } else {
        if (startMark.test(lines[i])) {
          foundStart = true;
          // Remove the start marker
          lines[i] = lines[i].replace(startMark, "");
          startLine = i;
          // If the line is empty, don't include it in the result
          if (!lines[i])
            startLine++;
        }
      }
    }
    lines = lines.splice(startLine, numberOfLines);

    var section = {};
    for (var i = 0; i < lines.length; i++) {
      var line = sTrim(lines[i]);

      var els = line.split("=");
      var name = sTrim(els.shift().toLowerCase());
      var value = sTrim(els.join("="));

      if (!name || !value)
        continue;

      // Strip leading and trailing whitespace, remove quotes from the
      // value, and remove any trailing slashes or ">" characters
      value = value.replace(/^["']/, "")
                   .replace(/["']\s*[\\\/]?>?\s*$/, "") || "";
      value = sTrim(value);

      // Don't clobber existing attributes
      if (!(name in section))
        section[name] = value;
    }
    return section;
  }

  // First try converting our byte array using the default Sherlock encoding.
  // If this fails, or if we find a sourceTextEncoding attribute, we need to
  // reconvert the byte array using the specified encoding.
  var sherlockLines, searchSection, sourceTextEncoding;
  try {
    sherlockLines = sherlockBytesToLines(data);
    searchSection = getSection(sherlockLines, "search");
    sourceTextEncoding = parseInt(searchSection["sourcetextencoding"]);
    if (sourceTextEncoding) {
      // Re-convert the bytes using the found sourceTextEncoding
      sherlockLines = sherlockBytesToLines(data, sourceTextEncoding);
      searchSection = getSection(sherlockLines, "search");
    }
  } catch (ex) {
    // The conversion using the default charset failed. Remove any non-ascii
    // bytes and try to find a sourceTextEncoding.
    var asciiBytes = data.filter(function (n) {return !(0x80 & n);});
    var asciiString = String.fromCharCode.apply(null, asciiBytes);
    sherlockLines = asciiString.split(NEW_LINES).filter(isUsefulLine);
    searchSection = getSection(sherlockLines, "search");
    sourceTextEncoding = parseInt(searchSection["sourcetextencoding"]);
    if (sourceTextEncoding) {
      sherlockLines = sherlockBytesToLines(data, sourceTextEncoding);
      searchSection = getSection(sherlockLines, "search");
    } else {
//      ERROR("Couldn't find a working charset", Cr.NS_ERROR_FAILURE);
    }
  }
  return searchSection["name"];
}

