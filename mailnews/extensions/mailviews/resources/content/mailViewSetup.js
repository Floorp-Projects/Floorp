/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2002 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 */

var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;
var gMailView = null;

var dialog;

function mailViewOnLoad()
{
  initializeSearchWidgets();
  initializeMailViewOverrides();
  dialog = {};

  if ("arguments" in window && window.arguments[0]) 
  {
    var args = window.arguments[0];
    if ("mailView" in args) 
      gMailView = window.arguments[0].mailView; 
    if ("onOkCallback" in args)
      dialog.okCallback =  window.arguments[0].onOkCallback;
  }

  dialog.OKButton = document.documentElement.getButton("accept");
  dialog.nameField = document.getElementById("name");
  dialog.nameField.focus();

  setSearchScope(nsMsgSearchScope.offlineMail);  

  if (gMailView)
  {
    dialog.nameField.value = gMailView.mailViewName;
    initializeSearchRows(nsMsgSearchScope.offlineMail, gMailView.searchTerms);

    if (gMailView.searchTerms.Count() > 1)
       gSearchLessButton.removeAttribute("disabled", "false");
  }
  else
    onMore(null);
 
  doEnabling();
}

function mailViewOnUnLoad()
{

}

function onOK()
{
  var mailViewList = Components.classes["@mozilla.org/messenger/mailviewlist;1"].getService(Components.interfaces.nsIMsgMailViewList);
  
  // reflect the search widgets back into the search session
  var newMailView = null;
  if (gMailView)
  {
    saveSearchTerms(gMailView.searchTerms, gMailView);
    gMailView.mailViewName = dialog.nameField.value;
  }
  else  
  {
    // otherwise, create a new mail view 
    newMailView = mailViewList.createMailView();

    saveSearchTerms(newMailView.searchTerms, newMailView);
    newMailView.mailViewName = dialog.nameField.value;
    // now add the mail view to our mail view list
    mailViewList.addMailView(newMailView);
  }
    
  mailViewList.save();
 
  if (dialog.okCallback)
    dialog.okCallback(gMailView ? gMailView : newMailView);

  return true;
}

function initializeMailViewOverrides()
{
  // replace some text with something we want. Need to add some ids to searchOverlay.js
  //var orButton = document.getElementById('or');
  //orButton.setAttribute('label', 'Any of the following');
  //var andButton = document.getElementById('and');
  //andButton.setAttribute('label', 'All of the following');
}

function doEnabling()
{
  if (dialog.nameField.value) 
  {
    if (dialog.OKButton.disabled)
      dialog.OKButton.disabled = false;
  } else 
  {
    if (!dialog.OKButton.disabled)
      dialog.OKButton.disabled = true;
  }
}

function onEnterInSearchTerm()
{
  // no-op for us...
}

function doHelpButton()
{
  openHelp("message-views-create-new");
}

