/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */

var CentralController = {
  supportsCommand: function (aCommand) 
  {
    _dd("supportscommand!");
    switch (aCommand) {
    case "cmd_quit":
    case "cmd_close":
    case "cmd_save":
    case "cmd_undo":
    case "cmd_redo":
      return true;
    default:
      return false;
    }
  },
  
  isCommandEnabled: function (aCommand)
  {
    switch (aCommand) {
    case "cmd_quit":
      return true;
    case "cmd_close":
      // see if there's a document window open
      return true;
    case "cmd_save":
      // see if the current document is dirty
      return true;
    case "cmd_undo":
      // peek at the transaction stack for the current document
      return true;
    case "cmd_redo":
      // peek at the transaction stack for the current document
      return true;
    default:
      return false;
    }
  },
  
  doCommand: function (aCommand)
  {
    _dd("doCommand!");
    switch (aCommand) {
    case "cmd_quit":
      // attempt to save changes to open documents and projects
      // kill the app
      vxUtils.shutDown();
      break;
    case "cmd_close":
      // find the most recent document
      var focusedWindow = vxUtils.getWindow("vixen:main").vxShell.mFocusedWindow;
      // attempt to save the loaded document if dirty
      this.doCommand("cmd_save");
      // close the window
      focusedWindow.close();
      break;
    case "cmd_save":
      // attempt to save the document in the active window
      break;
    case "cmd_undo":
      // the code in vxShell should eventually move into this file.
      vxUtils.getWindow("vixen:main").vxShell.undo();
      break;
    case "cmd_redo":
      // the code in vxShell should eventually move into this file.
      vxUtils.getWindow("vixen:main").vxShell.redo();
      break;
    default:
      break;
    }
  },
  
  onEvent: function (aEvent) 
  {
  }
};


 