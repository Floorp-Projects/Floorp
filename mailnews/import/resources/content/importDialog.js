/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

var importType = null;
var gImportMsgsBundle;
var importService = 0;
var successStr = null;
var errorStr = null;
var inputStr = null ;
var progressInfo = null;
var selectedModuleName = null;

var selLocIsHome = false ;
var addInterface = null ;

function OnLoadImportDialog()
{
  gImportMsgsBundle = document.getElementById("bundle_importMsgs");
  importService = Components.classes["@mozilla.org/import/import-service;1"].getService();
  importService = top.importService.QueryInterface(Components.interfaces.nsIImportService);

  progressInfo = { };
  progressInfo.progressWindow = null;
  progressInfo.importInterface = null;
  progressInfo.mainWindow = window;
  progressInfo.intervalState = 0;
  progressInfo.importSuccess = false;
  progressInfo.importType = null;
  progressInfo.localFolderExists = false;

  // look in arguments[0] for parameters
  if (window.arguments && window.arguments.length >= 1 &&
            "importType" in window.arguments[0] && window.arguments[0].importType)
  {
    // keep parameters in global for later
    importType = window.arguments[0].importType;
    progressInfo.importType = top.importType;
  }
  else
  {
    importType = "addressbook";
    progressInfo.importType = "addressbook";
  }

  SetUpImportType();
}


function SetUpImportType()
{
  // set dialog title
  var typeRadioGroup = document.getElementById("importFields");
  switch (importType)
  {

    case "mail":
      typeRadioGroup.selectedItem = document.getElementById("mailRadio");
      break;
    case "addressbook":
      typeRadioGroup.selectedItem = document.getElementById("addressbookRadio");
      break;
    case "settings":
      typeRadioGroup.selectedItem = document.getElementById("settingsRadio");
      break;
  }

  ListModules();
}


function SetDivText(id, text)
{
  var div = document.getElementById(id);

  if (div) {
    if (!div.childNodes.length) {
      var textNode = document.createTextNode(text);
      div.appendChild(textNode);
    }
    else if ( div.childNodes.length == 1 ) {
      div.childNodes[0].nodeValue = text;
    }
  }
}

function CheckIfLocalFolderExists()
{
  var acctMgr = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
  if (acctMgr) {
    try {
      if (acctMgr.localFoldersServer)
        progressInfo.localFolderExists = true; 
    }
    catch (ex) {
      progressInfo.localFolderExists = false;
    }
  }
}

