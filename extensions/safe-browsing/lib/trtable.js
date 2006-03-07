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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


// The listmanager knows how to update tables, and the wireformat
// stuff knows how to serialize them and deserialize them. However
// both classes just treat the actual data as opaque key/value pairs.
// The key/value pairs come in many forms, for example domains to
// 1's (domain lookup tables), URLs to 1's (URL lookup tables), 
// and hashed hosts to sequences of regexp's to match URLs against.
// The TRTable encapsulates the specifics of looking URLs up in
// each table type. 
//
// Each TRTable is-a Map. You pass it a URL and it tells you whether
// the URL is a member of its map, and it does this lookup according
// to the table type. The implementation is simply a Map with a
// specialized (type-aware) lookup function. For example, a TRTable
// storing enchash data knows how to hash hosts, decrypt regexps, and
// match them.
//
// The type of the table is encoded in its name, for example
// goog-white-domain is a domain lookup table. Right now valid 
// types are url, domain, and enchash.


/**
 * Create a new TRTable. 
 *
 * @param name A string used to name the table. This string must be of the
 *             form name-function-type, for example goog-white-domain. Note
 *             that all but the last component (the type) will be ignored.
 * @constructor
 */
function PROT_TRTable(name) {
  G_Map.call(this, name);
  this.debugZone = "trtable";

  // We use this to do the crypto required of lookups in type enchash
  this.enchashDecrypter_ = new PROT_EnchashDecrypter();

  var components = name.split("-");
  if (components.length != 3) 
    throw new Error("TRTables have names like name-function-type");

  if (components[2] == "url")
    this.find = this.lookupURL_;    
  else if (components[2] == "domain")
    this.find = this.lookupDomain_;
  else if (components[2] == "enchash")
    this.find = this.lookupEnchash_;
  else
    throw new Error("Unknown table type: " + components[2]);
}

PROT_TRTable.inherits(G_Map);


/**
 * Look up a URL in a URL table
 *
 * @returns Whatever the table value is for that URL: undefined if it doesn't
 *          exist, or most likely some true value like "1" if it does
 */
PROT_TRTable.prototype.lookupURL_ = function(url) {
  var canonicalized = PROT_URLCanonicalizer.canonicalizeURL_(url);
  // Uncomment for debugging
  // G_Debug(this, "Looking up: " + url + " (" + canonicalized + ")");
  return this.find_(url);
}

/**
 * Look up a URL in a domain table
 *
 * @returns Whatever the table value is for that domain: undefined if it 
 *          doesn't exist, or most likely a true value like "1" if it does
 */
PROT_TRTable.prototype.lookupDomain_ = function(url) {
  var urlObj = Cc["@mozilla.org/network/standard-url;1"]
               .createInstance(Ci.nsIURL);
  urlObj.spec = url;
  var host = urlObj.host;
  var components = host.split(".");

  // We don't have a good way map from hosts to domains, so we instead try
  // each possibility. Could probably optimize to start at the second dot?
  for (var i = 0; i < components.length - 1; i++) {
    host = components.slice(i).join(".");
    var val = this.find_(host);
    if (val) 
      return val;
  }
  return undefined;
}

/**
 * Look up a URL in an enchashDB
 *
 * @returns Boolean indicating whether the URL is in the table
 */
PROT_TRTable.prototype.lookupEnchash_ = function(url) {
  var host = this.enchashDecrypter_.getCanonicalHost(url);

  for (var i = 0; i < PROT_EnchashDecrypter.MAX_DOTS + 1; i++) {
    var key = this.enchashDecrypter_.getLookupKey(host);

    var encrypted = this.find_(key);
    if (encrypted) {
      G_Debug(this, "Enchash DB has host " + host);

      // We have encrypted regular expressions for this host. Let's 
      // decrypt them and see if we have a match.
      var decrypted = this.enchashDecrypter_.decryptData(encrypted, host);
      var res = this.enchashDecrypter_.parseRegExps(decrypted);
      
      for (var j = 0; j < res.length; j++)
        if (res[j].test(url))
          return true;
    }

    var index = host.indexOf(".");
    if (index == -1)
      break;
    host = host.substring(index + 1);
  }

  return undefined;
}


/**
 * Lame unittesting function
 */
function TEST_PROT_TRTable() {
  if (G_GDEBUG) {
    var z = "trtable UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");  

    var url = "http://www.yahoo.com?foo=bar";
    var urlTable = new PROT_TRTable("test-foo-url");
    urlTable.insert(url, "1");
    G_Assert(z, urlTable.find(url) == "1", "URL lookups broken");
    G_Assert(z, !urlTable.find("about:config"), "about:config breaks domlook");

    var dom1 = "bar.com";
    var dom2 = "amazon.co.uk";
    var dom3 = "127.0.0.1";
    var domainTable = new PROT_TRTable("test-foo-domain");
    domainTable.insert(dom1, "1");
    domainTable.insert(dom2, "1");
    domainTable.insert(dom3, "1");
    G_Assert(z, domainTable.find("http://www.bar.com/?zaz=asdf#url") == "1", 
             "Domain lookups broken (single dot)");
    G_Assert(z, domainTable.find("http://www.amazon.co.uk/?z=af#url") == "1", 
             "Domain lookups broken (two dots)");
    G_Assert(z, domainTable.find("http://127.0.0.1/?z=af#url") == "1", 
             "Domain lookups broken (IP)");

    G_Debug(z, "PASSED");
  }
}

