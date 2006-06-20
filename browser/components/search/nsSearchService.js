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
#   Joe Hughes  <joe@retrovirus.com
#   Pamela Greene <pamg.bugs@gmail.com>
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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

// Directory service keys
const NS_APP_SEARCH_DIR_LIST  = "SrchPluginsDL";
const NS_APP_USER_SEARCH_DIR  = "UsrSrchPlugns";

// See documentation in nsIBrowserSearchService.idl.
const SEARCH_ENGINE_TOPIC        = "browser-search-engine-modified";
const XPCOM_SHUTDOWN_TOPIC       = "xpcom-shutdown";

const SEARCH_ENGINE_REMOVED      = "engine-removed";
const SEARCH_ENGINE_ADDED        = "engine-added";
const SEARCH_ENGINE_CHANGED      = "engine-changed";
const SEARCH_ENGINE_LOADED       = "engine-loaded";
const SEARCH_ENGINE_CURRENT      = "engine-current";

const SEARCH_TYPE_MOZSEARCH      = Ci.nsISearchEngine.TYPE_MOZSEARCH;
const SEARCH_TYPE_OPENSEARCH     = Ci.nsISearchEngine.TYPE_OPENSEARCH;
const SEARCH_TYPE_SHERLOCK       = Ci.nsISearchEngine.TYPE_SHERLOCK;

const SEARCH_DATA_XML            = Ci.nsISearchEngine.DATA_XML;
const SEARCH_DATA_TEXT           = Ci.nsISearchEngine.DATA_TEXT;

// File extensions for search plugin description files
const XML_FILE_EXT      = "xml";
const SHERLOCK_FILE_EXT = "src";

const ICON_DATAURL_PREFIX = "data:image/x-icon;base64,";

// Supported extensions for Sherlock plugin icons
const SHERLOCK_ICON_EXTENSIONS = [".gif", ".png", ".jpg", ".jpeg"];

const NEW_LINES = /(\r\n|\r|\n)/;

// Set an arbitrary cap on the maximum icon size. Without this, large icons can
// cause big delays when loading them at startup.
const MAX_ICON_SIZE   = 10000;

// Default charset to use for sending search parameters. This is used to match
// previous nsInternetSearchService behavior.
const DEFAULT_QUERY_CHARSET = "ISO-8859-1";

const SEARCH_BUNDLE = "chrome://browser/locale/search.properties";
const BRAND_BUNDLE = "chrome://branding/locale/brand.properties";

const OPENSEARCH_NS_10  = "http://a9.com/-/spec/opensearch/1.0/";
const OPENSEARCH_NS_11  = "http://a9.com/-/spec/opensearch/1.1/";

// Although the specification at http://opensearch.a9.com/spec/1.1/description/
// gives the namespace names defined above, many existing OpenSearch engines
// are using the following versions.  We therefore allow either.
const OPENSEARCH_NAMESPACES = [ OPENSEARCH_NS_11, OPENSEARCH_NS_10,
                                "http://a9.com/-/spec/opensearchdescription/1.1/",
                                "http://a9.com/-/spec/opensearchdescription/1.0/"];
const OPENSEARCH_LOCALNAME = "OpenSearchDescription";

const MOZSEARCH_NS_10     = "http://www.mozilla.org/2006/browser/search/";
const MOZSEARCH_LOCALNAME = "SearchPlugin";

const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";
const URLTYPE_SEARCH_HTML  = "text/html";

// Empty base document used to serialize engines to file.
const EMPTY_DOC = "<?xml version=\"1.0\"?>\n" +
                  "<" + MOZSEARCH_LOCALNAME +
                  " xmlns=\"" + MOZSEARCH_NS_10 + "\"" +
                  " xmlns:os=\"" + OPENSEARCH_NS_11 + "\"" +
                  "/>";

const BROWSER_SEARCH_PREF = "browser.search.";

// Unsupported search parameters, which will be replaced with blanks.
// XXX We do use inputEncoding - should consider having it available. This
// would require doing multiple parameter substitution, so just having
// searchTerms is sufficient for now.
const kInvalidWords = /(\{count\})|(\{startIndex\})|(\{startPage\})|(\{language\})|(\{outputEncoding\})|(\{inputEncoding\})/;

// Supported search parameters.
const kValidWords = /\{searchTerms\}/gi;
const kUserDefined = "{searchTerms}";