function ImportDialogOKButton()
{
  var tree = document.getElementById('moduleList');
  var deck = document.getElementById("stateDeck");
  var header = document.getElementById("header");
  var progressMeterEl = document.getElementById("progressMeter");
  var progressStatusEl = document.getElementById("progressStatus");
  var progressTitleEl = document.getElementById("progressTitle");

  // better not mess around with navigation at this point
  var nextButton = document.getElementById("forward");
  nextButton.setAttribute("disabled", "true");
  var backButton = document.getElementById("back");
  backButton.setAttribute("disabled", "true");

  if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
  {
    var importTypeRadioGroup = document.getElementById("importFields");
    importType = importTypeRadioGroup.selectedItem.getAttribute("value");
    var index = tree.selectedItems[0].getAttribute('list-index');
    var module = importService.GetModule(importType, index);
    var name = importService.GetModuleName(importType, index);
    selectedModuleName = name;
    if (module)
    {
      // Fix for Bug 57839 & 85219
      // We use localFoldersServer(in nsIMsgAccountManager) to check if Local Folder exists.
      // We need to check localFoldersServer before importing "mail" or "settings".
      // Reason: We will create an account with an incoming server of type "none" after 
      // importing "mail", so the localFoldersServer is valid even though the Local Folder 
      // is not created.
      if (importType == "mail" || importType == "settings")
        CheckIfLocalFolderExists();

      var meterText = "";
      switch(importType)
      {
        case "mail":

          top.successStr = Components.classes["@mozilla.org/supports-wstring;1"].createInstance();
          if (top.successStr) {
            top.successStr = top.successStr.QueryInterface( Components.interfaces.nsISupportsWString);
          }
          top.errorStr = Components.classes["@mozilla.org/supports-wstring;1"].createInstance();
          if (top.errorStr)
            top.errorStr = top.errorStr.QueryInterface( Components.interfaces.nsISupportsWString);

          if (ImportMail( module, top.successStr, top.errorStr) == true)
          {
            // We think it was a success, either, we need to
            // wait for the import to finish
            // or we are done!
            if (top.progressInfo.importInterface == null) {
              ShowImportResults(true, 'Mail');
              return( true);
            }
            else {
              meterText = gImportMsgsBundle.getFormattedString('MailProgressMeterText',
                                                               [ name ]);
              header.setAttribute("description", meterText);

              progressStatusEl.setAttribute("label", "");
              progressTitleEl.setAttribute("label", meterText);

              deck.setAttribute("selectedIndex", "2");
              progressInfo.progressWindow = top.window;
              progressInfo.intervalState = setInterval("ContinueImportCallback()", 100);
              return( true);
            }
          }
          else
          {
            ShowImportResults(false, 'Mail');
            return( false);
          }
          break;

        case "addressbook":
          top.successStr = Components.classes["@mozilla.org/supports-wstring;1"].createInstance();
          if (top.successStr)
            top.successStr = top.successStr.QueryInterface( Components.interfaces.nsISupportsWString);
          top.errorStr = Components.classes["@mozilla.org/supports-wstring;1"].createInstance();
          if (top.errorStr)
            top.errorStr = top.errorStr.QueryInterface( Components.interfaces.nsISupportsWString);
          top.inputStr = Components.classes["@mozilla.org/supports-wstring;1"].createInstance();
          if (top.inputStr)
            top.inputStr = top.inputStr.QueryInterface( Components.interfaces.nsISupportsWString);

          if (ImportAddress( module, top.successStr, top.errorStr) == true) {
            // We think it was a success, either, we need to
            // wait for the import to finish
            // or we are done!
            if (top.progressInfo.importInterface == null) {
              ShowImportResults(true, 'Address');
              return( true);
            }
            else {
              meterText = gImportMsgsBundle.getFormattedString('MailProgressMeterText',
                                                               [ name ]);
              header.setAttribute("description", meterText);

              progressStatusEl.setAttribute("label", "");
              progressTitleEl.setAttribute("label", meterText);

              deck.setAttribute("selectedIndex", "2");
              progressInfo.progressWindow = top.window;
              progressInfo.intervalState = setInterval("ContinueImportCallback()", 100);

              return( true);
            }
          }
          else
          {
            ShowImportResults(false, 'Address');
            return( false);
          }
          break;

        case "settings":
          var error = new Object();
          error.value = null;
          var newAccount = new Object();
          if (!ImportSettings( module, newAccount, error))
          {
            if (error.value)
              ShowImportResultsRaw(gImportMsgsBundle.getString('ImportSettingsFailed'), null, false);
            // the user canceled the operation, shoud we dismiss
            // this dialog or not?
            return false;
          }
          else
            ShowImportResultsRaw(gImportMsgsBundle.getFormattedString('ImportSettingsSuccess', [ name ]), null, true);
          break;
      }
    }
  }

  return true;
}

function SetStatusText( val)
{
  var progressStatus = document.getElementById("progressStatus");
  progressStatus.setAttribute( "label", val);
}

function SetProgress( val)
{
  var progressMeter = document.getElementById("progressMeter");
  progressMeter.setAttribute( "label", val);
}

function ContinueImportCallback()
{
  progressInfo.mainWindow.ContinueImport( top.progressInfo);
}

function ImportSelectionChanged()
{
  var tree = document.getElementById('moduleList');
  if ( tree && tree.selectedItems && (tree.selectedItems.length == 1) )
  {
    var index = tree.selectedItems[0].getAttribute('list-index');
    SetDivText('description', top.importService.GetModuleDescription(top.importType, index));
  }
}

function ListModules() {
  if (top.importService == null)
    return;

  var body = document.getElementById( "bucketBody");
  var max = body.childNodes.length - 1;
  while (max >= 0) {
    body.removeChild( body.childNodes[max]);
    max--;
  }

  var count = top.importService.GetModuleCount( top.importType);
  for (var i = 0; i < count; i++) {
    AddModuleToList( top.importService.GetModuleName( top.importType, i), i);
  }
}

function AddModuleToList(moduleName, index)
{
  var body = document.getElementById("bucketBody");

  var item = document.createElement('treeitem');
  var row = document.createElement('treerow');
  var cell = document.createElement('treecell');
  cell.setAttribute('label', moduleName);
  item.setAttribute('list-index', index);

  row.appendChild(cell);
  item.appendChild(row);
  body.appendChild(item);
}


