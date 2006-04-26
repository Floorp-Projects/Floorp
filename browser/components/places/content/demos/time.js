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
 * The Original Code is Mozilla Places
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
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
 
window.addEventListener("load", BW_startup, false);

var BW_frame;
var BW_historyService = Components.classes["@mozilla.org/browser/nav-history-service;1"].
           getService(Components.interfaces.nsINavHistoryService);
var BW_result;

var loadedIframe = false;
var loadedBretts = false;

function BW_startup() {
  BW_frame = document.getElementById("theframe");

  var options = BW_historyService.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  options.maxResults = 200;
  var query = BW_historyService.getNewQuery();
  BW_result = BW_historyService.executeQuery(query, options);
  BW_result.root.containerOpen = true;

  BW_frame.contentWindow.addEventListener("load", BW_fill, true);
  loadedBretts = true;
  if (loadedIframe && loadedBretts)
    BW_fill();
  //BW_frame.onload = BW_fill;
  //BW_frame.contentDocument.onLoad = BW_fill;
  //BW_fill();
}

function BW_loadiframe() {
  loadedIframe = true;
  if (loadedIframe && loadedBretts)
    BW_fill();
}


function BW_getTLD(host) {
  var count = 0;
  for (var i = host.length - 2; i > 0; i --) {
    if (host[i] == '.') {
      count ++;
      if (count == 2) {
        return host.substr(i + 1);
      }
    }
  }
  return host;
}

var BW_filled = false;

