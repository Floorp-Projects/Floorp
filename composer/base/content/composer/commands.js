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
 * The Original Code is Mozilla Composer.
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

  _composerJSCommandControllerID: null,

   getComposerCommandTable: function ()
  {
    var controller;
    if (this._composerJSCommandControllerID)
    {
      try { 
        controller = window.content.controllers.getControllerById(this._composerJSCommandControllerID);
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
      this._composerJSCommandControllerID = window.controllers.getControllerId(controller);
    }
  
    if (controller)
    {
      var interfaceRequestor = controller.QueryInterface(nsIInterfaceRequestor);
      return interfaceRequestor.getInterface(nsIControllerCommandTable);
    }
    return null;
  },

   setupMainCommands: function ()
  {
    var commandTable = this.getComposerCommandTable();
    if (!commandTable)
      return;
    
    //dump("Registering plain text editor commands\n");
    commandTable.registerCommand("cmd_stopLoading", nsStopLoadingCommand);
  },

  goUpdateComposerMenuItems: function (commandset)
  {
    //dump("Updating commands for " + commandset.id + "\n");

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

  goUpdateCommandState: function(command)
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
        default: dump("no update for command: " +command+"\n");
      }
    }
    catch (e) { dump("An error occurred updating the "+command+" command: \n"+e+"\n"); }
  },

  newCommandParams: function ()
  {
    try {
      return Components.classes["@mozilla.org/embedcomp/command-params;1"].createInstance(Components.interfaces.nsICommandParams);
    }
    catch(e) { dump("error thrown in newCommandParams: "+e+"\n"); }
    return null;
  }

};

var nsStopLoadingCommand =
{
  isCommandEnabled: function(aCommand, dummy)
  {
    var res = false;
    try {
      var tab = document.getElementById("tabeditor").selectedTab;
      if (tab)
        res = tab.hasAttribute("busy");
    }
    catch(e) {}
    return res;
  },

  getCommandStateParams: function(aCommand, aParams, aRefCon) {},
  doCommandParams: function(aCommand, aParams, aRefCon) {},

  doCommand: function(aCommand)
  {
    document.getElementById("tabeditor").stopWebNavigation();
  }
};
