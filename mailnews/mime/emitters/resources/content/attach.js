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

/*
 * Attachment Command-specific code. This stuff should be called by the widgets
 */

var msgWindowContractID		   = "@mozilla.org/messenger/msgwindow;1";
var messenger = Components.classes['@mozilla.org/messenger;1'].createInstance();
messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);
//Create message window object
//var msgWindow = Components.classes[msgWindowContractID].createInstance();
//msgWindow = msgWindow.QueryInterface(Components.interfaces.nsIMsgWindow);

function OpenAttachURL(url, displayName, messageUri)
{
  dump("\nOpenAttachURL from XUL\n");
  dump(url);
  dump("\n");
  // messenger.SetWindow(window, msgWindow);
  messenger.openAttachment(url, displayName, messageUri);
}

