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

var gMain = null;
var gInspector = null;
var gPrefs = null;
var gLastNode = null;

function Startup()
{
  GetUIElements();

  if (window.opener &&
      "NotifierUtils" in window.opener)
    gMain = window.opener;
  else if (window.top && window.top.opener
           "NotifierUtils" in window.top.opener)
    gMain = window.top.opener;

  if (!gMain)
    return;

  gMain.NotifierUtils.addNotifierCallback("selection",
                                          Inspect,
                                          window);

  gInspector = Components.classes["@mozilla.org/inspector/dom-utils;1"]
                         .getService(Components.interfaces.inIDOMUtils);
  gPrefs = GetPrefs();
}

function Shutdown()
{
  if (gMain)
    gMain.NotifierUtils.removeNotifierCallback("selection",
                                               Inspect);
}

function Inspect(aKeyword, aNode)
{
  if (!gInspector || !aNode)
    return;

  gLastNode = aNode;

  while (gDialog.rules.hasChildNodes())
    gDialog.rules.removeChild(gDialog.rules.lastChild);

  var rules = gInspector.getCSSStyleRules(aNode);
  var l = rules.Count();
  var viewUAsheets = false;
  if (gPrefs)
  {
    try {
      viewUAsheets = gPrefs.getBoolPref("cssinspector.viewUAsheets");
    }
    catch(e) {}
  }

  for (var i = 0; i < l; i++)
  {
    var rule = rules.QueryElementAt(i, Components.interfaces.nsIDOMCSSStyleRule);
    var href = rule.parentStyleSheet.href;
    if (href)
    {
      var scheme = gMain.UrlUtils.getScheme(href);
      if (scheme == "resource" && !viewUAsheets)
        continue;
    }
    var label = document.createElement("listitem");
    label.setAttribute("label", rule.selectorText);

    try {
      label.setUserData("rule", rule, null);
    }
    catch(e) { dump("setUserData not implemented, Gecko 1.9 needed\n"); }

    gDialog.rules.appendChild(label);
  }
}

function ViewStyleRule()
{
  while (gDialog.declarations.hasChildNodes())
    gDialog.declarations.removeChild(gDialog.declarations.lastChild);

  var ruleItem = gDialog.rules.selectedItem;
  if (!ruleItem)
    return;

  var rule = ruleItem.getUserData("rule");

  var style  = rule.style;
  var length = style.length;
  for (var i = 0; i < length; i++)
  {
    var property = style.item(i);
    var value    = style.getPropertyValue(property);
    var item     = document.createElement("treeitem");
    var row      = document.createElement("treerow");
    var propCell = document.createElement("treecell");
    var valCell  = document.createElement("treecell");
    propCell.setAttribute("label", property);
    valCell.setAttribute("label", value);

    row.appendChild(propCell);
    row.appendChild(valCell);
    item.appendChild(row);
    gDialog.declarations.appendChild(item);
  }
}

function ToggleViewUASheets()
{
  var viewUAsheets = gDialog.viewUAsheets.checked;
  if (gPrefs)
    gPrefs.setBoolPref("cssinspector.viewUAsheets", viewUAsheets);
  Inspect("selection", gLastNode);
}
