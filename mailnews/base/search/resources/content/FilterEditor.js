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
 *   Håkan Waara <hwaara@chello.se>
 *   Seth Spitzer <sspitzer@netscape.com>
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

// the actual filter that we're editing
var gFilter;
// cache the key elements we need
var gFilterList;
var gFilterNameElement;
var gActionTargetElement;
var gActionValueDeck;
var gActionPriority;
var gActionLabel;
var gActionJunkScore;
var gFilterBundle;
var gPreFillName;
var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;
var gPrefBranch;
var gMailSession = null;
var gMoveToFolderCheckbox;
var gChangePriorityCheckbox;
var gLabelCheckbox;
var gJunkScoreCheckbox;
var gMarkReadCheckbox;
var gMarkFlaggedCheckbox;
var gDeleteCheckbox;
var gKillCheckbox;
var gWatchCheckbox;
var gDeleteFromServerCheckbox;
var gFetchBodyFromServerCheckbox;
var gFilterActionList;

var nsMsgFilterAction = Components.interfaces.nsMsgFilterAction;

var gFilterEditorMsgWindow=null;
     
function filterEditorOnLoad()
{
    initializeSearchWidgets();
    initializeFilterWidgets();

    gPrefBranch = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService).getBranch(null);
    gFilterBundle = document.getElementById("bundle_filter");
    InitMessageLabel();
    if ("arguments" in window && window.arguments[0]) {
        var args = window.arguments[0];

        if ("filter" in args) {
          // editing a filter
          gFilter = window.arguments[0].filter;
          initializeDialog(gFilter);
        } 
        else {
          gFilterList = args.filterList;
          
          if (gFilterList)
              setSearchScope(getScopeFromFilterList(gFilterList));

          // if doing prefill filter create a new filter and populate it.
          if ("filterName" in args) {
            gPreFillName = args.filterName;
            gFilter = gFilterList.createFilter(gPreFillName);
            var term = gFilter.createTerm();

            term.attrib = Components.interfaces.nsMsgSearchAttrib.Sender;
            term.op = Components.interfaces.nsMsgSearchOp.Is;

            var termValue = term.value;
            termValue.attrib = term.attrib;
            termValue.str = gPreFillName;

            term.value = termValue;

            gFilter.appendTerm(term);

            // the default action for news filters is Delete
            // for everything else, it's MoveToFolder
            var filterAction = gFilter.createAction();
            filterAction.type = (getScopeFromFilterList(gFilterList) == Components.interfaces.nsMsgSearchScope.newsFilter) ? nsMsgFilterAction.Delete : nsMsgFilterAction.MoveToFolder;
            gFilter.appendAction(filterAction);
            initializeDialog(gFilter);
            // Clear the default action added above now that the dialog is initialized.
            gFilter.clearActionList();
          }
          else{
            // fake the first more button press
            onMore(null);
          }
        }
    }

    if (!gFilter)
    {
      var stub = gFilterBundle.getString("untitledFilterName");
      var count = 1;
      var name = stub;

      // Set the default filter name to be "Untitled Filter"
      while (duplicateFilterNameExists(name)) 
      {
        count++;
        name = stub + " " + count.toString();
      }
      gFilterNameElement.value = name;

      SetUpFilterActionList(getScopeFromFilterList(gFilterList));
    }
    gFilterNameElement.select();
    // This call is required on mac and linux.  It has no effect
    // under win32.  See bug 94800.
    gFilterNameElement.focus();
    moveToAlertPosition();
}

function filterEditorOnUnload()
{
  if (gMailSession)
    gMailSession.RemoveFolderListener(gFolderListener);
}

function onEnterInSearchTerm()
{
  // do nothing.  onOk() will get called since this is a dialog
}

function onAccept()
{
    if (duplicateFilterNameExists(gFilterNameElement.value))
    {
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
        promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);

        if (promptService)
        {
            promptService.alert(window,
                gFilterBundle.getString("cannotHaveDuplicateFilterTitle"),
                gFilterBundle.getString("cannotHaveDuplicateFilterMessage")
            );
        }

        return false;
    }


    if (!saveFilter()) return false;

    // parent should refresh filter list..
    // this should REALLY only happen when some criteria changes that
    // are displayed in the filter dialog, like the filter name
    window.arguments[0].refresh = true;
    return true;
}

