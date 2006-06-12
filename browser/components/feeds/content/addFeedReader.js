/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Add Feed Reader Dialog.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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
const Cr = Components.results;

function LOG(str) {
  dump("*** " + str + "\n");
}

const TYPE_MAYBE_FEED = "application/vnd.mozilla.maybe.feed";
const PREF_SELECTED_WEB = "browser.feeds.handlers.webservice";

const TYPETYPE_MIME = 1;
const TYPETYPE_PROTOCOL = 2;

//
// window.arguments:
//
// 0          nsIDialogParamBlock containing user decision result
// 1          string uri of the service being registered
// 2          string title of the service being registered
// 3          string type of service being registered for
// 4          integer 1 = content type 2 = protocol


var AddFeedReader = {
  _result: null,
  _uri: null,
  _title: null,
  _type: null,
  _typeType: null,
  

  init: function AFR_init() {
    this._result = window.arguments[0].QueryInterface(Ci.nsIDialogParamBlock);
    this._uri = window.arguments[1];
    this._title = window.arguments[2];
    this._type = window.arguments[3];
    this._typeType = window.arguments[4];
  
    var strings = document.getElementById("strings");
    var dlg = document.documentElement;
    var addQuestion = document.getElementById("addQuestion");

    var wccr = 
        Cc["@mozilla.org/web-content-handler-registrar;1"].
        getService(Ci.nsIWebContentConverterService);
    var handler = 
        wccr.getWebContentHandlerByURI(this._type, this._uri);
    
    var key = handler != null ? "handlerRegistered" : "addHandler";
    var message = strings.getFormattedString(key, [this._title]);
    addQuestion.setAttribute("value", message);

    this._updateAddAsDefaultCheckbox();
    
    if (this._type != TYPE_MAYBE_FEED && this._typeType == TYPETYPE_MIME) {
      var mimeService = 
          Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
          getService(Ci.nsIMIMEService);
      var ext = mimeService.getPrimaryExtension(this._type, null);
      var imageBox = document.getElementById("imageBox");
      imageBox.style.backgroundImage = "url('moz-icon://goat." + ext + "?size=32');";
    }
        
    var site = document.getElementById("site");
    site.value = this._uri;
    
    if (handler)
      dlg.getButton("accept").focus();
    else {
      dlg.getButton("accept").label = strings.getString("addHandlerYes");
      dlg.getButton("cancel").label = strings.getString("addHandlerNo");
      dlg.getButton("cancel").focus();
    }
  },
  
  _updateAddAsDefaultCheckbox: function AFR__updateAddAsDefaultCheckbox() {
    var addAsDefaultCheckbox = 
        document.getElementById("addAsDefaultCheckbox");
    if (this._type != TYPE_MAYBE_FEED) {
      addAsDefaultCheckbox.hidden = true;
      return;
    }
    
    try {
      var ps = 
          Cc["@mozilla.org/preferences-service;1"].
          getService(Ci.nsIPrefBranch);
      var webHandler = 
          ps.getComplexValue(PREF_SELECTED_WEB, Ci.nsIPrefLocalizedString);
      if (webHandler.data == window.arguments[0]) {
        addAsDefaultCheckbox.checked = true;
        addAsDefaultCheckbox.disabled = true;
      }
    }
    catch (e) {
    }
  },
  
  add: function AFR_add() {
    // Used to tell the WCCR that the user chose to add the handler (rather 
    // than canceling) and whether or not they made it their default handler.
    const PARAM_SHOULD_ADD_HANDLER = 0;
    const PARAM_SHOULD_MAKE_DEFAULT = 1;
  
    this._result.SetInt(PARAM_SHOULD_ADD_HANDLER, 1);
    if (this._type == TYPE_MAYBE_FEED) {
      var addAsDefaultCheckbox = document.getElementById("addAsDefaultCheckbox");
      this._result.SetInt(PARAM_SHOULD_MAKE_DEFAULT, 
                          addAsDefaultCheckbox.checked ? 1 : 0);
    }
  }
};