function ContinueImport( info) {
  var isMail = info.importType == 'mail' ? true : false;
  var clear = true;
  var deck;
  var pcnt;

  if (info.importInterface) {
    if (!info.importInterface.ContinueImport()) {
      info.importSuccess = false;
      clearInterval( info.intervalState);
      if (info.progressWindow != null) {
        deck = document.getElementById("stateDeck");
        deck.setAttribute("selectedIndex", "3");
        info.progressWindow = null;
      }

      ShowImportResults(false, isMail ? 'Mail' : 'Address');
    }
    else if ((pcnt = info.importInterface.GetProgress()) < 100) {
      clear = false;
      if (info.progressWindow != null) {
        if (pcnt < 5)
          pcnt = 5;
        SetProgress( pcnt);
        if (isMail) {
          var mailName = info.importInterface.GetData( "currentMailbox");
          if (mailName) {
            mailName = mailName.QueryInterface( Components.interfaces.nsISupportsWString);
            if (mailName)
              SetStatusText( mailName.data);
          }
        }
      }
    }
    else {
      dump("*** WARNING! sometimes this shows results too early. \n");
      dump("    something screwy here. this used to work fine.\n");
      clearInterval( info.intervalState);
      info.importSuccess = true;
      if (info.progressWindow) {
        deck = document.getElementById("stateDeck");
        deck.setAttribute("selectedIndex", "3");
        info.progressWindow = null;
      }

      ShowImportResults(true, isMail ? 'Mail' : 'Address');
    }
  }
  if (clear) {
    info.intervalState = null;
    info.importInterface = null;
  }
}


function ShowResults(doesWantProgress, result)
{
       if (result)
       {
               if (doesWantProgress)
               {
                       var deck = document.getElementById("stateDeck");
                       var header = document.getElementById("header");
                       var progressStatusEl = document.getElementById("progressStatus");
                       var progressTitleEl = document.getElementById("progressTitle");

                       var meterText = gImportMsgsBundle.getFormattedString('MailProgressMeterText',
                                                                                                                  [ name ]);
                       header.setAttribute("description", meterText);
      
                       progressStatusEl.setAttribute("label", "");
                       progressTitleEl.setAttribute("label", meterText);

                       deck.setAttribute("selectedIndex", "2");
                       progressInfo.progressWindow = top.window;
                       progressInfo.intervalState = setInterval("ContinueImportCallback()", 100);
               }
               else
               {
                       ShowImportResults(true, 'Address');
               }
       }
       else
       {
        ShowImportResults(false, 'Address');
       }

       return true ;
}


function ShowImportResults(good, module)
{
  var modSuccess = 'Import' + module + 'Success';
  var modFailed = 'Import' + module + 'Failed';
  var results, title;
  if (good) {
    title = gImportMsgsBundle.getFormattedString(modSuccess, [ selectedModuleName ? selectedModuleName : '' ]);
    results = successStr.data;
  }
  else if (errorStr.data) {
    title = gImportMsgsBundle.getFormattedString(modFailed, [ selectedModuleName ? selectedModuleName : '' ]);
    results = errorStr.data;
  }

  if (results && title)
    ShowImportResultsRaw(title, results, good);
}

function ShowImportResultsRaw(title, results, good)
{
  SetDivText("status", title);
  var header = document.getElementById("header");
  header.setAttribute("description", title);
  dump("*** results = " + results + "\n");
  attachStrings("results", results);
  var deck = document.getElementById("stateDeck");
  deck.setAttribute("selectedIndex", "3");
  var nextButton = document.getElementById("forward");
  nextButton.label = nextButton.getAttribute("finishedval");
  nextButton.removeAttribute("disabled");
  var cancelButton = document.getElementById("cancel");
  cancelButton.setAttribute("disabled", "true");

  // If the Local Folder is not existed, create it after successfully 
  // import "mail" and "settings"
  var checkLocalFolder = (top.progressInfo.importType == 'mail' || top.progressInfo.importType == 'settings') ? true : false;
  if (good && checkLocalFolder && !top.progressInfo.localFolderExists) {
    var messengerMigrator = Components.classes["@mozilla.org/messenger/migrator;1"].getService(Components.interfaces.nsIMessengerMigrator);
    if (messengerMigrator)
      messengerMigrator.createLocalMailAccount(false);
  }
}

function attachStrings(aNode, aString)
{
  var attachNode = document.getElementById(aNode);
  if (!aString) {
    attachNode.parentNode.setAttribute("hidden", "true");
    return;
  }
  var strings = aString.split("\n");
  for (var i = 0; i < strings.length; i++) {
    if (strings[i]) {
      var currNode = document.createTextNode(strings[i]);
      attachNode.appendChild(currNode);
      var br = document.createElementNS("http://www.w3.org/1999/xhtml", 'br');
      attachNode.appendChild( br);
    }
  }
}

