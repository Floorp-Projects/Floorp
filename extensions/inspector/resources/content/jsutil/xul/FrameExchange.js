/*
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

/***************************************************************
* FrameExchange ----------------------------------------------
*  Utility for passing specific data to a document loaded
*  through an iframe
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// implementation ////////////////////

var FrameExchange = {
  mData: {},

  loadURL: function(aFrame, aURL, aData)
  {
    this.mData[aURL] = aData;
    aFrame.setAttribute("src", aURL);
  },

  receiveData: function(aWindow)
  {
    var id = aWindow.location.href;
    var data = this.mData[id];
    this.mData[id] = null;

    return data;
  }
};