// the folderListener object
var gFolderListener = {
    OnItemAdded: function(parentItem, item) {},

    OnItemRemoved: function(parentItem, item){},

    OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemIntPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue){},
    OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

    OnItemEvent: function(folder, event) {
        var eventType = event.toString();

        if (eventType == "FolderCreateCompleted") {
            SetFolderPicker(folder.URI, gActionTargetElement.id);
            SetBusyCursor(window, false);
        }     
        else if (eventType == "FolderCreateFailed") {
            SetBusyCursor(window, false);
        }

    }
}

function duplicateFilterNameExists(filterName)
{
    var args = window.arguments[0];
    var filterList;
    if ("filterList" in args)
      filterList = args.filterList;
    if (filterList)
      for (var i = 0; i < filterList.filterCount; i++)
     {
       if (filterName == filterList.getFilterAt(i).filterName)
         return true;
     }
    return false;   
}

function getScopeFromFilterList(filterList)
{
  if (!filterList) {
    dump("yikes, null filterList\n");
    return nsMsgSearchScope.offlineMail;
  }
  return filterList.folder.server.filterScope;
}

function getScope(filter) 
{
  return getScopeFromFilterList(filter.filterList);
}

function setLabelAttributes(labelID, menuItemID)
{
    var color;
    var prefString;

    try
    {
        if (labelID)
          color = gPrefBranch.getCharPref("mailnews.labels.color." + labelID);
        else
          color = "none";

        prefString = gPrefBranch.getComplexValue("mailnews.labels.description." + labelID,
                                           Components.interfaces.nsIPrefLocalizedString);
    }
    catch(ex)
    {
        dump("bad! " + ex + "\n");
    }

    document.getElementById(menuItemID).setAttribute("label", prefString);

    // the following is commented out for now until UE decides on how to show the
    // Labels menu items in the drop down list menu.
    //document.getElementById(menuItemID).setAttribute("style", ("color: " + color));
}

function initializeFilterWidgets()
{
    gFilterNameElement = document.getElementById("filterName");
    gActionTargetElement = document.getElementById("actionTargetFolder");
    gActionValueDeck = document.getElementById("actionValueDeck");
    gActionPriority = document.getElementById("actionValuePriority");
    gActionJunkScore = document.getElementById("actionValueJunkScore");
    gActionLabel = document.getElementById("actionValueLabel");
    gMoveToFolderCheckbox = document.getElementById("moveToFolder");
    gChangePriorityCheckbox = document.getElementById("changePriority");
    gChangeJunkScoreCheckbox = document.getElementById("setJunkScore");
    gLabelCheckbox = document.getElementById("label");
    gJunkScoreCheckbox = document.getElementById("setJunkScore");
    gMarkReadCheckbox = document.getElementById("markRead");
    gMarkFlaggedCheckbox = document.getElementById("markFlagged");
    gDeleteCheckbox = document.getElementById("delete");
    gKillCheckbox = document.getElementById("kill");
    gWatchCheckbox = document.getElementById("watch");
    gDeleteFromServerCheckbox = document.getElementById("deleteFromServer");
    gFetchBodyFromServerCheckbox = document.getElementById("fetchBodyFromServer");
    gFilterActionList = document.getElementById("filterActionList");
}