function ShowAddressComplete( good)
{
  var str = null;
  if (good == true) {
    if ((top.selectedModuleName != null) && (top.selectedModuleName.length > 0))
      str = gImportMsgsBundle.getFormattedString('ImportAddressSuccess', [ top.selectedModuleName ]);
    else
      str = gImportMsgsBundle.getFormattedString('ImportAddressSuccess', [ "" ]);
    str += "\n";
    str += "\n" + top.successStr.data;
  }
  else {
    if ((top.errorStr.data != null) && (top.errorStr.data.length > 0)) {
      if ((top.selectedModuleName != null) && (top.selectedModuleName.length > 0))
        str = gImportMsgsBundle.getFormattedString('ImportAddressFailed', [ top.selectedModuleName ]);
      else
        str = gImportMsgsBundle.getFormattedString('ImportAddressFailed', [ "" ]);
      str += "\n" + top.errorStr.data;
    }
  }

  if (str != null)
    alert( str);
}

function CreateNewFileSpecFromPath( inPath)
{
  var file = Components.classes["@mozilla.org/filespec;1"].createInstance();
  if (file != null) {
    file = file.QueryInterface( Components.interfaces.nsIFileSpec);
    if (file != null) {
      file.nativePath = inPath;
    }
  }

  return( file);
}

/*
  Import Settings from a specific module, returns false if it failed
  and true if successful.  A "local mail" account is returned in newAccount.
  This is only useful in upgrading - import the settings first, then
  import mail into the account returned from ImportSettings, then
  import address books.
  An error string is returned as error.value
*/
function ImportSettings( module, newAccount, error) {
  var setIntf = module.GetImportInterface( "settings");
  if (setIntf != null)
    setIntf = setIntf.QueryInterface( Components.interfaces.nsIImportSettings);
  if (setIntf == null) {
    error.value = gImportMsgsBundle.getString('ImportSettingsBadModule');
    return( false);
  }

  // determine if we can auto find the settings or if we need to ask the user
  var location = new Object();
  var description = new Object();
  var result = setIntf.AutoLocate( description, location);
  if (result == false) {
    // In this case, we couldn't not find the settings
    if (location.value != null) {
      // Settings were not found, however, they are specified
      // in a file, so ask the user for the settings file.
      var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance();
      if (filePicker != null) {
        filePicker = filePicker.QueryInterface( Components.interfaces.nsIFilePicker);
        if (filePicker != null) {
          var file = null;
          try {
            filePicker.init( top.window, gImportMsgsBundle.getString('ImportSelectSettings'), Components.interfaces.nsIFilePicker.modeOpen);
            filePicker.appendFilters( Components.interfaces.nsIFilePicker.filterAll);
            filePicker.show();
            if (filePicker.file && (filePicker.file.path.length > 0))
              file = CreateNewFileSpecFromPath( filePicker.file.path);
            else
              file = null;
          }
          catch(ex) {
            file = null;
            error.value = null;
            return( false);
          }
          if (file != null) {
            setIntf.SetLocation( file);
          }
          else {
            error.value = null;
            return( false);
          }
        }
        else {
          error.value = gImportMsgsBundle.getString('ImportSettingsNotFound');
          return( false);
        }
      }
      else {
        error.value = gImportMsgsBundle.getString('ImportSettingsNotFound');
        return( false);
      }
    }
    else {
      error.value = gImportMsgsBundle.getString('ImportSettingsNotFound');
      return( false);
    }
  }

  // interesting, we need to return the account that new
  // mail should be imported into?
  // that's really only useful for "Upgrade"
  result = setIntf.Import( newAccount);
  if (result == false) {
    error.value = gImportMsgsBundle.getString('ImportSettingsFailed');
  }
  return( result);
}

function CreateNewFileSpec( inFile)
{
  var file = Components.classes["@mozilla.org/filespec;1"].createInstance();
  if (file != null) {
    file = file.QueryInterface( Components.interfaces.nsIFileSpec);
    if (file != null) {
      file.fromFileSpec( inFile);
    }
  }

  return( file);
}

