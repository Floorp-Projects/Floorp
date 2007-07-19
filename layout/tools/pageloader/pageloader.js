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
var urlPrefix = null;

var doRenderTest = false;

var pages;
var pageIndex;
var results;
var start_time;
var cycle;
var report;
var renderReport;
var running = false;

var content;

var browserWindow = null;

function plInit() {
  if (running) {
    return;
  }
  running = true;

  cycle = 0;
  results = {};

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
    if (args.prefix) urlPrefix = args.prefix;
    doRenderTest = args.doRender;

    var ios = Cc["@mozilla.org/network/io-service;1"]
      .getService(Ci.nsIIOService);
    if (args.offline)
      ios.offline = true;
    var fileURI = ios.newURI(manifestURI, null, null);
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
    report = new Report(pages);

    if (doRenderTest)
      renderReport = new Report(pages);

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
                     content.addEventListener('load', plLoadHandler, true);
                     setTimeout(plLoadPage, 100);
                   }, 500);
      };

      browserWindow.addEventListener('load', browserLoadFunc, true);
    } else {
      window.resizeTo(winWidth, winHeight);

      content = document.getElementById('contentPageloader');
      content.addEventListener('load', plLoadHandler, true);

      setTimeout(plLoadPage, 0);
    }
  } catch(e) {
    dumpLine(e);
    plStop(true);
  }
}

function plLoadPage() {
  start_time = Date.now();
  content.loadURI(pages[pageIndex]);
}

function plLoadHandler(evt) {
  // make sure we pick up the right load event
  if (evt.type != 'load' ||
      (!evt.originalTarget instanceof Ci.nsIDOMHTMLDocument ||
       evt.originalTarget.defaultView.frameElement))
      return;

  var end_time = Date.now();
  var time = (end_time - start_time);

  var pageName = pages[pageIndex];

  results[pageName] = time;
  report.recordTime(pageIndex, time);

  if (doRenderTest)
    runRenderTest();

  if (pageIndex < pages.length-1) {
    pageIndex++;
    setTimeout(plLoadPage, 0);
  } else {
    plStop(false);
  }
}

function runRenderTest() {
  const redrawsPerSample = 5;
  const renderCycles = 10;

  if (!Ci.nsIDOMWindowUtils)
    return;

  var win;

  if (browserWindow)
    win = content.contentWindow;
  else 
    win = window;
  var wu = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

  for (var i = 0; i < renderCycles; i++) {
    var start = Date.now();
    for (var j = 0; j < redrawsPerSample; j++)
      wu.redraw();
    var end = Date.now();

    renderReport.recordTime(pageIndex, end - start);
  }
}

function plStop(force) {
  try {
    if (force == false) {
      pageIndex = 0;
      results = {};
      if (cycle < NUM_CYCLES-1) {
	cycle++;
	setTimeout(plLoadPage, 0);
	return;
      }

      var formats = reportFormat.split(",");

      for each (var fmt in formats)
	dumpLine(report.getReport(fmt));

      if (renderReport) {
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
function plLoadURLsFromURI(uri) {
  var data = "";
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"]
    .createInstance(Ci.nsIFileInputStream);
  var sstream = Cc["@mozilla.org/scriptableinputstream;1"]
    .createInstance(Ci.nsIScriptableInputStream);
  var uriFile = uri.QueryInterface(Ci.nsIFileURL);
  fstream.init(uriFile.file, -1, 0, 0);
  sstream.init(fstream); 
    
  var str = sstream.read(4096);
  while (str.length > 0) {
    data += str;
    str = sstream.read(4096);
  }
    
  sstream.close();
  fstream.close();
  var p = data.split("\n");

  // get rid of things that start with # (comments),
  // or that don't have the load string, if given
  p = p.filter(function(s) {
		 if (s == "" || s.indexOf("#") == 0)
		   return false;
		 if (pageFilterRegexp && !pageFilterRegexp.test(s))
		   return false;
		 return true;
	       });

  // stick urlPrefix to the start if necessary
  if (urlPrefix)
    p = p.map(function(s) { return urlPrefix + s; });

  return p;
}

function dumpLine(str) {
  dump(str);
  dump("\n");
}
