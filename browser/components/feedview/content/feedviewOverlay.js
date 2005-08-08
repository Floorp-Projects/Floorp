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
 * The Original Code is Feedview for Firefox.
 *
 * The Initial Developer of the Original Code is
 * Tom Germeau <tom.germeau@epigoon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


var gFeedviewPrefs;

// The values to which these variables are initialized are just backups
// in case we can't get the actual values from the prefs service.
var gFeedviewPrefArticleLength = 50;
var gFeedviewPrefShowBar = true;
var gFeedviewPrefShowImage = true;
var gFeedviewPrefTimerInterval = 0;
var gFeedviewPrefExternalCSS = "";

var gFeedviewProcessor;
var gFeedviewStylesheet;

function initFeedview() {
  try {
    gFeedviewPrefs = Components.classes["@mozilla.org/preferences-service;1"]
                               .getService(Components.interfaces.nsIPrefService)
                               .getBranch("browser.feedview.");

    gFeedviewPrefArticleLength = gFeedviewPrefs.getIntPref("articleLength");
    gFeedviewPrefShowBar = gFeedviewPrefs.getBoolPref("showBar");
    gFeedviewPrefShowImage = gFeedviewPrefs.getBoolPref("showImage");
    gFeedviewPrefTimerInterval = gFeedviewPrefs.getIntPref("timerInterval");
    gFeedviewPrefExternalCSS = gFeedviewPrefs.getCharPref("externalCSS");
  }
  catch (ex) {}
  
  gFeedviewProcessor = new XSLTProcessor();
  gFeedviewStylesheet = document.implementation.createDocument("", "", null);
  gFeedviewStylesheet.addEventListener("load", onLoadFeedviewStylesheet, false);
  gFeedviewStylesheet.load("chrome://browser/content/feedview/feedview.xsl");

  window.addEventListener("load", maybeLoadFeedview, true);
}

function onLoadFeedviewStylesheet() {
  gFeedviewProcessor.importStylesheet(gFeedviewStylesheet);
}

function feedviewIsFeed(doc) {
  const ATOM_10_NS = "http://www.w3.org/2005/Atom";
  const ATOM_03_NS = "http://purl.org/atom/ns#";
  const RSS_10_NS = "http://purl.org/rss/1.0/";
  const RSS_09_NS = "http://my.netscape.com/rdf/simple/0.9/";
  const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";

  var rootName = doc.localName;
  var rootNS = doc.namespaceURI;
  var channel;

  // Atom feeds have a root "feed" element in one of two namespaces.
  if (rootName == "feed" && (rootNS == ATOM_10_NS || rootNS == ATOM_03_NS))
    return true;

  // RSS 2.0, 0.92, and 0.91 feeds have a non-namespaced root "rss" element
  // containing a non-namespaced "channel" child.
  else if (rootName == "rss" && rootNS == null) {
    channel = doc.getElementsByTagName('channel')[0];
    if (channel && channel.parentNode == doc)
      return true;
  }

  // RSS 1.0 and 0.9 feeds have a root "RDF" element in the RDF namespace
  // and a "channel" child in the RSS 1.0 or 0.9 namespaces.
  else if (rootName == "RDF" && rootNS == RDF_NS) {
    channel = doc.getElementsByTagNameNS(RSS_10_NS, 'channel')[0]
              || doc.getElementsByTagNameNS(RSS_09_NS, 'channel')[0];
    if (channel && channel.parentNode == doc)
      return true;
  }

  // If it didn't match any criteria yet, it's probably not a feed,
  // or perhaps it's a nonconformist feed.  If you see a number of those
  // and they match some pattern, add a check for that pattern here,
  // making sure to specify the strictest check that matches that pattern
  // to minimize false positives.
  return false;
}

// Checks every document to see if it's a feed file, and transforms it if so.
function maybeLoadFeedview(evnt) {
  // See if the document is XML.  If not, it's definitely not a feed.
  if (!(evnt.originalTarget instanceof XMLDocument) &&
      // bandaid for bug 256084
      !(evnt.originalTarget instanceof XULDocument))
    return;

  var dataXML = evnt.originalTarget;

  // Make sure the document is loaded into one of the browser tabs.
  // Otherwise, it might have been loaded as data by some application
  // that expects it not to change, and we shouldn't break the app
  // by changing the document.
  var browser;
  var browsers = document.getElementById('content').browsers;
  for (var i = 0; i < browsers.length; i++) {
    if (dataXML == browsers[i].contentDocument) {
      browser = browsers[i];
      break;
    }
  }
  if (!browser)
    return;

  // See if the document we're dealing with is actually a feed.
  if (!feedviewIsFeed(dataXML.documentElement))
    return;

  var ownerDocument = document.implementation.createDocument("", "", null);

  var strbundle=document.getElementById("bundle_feedview");

  gFeedviewProcessor.setParameter(null, "feedUrl", evnt.originalTarget.documentURI);
  gFeedviewProcessor.setParameter(null, "feedTitle", strbundle.getString("title") );
  // XXX This should be in a DTD.
  gFeedviewProcessor.setParameter(null, "lengthSliderLabel", strbundle.getString("lengthSliderLabel")); 

  // XXX These are static values.  Can I set them once and have the processor
  // apply them every time I use it to transform a document?
  gFeedviewProcessor.setParameter(null, "articleLength", gFeedviewPrefArticleLength);
  gFeedviewProcessor.setParameter(null, "showBar", gFeedviewPrefShowBar);
  gFeedviewProcessor.setParameter(null, "showImage",  gFeedviewPrefShowImage);
  gFeedviewProcessor.setParameter(null, "timerInterval", gFeedviewPrefTimerInterval);
  gFeedviewProcessor.setParameter(null, "externalCSS", gFeedviewPrefExternalCSS );

  // Get the length and give it to the description thingie.
  var len = dataXML.getElementsByTagName("item").length; 
  if (len == 0)
    len = dataXML.getElementsByTagName("entry").length;
  gFeedviewProcessor.setParameter(null, "feedDescription", strbundle.getFormattedString("description", [len] ) );

  var newFragment = gFeedviewProcessor.transformToFragment(dataXML, ownerDocument);

  var oldLink = dataXML.documentElement.firstChild;

  // XXX: it would be better if we could replace the documentElement.. 
  // now the rdf/rss/feed tag remains , NOT good 
  dataXML.documentElement.replaceChild(newFragment, oldLink);

  oldLink = dataXML.documentElement.firstChild.nextSibling;

  // Kill all other child elements of the document element
  // so only our transformed feed remains.
  while (oldLink != null) {
    oldLinkX = oldLink.nextSibling;

    if (oldLinkX != null)
      dataXML.documentElement.removeChild(oldLink);

    oldLink = oldLinkX;
  }

  // Watch the article length slider so we can update the corresponding pref
  // when it changes.
  var slider = dataXML.getElementById('lengthSlider');
  slider.addEventListener("mouseclick", updateFeedviewPrefArticleLength, false);
  slider.addEventListener("mouseup", updateFeedviewPrefArticleLength, false);
}

function updateFeedviewPrefArticleLength(event) {
  if (gFeedviewPrefArticleLength != event.target.getAttribute("curpos")) {
    gFeedviewPrefArticleLength = event.target.getAttribute("curpos");
    gFeedviewPrefs.setIntPref("articleLength", gFeedviewPrefArticleLength);
  }
}
