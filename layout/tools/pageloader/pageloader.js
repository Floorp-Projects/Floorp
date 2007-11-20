/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is tp.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Helmer <rhelmer@mozilla.com>
 *   Vladimir Vukicevic <vladimir@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;

var NUM_CYCLES = 5;

var pageFilterRegexp = null;
var reportFormat = "js";
var useBrowser = true;
var winWidth = 1024;
var winHeight = 768;

var doRenderTest = false;

var pages;
var pageIndex;
var start_time;
var cycle;
var report;
var renderReport;
var noisy = false;
var timeout = -1;
var timeoutEvent = -1;
var running = false;
var forceCC = true;
var useCCApi = false;

var content;

var TEST_DOES_OWN_TIMING = 1;

var browserWindow = null;

// the io service
var gIOS = null;

function plInit() {
  if (running) {
    return;
  }
  running = true;

  cycle = 0;

  try {
    var args = window.arguments[0].wrappedJSObject;

    var manifestURI = args.manifest;
    var startIndex = 0;
    var endIndex = -1;
    if (args.startIndex) startIndex = parseInt(args.startIndex);
    if (args.endIndex) endIndex = parseInt(args.endIndex);
    if (args.numCycles) NUM_CYCLES = parseInt(args.numCycles);
    if (args.format) reportFormat = args.format;
    if (args.width) winWidth = parseInt(args.width);
    if (args.height) winHeight = parseInt(args.height);
    if (args.filter) pageFilterRegexp = new RegExp(args.filter);
    if (args.noisy) noisy = true;
    if (args.timeout) timeout = parseInt(args.timeout);
    forceCC = !args.noForceCC;
    doRenderTest = args.doRender;

    if (forceCC) {
      if (window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIDOMWindowUtils)
                .garbageCollect) {
        useCCApi = true;
      }
    }

    gIOS = Cc["@mozilla.org/network/io-service;1"]
      .getService(Ci.nsIIOService);
    if (args.offline)
      gIOS.offline = true;
    var fileURI = gIOS.newURI(manifestURI, null, null);
    pages = plLoadURLsFromURI(fileURI);

    if (!pages) {
      dumpLine('tp: could not load URLs, quitting');
      plStop(true);
    }

    if (pages.length == 0) {
      dumpLine('tp: no pages to test, quitting');
      plStop(true);
    }

    if (startIndex < 0)
      startIndex = 0;
    if (endIndex == -1 || endIndex >= pages.length)
      endIndex = pages.length-1;
    if (startIndex > endIndex) {
      dumpLine("tp: error: startIndex >= endIndex");
      plStop(true);
    }

    pages = pages.slice(startIndex,endIndex+1);
    var pageUrls = pages.map(function(p) { return p.url.spec.toString(); });
    report = new Report(pageUrls);

    if (doRenderTest)
      renderReport = new Report(pageUrls);

    pageIndex = 0;

    if (args.useBrowserChrome) {
      var wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"]
        .getService(Ci.nsIWindowWatcher);
      var blank = Cc["@mozilla.org/supports-string;1"]
        .createInstance(Ci.nsISupportsString);
      blank.data = "about:blank";
      browserWindow = wwatch.openWindow
        (null, "chrome://browser/content/", "_blank",
         "chrome,dialog=no,width=" + winWidth + ",height=" + winHeight, blank);

      // get our window out of the way
      window.resizeTo(10,10);

      var browserLoadFunc = function (ev) {
        browserWindow.removeEventListener('load', browserLoadFunc, true);

        // do this half a second after load, because we need to be
        // able to resize the window and not have it get clobbered
        // by the persisted values
        setTimeout(function () {
                     browserWindow.resizeTo(winWidth, winHeight);
                     browserWindow.moveTo(0, 0);
                     browserWindow.focus();

                     content = browserWindow.getBrowser();
                     setTimeout(plLoadPage, 100);
                   }, 500);
      };

      browserWindow.addEventListener('load', browserLoadFunc, true);
    } else {
      window.resizeTo(winWidth, winHeight);

      content = document.getElementById('contentPageloader');

      setTimeout(plLoadPage, 250);
    }
  } catch(e) {
    dumpLine(e);
    plStop(true);
  }
}

function plPageFlags() {
  return pages[pageIndex].flags;
}

// load the current page, start timing
var removeLastAddedListener = null;
function plLoadPage() {
  var pageName = pages[pageIndex].url.spec;

  if (removeLastAddedListener)
    removeLastAddedListener();

  if (plPageFlags() & TEST_DOES_OWN_TIMING) {
    // if the page does its own timing, use a capturing handler
    // to make sure that we can set up the function for content to call
    content.addEventListener('load', plLoadHandlerCapturing, true);
    removeLastAddedListener = function() {
      content.removeEventListener('load', plLoadHandlerCapturing, true);
    };
  } else {
    // if the page doesn't do its own timing, use a bubbling handler
    // to make sure that we're called after the page's own onload() handling

    // XXX we use a capturing event here too -- load events don't bubble up
    // to the <browser> element.  See bug 390263.
    content.addEventListener('load', plLoadHandler, true);
    removeLastAddedListener = function() {
      content.removeEventListener('load', plLoadHandler, true);
    };
  }

  if (timeout > 0) {
    timeoutEvent = setTimeout('loadFail()', timeout);
  } 
  start_time = Date.now();
  content.loadURI(pageName);
}

