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


// A helper class that does "trustrank" lookups on a remote
// server. Right now this lookup just indicates if a page is
// phishing. The response format is protocol4 name/value pairs.
// 
// Since we're sending full URLs to the server, we try to encrypt
// them before transmission. Else HTTPS query params could leak.

/**
 * A helper class that fetches trustrank values, parses them, and
 * passes them via an object to a callback.
  * 
 * @constructor
 */
function PROT_TRFetcher(opt_noCrypto) {
  this.debugZone = "trfetcher";
  this.useCrypto_ = !opt_noCrypto;
  this.lookupserverURL_ = PROT_GlobalStore.getLookupserverURL();
  this.protocol4Parser_ = new G_Protocol4Parser();

  // We lazily instantiate the UrlCrypto object due to:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=321024
  //
  // Otherwise here we would use:
  // this.urlCrypto_ = new PROT_UrlCrypto();
}

PROT_TRFetcher.TRY_REKEYING_RESPONSE = "pleaserekey";

/**
 * Query params we'll send. Don't touch unless you know what you're
 * doing and are prepared to carefully test. 
 */
PROT_TRFetcher.prototype.extraQueryParams = {
  sourceid: "firefox-antiphish",
  features: "TrustRank",
  client: "navclient-auto-ffox2"
};

/**
 * Get the URL of the request that will fetch us TR for the argument URL
 *
 * @param url String containing the URL we'd like to fetch info about
 *
 * @returns String containing the url we should use to fetch tr info
 */
PROT_TRFetcher.prototype.getRequestURL_ = function(url) {

  if (!this.urlCrypto_)
    this.urlCrypto_ = new PROT_UrlCrypto();

  G_Debug(this, "Fetching for " + url);
    
  var requestURL = this.lookupserverURL_;
  for (var param in this.extraQueryParams) 
    requestURL += param + "=" + this.extraQueryParams[param] + "&";

  if (this.useCrypto_) {
    var maybeCryptedParams = this.urlCrypto_.maybeCryptParams({ "q": url});
    
    for (var param in maybeCryptedParams) 
      requestURL += param + "=" + 
                    encodeURIComponent(maybeCryptedParams[param]) + "&";
  } else {
    requestURL += "q=" + encodeURIComponent(url);
  }

  G_Debug(this, "Request URL: " + requestURL);

  return requestURL;
};

/**
 * Fetches information about a page.
 * 
 * @param forPage URL for which to fetch info
 *
 * @param callback Function to call back when complete.
 */
PROT_TRFetcher.prototype.get = function(forPage, callback) {
  
  var url = this.getRequestURL_(forPage);
  var closure = BindToObject(this.onFetchComplete_, this, callback);
  (new PROT_XMLFetcher()).get(url, closure);
  
  // Make this true if you want to dump URLs in a format we can use 
  // for testing
  if (false)
    dump("\ntests[\"" + url + "\"] = \"" + forPage + "\";\n");
};

/**
 * Invoked when a fetch has completed.
 *
 * @param callback Function to invoke with parsed response object
 *
 * @param responseText Text of the protocol4 message
 */
PROT_TRFetcher.prototype.onFetchComplete_ = function(callback, responseText) {
  var responseObj = this.extractResponse_(responseText);

  // The server might tell us to rekey, for example if it sees that
  // our request was unencrypted (meaning that we might not yet have
  // a key). If so, pass this hint along to the crypto key manager.

  if (responseObj[PROT_TRFetcher.TRY_REKEYING_RESPONSE] == "1" &&
      this.urlCrypto_) {
    G_Debug(this, "We're supposed to re-key. Trying.");
    var manager = this.urlCrypto_.getManager();
    if (manager)
      manager.maybeReKey();
  }

  G_Debug(this, "TR Response:");
  for (var field in responseObj)
    G_Debug(this, field + "=" + responseObj[field]);

  callback(responseObj);
};

/**
 * Parse a protocol4 message (lookup server response)
 * 
 * @param responseText String containing the server's response
 *
 * @returns Object containing the returned values or null if no
 *          response was received
 */
PROT_TRFetcher.prototype.extractResponse_ = function(responseText) {
  return this.protocol4Parser_.parse(responseText);
};

#ifdef DEBUG
// Unittests
function TEST_PROT_TRFetcher() {
  if (G_GDEBUG) {
    var z = "trfetcher UNITTEST";
    G_debugService.enableZone(z);
    G_Debug(z, "Starting");

    // Old test from before protocol4parser was broken out on its own.
    // But they do no harm, so we leave 'em.
    var fetcher = new PROT_TRFetcher();
    var fakeResponse = "foo:3:foo\nbar:3:bar\nbaz:1:b";
    var fields = fetcher.extractResponse_(fakeResponse);
    G_Assert(z, fields["foo"] == "foo", "Bad parse: foo");
    G_Assert(z, fields["bar"] == "bar", "Bad parse: bar");
    G_Assert(z, fields["baz"] == "b", "Bad parse: baz");
    G_Assert(z, !fields["yourmom"], "Bad parse: yourmom");

    // Test re-keying by stubbing the fetcher's urlCrypto_ object.

    var calledRekey = false;
    var fakeManager = {
      maybeReKey: function() {
        calledRekey = true;
      }
    };

    fetcher.urlCrypto_ = {};
    fetcher.urlCrypto_.getManager = function() { return fakeManager; };
    var rekeyText = "foo:1:bar\n" + 
                    PROT_TRFetcher.TRY_REKEYING_RESPONSE + ":1:1";
    fetcher.onFetchComplete_(function() {}, rekeyText);
    G_Assert(z, calledRekey, "rekey didn't trigger call to maybeReKey()");
    
    G_Debug(z, "PASSED");
  }
}
#endif
