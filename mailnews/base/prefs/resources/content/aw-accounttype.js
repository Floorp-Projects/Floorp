/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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

function setAccountTypeData() 
{
  var rg = document.getElementById("acctyperadio");
  var selectedItemId = rg.selectedItem.id;

  if (selectedItemId == "mailaccount")
    setMailAccountTypeData();
  else if (selectedItemId == "newsaccount")
    setNewsAccountTypeData();
}

function acctTypePageUnload() {
    gCurrentAccountData = null;
    setAccountTypeData();
    initializeIspData();
    
    if (gCurrentAccountData && gCurrentAccountData.useOverlayPanels) {
      if ("testingIspServices" in this) {
        if ("SetPageMappings" in this && testingIspServices()) {
          SetPageMappings(document.documentElement.currentPage.id, "done");
        }
      }
    }

    var pageData = parent.GetPageData();

    // If we need to skip wizardpanels, set the wizard to jump to the
    // summary page i.e., last page. Otherwise, set the flow based
    // on type of account (mail or news) user is creating.
    var skipPanels = "";
    try {
      skipPanels = gCurrentAccountData.wizardSkipPanels.toString().toLowerCase();
    } catch(ex) {}

      // "done" is the only required panel for all accounts. We used to require an identity panel but not anymore.
      // initialize wizardPanels with the optional mail/news panels
      var wizardPanels, i;
      var isMailAccount = pageData.accounttype.mailaccount;
    if (skipPanels == "true") // Support old syntax of true/false for wizardSkipPanels
      wizardPanels = new Array("identitypage"); 
    else if (isMailAccount && isMailAccount.value)
        wizardPanels = new Array("identitypage", "serverpage", "loginpage", "accnamepage");
      else
        wizardPanels = new Array("identitypage", "newsserver", "accnamepage");

      // Create a hash table of the panels to skip
      skipArray  = skipPanels.split(",");
      var skipHash = new Array();
      for (i = 0; i < skipArray.length; i++)
        skipHash[skipArray[i]] = skipArray[i];

      // Remove skipped panels
      i = 0;
      while (i < wizardPanels.length) {
        if (wizardPanels[i] in skipHash)
          wizardPanels.splice(i, 1);
        else
          i++;
      }
     
      wizardPanels.push("done");

      // Set up order of panels
      for (i = 0; i < (wizardPanels.length-1); i++)
        setNextPage(wizardPanels[i], wizardPanels[i+1]);
    
      // make the account type page go to the very first of our approved wizard panels...this is usually going to
      // be accounttype --> identitypage unless we were configured to skip the identity page
      setNextPage("accounttype",wizardPanels[0]);

    return true;
}

function initializeIspData()
{
    if (!document.getElementById("mailaccount").selected) {
      parent.SetCurrentAccountData(null);
    }    

    // now reflect the datasource up into the parent
    var accountSelection = document.getElementById("acctyperadio");

    var ispName = accountSelection.selectedItem.id;

    dump("initializing ISP data for " + ispName + "\n");
    
    if (!ispName || ispName == "") return;

    parent.PrefillAccountForIsp(ispName);
}

function setMailAccountTypeData() {
  var pageData = parent.GetPageData();
  setPageData(pageData, "accounttype", "mailaccount", true);
  setPageData(pageData, "accounttype", "newsaccount", false);
}

function setNewsAccountTypeData() {
  var pageData = parent.GetPageData();
  setPageData(pageData, "accounttype", "newsaccount", true);
  setPageData(pageData, "accounttype", "mailaccount", false);
}