function loadFail() {
  var pageName = pages[pageIndex].url.spec;
  dumpLine("__FAILTimeout exceeded on " + pageName + "__FAIL")
  plStop(true);
}

function plNextPage() {
  if (pageIndex < pages.length-1) {
    pageIndex++;

    if (forceCC) {
      var tccstart = new Date();
      if (useCCApi) {
        window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
              .getInterface(Components.interfaces.nsIDOMWindowUtils)
              .garbageCollect(); 
      }
      else {
        Components.utils.forceGC();
      }
      var tccend = new Date();
      report.recordCCTime(tccend - tccstart);
    }

    setTimeout(plLoadPage, 250);
  } else {
    plStop(false);
  }
}

function plRecordTime(time) {
  var pageName = pages[pageIndex].url.spec;
  report.recordTime(pageIndex, time);
  if (noisy) {
    dumpLine("Cycle " + (cycle+1) + ": loaded " + pageName);
  }
}

function plLoadHandlerCapturing(evt) {
  // make sure we pick up the right load event
  if (evt.type != 'load' ||
       evt.originalTarget.defaultView.frameElement)
      return;
  if (timeout > 0) { 
    clearTimeout(timeoutEvent);
  }

  if (!(plPageFlags() & TEST_DOES_OWN_TIMING)) {
    dumpLine("tp: Capturing onload handler used with page that doesn't do its own timing?");
    plStop(true);
  }

  // set up the function for content to call
  content.contentWindow.wrappedJSObject.tpRecordTime = function (time) {
    plRecordTime(time);
    setTimeout(plNextPage, 250);
  };
}

// the onload handler
function plLoadHandler(evt) {
  // make sure we pick up the right load event
  if (evt.type != 'load' ||
       evt.originalTarget.defaultView.frameElement)
      return;
  if (timeout > 0) { 
    clearTimeout(timeoutEvent);
  }
  var end_time = Date.now();
  var time = (end_time - start_time);

  // does this page want to do its own timing?
  // if so, we shouldn't be here
  if (plPageFlags() & TEST_DOES_OWN_TIMING) {
    dumpLine("tp: Bubbling onload handler used with page that does its own timing?");
    plStop(true);
  }

  plRecordTime(time);

  if (doRenderTest)
    runRenderTest();

  plNextPage();
}

function runRenderTest() {
  const redrawsPerSample = 500;

  if (!Ci.nsIDOMWindowUtils)
    return;

  var win;

  if (browserWindow)
    win = content.contentWindow;
  else 
    win = window;
  var wu = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

  var start = Date.now();
  for (var j = 0; j < redrawsPerSample; j++)
    wu.redraw();
  var end = Date.now();

  renderReport.recordTime(pageIndex, end - start);
}

function plStop(force) {
  try {
    if (force == false) {
      pageIndex = 0;
      if (cycle < NUM_CYCLES-1) {
        cycle++;
        setTimeout(plLoadPage, 250);
        return;
      }

      var formats = reportFormat.split(",");

      if (!renderReport) {
        for each (var fmt in formats)
          dumpLine(report.getReport(fmt));
      }
      else {
        dumpLine ("*************** Render report *******************");
        for each (var fmt in formats)
          dumpLine(renderReport.getReport(fmt));
      }
    }
  } catch (e) {
    dumpLine(e);
  }

  if (content)
    content.removeEventListener('load', plLoadHandler, true);

  goQuitApplication();
}

/* Returns array */
function plLoadURLsFromURI(manifestUri) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"]
    .createInstance(Ci.nsIFileInputStream);
  var uriFile = manifestUri.QueryInterface(Ci.nsIFileURL);

  fstream.init(uriFile.file, -1, 0, 0);
  var lstream = fstream.QueryInterface(Ci.nsILineInputStream);

  var d = [];

  var lineNo = 0;
  var line = {value:null};
  var more;
  do {
    lineNo++;
    more = lstream.readLine(line);
    var s = line.value;

    // strip comments
    s = s.replace(/#.*/, '');

    // strip leading and trailing whitespace
    s = s.replace(/^\s*/, '').replace(/s\*$/, '');

    if (!s)
      continue;

    var flags = 0;
    var urlspec = s;

    // split on whitespace, and figure out if we have any flags
    var items = s.split(/\s+/);
    if (items[0] == "include") {
      if (items.length != 2) {
        dumpLine("tp: Error on line " + lineNo + " in " + manifestUri.spec + ": include must be followed by the manifest to include!");
        return null;
      }

      var subManifest = gIOS.newURI(items[1], null, manifestUri);
      if (subManifest == null) {
        dumpLine("tp: invalid URI on line " + manifestUri.spec + ":" + lineNo + " : '" + line.value + "'");
        return null;
      }

      var subItems = plLoadURLsFromURI(subManifest);
      if (subItems == null)
        return null;
      d = d.concat(subItems);
    } else {
      if (items.length == 2) {
        if (items[0].indexOf("%") != -1)
          flags |= TEST_DOES_OWN_TIMING;

        urlspec = items[1];
      } else if (items.length != 1) {
        dumpLine("tp: Error on line " + lineNo + " in " + manifestUri.spec + ": whitespace must be %-escaped!");
        return null;
      }

      var url = gIOS.newURI(urlspec, null, manifestUri);

      if (pageFilterRegexp && !pageFilterRegexp.test(url.spec))
        continue;

      d.push({   url: url,
               flags: flags });
    }
  } while (more);

  return d;
}

function dumpLine(str) {
  dump(str);
  dump("\n");
}
