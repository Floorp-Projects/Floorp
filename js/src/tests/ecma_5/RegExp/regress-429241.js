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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   <x00000000@freenet.de>
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

var gTestfile = 'regress-429241.js';
var BUGNUMBER = 429241;
var summary = '\\x or \\u followed by too few hex digits';
var r;

reportCompare(
  "x",
  (r = /[\x]+/.exec("\\x\0")) && r[0],
  "Section 1"
);

reportCompare(
  "xy",
  (r = /[\xy]+/.exec("\\xy\0")) && r[0],
  "Section 2"
);

reportCompare(
  "x0",
  (r = /[\x0]+/.exec("\\x0\0")) && r[0],
  "Section 3"
);

reportCompare(
  "x0y",
  (r = /[\x0y]+/.exec("\\x0y\0")) && r[0],
  "Section 4"
);

reportCompare(
  "\0",
  (r = /[\x00]+/.exec("\\x\0")) && r[0],
  "Section 5"
);

reportCompare(
  "0\0",
  (r = /[\x000]+/.exec("0\0")) && r[0],
  "Section 6"
);

reportCompare(
  "x",
  (r = /^\x$/.exec("x")) && r[0],
  "Section 7"
);

reportCompare(
  "xy",
  (r = /^\xy$/.exec("xy")) && r[0],
  "Section 8"
);

reportCompare(
  "x0",
  (r = /^\x0$/.exec("x0")) && r[0],
  "Section 9"
);

reportCompare(
  "x0y",
  (r = /^\x0y$/.exec("x0y")) && r[0],
  "Section 10"
);

reportCompare(
  null,
  /^\x00$/.exec("\0" + "0"),
  "Section 11"
);

reportCompare(
  "\0" + "0",
  (r = /^\x000$/.exec("\0" + "0")) && r[0],
  "Section 12"
);

reportCompare(
  "u",
  (r = /[\u]+/.exec("\\u\0")) && r[0],
  "Section 13"
);

reportCompare(
  "uy",
  (r = /[\uy]+/.exec("\\uy\0")) && r[0],
  "Section 14"
);

reportCompare(
  "u0",
  (r = /[\u0]+/.exec("\\u0\0")) && r[0],
  "Section 15"
);

reportCompare(
  "u0",
  (r = /[\u00]+/.exec("\\u0\0")) && r[0],
  "Section 16"
);

reportCompare(
  "u0",
  (r = /[\u000]+/.exec("\\u0\0")) && r[0],
  "Section 17"
);

reportCompare(
  "u0y",
  (r = /[\u0y]+/.exec("\\u0y\0")) && r[0],
  "Section 18"
);

reportCompare(
  "u0y",
  (r = /[\u00y]+/.exec("\\u0y\0")) && r[0],
  "Section 19"
);

reportCompare(
  "u0y",
  (r = /[\u000y]+/.exec("\\u0y\0")) && r[0],
  "Section 20"
);

reportCompare(
  "\0",
  (r = /[\u0000]+/.exec("\\u\0")) && r[0],
  "Section 21"
);

reportCompare(
  "0\0",
  (r = /[\u00000]+/.exec("0\0")) && r[0],
  "Section 22"
);

reportCompare(
  "u",
  (r = /^\u$/.exec("u")) && r[0],
  "Section 23"
);

reportCompare(
  "uy",
  (r = /^\uy$/.exec("uy")) && r[0],
  "Section 24"
);

reportCompare(
  "u0",
  (r = /^\u0$/.exec("u0")) && r[0],
  "Section 25"
);

reportCompare(
  "u00",
  (r = /^\u00$/.exec("u00")) && r[0],
  "Section 26"
);

reportCompare(
  "u000",
  (r = /^\u000$/.exec("u000")) && r[0],
  "Section 27"
);

reportCompare(
  "u0y",
  (r = /^\u0y$/.exec("u0y")) && r[0],
  "Section 28"
);

reportCompare(
  "u00y",
  (r = /^\u00y$/.exec("u00y")) && r[0],
  "Section 29"
);

reportCompare(
  "u000y",
  (r = /^\u000y$/.exec("u000y")) && r[0],
  "Section 30"
);

reportCompare(
  null,
  /^\u0000$/.exec("\0" + "0"),
  "Section 31"
);

reportCompare(
  "\0" + "0",
  (r = /^\u00000$/.exec("\0" + "0")) && r[0],
  "Section 32"
);
