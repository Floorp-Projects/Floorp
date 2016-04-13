/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test ThirdPartyUtil methods. See mozIThirdPartyUtil.

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);

// Since this test creates a TYPE_DOCUMENT channel via javascript, it will
// end up using the wrong LoadInfo constructor. Setting this pref will disable
// the ContentPolicyType assertion in the constructor.
prefs.setBoolPref("network.loadinfo.skip_type_assertion", true);

var NS_ERROR_INVALID_ARG = Components.results.NS_ERROR_INVALID_ARG;

function do_check_throws(f, result, stack)
{
  if (!stack) {
    try {
      // We might not have a 'Components' object.
      stack = Components.stack.caller;
    } catch (e) {
    }
  }

  try {
    f();
  } catch (exc) {
    do_check_eq(exc.result, result);
    return;
  }
  do_throw("expected " + result + " exception, none thrown", stack);
}

function run_test() {
  let util = Cc["@mozilla.org/thirdpartyutil;1"].getService(Ci.mozIThirdPartyUtil);

  // Create URIs and channels pointing to foo.com and bar.com.
  // We will use these to put foo.com into first and third party contexts.
  let spec1 = "http://foo.com/foo.html";
  let spec2 = "http://bar.com/bar.html";
  let uri1 = NetUtil.newURI(spec1);
  let uri2 = NetUtil.newURI(spec2);
  const contentPolicyType = Ci.nsIContentPolicy.TYPE_DOCUMENT;
  let channel1 = NetUtil.newChannel({uri: uri1, loadUsingSystemPrincipal: true, contentPolicyType});
  let channel2 = NetUtil.newChannel({uri: uri2, loadUsingSystemPrincipal: true, contentPolicyType});

  // Create some file:// URIs.
  let filespec1 = "file://foo.txt";
  let filespec2 = "file://bar.txt";
  let fileuri1 = NetUtil.newURI(filespec1);
  let fileuri2 = NetUtil.newURI(filespec2);
  let filechannel1 = NetUtil.newChannel({uri: fileuri1, loadUsingSystemPrincipal: true});
  let filechannel2 = NetUtil.newChannel({uri: fileuri2, loadUsingSystemPrincipal: true});

  // Test isThirdPartyURI.
  do_check_false(util.isThirdPartyURI(uri1, uri1));
  do_check_true(util.isThirdPartyURI(uri1, uri2));
  do_check_true(util.isThirdPartyURI(uri2, uri1));
  do_check_false(util.isThirdPartyURI(fileuri1, fileuri1));
  do_check_false(util.isThirdPartyURI(fileuri1, fileuri2));
  do_check_true(util.isThirdPartyURI(uri1, fileuri1));
  do_check_throws(function() { util.isThirdPartyURI(uri1, null); },
    NS_ERROR_INVALID_ARG);
  do_check_throws(function() { util.isThirdPartyURI(null, uri1); },
    NS_ERROR_INVALID_ARG);
  do_check_throws(function() { util.isThirdPartyURI(null, null); },
    NS_ERROR_INVALID_ARG);

  // We can't test isThirdPartyWindow since we can't really set up a window
  // hierarchy. We leave that to mochitests.

  // Test isThirdPartyChannel. As above, we can't test the bits that require
  // a load context or window heirarchy. Because of bug 1259873, we assume
  // that these are not third-party.
  do_check_throws(function() { util.isThirdPartyChannel(null); },
    NS_ERROR_INVALID_ARG);
  do_check_false(util.isThirdPartyChannel(channel1));
  do_check_false(util.isThirdPartyChannel(channel1, uri1));
  do_check_true(util.isThirdPartyChannel(channel1, uri2));

  let httpchannel1 = channel1.QueryInterface(Ci.nsIHttpChannelInternal);
  httpchannel1.forceAllowThirdPartyCookie = true;
  do_check_false(util.isThirdPartyChannel(channel1));
  do_check_false(util.isThirdPartyChannel(channel1, uri1));
  do_check_true(util.isThirdPartyChannel(channel1, uri2));
}

