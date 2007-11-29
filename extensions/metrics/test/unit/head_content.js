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
 * The Original Code is the Metrics Service.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

// Common functions for MetricsService tests

function getMetricsService() {
  return Components.classes["@mozilla.org/extensions/metrics/service;1"]
    .getService(Components.interfaces.nsIMetricsService);
}

function createEventItem(ns, name) {
  return getMetricsService().createEventItem(ns, name);
}

const EventNS = "http://www.mozilla.org/metrics";

function buildItemChildren(item) {
  var items = [];
  var child;
  for (var i = 0; i < 10; ++i) {
    child = createEventItem(EventNS, "child" + i);
    items.push(child);
    item.appendChild(child);
  }
  return items;
}

function compareItemChildren(item, children) {
  do_check_eq(item.childCount, children.length);
  for (var i = 0; i < children.length; ++i) {
    do_check_eq(item.childAt(i), children[i]);
  }
}
