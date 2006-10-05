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
 * The Original Code is Composer.
 *
 * The Initial Developer of the Original Code is
 * Disruptive Innovations SARL.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <daniel.glazman@disruptive-innovations.com>, Original author
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

function Startup()
{
  if(window.arguments.length != 4)
    return;

  GetUIElements();

  gDialog.name  = window.arguments[0];
  gDialog.label = window.arguments[1];
  gDialog.src   = window.arguments[2];
  gDialog.sidebaritem = window.arguments[3];

  document.documentElement.setAttribute("id", gDialog.name);
  gDialog.iframe.setAttribute("src", gDialog.src);
  document.title = gDialog.label;

  var root = document.documentElement;
  var item = gDialog.sidebaritem;
  root.setAttribute("screenX", item.getAttribute("screenX"));
  root.setAttribute("screenY", item.getAttribute("screenY"));
  root.setAttribute("width",   item.getAttribute("width"));
  root.setAttribute("height",  item.getAttribute("height"));
}

function Onclose()
{
  gDialog.sidebaritem.removeAttribute("standalone");

  return true;
}

function Shutdown()
{
  var item = gDialog.sidebaritem;
  var root = document.documentElement;

  item.setAttribute("screenX", window.screenX);
  item.setAttribute("screenY", window.screenY);
  item.setAttribute("width",   document.documentElement.width);
  item.setAttribute("height",  document.documentElement.height);

  var doc = gDialog.sidebaritem.ownerDocument;
  var id  = item.getAttribute("id");
  doc.persist(id, "standalone");
  doc.persist(id, "screenX");
  doc.persist(id, "screenY");
  doc.persist(id, "width");
  doc.persist(id, "height");
}