function initializeDialog(filter)
{
    gFilterNameElement.value = filter.filterName;
    var actionList = filter.actionList;
    var numActions = actionList.Count();
    
    for (var actionIndex=0; actionIndex < numActions; actionIndex++)
    {
      var filterAction = actionList.QueryElementAt(actionIndex, Components.interfaces.nsIMsgRuleAction);

      if (filterAction.type == nsMsgFilterAction.MoveToFolder) {
        // preselect target folder
        gMoveToFolderCheckbox.checked = true;
        var target = filterAction.targetFolderUri;
        if (target) 
          SetFolderPicker(target, gActionTargetElement.id);
      }
      else if (filterAction.type == nsMsgFilterAction.ChangePriority) 
      {
        gChangePriorityCheckbox.checked = true;
        // initialize priority
        var selectedPriority = gActionPriority.getElementsByAttribute("value", filterAction.priority).item(0);

        if (selectedPriority) 
        {
          gActionPriority.selectedItem = selectedPriority;
        }
      }
      else if (filterAction.type == nsMsgFilterAction.Label) 
      {
        gLabelCheckbox.checked = true;
        // initialize label
        var selectedLabel = gActionLabel.getElementsByAttribute("value", filterAction.label).item(0);
        if (selectedLabel) 
        {
          gActionLabel.selectedItem = selectedLabel;
        }
      }
      else if (filterAction.type == nsMsgFilterAction.JunkScore) 
      {
        gChangeJunkScoreCheckbox.checked = true;
        // initialize junk score
        var selectedJunkScore = gActionJunkScore.getElementsByAttribute("value", filterAction.junkScore).item(0);

        if (selectedJunkScore) 
        {
          gActionJunkScore.selectedItem = selectedJunkScore;
        }
      }
      else if (filterAction.type == nsMsgFilterAction.MarkRead)
        gMarkReadCheckbox.checked = true;
      else if (filterAction.type == nsMsgFilterAction.MarkFlagged)
        gMarkFlaggedCheckbox.checked = true;
      else if (filterAction.type == nsMsgFilterAction.Delete)
        gDeleteCheckbox.checked = true;
      else if (filterAction.type == nsMsgFilterAction.WatchThread)
        gWatchCheckbox.checked = true;
      else if (filterAction.type == nsMsgFilterAction.KillThread)
        gKillCheckbox.checked = true;
      else if (filterAction.type == nsMsgFilterAction.DeleteFromPop3Server)
        gDeleteFromServerCheckbox.checked = true;
      else if (filterAction.type == nsMsgFilterAction.FetchBodyFromPop3Server)
        gFetchBodyFromServerCheckbox.checked = true;

      SetUpFilterActionList(getScope(filter));
    }

    var scope = getScope(filter);
    setSearchScope(scope);
    initializeSearchRows(scope, filter.searchTerms);
    if (filter.searchTerms.Count() > 1)
      gSearchLessButton.removeAttribute("disabled", "false");

}

function InitMessageLabel()
{
    /* this code gets the label strings and changes the menu labels */
    var lastLabel = 5;
    for (var i = 0; i <= lastLabel; i++)
    {
        setLabelAttributes(i, "labelMenuItem" + i);
    }
    
    /* We set the label attribute for labels here because they have to be read from prefs. Default selection
       is always 0 and the picker thinks that it is already showing the label but it is not. Just setting selectedIndex 
       to 0 doesn't do it because it thinks that it is already showing 0. So we need to set it to something else than 0 
       and then back to 0, to make it show the default case- probably some sort of dom optimization*/

    gActionLabel.selectedIndex = 1;
    gActionLabel.selectedIndex = 0;

    document.commandDispatcher.updateCommands('create-menu-label');
}

