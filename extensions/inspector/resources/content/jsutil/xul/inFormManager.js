/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/***************************************************************
* inFormManager -------------------------------------------------
*  Manages the reading and writing of forms via simple maps of
*  attribute/value pairs.  A "form" is simply a XUL window which
*  contains "form widgets" such as textboxes and menulists.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
* REQUIRED IMPORTS:
****************************************************************/

////////////////////////////////////////////////////////////////////////////
//// class inFormManager

var inFormManager =
{
  // Object
  readWindow: function(aWindow, aIds)
  {
    var el, fn;
    var doc = aWindow.document;
    var map = {};
    for (var i = 0; i < aIds.length; i++) {
      el = doc.getElementById(aIds[i]);
      if (el) {
        this.persistIf(doc, el);
        fn = this["read_"+el.localName];
        if (fn)
          map[aIds[i]] = fn(el);
      }
    }

    return map;
  },

  // void
  writeWindow: function(aWindow, aMap)
  {
    var el, fn;
    var doc = aWindow.document;
    for (var i = 0; i < aIds.length; i++) {
      el = doc.getElementById(aIds[i]);
      if (el) {
        fn = this["write_"+el.localName];
        if (fn)
          fn(el, aMap[aIds[i]]);
      }
    }
  },

  persistIf: function(aDoc, aEl)
  {
    if (aEl.getAttribute("persist") == "true" && aEl.hasAttribute("id")) {
      aEl.setAttribute("value", aEl.value);
      aDoc.persist(aEl.getAttribute("id"), "value");
    }
  },

  read_textbox: function(aEl)
  {
    return aEl.value;
  },

  read_menulist: function(aEl)
  {
    return aEl.getAttribute("value");
  },

  read_checkbox: function(aEl)
  {
    return aEl.getAttribute("checked") == "true";
  },

  read_radiogroup: function(aEl)
  {
    return aEl.getAttribute("value");
  },

  read_colorpicker: function(aEl)
  {
    return aEl.getAttribute("color");
  },

  write_textbox: function(aEl, aValue)
  {
    aEl.setAttribute("value", aValue);
  },

  write_menulist: function(aEl, aValue)
  {
    aEl.setAttribute("value", aValue);
  },

  write_checkbox: function(aEl, aValue)
  {
    aEl.setAttribute("checked", aValue);
  },

  write_radiogroup: function(aEl, aValue)
  {
    aEl.setAttribute("value", aValue);
  },

  write_colorpicker: function(aEl, aValue)
  {
    aEl.color = aValue;
  }
};