function ImportMail( module, success, error) {
  if (top.progressInfo.importInterface || top.progressInfo.intervalState) {
    error.data = gImportMsgsBundle.getString('ImportAlreadyInProgress');
    return( false);
  }

  top.progressInfo.importSuccess = false;

  var mailInterface = module.GetImportInterface( "mail");
  if (mailInterface != null)
    mailInterface = mailInterface.QueryInterface( Components.interfaces.nsIImportGeneric);
  if (mailInterface == null) {
    error.data = gImportMsgsBundle.getString('ImportMailBadModule');
    return( false);
  }

  var loc = mailInterface.GetData( "mailLocation");

  if (loc == null) {
    // No location found, check to see if we can ask the user.
    if (mailInterface.GetStatus( "canUserSetLocation") != 0) {
      var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance();
      if (filePicker != null) {
        filePicker = filePicker.QueryInterface( Components.interfaces.nsIFilePicker);
        if (filePicker != null) {
          try {
            filePicker.init( top.window, gImportMsgsBundle.getString('ImportSelectMailDir'), Components.interfaces.nsIFilePicker.modeGetFolder);
            filePicker.appendFilters( Components.interfaces.nsIFilePicker.filterAll);
            filePicker.show();
            if (filePicker.file && (filePicker.file.path.length > 0))
              mailInterface.SetData( "mailLocation", CreateNewFileSpecFromPath( filePicker.file.path));
            else
              return( false);
          } catch( ex) {
            // don't show an error when we return!
            return( false);
          }
        }
        else {
          error.data = gImportMsgsBundle.getString('ImportMailNotFound');
          return( false);
        }
      }
      else {
        error.data = gImportMsgsBundle.getString('ImportMailNotFound');
        return( false);
      }
    }
    else {
      error.data = gImportMsgsBundle.getString('ImportMailNotFound');
      return( false);
    }
  }

  if (mailInterface.WantsProgress()) {
   if (mailInterface.BeginImport( success, error, false)) {	
      top.progressInfo.importInterface = mailInterface;
      // top.intervalState = setInterval( "ContinueImport()", 100);
      return true;
    }
    else
      return false;
  }
  else
    return mailInterface.BeginImport( success, error, false) ? true : false;
}


// The address import!  A little more complicated than the mail import
// due to field maps...
function ImportAddress( module, success, error) {
  if (top.progressInfo.importInterface || top.progressInfo.intervalState) {
    error.data = gImportMsgsBundle.getString('ImportAlreadyInProgress');
    return( false);
  }

  top.progressInfo.importSuccess = false;

  addInterface = module.GetImportInterface( "addressbook");
  if (addInterface != null)
    addInterface = addInterface.QueryInterface( Components.interfaces.nsIImportGeneric);
  if (addInterface == null) {
    error.data = gImportMsgsBundle.getString('ImportAddressBadModule');
    return( false);
  }

  var path ;
  var loc = addInterface.GetStatus( "autoFind");
  if (loc == false) {
    loc = addInterface.GetData( "addressLocation");
    if (loc != null) {
      loc = loc.QueryInterface( Components.interfaces.nsIFileSpec);
      if (loc != null) {
        if (!loc.exists)
          loc = null;
      }
    }
  }

  if (loc == null) {
    // Couldn't find the address book, see if we can
    // as the user for the location or not?
    if (addInterface.GetStatus( "canUserSetLocation") == 0) {
      // an autofind address book that could not be found!
      error.data = gImportMsgsBundle.getString('ImportAddressNotFound');
      return( false);
    }

    var filePicker = Components.classes["@mozilla.org/filepicker;1"].createInstance();
    if (filePicker != null) {
      filePicker = filePicker.QueryInterface( Components.interfaces.nsIFilePicker);
      if (filePicker == null) {
        error.data = gImportMsgsBundle.getString('ImportAddressNotFound');
        return( false);
      }
    }
    else {
      error.data = gImportMsgsBundle.getString('ImportAddressNotFound');
      return( false);
    }

    // The address book location was not found.
    // Determine if we need to ask for a directory
    // or a single file.
    var file = null;
    if (addInterface.GetStatus( "supportsMultiple") != 0) {
      // ask for dir
      try {
        filePicker.init( top.window, gImportMsgsBundle.getString('ImportSelectAddrDir'), Components.interfaces.nsIFilePicker.modeGetFolder);
        filePicker.appendFilters( Components.interfaces.nsIFilePicker.filterAll);
        filePicker.show();
        if (filePicker.file && (filePicker.file.path.length > 0))
          file = filePicker.file.path;
        else
          file = null;
      } catch( ex) {
        file = null;
      }
    }
    else {
      // ask for file
      try {
        filePicker.init( top.window, gImportMsgsBundle.getString('ImportSelectAddrFile'), Components.interfaces.nsIFilePicker.modeOpen);
	if (selectedModuleName == gImportMsgsBundle.getString('Comm4xImportName'))
		filePicker.appendFilter(gImportMsgsBundle.getString('Comm4xFiles'),"*.na2");
        filePicker.appendFilters( Components.interfaces.nsIFilePicker.filterAll);
        filePicker.show();
        if (filePicker.file && (filePicker.file.path.length > 0))
          file = filePicker.file.path;
        else
          file = null;
      } catch( ex) {
        file = null;
      }
    }

    path = filePicker.file.leafName;
	
    if (file == null) {
      return( false);
    }

    file = CreateNewFileSpecFromPath( file);

    addInterface.SetData( "addressLocation", file);
  }

  // no need to use the fieldmap for 4.x import since we are using separate dialog
  if (selectedModuleName == gImportMsgsBundle.getString('Comm4xImportName'))
  {
               var deck = document.getElementById("stateDeck");
               deck.setAttribute("selectedIndex", "4");
               var isHomeRadioGroup = document.getElementById("homeorwork");
               isHomeRadioGroup.selectedItem = document.getElementById("workRadio");
               var forwardButton = document.getElementById("forward");
               forwardButton.removeAttribute("disabled");
               var warning = document.getElementById("warning");
               var textStr = "   " + path ;
               warning.setAttribute ('value', textStr) ;
               return false;
  }

  var map = addInterface.GetData( "fieldMap");
  if (map != null) {
    map = map.QueryInterface( Components.interfaces.nsIImportFieldMap);
    if (map != null) {
      var result = new Object();
      result.ok = false;
      top.window.openDialog(
        "chrome://messenger/content/fieldMapImport.xul",
        "",
        "chrome,modal,titlebar",
        {fieldMap: map,
         addInterface: addInterface,
         result: result});
    }
    if (result.ok == false)
      return( false);
  }

  if (addInterface.WantsProgress()) {
    if (addInterface.BeginImport( success, error, selLocIsHome)) {   	
      top.progressInfo.importInterface = addInterface;
      // top.intervalState = setInterval( "ContinueImport()", 100);
      return( true);
    }
    else {
      return( false);
    }
  }
  else {
    if (addInterface.BeginImport( success, error, selLocIsHome)) {	
      return( true);
    }
    else {
      return( false);
    }
  }
}