// move to overlay
function saveFilter() 
{
  var isNewFilter;
  var str;
  var filterAction; 

  var filterName= gFilterNameElement.value;
  if (!filterName || filterName == "") 
  {
    str = gFilterBundle.getString("mustEnterName");
    window.alert(str);
    gFilterNameElement.focus();
    return false;
  }

  if (!gFilter) 
  {
    gFilter = gFilterList.createFilter(gFilterNameElement.value);
    isNewFilter = true;
    gFilter.enabled=true;
  } 
  else 
  {
    gFilter.filterName = gFilterNameElement.value;
    //Prefilter is treated as a new filter.
    if (gPreFillName) 
    {
      isNewFilter = true;
      gFilter.enabled=true;
    }
    else 
    {
      isNewFilter = false;
      gFilter.clearActionList();
    }
  }

  if (gMoveToFolderCheckbox.checked)
  {
    if (gActionTargetElement)
      targetUri = gActionTargetElement.getAttribute("uri");
    if (!targetUri || targetUri == "") 
    {
      str = gFilterBundle.getString("mustSelectFolder");
      window.alert(str);
      return false;
    }
      
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.MoveToFolder;
    filterAction.targetFolderUri = targetUri;
    gFilter.appendAction(filterAction);
  }
    
  if (gChangePriorityCheckbox.checked)  
  {
    if (!gActionPriority.selectedItem) 
    {
      str = gFilterBundle.getString("mustSelectPriority");
      window.alert(str);
      return false;
    }

    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.ChangePriority;
    filterAction.priority = gActionPriority.selectedItem.getAttribute("value");
    gFilter.appendAction(filterAction);
  }

  if (gLabelCheckbox.checked) 
  {
    if (!gActionLabel.selectedItem) 
    {
      str = gFilterBundle.getString("mustSelectLabel");
      window.alert(str);
      return false;
    }

    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.Label;
    filterAction.label = gActionLabel.selectedItem.getAttribute("value");
    gFilter.appendAction(filterAction);
  }

  if (gJunkScoreCheckbox.checked) 
  {
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.JunkScore;
    filterAction.junkScore = gActionJunkScore.selectedItem.getAttribute("value");
    gFilter.appendAction(filterAction);
  }

  if (gMarkReadCheckbox.checked) 
  {
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.MarkRead;
    gFilter.appendAction(filterAction);
  }

  if (gMarkFlaggedCheckbox.checked) 
  {
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.MarkFlagged;
    gFilter.appendAction(filterAction);
  }

  if (gDeleteCheckbox.checked) 
  {
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.Delete;
    gFilter.appendAction(filterAction);
  }

  if (gWatchCheckbox.checked) 
  {
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.WatchThread;
    gFilter.appendAction(filterAction);
  }
  
  if (gKillCheckbox.checked) 
  {
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.KillThread;
    gFilter.appendAction(filterAction);
  }

  if (gDeleteFromServerCheckbox.checked)
  {
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.DeleteFromPop3Server;
    gFilter.appendAction(filterAction);
  }

  if (gFetchBodyFromServerCheckbox.checked)
  {
    filterAction = gFilter.createAction();
    filterAction.type = nsMsgFilterAction.FetchBodyFromPop3Server;
    gFilter.appendAction(filterAction);
  }

  if (gFilter.actionList.Count() <= 0)
  {
    str = gFilterBundle.getString("mustSelectAction");
    window.alert(str);

    // reset gFilter so that filter is still saved next time around
    // see bug #186217
    gFilter = null;
    return false;
  }

  if (getScope(gFilter) == Components.interfaces.nsMsgSearchScope.newsFilter)
    gFilter.filterType = Components.interfaces.nsMsgFilterType.NewsRule;
  else
    gFilter.filterType = Components.interfaces.nsMsgFilterType.InboxRule;

  saveSearchTerms(gFilter.searchTerms, gFilter);

  if (isNewFilter) 
  {
    // new filter - insert into gFilterList
    gFilterList.insertFilterAt(0, gFilter);
  }

  // success!
  return true;
}

function onTargetFolderSelected(event)
{
    SetFolderPicker(event.target.id, gActionTargetElement.id);
}

