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
 *   Darin Fisher <darin@meer.net>
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

// Constructor
function Report(pages) {
  this.pages = pages;
  this.timeVals = new Array(pages.length);  // matrix of times
  for (var i = 0; i < this.timeVals.length; ++i) {
    this.timeVals[i] = new Array();
  }
  this.totalCCTime = 0;
  this.showTotalCCTime = false;
}

// given an array of strings, finds the longest common prefix
function findCommonPrefixLength(strs) {
  if (strs.length < 2)
    return 0;

  var len = 0;
  do {
    var newlen = len + 1;
    var newprefix = null;
    var failed = false;
    for (var i = 0; i < strs.length; i++) {
      if (newlen > strs[i].length) {
	failed = true;
	break;
      }

      var s = strs[i].substr(0, newlen);
      if (newprefix == null) {
	newprefix = s;
      } else if (newprefix != s) {
	failed = true;
	break;
      }
    }

    if (failed)
      break;

    len++;
  } while (true);
  return len;
}

// returns an object with the following properties:
//   min  : min value of array elements
//   max  : max value of array elements
//   mean : mean value of array elements
//   vari : variance computation
//   stdd : standard deviation, sqrt(vari)
//   indexOfMax : index of max element (the element that is
//                removed from the mean computation)
function getArrayStats(ary) {
  var r = {};
  r.min = ary[0];
  r.max = ary[0];
  r.indexOfMax = 0;
  var sum = 0;
  for (var i = 0; i < ary.length; ++i) {
    if (ary[i] < r.min) {
      r.min = ary[i];
    } else if (ary[i] > r.max) {
      r.max = ary[i];
      r.indexOfMax = i;
    }
    sum = sum + ary[i];
  }

  // median
  sorted_ary = ary.concat();
  sorted_ary.sort();
  // remove longest run
  sorted_ary.pop();
  if (sorted_ary.length%2) {
    r.median = sorted_ary[(sorted_ary.length-1)/2]; 
  }else{
    var n = Math.floor(sorted_ary.length / 2);
    if (n >= sorted_ary.length)
      r.median = sorted_ary[n];
    else
      r.median = (sorted_ary[n] + sorted_ary[n + 1]) / 2;
  }

  // ignore max value when computing mean and stddev
  if (ary.length > 1)
    r.mean = (sum - r.max) / (ary.length - 1);
  else
    r.mean = ary[0];

  r.vari = 0;
  for (var i = 0; i < ary.length; ++i) {
    if (i == r.indexOfMax)
      continue;
    var d = r.mean - ary[i];
    r.vari = r.vari + d * d;
  }

  if (ary.length > 1) {
    r.vari = r.vari / (ary.length - 1);
    r.stdd = Math.sqrt(r.vari);
  } else {
    r.vari = 0.0;
    r.stdd = 0.0;
  }
  return r;
}

function strPad(o, len, left) {
  var str = o.toString();
  if (!len)
    len = 6;
  if (left == null)
    left = true;

  if (str.length < len) {
    len -= str.length;
    while (--len) {
      if (left)
	str = " " + str;
      else
	str += " ";
    }
  }

  str += " ";
  return str;
}

function strPadFixed(n, len, left) {
  return strPad(n.toFixed(0), len, left);
}

Report.prototype.getReport = function(format) {
  // avg and avg median are cumulative for all the pages
  var avgs = new Array();
  var medians = new Array();
  for (var i = 0; i < this.timeVals.length; ++i) {
     avgs[i] = getArrayStats(this.timeVals[i]).mean;
     medians[i] = getArrayStats(this.timeVals[i]).median;
  }
  var avg = getArrayStats(avgs).mean;
  var avgmed = getArrayStats(medians).mean;

  var report;

  var prefixLen = findCommonPrefixLength(this.pages);

  if (format == "js") {
    // output "simple" js format;
    // array of { page: "str", value: 123.4, stddev: 23.3 } objects
    report = "([";
    for (var i = 0; i < this.timeVals.length; i++) {
      var stats = getArrayStats(this.timeVals[i]);
      report += uneval({ page: this.pages[i].substr(prefixLen), value: stats.mean, stddev: stats.stdd});
      report += ",";
    }
    report += "])";
  } else if (format == "jsfull") {
    // output "full" js format, with raw values
  } else if (format == "text") {
    // output text format suitable for dumping
    report = "============================================================\n";
    report += "    " + strPad("Page", 40, false) + strPad("mean") + strPad("stdd") + strPad("min") + strPad("max") + "raw" + "\n";
    for (var i = 0; i < this.timeVals.length; i++) {
      var stats = getArrayStats(this.timeVals[i]);
      report +=
        strPad(i, 4, true) +
        strPad(this.pages[i].substr(prefixLen), 40, false) +
        strPadFixed(stats.mean) +
        strPadFixed(stats.stdd) +
        strPadFixed(stats.min) +
        strPadFixed(stats.max) +
        this.timeVals[i] +
        "\n";
    }
    if (this.showTotalCCTime) {
      report += "Cycle collection: " + this.totalCCTime + "\n"
    }
    report += "============================================================\n";
  } else if (format == "tinderbox") {
    report = "__start_tp_report\n";
    report += "_x_x_mozilla_page_load,"+avgmed+",NaN,NaN\n";  // max and min are just 0, ignored
    report += "_x_x_mozilla_page_load_details,avgmedian|"+avgmed+"|average|"+avg.toFixed(2)+"|minimum|NaN|maximum|NaN|stddev|NaN";

    for (var i = 0; i < this.timeVals.length; i++) {
      var r = getArrayStats(this.timeVals[i]);
      report += '|'+
        i + ';'+
        this.pages[i].substr(prefixLen) + ';'+
        r.median + ';'+
        r.mean + ';'+
        r.min + ';'+
        r.max + ';'+
        this.timeVals[i].join(";") +
        "\n";
    }
    report += "__end_tp_report\n";
    if (this.showTotalCCTime) {
      report += "__start_cc_report\n";
      report += "_x_x_mozilla_cycle_collect," + this.totalCCTime + "\n";
      report += "__end_cc_report\n";
    }
  } else {
    report = "Unknown report format";
  }

  return report;
}

Report.prototype.recordTime = function(pageIndex, ms) {
  this.timeVals[pageIndex].push(ms);
}

Report.prototype.recordCCTime = function(ms) {
  this.totalCCTime += ms;
  this.showTotalCCTime = true;
}