function BW_fill() {
  if (BW_filled)
    return;
  BW_filled = true;

  BW_frame.setAttribute('onload', '');

  var container = BW_result.root;
  var length = container.childCount;
  dump("doc = " + BW_frame.contentDocument + "\n");
  var doc = BW_frame.contentDocument;

  var ios = Components.classes["@mozilla.org/network/io-service;1"].
        getService(Components.interfaces.nsIIOService);
  var dateformat = Components.classes["@mozilla.org/intl/scriptabledateformat;1"]
                   .getService(Components.interfaces.nsIScriptableDateFormat);

  var table = doc.createElement('table');
  doc.body.appendChild(table);

  var counts = new Array(240);
  for (var i = 0; i < counts.length; i ++) {
    counts[i] = 0;
  }

  var now = new Date();
  now.setHours(0);
  now.setMinutes(0);
  now.setSeconds(0);
  now.setMilliseconds(0);
  now.setDate(now.getDate()+1);
  var tonightUS = now.getTime() * 1000;
  var usPerHour = 3600000000;

  var previousSession = -1;
  var previousMS = 18437736874454810627;
  var previousHost = "";
  for (var i = 0; i < length; i ++) {
    var child = container.getChild(i);
    child.QueryInterface(Components.interfaces.nsINavHistoryVisitResultNode);
    var session = child.sessionId;

    var thisBin = Math.floor((tonightUS - child.time) / usPerHour);
    if (thisBin >= 0 && thisBin < counts.length) {
      counts[thisBin] = counts[thisBin] + 1;
    }

    var ms = child.time / 1000;
    var addedTime = false;
    if (previousMS - ms > 600000) {
      addedTime = true;
      var t = new Date(ms);
      var tr = doc.createElement('tr');
      table.appendChild(tr);
      var td = doc.createElement('td');
      td.setAttribute('colspan', '2');
      td.setAttribute('class', 'time');
      tr.appendChild(td);

      var timestring = dateformat.FormatDateTime("",
                                dateformat.dateFormatShort,
                                dateformat.timeFormatNoSeconds,
                                t.getFullYear(),
                                t.getMonth(),
                                t.getDate(),
                                t.getHours(),
                                t.getMinutes(),
                                0);
      var timetext = doc.createTextNode(timestring);
      td.appendChild(timetext);
    }
    previousMS = ms;

    var tr = doc.createElement('tr');
    table.appendChild(tr);

    // URL
    var spec;
    var uri;
    try {
      spec = child.uri;
      uri = ios.newURI(spec, null, null);
    } catch(e) {
      spec = null;
      uri = null;
    }

    // host name if needed on left
    var td = doc.createElement('td');
    td.setAttribute('valign', 'top');
    td.setAttribute('align', 'right');
    td.setAttribute('class', 'host');
    tr.appendChild(td);
    var host = BW_getTLD(uri.host);
    if (addedTime || host != previousHost) {
      // add host name
      var hosttext = doc.createTextNode(host);
      td.appendChild(hosttext);
    }
    previousHost = host;

    // right section
    var td = doc.createElement('td');
    td.setAttribute('valign', 'top');
    tr.appendChild(td);

    if (! addedTime && (i == 0 || child.sessionId != previousSession))
      td.setAttribute('class', 'itemnew');
    else
      td.setAttribute('class', 'item');
    previousSession = session;

    // title div and text
    var titlediv = doc.createElement('div');
    titlediv.setAttribute('class', 'title');

    var imgelt = doc.createElement('img');
    if (child.icon)
      imgelt.setAttribute('src', child.icon.spec);
    else
      imgelt.setAttribute('src', 'chrome://browser/skin/places/defaultFavicon.png');
    imgelt.setAttribute('width', 16);
    imgelt.setAttribute('height', 16);
    imgelt.setAttribute('class', 'favicon');
    titlediv.appendChild(imgelt);

    var titletext = doc.createTextNode(child.title);
    titlediv.appendChild(titletext);
    td.appendChild(titlediv);

    // URL
    if (spec) {
      /* // this does bold host names
      var host = uri.host;
      var hostStart = spec.indexOf(host);
      if (hostStart >= 0) {
        var prehost = spec.substring(0, hostStart);
        var prehosttext = doc.createTextNode(prehost);
        var posthost = spec.substring(hostStart + host.length, spec.length);
        var posthosttext = doc.createTextNode(posthost);
        var hosttext = doc.createTextNode(host);

        var boldElement = doc.createElement('b');
        boldElement.appendChild(hosttext);

        var urldiv = doc.createElement('div');
        urldiv.setAttribute('class', 'url');
        urldiv.appendChild(prehosttext);
        urldiv.appendChild(boldElement);
        urldiv.appendChild(posthosttext);
        td.appendChild(urldiv);
      }
      */
      var urldiv = doc.createElement('div');
      urldiv.setAttribute('class', 'url');
      var urltext = doc.createTextNode(spec);
      urldiv.appendChild(urltext);
      td.appendChild(urldiv);
    }
  }

  // smooth the counts
  var counts2 = new Array(counts.length);
  for (var i = 0; i < counts.length; i ++) {
    var ttl = 0;
    var acc = 0;
    for (var j = -2; j <= 2; j ++) {
      if (i + j < 0) continue;
      if (i + j >= counts.length) continue;
      var scale;
      if (j == -2 || j == 2) scale = 0.33;
      else if (j == -1 || j == 1) scale = 0.66;
      else scale = 1.0;
      acc += counts[i+j] * scale;
      ttl += scale;
    }
    counts2[i] = Math.round(acc);
  }

  // fill in the day bargraphs
  var daylist = document.getElementById("daylist");
  for (var i = 0; i < counts2.length / 24; i ++) {
    var day = document.createElement('hbox');
    day.setAttribute('align', 'center');
    if (i % 2)
      day.setAttribute('class', 'day2');
    else
      day.setAttribute('class', 'day1');
    daylist.appendChild(day);

    var text = document.createTextNode("Today - " + i );
    var description = document.createElement('description');
    description.setAttribute('flex', '1');
    description.appendChild(text);
    day.appendChild(description);

    var bars = document.createElement('vbox');
    bars.setAttribute('align', 'end');
    day.appendChild(bars);

    for (var b = 0; b < 24;  b++) {
      var box = document.createElement('hbox');
      box.setAttribute('width', '' + counts2[i*24 + b]);
      box.setAttribute('height', '1');
      box.setAttribute('class', 'green');
      bars.appendChild(box);
    }
  }
}