function SetUpFilterActionList(aScope)
{
  var element, elements, i;

  // disable / enable all elements in the "filteractionlist"
  // based on the scope and the "enablefornews" attribute
  elements = gFilterActionList.getElementsByAttribute("enablefornews","true");
  for (i=0;i<elements.length;i++) 
  {
    element = elements[i];

    if (aScope == Components.interfaces.nsMsgSearchScope.newsFilter)
      element.removeAttribute("disabled");
    else
      element.setAttribute("disabled", "true");
  }

  elements = gFilterActionList.getElementsByAttribute("enablefornews","false");
  for (i=0;i<elements.length;i++) 
  {
    element = elements[i];

    if (aScope != Components.interfaces.nsMsgSearchScope.newsFilter)
      element.removeAttribute("disabled");
    else
      element.setAttribute("disabled", "true");
  }

  elements = gFilterActionList.getElementsByAttribute("enableforpop3","true");
  for (i=0;i<elements.length;i++) 
  {
    element = elements[i];

    if (aScope == Components.interfaces.nsMsgSearchScope.offlineMailFilter)
      element.removeAttribute("disabled");
    else
      element.setAttribute("disabled", "true");
  }

  // ensure that the first checked action is visible in the listbox of actions.
  elements = gFilterActionList.getElementsByTagName("checkbox");
  for (i=0;i<elements.length;i++)
  {
    element = elements[i];
    if (element.checked) 
    {
      gFilterActionList.ensureElementIsVisible(getListItemForElement(element));
      break;
    }
  }
}

function getListItemForElement(element)
{
  for (var parent = element.parentNode; parent; parent = parent.parentNode) 
  {
    if (parent.localName == "listitem")
      return parent;
  }
  return null;
}

function onLabelListChanged(event)
{
    var menuitem = event.target;
    showLabelColorFor(menuitem);
}

function showLabelColorFor(menuitem)
{
    if (!menuitem) return;
    var indexValue = menuitem.getAttribute("value");
    var labelID = gActionLabel.selectedItem.getAttribute("value");

    gActionLabel.setAttribute("selectedIndex", indexValue);
    setLabelAttributes(labelID, "actionValueLabel");
}

function GetFirstSelectedMsgFolder()
{
    var selectedFolder = gActionTargetElement.getAttribute("uri");

    var msgFolder = GetMsgFolderFromUri(selectedFolder, true);
    return msgFolder;
}

function SearchNewFolderOkCallback(name,uri)
{
  var msgFolder = GetMsgFolderFromUri(uri, true);
  var imapFolder = null;
  try
  {
    imapFolder = msgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
  }
  catch(ex) {}
  var mailSessionContractID = "@mozilla.org/messenger/services/session;1";
  if (imapFolder) //imapFolder creation is asynchronous.
  {
    if (!gMailSession)
      gMailSession = Components.classes[mailSessionContractID].getService(Components.interfaces.nsIMsgMailSession);
    try 
    {
      var nsIFolderListener = Components.interfaces.nsIFolderListener;
      var notifyFlags = nsIFolderListener.event;
      gMailSession.AddFolderListener(gFolderListener, notifyFlags);
    } 
    catch (ex) 
    {
      dump("Error adding to session: " +ex + "\n");
    }
  }

  var msgWindow = GetFilterEditorMsgWindow();

  if (imapFolder)
    SetBusyCursor(window, true);

  msgFolder.createSubfolder(name, msgWindow);
  
  if (!imapFolder)
  {
    var curFolder = uri+"/"+encodeURIComponent(name);
    SetFolderPicker(curFolder, gActionTargetElement.id);
  }
}

function UpdateAfterCustomHeaderChange()
{
  updateSearchAttributes();
}

//if you use msgWindow, please make sure that destructor gets called when you close the "window"
function GetFilterEditorMsgWindow()
{
  if (!gFilterEditorMsgWindow)
  {
    var msgWindowContractID = "@mozilla.org/messenger/msgwindow;1";
    var nsIMsgWindow = Components.interfaces.nsIMsgWindow;
    gFilterEditorMsgWindow = Components.classes[msgWindowContractID].createInstance(nsIMsgWindow);
    gFilterEditorMsgWindow.SetDOMWindow(window); 
  }
  return gFilterEditorMsgWindow;
}

function SetBusyCursor(window, enable)
{
    // setCursor() is only available for chrome windows.
    // However one of our frames is the start page which 
    // is a non-chrome window, so check if this window has a
    // setCursor method
    if ("setCursor" in window) {
        if (enable)
            window.setCursor("wait");
        else
            window.setCursor("auto");
    }
}

function doHelpButton()
{
  openHelp("mail-filters");
}
