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
 
var vxVFD =
{
  mParams       : null,
  mTxMgrShell   : null,
  mSelection    : null,
  
  getContent: function (asContext)
  {
    return asContext ? frames["vfdDocument"] : document.getElementById("vfdDocument");
  },
  
  startup: function ()
  {
    // window parameters
    this.mParams = window.arguments[0];
    
    // load the document
    var content = this.getContent(false);
    var docURL = this.mParams.documentURL;
    const scratch = "chrome://vixen/content/vfd/scratch.xul";
    content.setAttribute("src", docURL ? docURL : scratch);
    
    var windowNode = document.getElementById("vxVFD");
    windowNode.setAttribute("url", docURL);
    
    // initialise the transaction manager shell
    this.mTxMgrShell = new vxVFDTransactionManager();
    
    // initialise the selection manager
    this.mSelection = new vxVFDSelectionManager();
  },
  
  focusVFD: function ()
  {
    var vixenMain = vxUtils.getWindow("vixen:main");
    // for a focused vfd, send "vfd,url"
    // vixenMain.vxShell.mFocusObserver.Notify({ }, "window_focus", ("vfd," + this.mParams.documentURL));
    vixenMain.vxShell.mFocusedWindow = window;
    _ddf("focused window, according to our parent, is", vixenMain.vxShell.mFocusedWindow);
  },
  
  get vfdDocumentWindowNode()
  {
    var content = this.getContent(true);
    if (content && content.document) {
      if (content.document.hasChildNodes()) {
        var kids = content.document.childNodes;
        for (var i = 0; i < kids.length; i++) {
          if (kids[i].localName == "window") 
            return kids[i];
        }
      }
    }
    return null;
  },
  
  getInsertionPoint: function ()
  {
    if (this.mSelection && this.mSelection.selectionExists) {
      // compute the insertion point based on selection
      // XXX - TODO
    }
    else {
      // if no selection, return the window
      return { parent: this.vfdDocumentWindowNode, index: 0 };
    }
    return null;
  }
}; 