// Returns false for whitespace-only or commented out lines in a
// Sherlock file, true otherwise.
function isUsefulLine(aLine) {
  return !(/^\s*($|#)/i.test(aLine));
}

/**
 * Used to determine whether an "input" line from a Sherlock file is a "user
 * defined" input. That is, check for the string "user", preceded by either
 * whitespace or a quote, followed by any of ">", "=", """, "'", whitespace,
 * a slash, "+", or EOL.
 */
const kIsUserInput = /(\s|["'=])user(\s|[>="'\/\\+]|$)/i;

var MozStorageStatementWrapper = null;

/**
 * Prefixed to all search debug output.
 */
const SEARCH_LOG_PREFIX = "*** Search: ";

/**
 * Outputs aText to the JavaScript console as well as to stdout, if the search
 * logging pref (browser.search.log) is set to true.
 */
function LOG(aText) {
  var prefB = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  var shouldLog = false;
  try {
    shouldLog = prefB.getBoolPref(BROWSER_SEARCH_PREF + "log");
  } catch (ex) {}

  if (shouldLog) {
    dump(SEARCH_LOG_PREFIX + aText + "\n");
    var consoleService = Cc["@mozilla.org/consoleservice;1"].
                         getService(Ci.nsIConsoleService);
    consoleService.logStringMessage(aText);
  }
}

function ERROR(message, resultCode) {
  NS_ASSERT(false, SEARCH_LOG_PREFIX + message);
  throw resultCode;
}

/**
 * Ensures an assertion is met before continuing. Should be used to indicate
 * fatal errors.
 * @param  assertion
 *         An assertion that must be met
 * @param  message
 *         A message to display if the assertion is not met
 * @param  resultCode
 *         The NS_ERROR_* value to throw if the assertion is not met
 * @throws resultCode
 */
function ENSURE_WARN(assertion, message, resultCode) {
  NS_ASSERT(assertion, SEARCH_LOG_PREFIX + message);
  if (!assertion)
    throw resultCode;
}

/**
 * Ensures an assertion is met before continuing, but does not warn the user.
 * Used to handle normal failure conditions.
 * @param  assertion
 *         An assertion that must be met
 * @param  message
 *         A message to display if the assertion is not met
 * @param  resultCode
 *         The NS_ERROR_* value to throw if the assertion is not met
 * @throws resultCode
 */
function ENSURE(assertion, message, resultCode) {
  if (!assertion) {
    LOG(message);
    throw resultCode;
  }
}

/**
 * Ensures an argument assertion is met before continuing.
 * @param  assertion
 *         An argument assertion that must be met
 * @param  message
 *         A message to display if the assertion is not met
 * @throws NS_ERROR_INVALID_ARG for invalid arguments
 */
function ENSURE_ARG(assertion, message) {
  ENSURE(assertion, message, Cr.NS_ERROR_INVALID_ARG);
}

//XXX Bug 326854: no btoa for components, use our own
/**
 * Encodes an array of bytes into a string using the base 64 encoding scheme.
 * @param aBytes
 *        An array of bytes to encode.
 */
function b64(aBytes) {
  const B64_CHARS =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  var out = "", bits, i, j;

  while (aBytes.length >= 3) {
    bits = 0;
    for (i = 0; i < 3; i++) {
      bits <<= 8;
      bits |= aBytes[i];
    }
    for (j = 18; j >= 0; j -= 6)
      out += B64_CHARS[(bits>>j) & 0x3F];

    aBytes.splice(0, 3);
  }

  switch (aBytes.length) {
    case 2:
      out += B64_CHARS[(aBytes[0]>>2) & 0x3F];
      out += B64_CHARS[((aBytes[0] & 0x03) << 4) | ((aBytes[1] >> 4) & 0x0F)];
      out += B64_CHARS[((aBytes[1] & 0x0F) << 2)];
      out += "=";
      break;
    case 1:
      out += B64_CHARS[(aBytes[0]>>2) & 0x3F];
      out += B64_CHARS[(aBytes[0] & 0x03) << 4];
      out += "==";
      break;
  }
  return out;
}

function loadListener(aChannel, aEngine, aCallback) {
  this._channel = aChannel;
  this._bytes = [],
  this._engine = aEngine;
  this._callback = aCallback;
}
loadListener.prototype = {
  _callback: null,
  _channel: null,
  _countRead: 0,
  _engine: null,
  _stream: null,

  QueryInterface: function SRCH_loadQI(aIID) {
    if (aIID.equals(Ci.nsISupports)           ||
        aIID.equals(Ci.nsIRequestObserver)    ||
        aIID.equals(Ci.nsIStreamListener)     ||
        aIID.equals(Ci.nsIChannelEventSink)   ||
        aIID.equals(Ci.nsIInterfaceRequestor) ||
        // See XXX comment below
        aIID.equals(Ci.nsIHttpEventSink)      ||
        aIID.equals(Ci.nsIProgressEventSink)  ||
        false)
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  // nsIRequestObserver
  onStartRequest: function SRCH_loadStartR(aRequest, aContext) {
    LOG("loadListener: Starting request: " + aRequest.name);
    this._stream = Cc["@mozilla.org/binaryinputstream;1"].
                   createInstance(Ci.nsIBinaryInputStream);
  },

  onStopRequest: function SRCH_loadStopR(aRequest, aContext, aStatusCode) {
    LOG("loadListener: Stopping request: " + aRequest.name);

    var httpFailed = (aRequest instanceof Ci.nsIHttpChannel) &&
                     !aRequest.requestSucceeded;
    if (httpFailed || !Components.isSuccessCode(aStatusCode) ||
        this._countRead == 0) {
      LOG("loadListener: request failed!");
      // send null so the callback can deal with the failure
      this._callback(null, this._engine);
    } else
      this._callback(this._bytes, this._engine);
    this._channel = null;
    this._engine  = null;
  },

  // nsIStreamListener
  onDataAvailable: function SRCH_loadDAvailable(aRequest, aContext,
                                                aInputStream, aOffset,
                                                aCount) {
    this._stream.setInputStream(aInputStream);

    // Get a byte array of the data
    this._bytes = this._bytes.concat(this._stream.readByteArray(aCount));
    this._countRead += aCount;
  },

  // nsIChannelEventSink
  onChannelRedirect: function SRCH_loadCRedirect(aOldChannel, aNewChannel,
                                                 aFlags) {
    this._channel = aNewChannel;
  },

  // nsIInterfaceRequestor
  getInterface: function SRCH_load_GI(aIID) {
    return this.QueryInterface(aIID);
  },

  // XXX bug 253127
  // nsIHttpEventSink
  onRedirect: function (aChannel, aNewChannel) {},
  // nsIProgressEventSink
  onProgress: function (aRequest, aContext, aProgress, aProgressMax) {},
  onStatus: function (aRequest, aContext, aStatus, aStatusArg) {}
}


/**
 * Used to verify a given DOM node's localName and namespaceURI.
 * @param aElement
 *        The element to verify.
 * @param aLocalNameArray
 *        An array of strings to compare against aElement's localName.
 * @param aNameSpaceArray
 *        An array of strings to compare against aElement's namespaceURI.
 *
 * @returns false if aElement is null, or if its localName or namespaceURI
 *          does not match one of the elements in the aLocalNameArray or
 *          aNameSpaceArray arrays, respectively.
 * @throws NS_ERROR_INVALID_ARG if aLocalNameArray or aNameSpaceArray are null.
 */
function checkNameSpace(aElement, aLocalNameArray, aNameSpaceArray) {
  ENSURE_ARG(aLocalNameArray && aNameSpaceArray, "missing aLocalNameArray or \
             aNameSpaceArray for checkNameSpace");
  return (aElement                                                &&
          (aLocalNameArray.indexOf(aElement.localName)    != -1)  &&
          (aNameSpaceArray.indexOf(aElement.namespaceURI) != -1));
}

/**
 * Safely close a nsISafeOutputStream.
 * @param aFOS
 *        The file output stream to close.
 */
function closeSafeOutputStream(aFOS) {
  if (aFOS instanceof Ci.nsISafeOutputStream) {
    try {
      aFOS.finish();
      return;
    } catch (e) { }
  }
  aFOS.close();
}

/**
 * Wrapper function for nsIIOService::newURI.
 * @param aURLSpec
 *        The URL string from which to create an nsIURI.
 * @returns an nsIURI object, or null if the creation of the URI failed.
 */
function makeURI(aURLSpec, aCharset) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  try {
    return ios.newURI(aURLSpec, aCharset, null);
  } catch (ex) { }

  return null;
}

/**
 * Gets a directory from the directory service.
 * @param aKey
 *        The directory service key indicating the directory to get.
 */
function getDir(aKey) {
  ENSURE_ARG(aKey, "getDir requires a directory key!");

  var fileLocator = Cc["@mozilla.org/file/directory_service;1"].
                    getService(Ci.nsIProperties);
  var dir = fileLocator.get(aKey, Ci.nsIFile);
  return dir;
}

/**
 * The following two functions are essentially copied from
 * nsInternetSearchService. They are required for backwards compatibility.
 */
function queryCharsetFromCode(aCode) {
  const codes = [];
  codes[0] = "x-mac-roman";
  codes[6] = "x-mac-greek";
  codes[35] = "x-mac-turkish";
  codes[513] = "ISO-8859-1";
  codes[514] = "ISO-8859-2";
  codes[517] = "ISO-8859-5";
  codes[518] = "ISO-8859-6";
  codes[519] = "ISO-8859-7";
  codes[520] = "ISO-8859-8";
  codes[521] = "ISO-8859-9";
  codes[1049] = "IBM864";
  codes[1280] = "windows-1252";
  codes[1281] = "windows-1250";
  codes[1282] = "windows-1251";
  codes[1283] = "windows-1253";
  codes[1284] = "windows-1254";
  codes[1285] = "windows-1255";
  codes[1286] = "windows-1256";
  codes[1536] = "us-ascii";
  codes[1584] = "GB2312";
  codes[1585] = "x-gbk";
  codes[1600] = "EUC-KR";
  codes[2080] = "ISO-2022-JP";
  codes[2096] = "ISO-2022-CN";
  codes[2112] = "ISO-2022-KR";
  codes[2336] = "EUC-JP";
  codes[2352] = "GB2312";
  codes[2353] = "x-euc-tw";
  codes[2368] = "EUC-KR";
  codes[2561] = "Shift_JIS";
  codes[2562] = "KOI8-R";
  codes[2563] = "Big5";
  codes[2565] = "HZ-GB-2312";
  
  if (codes[aCode])
    return codes[aCode];

  return getLocalizedPref("intl.charset.default", DEFAULT_QUERY_CHARSET);
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
  var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  LOG("bytesToString: converting using charset: " + aCharset);

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
  ENSURE(dataString, "sherlockBytesToLines: Couldn't convert byte array!",
         Cr.NS_ERROR_FAILURE);

  // Split the string into lines, and filter out comments and
  // whitespace-only lines
  return dataString.split(NEW_LINES).filter(isUsefulLine);
}

/**
 * Wrapper for nsIPrefBranch::getComplexValue.
 * @param aPrefName
 *        The name of the pref to get.
 * @returns aDefault if the requested pref doesn't exist.
 */
function getLocalizedPref(aPrefName, aDefault) {
  var prefB = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  const nsIPLS = Ci.nsIPrefLocalizedString;
  try {
    return prefB.getComplexValue(aPrefName, nsIPLS).data;
  } catch (ex) {}

  return aDefault;
}

/**
 * Wrapper for nsIPrefBranch::setComplexValue.
 * @param aPrefName
 *        The name of the pref to set.
 * @param aValue
 *        The value of the pref.
 */
function setLocalizedPref(aPrefName, aValue) {
  var prefB = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  var pls = Cc["@mozilla.org/pref-localizedstring;1"].
            createInstance(Ci.nsIPrefLocalizedString);
  pls.data = aValue;
  prefB.setComplexValue(aPrefName, Ci.nsIPrefLocalizedString, pls);
}

/**
 * Wrapper for nsIPrefBranch::getBoolPref.
 * @param aPrefName
 *        The name of the pref to get.
 * @returns aDefault if the requested pref doesn't exist.
 */
function getBoolPref(aName, aDefault) {
  var prefB = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  try {
    return prefB.getBoolPref(aName);
  } catch (ex) {
    return aDefault;
  }
}

/**
 * Get a unique nsIFile object with a sanitized name, based on the engine name.
 * @param aName
 *        A name to "sanitize". Can be an empty string, in which case a random
 *        8 character filename will be produced.
 * @returns A nsIFile object in the user's search engines directory with a
 *          unique sanitized name.
 */
function getSanitizedFile(aName) {
  var fileName = sanitizeName(aName) + "." + XML_FILE_EXT;
  var file = getDir(NS_APP_USER_SEARCH_DIR);
  file.append(fileName);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  return file;
}

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
  name = name.split("").filter(function (el) {
                                 return chars.indexOf(el) != -1;
                               }).join("");

  if (!name) {
    // Our input had no valid characters - use a random name
    var cl = chars.length - 1;
    for (var i = 0; i < 8; ++i)
      name += chars.charAt(Math.round(Math.random() * cl));
  }

  return name;
}

/**
 * Notifies watchers of SEARCH_ENGINE_TOPIC about changes to an engine or to
 * the state of the search service.
 *
 * @param aEngine
 *        The nsISearchEngine object to which the change applies.
 * @param aVerb
 *        A verb describing the change.
 *
 * @see nsIBrowserSearchService.idl
 */
function notifyAction(aEngine, aVerb) {
  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  LOG("NOTIFY: Engine: \"" + aEngine.name + "\"; Verb: \"" + aVerb + "\"");
  os.notifyObservers(aEngine, SEARCH_ENGINE_TOPIC, aVerb);
}

/**
 * Simple object representing a name/value pair.
 * @throws NS_ERROR_NOT_IMPLEMENTED if the provided value includes unsupported
 *         parameters.
 * @see kInvalidWords.
 */
function QueryParameter(aName, aValue) {
  ENSURE_ARG(aName && (aValue != null), "missing name or value for QueryParameter!");

  ENSURE(!kInvalidWords.test(aValue),
         "Illegal value while creating a QueryParameter",
         Cr.NS_ERROR_NOT_IMPLEMENTED);

  this.name = aName;
  this.value = aValue;
}

/**
 * Creates a mozStorage statement that can be used to access the database we
 * use to hold metadata.
 *
 * @param dbconn  the database that the statement applies to
 * @param sql     a string specifying the sql statement that should be created
 */
function createStatement (dbconn, sql) {
  var stmt = dbconn.createStatement(sql);
  var wrapper = new MozStorageStatementWrapper();

  wrapper.initialize(stmt);
  return wrapper;
}

/**
 * Creates an engineURL object, which holds the query URL and all parameters.
 *
 * @param aType
 *        A string containing the name of the MIME type of the search results
 *        returned by this URL.
 * @param aMethod
 *        The HTTP request method. Must be a case insensitive value of either
 *        "GET" or "POST".
 * @param aTemplate
 *        The URL to which search queries should be sent. For GET requests,
 *        must contain the string "{searchTerms}", to indicate where the user
 *        entered search terms should be inserted.
 *
 * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag
 *
 * @throws NS_ERROR_NOT_IMPLEMENTED if aType is unsupported.  If invalid
 *         (unsupported) parameters are included in aTemplate, they will be
 *         replaced with blanks in the final query, so no error needs to be
 *         returned here.
 */
function EngineURL(aType, aMethod, aTemplate) {
  ENSURE_ARG(aType && aMethod && aTemplate,
             "missing type, method or template for EngineURL!");

  var method = aMethod.toUpperCase();
  var type   = aType.toLowerCase();

  ENSURE_ARG(method == "GET" || method == "POST",
             "method passed to EngineURL must be \"GET\" or \"POST\"");

  this.type     = type;
  this.method   = method;
  this.params   = [];

  var templateURI = makeURI(aTemplate);
  ENSURE(templateURI, "new EngineURL: template is not a valid URI!",
         Cr.NS_ERROR_FAILURE);

  switch (templateURI.scheme) {
    case "http":
    case "https":
    // Disable these for now, see bug 295018
    // case "file":
    // case "resource":
      this.template = templateURI.spec;
      break;
    default:
      ENSURE(false, "new EngineURL: template uses invalid scheme!",
             Cr.NS_ERROR_FAILURE);
  }
}
EngineURL.prototype = {

  addParam: function SRCH_EURL_addParam(aName, aValue) {
    this.params.push(new QueryParameter(aName, aValue));
  },

  getSubmission: function SRCH_EURL_getSubmission(aData) {
    /**
     * From an array of QueryParameter objects, generates a string in the
     * application/x-www-form-urlencoded format:
     * name=value&name=value&name=value...
     * Any invalid or unimplemented query fields will be replaced with empty
     * strings.
     * @param   aParams
     *          An array of QueryParameter objects
     * @param   aData
     *          Data to be substituted into parameter values using the
     *          |kValidWords| regexp
     * @returns A string of encoded param names and values in
     *          application/x-www-form-urlencoded format.
     *
     * @see kInvalidWords
     */
    function makeQueryString(aParams, aData) {
      var str = "";
      for (var i = 0; i < aParams.length; ++i) {
        var param = aParams[i];
        var value = param.value.replace(kValidWords, aData);
        str += (i > 0 ? "&" : "") + param.name + "=" + value;
      }
      return str;
    }

    // Replace known fields with given parameters and clear unknown fields.
    var url = this.template.replace(kValidWords, aData);
    url = url.replace(kInvalidWords, "");
    var postData = null;
    var dataString = makeQueryString(this.params, aData);
    if (this.method == "GET") {
      // GET method requests have no post data, and append the encoded
      // text to the url...
      if (url.indexOf("?") == -1 && dataString)
        url += "?";
      url += dataString;
    } else if (this.method == "POST") {
      // POST method requests must wrap the encoded text in a MIME
      // stream and supply that as POSTDATA.
      var stringStream = Cc["@mozilla.org/io/string-input-stream;1"].
                         createInstance(Ci.nsIStringInputStream);
#ifdef MOZILLA_1_8_BRANCH
# bug 318193
      stringStream.setData(dataString, dataString.length);
#else
      stringStream.data = dataString;
#endif

      postData = Cc["@mozilla.org/network/mime-input-stream;1"].
                 createInstance(Ci.nsIMIMEInputStream);
      postData.addHeader("Content-Type", "application/x-www-form-urlencoded");
      postData.addContentLength = true;
      postData.setData(stringStream);
    }

    return new Submission(makeURI(url), postData);
  },

  /**
   * Serializes the engine object to a OpenSearch Url element.
   * @param aDoc
   *        The document to use to create the Url element.
   * @param aElement
   *        The element to which the created Url element is appended.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag
   */
  _serializeToElement: function SRCH_EURL_serializeToEl(aDoc, aElement) {
    var url = aDoc.createElementNS(OPENSEARCH_NS_11, "Url");
    url.setAttribute("type", this.type);
    url.setAttribute("method", this.method);
    url.setAttribute("template", this.template);

    for (var i = 0; i < this.params.length; ++i) {
      var param = aDoc.createElementNS(OPENSEARCH_NS_11, "Param");
      param.setAttribute("name", this.params[i].name);
      param.setAttribute("value", this.params[i].value);
      url.appendChild(aDoc.createTextNode("\n  "));
      url.appendChild(param);
    }
    url.appendChild(aDoc.createTextNode("\n"));
    aElement.appendChild(url);
  }
};

/**
 * nsISearchEngine constructor.
 * @param aLocation
 *        A nsILocalFile or nsIURI object representing the location of the
 *        search engine data file.
 * @param aSourceDataType
 *        The data type of the file used to describe the engine. Must be either
 *        DATA_XML or DATA_TEXT.
 * @param aIsReadOnly
 *        Boolean indicating whether the engine should be treated as read-only.
 *        Read only engines cannot be serialized to file.
 */
function Engine(aLocation, aSourceDataType, aIsReadOnly) {
  this._dataType = aSourceDataType;
  this._readOnly = aIsReadOnly;
  this._urls = [];

  if (aLocation instanceof Ci.nsILocalFile) {
    // we already have a file (e.g. loading engines from disk)
    this._file = aLocation;
  } else if (aLocation instanceof Ci.nsIURI) {
    this._uri = aLocation;
    switch (aLocation.scheme) {
      case "https":
      case "http":
      case "ftp":
      case "data":
      case "file":
      case "resource":
        this._uri = aLocation;
        break;
      default:
        ERROR("Invalid URI passed to the nsISearchEngine constructor",
              Cr.NS_ERROR_INVALID_ARG);
    }
  } else
    ERROR("Engine location is neither a File nor a URI object",
          Cr.NS_ERROR_INVALID_ARG);
}

Engine.prototype = {
  // The engine's alias.
  _alias: null,
  // The data describing the engine. Is either an array of bytes, for Sherlock
  // files, or an XML document element, for XML plugins.
  _data: null,
  // The engine's data type. See data types (DATA_) defined above.
  _dataType: null,
  // Whether or not the engine is readonly.
  _readOnly: true,
  // The engine's description
  _description: "",
  // The file from which the plugin was loaded.
  _file: null,
  // Set to true if the engine has a preferred icon (an icon that should not be
  // overridden by a non-preferred icon).
  _hasPreferredIcon: null,
  // Whether the engine is hidden from the user.
  _hidden: null,
  // The engine's name.
  _name: null,
  // The engine type. See engine types (TYPE_) defined above.
  _type: null,
  // The name of the charset used to submit the search terms.
  _queryCharset: null,
  // A URL string pointing to the engine's search form.
  _searchForm: null,
  // The URI object from which the engine was retrieved.
  // This is null for local plugins, and is used for error messages, logging,
  // and determining whether to start using a newly added engine right away.
  _uri: null,

  /**
   * Retrieves the data from the engine's file. If the engine's dataType is
   * XML, the document element is placed in the engine's data field. For text
   * engines, the data is just read directly from file and placed as an array
   * of lines in the engine's data field.
   */
  _initFromFile: function SRCH_ENG_initFromFile() {
    ENSURE(this._file && this._file.exists(),
           "File must exist before calling initFromFile!",
           Cr.NS_ERROR_UNEXPECTED);
  
    var fileInStream = Cc["@mozilla.org/network/file-input-stream;1"].
                       createInstance(Ci.nsIFileInputStream);

    fileInStream.init(this._file, MODE_RDONLY, PERMS_FILE, false);

    switch (this._dataType) {
      case SEARCH_DATA_XML:

        var domParser = Cc["@mozilla.org/xmlextras/domparser;1"].
                        createInstance(Ci.nsIDOMParser);
        var doc = domParser.parseFromStream(fileInStream, "UTF-8",
                                            this._file.fileSize,
                                            "text/xml");

        this._data = doc.documentElement;
        break;
      case SEARCH_DATA_TEXT:
        var binaryInStream = Cc["@mozilla.org/binaryinputstream;1"].
                             createInstance(Ci.nsIBinaryInputStream);
        binaryInStream.setInputStream(fileInStream);

        var bytes = binaryInStream.readByteArray(binaryInStream.available());
        this._data = bytes;

        break;
      default:
        ERROR("Bogus engine _dataType: \"" + this._dataType + "\"",
              Cr.NS_ERROR_UNEXPECTED);
    }
    fileInStream.close();

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Retrieves the engine data from a URI.
   * @param   aURI
   *          The URL to transfer from.
   */
  _initFromURI: function SRCH_ENG_initFromURI() {
    ENSURE_WARN(this._uri instanceof Ci.nsIURI,
                "Must have URI when calling _initFromURI!",
                Cr.NS_ERROR_UNEXPECTED);

    LOG("_initFromURI: Downloading engine from: \"" + this._uri.spec + "\".");

    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    var chan = ios.newChannelFromURI(this._uri);

    var listener = new loadListener(chan, this, this._onLoad);
    chan.notificationCallbacks = listener;
    chan.asyncOpen(listener, null);
  },

  /**
   * Attempts to find an EngineURL object in the set of EngineURLs for
   * this Engine that has the given type string.  (This corresponds to the
   * "type" attribute in the "Url" node in the OpenSearch spec.)
   * This method will return the first matching URL object found, or null
   * if no matching URL is found.
   *
   * @param aType string to match the EngineURL's type attribute
   */
  _getURLOfType: function SRCH_ENG__getURLOfType(aType) {
    for (var i = 0; i < this._urls.length; ++i) {
      if (this._urls[i].type == aType)
        return this._urls[i];
    }

    return null;
  },

  /**
   * Handle the successful download of an engine. Initializes the engine and
   * triggers parsing of the data. The engine is then flushed to disk. Notifies
   * the search service once initialization is complete.
   */
  _onLoad: function SRCH_ENG_onLoad(aBytes, aEngine) {
    /**
     * Handle an error during the load of an engine by prompting the user to
     * notify him that the load failed.
     */
    function onError() {
      var sbs = Cc["@mozilla.org/intl/stringbundle;1"].
                getService(Ci.nsIStringBundleService);
      var searchBundle = sbs.createBundle(SEARCH_BUNDLE);
      var brandBundle = sbs.createBundle(BRAND_BUNDLE);
      var brandName = brandBundle.GetStringFromName("brandShortName");
      var title = searchBundle.GetStringFromName("error_loading_engine_title");
      var text = searchBundle.formatStringFromName("error_loading_engine_msg",
                                                   [brandName, aEngine._location],
                                                   2);

      var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
               getService(Ci.nsIWindowWatcher);
      ww.getNewPrompter(null).alert(title, text);
    }

    if (!aBytes) {
      onError();
      return;
    }

    switch (aEngine._dataType) {
      case SEARCH_DATA_XML:
        var dataString = bytesToString(aBytes, "UTF-8");
        ENSURE(dataString, "_onLoad: Couldn't convert byte array!",
               Cr.NS_ERROR_FAILURE);
        var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                     createInstance(Ci.nsIDOMParser);
        var doc = parser.parseFromString(dataString, "text/xml");
        aEngine._data = doc.documentElement;
        break;
      case SEARCH_DATA_TEXT:
        aEngine._data = aBytes;
        break;
      default:
        onError();
        LOG("_onLoad: Bogus engine _dataType: \"" + this._dataType + "\"");
        return;
    }
    try {
      // Initialize the engine from the obtained data
      aEngine._initFromData();
    } catch (ex) {
      LOG("_onLoad: Failed to init engine!\n" + ex);
      // Report an error to the user
      onError();
      return;
    }

    // Write the engine to file
    aEngine._serializeToFile();

    // Notify the search service of the sucessful load
    notifyAction(aEngine, SEARCH_ENGINE_LOADED);
  },

  /**
   * Sets the .iconURI property of the engine.
   *
   *  @param aIconURL
   *         A URI string pointing to the engine's icon. Must have a http[s],
   *         ftp, or data scheme. Icons with HTTP[S] or FTP schemes will be
   *         downloaded and converted to data URIs for storage in the engine
   *         XML files, if the engine is not readonly.
   *  @param aIsPreferred
   *         Whether or not this icon is to be preferred. Preferred icons can
   *         override non-preferred icons.
   */
  _setIcon: function SRCH_ENG_setIcon(aIconURL, aIsPreferred) {
    // If we already have a preferred icon, and this isn't a preferred icon,
    // just ignore it.
    if (this._hasPreferredIcon && !aIsPreferred)
      return;

    var uri = makeURI(aIconURL);

    // Ignore bad URIs
    if (!uri)
      return;

    LOG("_setIcon: Setting icon url \"" + uri.spec + "\" for engine \""
        + this.name + "\".");
    // Only accept remote icons from http[s] or ftp
    switch (uri.scheme) {
      case "data":
        this._iconURI = uri;
        notifyAction(this, SEARCH_ENGINE_CHANGED);
        this._hasPreferredIcon = aIsPreferred;
        break;
      case "http":
      case "https":
      case "ftp":
        // No use downloading the icon if the engine file is read-only
        // XXX could store the data: URI in a pref... ew?
        if (!this._readOnly) {
          LOG("_setIcon: Downloading icon: \"" + uri.spec +
              "\" for engine: \"" + this.name + "\"");
          var ios = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
          var chan = ios.newChannelFromURI(uri);

          function iconLoadCallback(aByteArray, aEngine) {
            // This callback may run after we've already set a preferred icon,
            // so check again.
            if (aEngine._hasPreferredIcon && !aIsPreferred)
              return;

            if (!aByteArray || aByteArray.length > MAX_ICON_SIZE) {
              LOG("iconLoadCallback: engine too large!");
              return;
            }

            var str = b64(aByteArray);
            aEngine._iconURI = makeURI(ICON_DATAURL_PREFIX + str);

            // The engine might not have a file yet, if it's being downloaded,
            // because the request for the engine file itself (_onLoad) may not
            // yet be complete. In that case, this change will be written to
            // file when _onLoad is called.
            if (aEngine._file)
              aEngine._serializeToFile();

            notifyAction(aEngine, SEARCH_ENGINE_CHANGED);
            aEngine._hasPreferredIcon = aIsPreferred;
          }

          var listener = new loadListener(chan, this, iconLoadCallback);
          chan.notificationCallbacks = listener;
          chan.asyncOpen(listener, null);
        }
        break;
    }
  },

  /**
   * Initialize this Engine object from the collected data.
   */
  _initFromData: function SRCH_ENG_initFromData() {

    ENSURE_WARN(this._data, "Can't init an engine with no data!",
                Cr.NS_ERROR_UNEXPECTED);

    // Find out what type of engine we are
    switch (this._dataType) {
      case SEARCH_DATA_XML:
        if (checkNameSpace(this._data, [MOZSEARCH_LOCALNAME],
            [MOZSEARCH_NS_10])) {

          LOG("_init: Initing MozSearch plugin from " + this._location);

          this._type = SEARCH_TYPE_MOZSEARCH;
          this._parseAsMozSearch();

        } else if (checkNameSpace(this._data, [OPENSEARCH_LOCALNAME],
                                  OPENSEARCH_NAMESPACES)) {

          LOG("_init: Initing OpenSearch plugin from " + this._location);

          this._type = SEARCH_TYPE_OPENSEARCH;
          this._parseAsOpenSearch();

        } else
          ENSURE(false, this._location + " is not a valid search plugin.",
                 Cr.NS_ERROR_FAILURE);

        break;
      case SEARCH_DATA_TEXT:
        LOG("_init: Initing Sherlock plugin from " + this._location);

        // the only text-based format we support is Sherlock
        this._type = SEARCH_TYPE_SHERLOCK;
        this._parseAsSherlock();
    }

    // If we don't yet have a file (i.e. we instantiated an engine object from
    // passed in URL), get one now
    if (!this._file)
      this._file = getSanitizedFile(this.name);
  },

  /**
   * Initialize this Engine object from a collection of metadata.
   */
  _initFromMetadata: function SRCH_ENG_initMetaData(aName, aIconURL, aAlias,
                                                    aDescription, aMethod,
                                                    aTemplate) {
    ENSURE_WARN(!this._readOnly,
                "Can't call _initFromMetaData on a readonly engine!",
                Cr.NS_ERROR_FAILURE);

    this._urls.push(new EngineURL("text/html", aMethod, aTemplate));

    this._name = aName;
    this._alias = aAlias;
    this._description = aDescription;
    this._setIcon(aIconURL, true);

    this._serializeToFile();
  },

  /**
   * Extracts data from an OpenSearch URL element and creates an EngineURL
   * object which is then added to the engine's list of URLs.
   *
   * @throws NS_ERROR_FAILURE if a URL object could not be created.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag.
   * @see EngineURL()
   */
  _parseURL: function SRCH_ENG_parseURL(aElement) {
    var type     = aElement.getAttribute("type");
    // According to the spec, method is optional, defaulting to "GET" if not
    // specified
    var method   = aElement.getAttribute("method") || "GET";
    var template = aElement.getAttribute("template");

    try {
      var url = new EngineURL(type, method, template);
    } catch (ex) {
      LOG("_parseURL: failed to add " + template + " as a URL");
      throw Cr.NS_ERROR_FAILURE;
    }

    for (var i = 0; i < aElement.childNodes.length; ++i) {
      var param = aElement.childNodes[i];
      if (param.localName == "Param") {
        try {
          url.addParam(param.getAttribute("name"), param.getAttribute("value"));
        } catch (ex) {
          // Ignore failure
          LOG("_parseURL: Url element has an invalid param");
        }
      }
    }

    this._urls.push(url);
  },

  /**
   * Get the icon from an OpenSearch Image element.
   * @see http://opensearch.a9.com/spec/1.1/description/#image
   */
  _parseImage: function SRCH_ENG_parseImage(aElement) {
    LOG("_parseImage: Image textContent: \"" + aElement.textContent + "\"");
    if (aElement.getAttribute("width")  == "16" &&
        aElement.getAttribute("height") == "16") {
      this._setIcon(aElement.textContent, true);
    }
  },

  _parseAsMozSearch: function SRCH_ENG_parseAsMoz() {
    //forward to the OpenSearch parser
    this._parseAsOpenSearch();
  },

  /**
   * Extract search engine information from the collected data to initialize
   * the engine object.
   */
  _parseAsOpenSearch: function SRCH_ENG_parseAsOS() {
    var doc = this._data;

    for (var i = 0; i < doc.childNodes.length; ++i) {
      var child = doc.childNodes[i];
      switch (child.localName) {
        case "ShortName":
          this._name = child.textContent;
          break;
        case "Description":
          this._description = child.textContent;
          break;
        case "Url":
          try {
            this._parseURL(child);
          } catch (ex) {
            // Parsing of the element failed, just skip it.
          }
          break;
        case "Image":
          this._parseImage(child);
          break;
        case "InputEncoding":
          this._queryCharset = child.textContent.toUpperCase();
          break;
        // Non-OpenSearch elements
        case "Alias":
          this._alias = child.textContent;
          break;
        case "SearchForm":
          this._searchForm = child.textContent;
          break;
      }
    }
    ENSURE(this.name && (this._urls.length > 0),
           "_parseAsOpenSearch: No name, or missing URL!",
           Cr.NS_ERROR_FAILURE);
  },

  /**
   * Extract search engine information from the collected data to initialize
   * the engine object.
   */
  _parseAsSherlock: function SRCH_ENG_parseAsSherlock() {
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
      LOG("_parseAsSherlock::getSection: Sherlock lines:\n" +
          aLines.join("\n"));
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
      LOG("_parseAsSherlock::getSection: Start index: " + startLine +
          "\nNumber of lines: " + numberOfLines);
      lines = lines.splice(startLine, numberOfLines);
      LOG("_parseAsSherlock::getSection: Section lines:\n" +
          lines.join("\n"));

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

    /**
     * Returns an array of name-value pair arrays representing the Sherlock
     * file's input elements. User defined inputs return kUserDefined as the
     * value. Elements are returned in the order they appear in the source
     * file.
     *
     *   Example:
     *      <input name="foo" value="bar">
     *      <input name="foopy" user>
     *   Returns:
     *      [["foo", "bar"], ["foopy", "{searchTerms}"]]
     *
     * @param aLines
     *        An array of lines from the source file.
     */
    function getInputs(aLines) {

      /**
       * Extracts an attribute value from a given a line of text.
       *    Example: <input value="foo" name="bar">
       *      Extracts the string |foo| or |bar| given an input aAttr of
       *      |value| or |name|.
       * Attributes may be quoted or unquoted. If unquoted, any whitespace
       * indicates the end of the attribute value.
       *    Example: < value=22 33 name=44\334 >
       *      Returns |22| for "value" and |44\334| for "name".
       *
       * @param aAttr
       *        The name of the attribute for which to obtain the value. This
       *        value is not case sensitive.
       * @param aLine
       *        The line containing the attribute.
       *
       * @returns the attribute value, or an empty string if the attribute
       *          doesn't exist.
       */
      function getAttr(aAttr, aLine) {
        LOG("_parseAsSherlock::getAttr: Getting attr: \"" +
            aAttr + "\" for line: \"" + aLine + "\"");
        // We're not case sensitive, but we want to return the attribute value
        // in its original case, so create a copy of the source
        var lLine = aLine.toLowerCase();
        var attr = aAttr.toLowerCase();

        var attrStart = lLine.search(new RegExp("\\s" + attr, "i"));
        if (attrStart == -1) {
          // If this is the "user defined input" (i.e. contains the empty
          // "user" attribute), return our special keyword
          if (kIsUserInput.test(lLine) && attr == "value") {
            LOG("_parseAsSherlock::getAttr: Found user input!\nLine:\"" + lLine
                + "\"");
            return kUserDefined;
          }
          // The attribute doesn't exist - ignore
          LOG("_parseAsSherlock::getAttr: Failed to find attribute:\nLine:\""
              + lLine + "\"\nAttr:\"" + attr + "\"");
          return "";
        }

        var valueStart = lLine.indexOf("=", attrStart) + "=".length;
        if (valueStart == -1)
          return "";

        var quoteStart = lLine.indexOf("\"", valueStart);
        if (quoteStart == -1) {

          // Unquoted attribute, get the rest of the line, trimmed at the first
          // sign of whitespace. If the rest of the line is only whitespace,
          // returns a blank string.
          return lLine.substr(valueStart).replace(/\s.*$/, "");

        } else {
          // Make sure that there's only whitespace between the start of the
          // value and the first quote. If there is, end the attribute value at
          // the first sign of whitespace. This prevents us from falling into
          // the next attribute if this is an unquoted attribute followed by a
          // quoted attribute.
          var betweenEqualAndQuote = lLine.substring(valueStart, quoteStart);
          if (/\S/.test(betweenEqualAndQuote))
            return lLine.substr(valueStart).replace(/\s.*$/, "");

          // Adjust the start index to account for the opening quote
          valueStart = quoteStart + "\"".length;
          // Find the closing quote
          valueEnd = lLine.indexOf("\"", valueStart);
          // If there is no closing quote, just go to the end of the line
          if (valueEnd == -1)
            valueEnd = aLine.length;
        }
        return aLine.substring(valueStart, valueEnd);
      }

      var inputs = [];

      LOG("_parseAsSherlock::getInputs: Lines:\n" + aLines);
      // Filter out everything but non-inputs
      lines = aLines.filter(function (line) {
        return /^\s*<input/i.test(line);
      });
      LOG("_parseAsSherlock::getInputs: Filtered lines:\n" + lines);

      lines.forEach(function (line) {
        // Strip leading/trailing whitespace and remove the surrounding markup
        // ("<input" and ">")
        line = sTrim(line).replace(/^<input/i, "").replace(/>$/, "");

        // If this is one of the "directional" inputs (<inputnext>/<inputprev>)
        const directionalInput = /^(prev|next)/i;
        if (directionalInput.test(line)) {

          // Make it look like a normal input by removing "prev" or "next"
          line = line.replace(directionalInput, "");

          // If it has a name, give it a dummy value to match previous
          // nsInternetSearchService behavior
          if (/name\s*=/i.test(line)) {
            line += " value=\"0\"";
          } else
            return; // Line has no name, skip it
        }

        var attrName = getAttr("name", line);
        var attrValue = getAttr("value", line);
        LOG("_parseAsSherlock::getInputs: Got input:\nName:\"" + attrName +
            "\"\nValue:\"" + attrValue + "\"");
        if (attrValue)
          inputs.push([attrName, attrValue]);
      });
      return inputs;
    }

    function err(aErr) {
      LOG("_parseAsSherlock::err: Sherlock param error:\n" + aErr);
      throw Cr.NS_ERROR_FAILURE;
    }

    // First try converting our byte array using the default Sherlock encoding.
    // If this fails, or if we find a sourceTextEncoding attribute, we need to
    // reconvert the byte array using the specified encoding.
    var sherlockLines, searchSection, sourceTextEncoding;
    try {
      sherlockLines = sherlockBytesToLines(this._data);
      searchSection = getSection(sherlockLines, "search");
      sourceTextEncoding = parseInt(searchSection["sourcetextencoding"]);
      if (sourceTextEncoding) {
        // Re-convert the bytes using the found sourceTextEncoding
        sherlockLines = sherlockBytesToLines(this._data, sourceTextEncoding);
        searchSection = getSection(sherlockLines, "search");
      }
    } catch (ex) {
      // The conversion using the default charset failed. Remove any non-ascii
      // bytes and try to find a sourceTextEncoding.
      var asciiBytes = this._data.filter(function (n) {return !(0x80 & n);});
      var asciiString = String.fromCharCode.apply(null, asciiBytes);
      sherlockLines = asciiString.split(NEW_LINES).filter(isUsefulLine);
      searchSection = getSection(sherlockLines, "search");
      sourceTextEncoding = parseInt(searchSection["sourcetextencoding"]);
      if (sourceTextEncoding) {
        sherlockLines = sherlockBytesToLines(this._data, sourceTextEncoding);
        searchSection = getSection(sherlockLines, "search");
      } else
        ERROR("Couldn't find a working charset", Cr.NS_ERROR_FAILURE);
    }

    LOG("_parseAsSherlock: Search section:\n" + searchSection.toSource());

    this._name = searchSection["name"] || err("Missing name!");
    this._description = searchSection["description"] || "";
    this._queryCharset = searchSection["querycharset"] ||
                         queryCharsetFromCode(searchSection["queryencoding"]);

    // XXX should this really fall back to GET?
    var method = (searchSection["method"] || "GET").toUpperCase();
    var template = searchSection["action"] || err("Missing action!");

    var inputs = getInputs(sherlockLines);
    LOG("_parseAsSherlock: Inputs:\n" + inputs.toSource());

    var url = null;

    if (method == "GET") {
      // Here's how we construct the input string:
      // <input> is first:  Name Attr:  Prefix      Data           Example:
      // YES                EMPTY       None        <value>        TEMPLATE<value>
      // YES                NON-EMPTY   ?           <name>=<value> TEMPLATE?<name>=<value>
      // NO                 EMPTY       ------------- <ignored> --------------
      // NO                 NON-EMPTY   &           <name>=<value> TEMPLATE?<n1>=<v1>&<n2>=<v2>
      for (var i = 0; i < inputs.length; i++) {
        var name  = inputs[i][0];
        var value = inputs[i][1];
        if (i==0) {
          if (name == "")
            template += kUserDefined;
          else
            template += "?" + name + "=" + value;
        } else if (name != "")
          template += "&" + name + "=" + value;
      }
      url = new EngineURL("text/html", method, template);

    } else if (method == "POST") {
      // Create the URL object and just add the parameters directly
      url = new EngineURL("text/html", method, template);
      for (var i = 0; i < inputs.length; i++) {
        var name  = inputs[i][0];
        var value = inputs[i][1];
        if (name)
          url.addParam(name, value);
      }
    } else
      err("Invalid method!");

    this._urls.push(url);
  },

  /**
   * Returns an XML document object containing the search plugin information,
   * which can later be used to reload the engine.
   */
  _serializeToElement: function SRCH_ENG_serializeToEl() {
    var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);

    var doc = parser.parseFromString(EMPTY_DOC, "text/xml");
    docElem = doc.documentElement;

    docElem.appendChild(doc.createTextNode("\n"));

    var shortName = doc.createElementNS(OPENSEARCH_NS_11, "ShortName");
    shortName.appendChild(doc.createTextNode(this.name));
    docElem.appendChild(shortName);
    docElem.appendChild(doc.createTextNode("\n"));

    var description = doc.createElementNS(OPENSEARCH_NS_11, "Description");
    description.appendChild(doc.createTextNode(this._description));
    docElem.appendChild(description);
    docElem.appendChild(doc.createTextNode("\n"));

    var inputEncoding = doc.createElementNS(OPENSEARCH_NS_11, "InputEncoding");
    inputEncoding.appendChild(doc.createTextNode(this._queryCharset));
    docElem.appendChild(inputEncoding);
    docElem.appendChild(doc.createTextNode("\n"));

    if (this._iconURI) {
      var image = doc.createElementNS(OPENSEARCH_NS_11, "Image");
      image.appendChild(doc.createTextNode(this._iconURL));
      image.setAttribute("width", "16");
      image.setAttribute("height", "16");
      docElem.appendChild(image);
      docElem.appendChild(doc.createTextNode("\n"));
    }

    if (this._alias) {
      var alias = doc.createElementNS(MOZSEARCH_NS_10, "Alias");
      alias.appendChild(doc.createTextNode(this.alias));
      docElem.appendChild(alias);
      docElem.appendChild(doc.createTextNode("\n"));
    }

    for (var i = 0; i < this._urls.length; ++i)
      this._urls[i]._serializeToElement(doc, docElem);
    docElem.appendChild(doc.createTextNode("\n"));

    return doc;
  },

  /**
   * Serializes the engine object to file.
   */
  _serializeToFile: function SRCH_ENG_serializeToFile() {
    var file = this._file;
    ENSURE_WARN(!this._readOnly, "Can't serialize a read only engine!",
                Cr.NS_ERROR_FAILURE);
    ENSURE_WARN(file.exists(), "Can't serialize: file doesn't exist!",
                Cr.NS_ERROR_UNEXPECTED);

    var fos = Cc["@mozilla.org/network/safe-file-output-stream;1"].
              createInstance(Ci.nsIFileOutputStream);

    // Serialize the engine first - we don't want to overwrite a good file
    // if this somehow fails.
    doc = this._serializeToElement();

    fos.init(file, (MODE_WRONLY | MODE_TRUNCATE), PERMS_FILE, 0);

    try {
      var serializer = Cc["@mozilla.org/xmlextras/xmlserializer;1"].
                       createInstance(Ci.nsIDOMSerializer);
      serializer.serializeToStream(doc.documentElement, fos, null);
    } catch (e) {
      LOG("_serializeToFile: Error serializing engine:\n" + e);
    }

    closeSafeOutputStream(fos);
  },

  /**
   * Remove the engine's file from disk. The search service calls this once it
   * removes the engine from its internal store. This function will throw if
   * the file cannot be removed.
   */
  _remove: function SRCH_ENG_remove() {
    ENSURE(!this._readOnly, "Can't remove read only engine!",
           Cr.NS_ERROR_FAILURE);
    ENSURE(this._file && this._file.exists(),
           "Can't remove engine: file doesn't exist!",
           Cr.NS_ERROR_FILE_NOT_FOUND);

    this._file.remove(false);
  },

  // nsISearchEngine
  get alias() {
    if (this._alias === null)
      this._alias = engineMetadataService.getAttr(this, "alias");

    return this._alias;
  },
  set alias(val) {
    this._alias = val;
    engineMetadataService.setAttr(this, "alias", val);
    notifyAction(this, SEARCH_ENGINE_CHANGED);
  },

  get description() {
    return this._description;
  },

  get hidden() {
    if (this._hidden === null)
      this._hidden = engineMetadataService.getAttr(this, "hidden");
    return this._hidden;
  },
  set hidden(val) {
    var value = !!val;
    if (value != this._hidden) {
      this._hidden = value;
      engineMetadataService.setAttr(this, "hidden", value);
      notifyAction(this, SEARCH_ENGINE_CHANGED);
    }
  },

  get iconURI() {
    return this._iconURI;
  },

  get _iconURL() {
    if (!this._iconURI)
      return "";
    return this._iconURI.spec;
  },

  // Where the engine is being loaded from: will return the URI's spec if the
  // engine is being downloaded and does not yet have a file. This is only used
  // for logging.
  get _location() {
    if (this._file)
      return this._file.path;

    if (this._uri)
      return this._uri.spec;

    return "";
  },

  // The file that the plugin is loaded from is a unique identifier for it.  We
  // use this as the identifier to store data in the sqlite database
  get _id() {
    ENSURE_WARN(this._file, "No _file for id!", Cr.NS_ERROR_FAILURE);
    return this._file.path;
  },

  get name() {
    return this._name;
  },

  get type() {
    return this._type;
  },

  // This getter is used in SearchService.observer.  It is not intended to be
  // used (or needed) by callers outside this file.
  get uri() {
    return this._uri;
  },

  get searchForm() {
    if (!this._searchForm) {
      // No searchForm specified in the engine definition file, use the prePath
      // (e.g. https://foo.com for https://foo.com/search.php?q=bar).
      var htmlUrl = this._getURLOfType(URLTYPE_SEARCH_HTML);
      ENSURE_WARN(htmlUrl, "Engine has no HTML URL!", Cr.NS_ERROR_UNEXPECTED);
      this._searchForm = makeURI(htmlUrl.template).prePath;
    }

    return this._searchForm;
  },

  get queryCharset() {
    if (this._queryCharset)
      return this._queryCharset;
    return this._queryCharset = queryCharsetFromCode(/* get the default */);
  },

  // from nsISearchEngine
  addParam: function SRCH_ENG_addParam(aName, aValue, aResponseType) {
    ENSURE_ARG(aName && (aValue != null),
               "missing name or value for nsISearchEngine::addParam!");
    ENSURE_WARN(!this._readOnly,
                "called nsISearchEngine::addParam on a read-only engine!",
                Cr.NS_ERROR_FAILURE);
    if (!aResponseType)
      aResponseType = URLTYPE_SEARCH_HTML;

    var url = this._getURLOfType(aResponseType);

    ENSURE(url, "Engine object has no URL for response type " + aResponseType,
           Cr.NS_ERROR_FAILURE);

    url.addParam(aName, aValue);
  },

  // from nsISearchEngine
  getSubmission: function SRCH_ENG_getSubmission(aData, aResponseType) {
    if (!aResponseType)
      aResponseType = URLTYPE_SEARCH_HTML;

    var url = this._getURLOfType(aResponseType);

    if (!url)
      return null;

    if (!aData) {
      // Return a dummy submission object with our searchForm attribute
      return new Submission(makeURI(this.searchForm), null);
    }

    LOG("getSubmission: In data: \"" + aData + "\"");
    var textToSubURI = Cc["@mozilla.org/intl/texttosuburi;1"].
                       getService(Ci.nsITextToSubURI);
    var data = "";
    try {
      data = textToSubURI.ConvertAndEscape(this.queryCharset, aData);
    } catch (ex) {
      LOG("getSubmission: Falling back to default queryCharset!");
      data = textToSubURI.ConvertAndEscape(DEFAULT_QUERY_CHARSET, aData);
    }
    LOG("getSubmission: Out data: \"" + data + "\"");
    return url.getSubmission(data);
  },

  // from nsISearchEngine
  supportsResponseType: function SRCH_ENG_supportsResponseType(type) {
    return (this._getURLOfType(type) != null);
  },

  // nsISupports
  QueryInterface: function SRCH_ENG_QI(aIID) {
    if (aIID.equals(Ci.nsISearchEngine) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  get wrappedJSObject() {
    return this;
  }

};

// nsISearchSubmission
function Submission(aURI, aPostData) {
  this._uri = aURI;
  this._postData = aPostData;
}
Submission.prototype = {
  get uri() {
    return this._uri;
  },
  get postData() {
    return this._postData;
  },
  QueryInterface: function SRCH_SUBM_QI(aIID) {
    if (aIID.equals(Ci.nsISearchSubmission) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

// nsIBrowserSearchService
function SearchService() {
  this._init();
}
SearchService.prototype = {
  _engines: { },
  _sortedEngines: [],

  // If this is set to the URI of the description of a search engine being added
  // to the list (typically by calling addEngine()), that engine will be
  // selected as the current engine when it finishes loading.  If another
  // engine is added with "start using this one now" before the first selected
  // engine finishes loading, the second choice will override the first one.
  // If the selected engine fails to load, this marker will be cleared.
  _selectNewEngineURI: null,

  _init: function() {
    engineMetadataService.init();
    this._addObservers();

    var fileLocator = Cc["@mozilla.org/file/directory_service;1"].
                      getService(Ci.nsIProperties);
    var locations = fileLocator.get(NS_APP_SEARCH_DIR_LIST,
                                    Ci.nsISimpleEnumerator);

    while (locations.hasMoreElements()) {
      var location = locations.getNext().QueryInterface(Ci.nsIFile);
      this._loadEngines(location);
    }

    this._convertOldPrefs();

    // Now that all engines are loaded, build the sorted engine list
    this._buildSortedEngineList();

    selectedEngineName = getLocalizedPref(BROWSER_SEARCH_PREF +
                                          "selectedEngine");
    this._currentEngine = this.getEngineByName(selectedEngineName) ||
                          this.defaultEngine;
  },

  _addEngineToStore: function SRCH_SVC_addEngineToStore(aEngine) {
    LOG("_addEngineToStore: Adding engine: \"" + aEngine.name + "\"");
    // XXX Prefer XML files?
    if (aEngine.name in this._engines) {
      LOG("_addEngineToStore: Duplicate engine found, aborting!");
      return;
      // XXX handle duplicates better?
      // might want to prompt the user in the case where the engine is being
      // added through a user action
    }

    if (!aEngine.supportsResponseType(URLTYPE_SEARCH_HTML)) {
      LOG("_addEngineToStore: Won't add engines that have no HTML output!");
      return;
    }

    this._engines[aEngine.name] = aEngine;
    this._sortedEngines.push(aEngine);
    notifyAction(aEngine, SEARCH_ENGINE_ADDED);
  },

  _loadEngines: function SRCH_SVC_loadEngines(aDir) {
    LOG("_loadEngines: Searching in " + aDir.path + " for search engines.");

    // Check whether aDir is the user profile dir
    var isInProfile = aDir.equals(getDir(NS_APP_USER_SEARCH_DIR));

    var files = aDir.directoryEntries
                    .QueryInterface(Ci.nsIDirectoryEnumerator);
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);

    while (files.hasMoreElements()) {
      var file = files.nextFile;

      // Ignore hidden and empty files, and directories
      if (!file.isFile() || file.fileSize == 0 || file.isHidden())
        continue;

      var fileURL = ios.newFileURI(file).QueryInterface(Ci.nsIURL);
      var fileExtension = fileURL.fileExtension.toLowerCase();
      var isWritable = isInProfile && file.isWritable();

      var dataType;
      switch (fileExtension) {
        case XML_FILE_EXT:
          dataType = SEARCH_DATA_XML;
          break;
        case SHERLOCK_FILE_EXT:
          dataType = SEARCH_DATA_TEXT;
          break;
        default:
          // Not an engine
          continue;
      }

      var addedEngine = null;
      try {
        addedEngine = new Engine(file, dataType, !isWritable);
        addedEngine._initFromFile();
      } catch (ex) {
        LOG("_loadEngines: Failed to load " + file.path + "!\n" + ex);
        continue;
      }

      if (fileExtension == SHERLOCK_FILE_EXT) {
        if (isWritable) {
          try {
            this._convertSherlockFile(addedEngine, fileURL.fileBaseName);
          } catch (ex) {
            LOG("_loadEngines: Failed to convert: " + fileURL.path + "\n" + ex);
            // The engine couldn't be converted, mark it as read-only
            addedEngine._readOnly = true;
          }
        }

        // If the engine still doesn't have an icon, see if we can find one
        if (!addedEngine._iconURI) {
          var icon = this._findSherlockIcon(file, fileURL.fileBaseName);
          if (icon)
            addedEngine._iconURI = ios.newFileURI(icon);
        }
      }

      this._addEngineToStore(addedEngine);
    }
  },

  _saveSortedEngineList: function SRCH_SVC_saveSortedEngineList() {
    var engines = this._getSortedEngines(false);
    for (var i = 0; i < engines.length; ++i)
      engineMetadataService.setAttr(engines[i], "order", i+1);
  },

  _buildSortedEngineList: function SRCH_SVC_buildSortedEngineList() {
    var addedEngines = { };
    this._sortedEngines = new Array(this._engines.length);
    var engine;

    for each (engine in this._engines) {
      var orderNumber = engineMetadataService.getAttr(engine, "order");
      if (orderNumber) {
        this._sortedEngines[orderNumber-1] = engine;
        addedEngines[engine.name] = engine;
      }
    }

    // Array for the remaining engines, alphabetically sorted
    var alphaEngines = [];

    for each (engine in this._engines) {
      if (!(engine.name in addedEngines))
        alphaEngines.push(this._engines[engine.name]);
    }
    alphaEngines = alphaEngines.sort(function (a, b) {
                                       if (a.name < b.name)
                                         return -1;
                                       if (a.name > b.name)
                                         return 1;
                                       return 0;
                                     });
    this._sortedEngines = this._sortedEngines.concat(alphaEngines);
  },

  /**
   *  On first startup, there are some default prefs that we need to migrate 
   *  into our sqlite database.  This function moves those values, along with
   *  any values users may have set from builds prior to the introduction of
   *  the database.
   */
  _convertOldPrefs: function SRCH_SVC_convertOrder() {
    if (!engineMetadataService.newTable)
      return;
    var i = 0;
    var engineName, engine;
    while (true) {
      engineName = getLocalizedPref(BROWSER_SEARCH_PREF + "order." + (++i));
      if (!engineName)
        break;

      engine = this._engines[engineName];
      if (!engine)
        continue;
      engineMetadataService.setAttr(engine, "order", i);
    }

    var prefService = Cc["@mozilla.org/preferences-service;1"].
                        getService(Ci.nsIPrefService);
    for each (engine in this._engines) {
      var basePref = BROWSER_SEARCH_PREF + "engine." + 
                     encodeURIComponent(engine.name) + ".";

      var alias = getLocalizedPref(basePref + "alias", "");
      if (alias != "")
        engineMetadataService.setAttr(engine, "alias", alias);

      var hidden = getBoolPref(basePref + "hidden", false);
      engineMetadataService.setAttr(engine, "hidden", hidden);
      var engineBranch = prefService.getBranch(basePref);
      engineBranch.deleteBranch("");
    }
},

  /**
   * Converts a Sherlock file and its icon into the custom XML format used by
   * the Search Service. Saves the engine's icon (if present) into the XML as a
   * data: URI and changes the extension of the source file from ".src" to
   * ".xml". The engine data is then written to the file as XML.
   * @param aEngine
   *        The Engine object that needs to be converted.
   * @param aBaseName
   *        The basename of the Sherlock file.
   *          Example: "foo" for file "foo.src".
   *
   * @throws NS_ERROR_FAILURE if the file could not be converted.
   *
   * @see nsIURL::fileBaseName
   */
  _convertSherlockFile: function SRCH_SVC_convertSherlock(aEngine, aBaseName) {
    var oldSherlockFile = aEngine._file;

    // Back up the old file
    try {
      var backupDir = oldSherlockFile.parent;
      backupDir.append("searchplugins-backup");

      if (!backupDir.exists())
        backupDir.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);

      oldSherlockFile.copyTo(backupDir, null);
    } catch (ex) {
      // Just bail. Engines that can't be backed up won't be converted, but
      // engines that aren't converted are loaded as readonly.
      LOG("_convertSherlockFile: Couldn't back up " + oldSherlockFile.path +
          ":\n" + ex);
      throw Cr.NS_ERROR_FAILURE;
    }

    // Rename the file, but don't clobber existing files
    var newXMLFile = oldSherlockFile.parent.clone();
    newXMLFile.append(aBaseName + "." + XML_FILE_EXT);

    if (newXMLFile.exists()) {
      // There is an existing file with this name, create a unique file
      newXMLFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
    }

    // Rename the .src file to .xml
    oldSherlockFile.moveTo(null, newXMLFile.leafName);

    aEngine._file = newXMLFile;

    // Write the converted engine to disk
    aEngine._serializeToFile();

    // Update the engine's _type.
    aEngine._type = SEARCH_TYPE_MOZSEARCH;

    // See if it has a corresponding icon
    try {
      var icon = this._findSherlockIcon(aEngine._file, aBaseName);
      if (icon && icon.fileSize < MAX_ICON_SIZE) {
        // Use this as the engine's icon
        var bStream = Cc["@mozilla.org/binaryinputstream;1"].
                        createInstance(Ci.nsIBinaryInputStream);
        var fileInStream = Cc["@mozilla.org/network/file-input-stream;1"].
                           createInstance(Ci.nsIFileInputStream);

        fileInStream.init(icon, MODE_RDONLY, PERMS_FILE, 0);
        bStream.setInputStream(fileInStream);

        var bytes = [];
        while (bStream.available() != 0)
          bytes = bytes.concat(bStream.readByteArray(bStream.available()));
        bStream.close();

        // Convert the byte array to a base64-encoded string
        var str = b64(bytes);

        aEngine._iconURI = makeURI(ICON_DATAURL_PREFIX + str);
        LOG("_importSherlockEngine: Set sherlock iconURI to: \"" +
            aEngine._iconURL + "\"");

        // Write the engine to disk to save changes
        aEngine._serializeToFile();

        // Delete the icon now that we're sure everything's been saved
        icon.remove(false);
      }
    } catch (ex) { LOG("_convertSherlockFile: Error setting icon:\n" + ex); }
  },

  /**
   * Finds an icon associated to a given Sherlock file. Searches the provided
   * file's parent directory looking for files with the same base name and one
   * of the file extensions in SHERLOCK_ICON_EXTENSIONS.
   * @param aEngineFile
   *        The Sherlock plugin file.
   * @param aBaseName
   *        The basename of the Sherlock file.
   *          Example: "foo" for file "foo.src".
   * @see nsIURL::fileBaseName
   */
  _findSherlockIcon: function SRCH_SVC_findSherlock(aEngineFile, aBaseName) {
    for (var i = 0; i < SHERLOCK_ICON_EXTENSIONS.length; i++) {
      var icon = aEngineFile.parent.clone();
      icon.append(aBaseName + SHERLOCK_ICON_EXTENSIONS[i]);
      if (icon.exists() && icon.isFile())
        return icon;
    }
    return null;
  },

  /**
   * Get a sorted array of engines.
   * @param aWithHidden
   *        True if hidden plugins should be included in the result.
   */
  _getSortedEngines: function SRCH_SVC_getSorted(aWithHidden) {
    if (aWithHidden)
      return this._sortedEngines;

    return this._sortedEngines.filter(function (engine) {
                                        return !engine.hidden;
                                      });
  },

  // nsIBrowserSearchService
  getEngines: function SRCH_SVC_getEngines(aCount) {
    LOG("getEngines: getting all engines");
    var engines = this._getSortedEngines(true);
    aCount.value = engines.length;
    return engines;
  },

  getVisibleEngines: function SRCH_SVC_getVisible(aCount) {
    LOG("getVisibleEngines: getting all visible engines");
    var engines = this._getSortedEngines(false);
    aCount.value = engines.length;
    return engines;
  },

  getEngineByName: function SRCH_SVC_getEngineByName(aEngineName) {
    return this._engines[aEngineName] || null;
  },

  getEngineByAlias: function SRCH_SVC_getEngineByAlias(aAlias) {
    for (var engineName in this._engines) {
      var engine = this._engines[engineName];
      if (engine && engine.alias == aAlias)
        return engine;
    }
    return null;
  },

  addEngineWithDetails: function SRCH_SVC_addEWD(aName, aIconURL, aAlias,
                                                 aDescription, aMethod,
                                                 aTemplate) {
    ENSURE_ARG(aName, "Invalid name passed to addEngineWithDetails!");
    ENSURE_ARG(aMethod, "Invalid method passed to addEngineWithDetails!");
    ENSURE_ARG(aTemplate, "Invalid template passed to addEngineWithDetails!");

    ENSURE(!this._engines[aName], "An engine with that name already exists!",
           Cr.NS_ERROR_FILE_ALREADY_EXISTS);

    var engine = new Engine(getSanitizedFile(aName), SEARCH_DATA_XML, false);
    engine._initFromMetadata(aName, aIconURL, aAlias, aDescription,
                             aMethod, aTemplate);
    this._addEngineToStore(engine);
  },

  // If aSelect is true, the newly added engine will be selected as the current
  // engine when it finishes loading.
  addEngine: function SRCH_SVC_addEngine(aEngineURL, aType, aIconURL, aSelect) {
    LOG("addEngine: Adding \"" + aEngineURL + "\".");
    try {
      var uri = makeURI(aEngineURL);
      var engine = new Engine(uri, aType, false);
      engine._initFromURI();

      if (aSelect)
        this._selectNewEngineURI = uri;
    } catch (ex) {
      LOG("addEngine: Error adding engine:\n" + ex);
      throw Cr.NS_ERROR_FAILURE;
    }
    engine._setIcon(aIconURL, false);
  },

  removeEngine: function SRCH_SVC_removeEngine(aEngine) {
    ENSURE_ARG(aEngine, "no engine passed to removeEngine!");

    var engineToRemove = null;
    for (var e in this._engines)
      if (aEngine.wrappedJSObject == this._engines[e])
        engineToRemove = this._engines[e];

    ENSURE(engineToRemove, "removeEngine: Can't find engine to remove!",
           Cr.NS_ERROR_FILE_NOT_FOUND);

    if (engineToRemove == this.currentEngine)
      this._currentEngine = null;

    if (engineToRemove._readOnly) {
      // Just hide it (the "hidden" setter will notify)
      engineToRemove.hidden = true;
    } else {
      // Remove the engine file from disk (this might throw)
      engineToRemove._remove();

      // Remove the engine from _sortedEngines
      var index = this._sortedEngines.indexOf(engineToRemove);
      ENSURE(index != -1, "Can't find engine to remove in _sortedEngines!",
             Cr.NS_ERROR_FAILURE);
      this._sortedEngines.splice(index, 1);

      // Remove the engine from the internal store
      delete this._engines[engineToRemove.name];

      notifyAction(engineToRemove, SEARCH_ENGINE_REMOVED);
    }
  },

  moveEngine: function SRCH_SVC_moveEngine(aEngine, aNewIndex) {
    ENSURE_ARG((aNewIndex < this._sortedEngines.length) && (aNewIndex >= 0),
               "SRCH_SVC_moveEngine: Index out of bounds!");
    ENSURE_ARG(aEngine instanceof Ci.nsISearchEngine,
               "SRCH_SVC_moveEngine: Invalid engine passed to moveEngine!");

    var engine = aEngine.wrappedJSObject;

    var currentIndex = this._sortedEngines.indexOf(engine);
    ENSURE(currentIndex != -1, "moveEngine: Can't find engine to move!",
           Cr.NS_ERROR_UNEXPECTED);

    // Move the engine
    var movedEngine = this._sortedEngines.splice(currentIndex, 1)[0];
    this._sortedEngines.splice(aNewIndex, 0, movedEngine);

    notifyAction(engine, SEARCH_ENGINE_CHANGED);
  },

  get defaultEngine() {
    const defPref = BROWSER_SEARCH_PREF + "defaultenginename";
    // Get the default engine - this pref should always exist, but the engine
    // might be hidden
    this._defaultEngine = this.getEngineByName(getLocalizedPref(defPref, ""));
    if (!this._defaultEngine || this._defaultEngine.hidden)
      this._defaultEngine = this._getSortedEngines(false)[0] || null;
    return this._defaultEngine;
  },

  get currentEngine() {
    if (!this._currentEngine || this._currentEngine.hidden)
      this._currentEngine = this.defaultEngine;
    return this._currentEngine;
  },
  set currentEngine(val) {
    ENSURE_ARG(val instanceof Ci.nsISearchEngine,
               "Invalid argument passed to currentEngine setter");

    var newCurrentEngine = this.getEngineByName(val.name);
    ENSURE(newCurrentEngine, "Can't find engine in store!",
           Cr.NS_ERROR_UNEXPECTED);

    this._currentEngine = newCurrentEngine;
    setLocalizedPref(BROWSER_SEARCH_PREF + "selectedEngine",
                     this._currentEngine.name);
    notifyAction(this._currentEngine, SEARCH_ENGINE_CURRENT);
  },

  // nsIObserver
  observe: function SRCH_SVC_observe(aEngine, aTopic, aVerb) {
    switch (aTopic) {
      case SEARCH_ENGINE_TOPIC:
        if (aVerb == SEARCH_ENGINE_LOADED) {
          var engine = aEngine.QueryInterface(Ci.nsISearchEngine);
          LOG("nsISearchEngine::observe: Done installation of " + engine.name
              + ".");
          this._addEngineToStore(engine.wrappedJSObject);
          // Optionally start using this engine now.
          if (this._selectNewEngineURI &&
              this._selectNewEngineURI == engine.wrappedJSObject.uri) {
            this.currentEngine = aEngine;
            this._selectNewEngineURI = null;
          }
        }
        break;
      case XPCOM_SHUTDOWN_TOPIC:
        this._saveSortedEngineList();
        this._removeObservers();
        break;
    }
  },

  _addObservers: function SRCH_SVC_addObservers() {
    var os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.addObserver(this, SEARCH_ENGINE_TOPIC, false);
    os.addObserver(this, XPCOM_SHUTDOWN_TOPIC, false);
  },

  _removeObservers: function SRCH_SVC_removeObservers() {
    var os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.removeObserver(this, SEARCH_ENGINE_TOPIC);
    os.removeObserver(this, XPCOM_SHUTDOWN_TOPIC);
  },

  QueryInterface: function SRCH_SVC_QI(aIID) {
    if (aIID.equals(Ci.nsIBrowserSearchService) ||
        aIID.equals(Ci.nsIObserver)             ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var engineMetadataService = {
  // Keeps track of whether init() actually created a new table, or the table
  // already existed.
  newTable: false,

  init: function epsInit() {
    MozStorageStatementWrapper = 
      new Components.Constructor("@mozilla.org/storage/statement-wrapper;1",
                                 Ci.mozIStorageStatementWrapper);
    var engineDataTable = "id INTEGER PRIMARY KEY, engineid STRING, name STRING, value STRING";
    var dbService = Cc["@mozilla.org/storage/service;1"].
                      getService(Ci.mozIStorageService);
    this.mDB = dbService.openSpecialDatabase("profile");

    try {
      this.mDB.createTable("engine_data", engineDataTable);
      this.newTable = true;
    } catch (ex) {
      this.newTable = false;
      // Fails if the table already exists, which is fine
    }

    this.mGetData = createStatement (
      this.mDB,
      "SELECT value FROM engine_data WHERE engineid = :engineid AND name = :name");
    this.mDeleteData = createStatement (
      this.mDB,
      "DELETE FROM engine_data WHERE engineid = :engineid AND name = :name");
    this.mInsertData = createStatement (
      this.mDB,
      "INSERT INTO engine_data (engineid, name, value) " +
      "VALUES (:engineid, :name, :value)");
  },
  getAttr: function epsGetAttr(engine, name) {
     // attr names must be lower case
     name = name.toLowerCase();

    var stmt = this.mGetData;
    stmt.reset();
    var pp = stmt.params;
    pp.engineid = engine._id;
    pp.name = name;

    var value = null;
    if (stmt.step())
      value = stmt.row.value;
    stmt.reset();
    return value;
  },

  setAttr: function epsSetAttr(engine, name, value) {
    // attr names must be lower case
    name = name.toLowerCase();

    this.mDB.beginTransaction();

    var pp = this.mDeleteData.params;
    pp.engineid = engine._id;
    pp.name = name;
    this.mDeleteData.step();
    this.mDeleteData.reset();

    pp = this.mInsertData.params;
    pp.engineid = engine._id;
    pp.name = name;
    pp.value = value;
    this.mInsertData.step();
    this.mInsertData.reset();

    this.mDB.commitTransaction();
  },

  deleteEngineData: function epsDelData(engine, name) {
    // attr names must be lower case
    name = name.toLowerCase();

    var pp = this.mDeleteData.params;
    pp.engineid = engine._id;
    pp.name = name;
    this.mDeleteData.step();
    this.mDeleteData.reset();
  }
}

const kClassID    = Components.ID("{7319788a-fe93-4db3-9f39-818cf08f4256}");
const kClassName  = "Browser Search Service";
const kContractID = "@mozilla.org/browser/search-service;1";

// nsIFactory
const kFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return (new SearchService()).QueryInterface(iid);
  }
};

// nsIModule
const gModule = {
  registerSelf: function (componentManager, fileSpec, location, type) {
    componentManager.QueryInterface(Ci.nsIComponentRegistrar);
    componentManager.registerFactoryLocation(kClassID,
                                             kClassName,
                                             kContractID,
                                             fileSpec, location, type);
  },

  unregisterSelf: function(componentManager, fileSpec, location) {
    componentManager.QueryInterface(Ci.nsIComponentRegistrar);
    componentManager.unregisterFactoryLocation(kClassID, fileSpec);
  },

  getClassObject: function (componentManager, cid, iid) {
    if (!cid.equals(kClassID))
      throw Cr.NS_ERROR_NO_INTERFACE;
    if (!iid.equals(Ci.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    return kFactory;
  },

  canUnload: function (componentManager) {
    return true;
  }
};

function NSGetModule(componentManager, fileSpec) {
  return gModule;
}

#include ../../../toolkit/content/debug.js