function SwitchType( newType)
{
  top.importType = newType;
  top.progressInfo.importType = newType;

  SetUpImportType();

  SetDivText('description', "");
}


function next()
{
  var deck = document.getElementById("stateDeck");
  var index = deck.getAttribute("selectedIndex");
  switch (index) {
  case "0":
    var backButton = document.getElementById("back");
    backButton.removeAttribute("disabled");
    var radioGroup = document.getElementById("importFields");
    SwitchType(radioGroup.value);
    deck.setAttribute("selectedIndex", "1");
    enableAdvance();
    break;
  case "1":
    ImportDialogOKButton();
    break;
  case "3":
    close();
    break;
  case "4" :
    var isHomeRadioGroup = document.getElementById("homeorwork");
    if (isHomeRadioGroup.selectedItem.getAttribute("value") == "Home")
               selLocIsHome = true ;
       ExportComm4x() ;
       break ;
  }
}

function ExportComm4x()
{
  var result ;
  if (addInterface.WantsProgress())
  {
    result = addInterface.BeginImport( successStr, errorStr, selLocIsHome) ;
       top.progressInfo.importInterface = addInterface;
       ShowResults(true, result) ;
  }
  else
  {
    result = addInterface.BeginImport( successStr, errorStr, selLocIsHome) ;
       ShowResults(false, result) ;
  }

  return true ;
}

function enableAdvance()
{
  var tree = document.getElementById("moduleList");
  var nextButton = document.getElementById("forward");
  if (tree.selectedItems.length)
    nextButton.removeAttribute("disabled");
  else
    nextButton.setAttribute("disabled", "true");
}

function back()
{
  var deck = document.getElementById("stateDeck");
  if (deck.getAttribute("selectedIndex") == "1") {
    var backButton = document.getElementById("back");
    backButton.setAttribute("disabled", "true");
    var nextButton = document.getElementById("forward");
    nextButton.label = nextButton.getAttribute("nextval");
    nextButton.removeAttribute("disabled");
    deck.setAttribute("selectedIndex", "0");
  }
}
