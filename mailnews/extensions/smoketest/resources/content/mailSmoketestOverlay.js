/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

function StartSmoketestTimers() 
{
  dump("Inside StartSmoketestTimers \n\n\n");
  setTimeout("SelectInboxFolderMessage();",10000); //at 10 seconds, load the 1st message in the Inbox
  setTimeout("MsgReplySender();",15000); //at 15 seconds, reply to the message
  setTimeout("SendMessageNow();",25000); //at 25 seconds, send the message
  setTimeout("window.close();",35000); //at 35 seconds, close the window
}

function SelectInboxFolderMessage()
{
  var folderURI = window.parent.GetSelectedFolderURI();
  var server = GetServer(folderURI);
  try {
    OpenInboxForServer(server); //open the Inbox
  }
  catch(ex) {
    dump("Error -> " + ex + "\n");
  } 
  MsgNextMessage(); //select and load the next message in the Inbox
}

function SendMessageNow()
{
  var cwindowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  var iwindowManager = Components.interfaces.nsIWindowMediator;           
  var windowManager  = cwindowManager.QueryInterface(iwindowManager);     
  var composeWindow = windowManager.getMostRecentWindow('msgcompose'); //find the open msgcompose window and get it      
  composeWindow.SendMessage(); //send the message that we've invoked a Reply To window for
}

addEventListener("load",StartSmoketestTimers,false);
