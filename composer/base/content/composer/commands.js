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

const kBASE_COMMAND_CONTROLLER_CID = "@mozilla.org/embedcomp/base-command-controller;1";

const nsIControllerContext = interfaces.nsIControllerContext;
const nsIInterfaceRequestor = interfaces.nsIInterfaceRequestor;
const nsIControllerCommandTable = interfaces.nsIControllerCommandTable;

var ComposerCommands = {

  mComposerJSCommandControllerID: null,
  mSelectionTimeOutId: null,

  getComposerCommandTable: function getComposerCommandTable()
  {
    var controller;
    if (this.mComposerJSCommandControllerID)
    {
      try { 
        controller = window.content.controllers.getControllerById(this.mComposerJSCommandControllerID);
      } catch (e) {}
    }
    if (!controller)
    {
      //create it
      controller = Components.classes[kBASE_COMMAND_CONTROLLER_CID].createInstance();
  
      var editorController = controller.QueryInterface(nsIControllerContext);
      editorController.init(null);
      editorController.setCommandContext(null);
      window.controllers.insertControllerAt(0, controller);
    
      // Store the controller ID so we can be sure to get the right one later
      this.mComposerJSCommandControllerID = window.controllers.getControllerId(controller);
    }
  
    if (controller)
    {
      var interfaceRequestor = controller.QueryInterface(nsIInterfaceRequestor);
      return interfaceRequestor.getInterface(nsIControllerCommandTable);
    }
    return null;
  },

  goUpdateComposerMenuItems: function goUpdateComposerMenuItems(commandset)
  {
    dump("Updating commands for " + commandset.id + "\n");
    for (var i = 0; i < commandset.childNodes.length; i++)
    {
      var commandNode = commandset.childNodes[i];
      var commandID = commandNode.id;
      if (commandID)
      {
        goUpdateCommand(commandID);  // enable or disable
        if (commandNode.hasAttribute("state"))
          this.goUpdateCommandState(commandID);
      }
    }
  },

  goUpdateCommandState: function goUpdateCommandState(command)
  {
    try
    {
      var controller = top.document.commandDispatcher.getControllerForCommand(command);
      if (!(controller instanceof Components.interfaces.nsICommandController))
        return;

      var params = this.newCommandParams();
      if (!params) return;

      controller.getCommandStateWithParams(command, params);

      switch (command)
      {
        case "cmd_bold":
        case "cmd_italic":
        case "cmd_underline":
        case "cmd_strong":
        case "cmd_em":

        case "cmd_ul":
        case "cmd_ol":
          this.pokeStyleUI(command, params.getBooleanValue("state_all"));
          break;

        case "cmd_paragraphState":
          this.pokeMultiStateUI(command, params);
          break;

        default: dump("no update for command: " +command+"\n");
      }
    }
    catch (e) { dump("An error occurred updating the "+command+" command: \n"+e+"\n"); }
  },

  pokeStyleUI: function pokeStyleUI(uiID, aDesiredState)
  {
   try {
    var commandNode = top.document.getElementById(uiID);
    if (!commandNode)
      return;

    var uiState = ("true" == commandNode.getAttribute("state"));
    if (aDesiredState != uiState)
    {
      var newState;
      if (aDesiredState)
        newState = "true";
      else
        newState = "false";
      commandNode.setAttribute("state", newState);
    }
   } catch(e) { dump("poking UI for "+uiID+" failed: "+e+"\n"); }
  },

  newCommandParams: function newCommandParams()
  {
    try {
      return Components.classes["@mozilla.org/embedcomp/command-params;1"].createInstance(Components.interfaces.nsICommandParams);
    }
    catch(e) { dump("error thrown in newCommandParams: "+e+"\n"); }
    return null;
  },

  pokeMultiStateUI: function pokeMultiStateUI(uiID, cmdParams)
  {
    try
    {
      var commandNode = document.getElementById(uiID);
      if (!commandNode)
        return;

      var isMixed = cmdParams.getBooleanValue("state_mixed");
      var desiredAttrib;
      if (isMixed)
        desiredAttrib = "mixed";
      else
        desiredAttrib = cmdParams.getCStringValue("state_attribute");

      var uiState = commandNode.getAttribute("state");
      if (desiredAttrib != uiState)
      {
        commandNode.setAttribute("state", desiredAttrib);
      }
    } catch(e) {}
  },

  doStyleUICommand: function doStyleUICommand(cmdStr)
  {
    try
    {
      var cmdParams = this.newCommandParams();
      this.goDoCommandParams(cmdStr, cmdParams);
    } catch(e) {}
  },

  doStatefulCommand: function doStatefulCommand(commandID, newState)
  {
    var commandNode = document.getElementById(commandID);
    if (commandNode)
        commandNode.setAttribute("state", newState);

    try
    {
      var cmdParams = this.newCommandParams();
      if (!cmdParams) return;

      cmdParams.setCStringValue("state_attribute", newState);
      this.goDoCommandParams(commandID, cmdParams);

      this.pokeMultiStateUI(commandID, cmdParams);

    } catch(e) { dump("error thrown in doStatefulCommand: "+e+"\n"); }
  },

  goDoCommandParams: function goDoCommandParams(command, params)
  {
    try
    {
      var controller = top.document.commandDispatcher.getControllerForCommand(command);
      if (controller && controller.isCommandEnabled(command))
      {
        if (controller instanceof Components.interfaces.nsICommandController)
        {
          controller.doCommandWithParams(command, params);

          // the following two lines should be removed when we implement observers
          if (params)
            controller.getCommandStateWithParams(command, params);
        }
        else
        {
          controller.doCommand(command);
        }
      }
    }
    catch (e)
    {
      dump("An error occurred executing the "+command+" command\n");
    }
  },

  setupMainCommands: function setupMainCommands()
  {
    var commandTable = this.getComposerCommandTable();
    if (!commandTable)
      return;
    
    //dump("Registering plain text editor commands\n");
    commandTable.registerCommand("cmd_stopLoading", cmdStopLoading);
    commandTable.registerCommand("cmd_open",        cmdOpen);
    commandTable.registerCommand("cmd_fullScreen",  cmdFullScreen);
    commandTable.registerCommand("cmd_new",         cmdNew);
    commandTable.registerCommand("cmd_renderedHTMLEnabler", cmdDummyHTML);
  },

  setupFormatCommands: function setupFormatCommands()
  {
    try {
      var commandManager = EditorUtils.getCurrentCommandManager();

      commandManager.addCommandObserver(gEditorDocumentObserver, "obs_documentCreated");
      commandManager.addCommandObserver(gEditorDocumentObserver, "cmd_setDocumentModified");
      commandManager.addCommandObserver(gEditorDocumentObserver, "obs_documentWillBeDestroyed");
      commandManager.addCommandObserver(gEditorDocumentObserver, "obs_documentLocationChanged");

      // cmd_bold is a proxy, that's the only style command we add here
      commandManager.addCommandObserver(gEditorDocumentObserver, "cmd_bold");
    } catch (e) { alert(e); }
  },

  updateSelectionBased: function updateSelectionBased()
  {
    try {
      var mixed = EditorUtils.getSelectionContainer();
      if (!mixed) return;
      var element = mixed.node;
      var oneElementSelected = mixed.oneElementSelected;

      if (!element) return;

      if (this.mSelectionTimeOutId)
        clearTimeout(this.mSelectionTimeOutId);

      this.mSelectionTimeOutId = setTimeout(this._updateSelectionBased, 500, element, oneElementSelected);
    }
    catch(e) {}
  },

  _updateSelectionBased: function _updateSelectionBased(aElement, aOneElementSelected)
  {
    NotifierUtils.notify("selection", aElement, aOneElementSelected);
  }
};

#include navigationCommands.inc
#include fileCommands.inc
#include viewCommands.inc
#include dummyCommands.inc
