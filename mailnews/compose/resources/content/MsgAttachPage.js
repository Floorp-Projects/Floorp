/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Lowe   <michael.lowe@bigfoot.com>
 *   Blake Ross     <blaker@netscape.com>
 *   Neil Rashbrook <neil@parkwaycc.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var browser;
var dialog = {};
var pref = null;
try {
  pref = Components.classes["@mozilla.org/preferences-service;1"]
                   .getService(Components.interfaces.nsIPrefBranch);
} catch (ex) {
  // not critical, remain silent
}

function onLoad()
{
  dialog.input = document.getElementById("dialog.input");
  dialog.attach = document.documentElement.getButton("accept");
  dialog.bundle = document.getElementById("attachWebPageBundle");

  // change OK button text to 'attach'
  dialog.attach.label = dialog.bundle.getString("attachButtonLabel");

  if (pref) {
    try {
      dialog.input.value =
        pref.getComplexValue("mailnews.attach_web_page.last_url",
          Components.interfaces.nsISupportsString).data;
    }
    catch(ex) {
    }
  }

  doEnabling();
}

function doEnabling()
{
  dialog.attach.disabled = !dialog.input.value;
}

function attach()
{
  var attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"]
                             .createInstance(Components.interfaces.nsIMsgAttachment);
  attachment.url = dialog.input.value;
  window.arguments[0].attachment = attachment;
  if (pref) {
    var str = Components.classes["@mozilla.org/supports-string;1"]
                        .createInstance(Components.interfaces.nsISupportsString);
    str.data = dialog.input.value;
    pref.setComplexValue("mailnews.attach_web_page.last_url",
                         Components.interfaces.nsISupportsString, str);
  }
  return true;
}

function onChooseFile()
{
  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(Components.interfaces.nsIFilePicker);
    fp.init(window, dialog.bundle.getString("chooseFileDialogTitle"), fp.modeOpen);
    fp.appendFilters(fp.filterHTML | fp.filterText |
                     fp.filterAll | fp.filterImages | fp.filterXML);
    if (fp.show() == fp.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0)
      dialog.input.value = fp.fileURL.spec;
  }
  catch(ex) {
  }
  doEnabling();
}

function useUBHistoryItem(aMenuItem)
{
  var urlbar = document.getElementById("dialog.input");
  urlbar.value = aMenuItem.getAttribute("label");
  urlbar.select();
  doEnabling();
}
