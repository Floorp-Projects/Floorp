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
 
/** 
 * This is a non-extensible palette. It will do for now.
 */
var vxPalette = 
{
  // we want to use controllers, but this will do to test our txns.
  insertButtonElement: function (aType) 
  {
    _dd("insertButtonElement");
    var vixenMain = vxUtils.getWindow("vixen:main");
    var focusedWindow = vixenMain.vxShell.mFocusedWindow;
    var insertionPoint = focusedWindow.vxVFD.getInsertionPoint();
    
    var vfdDocument = focusedWindow.vxVFD.getContent(true).document;
    _dd(vfdDocument);
    var buttontxn = new vxCreateElementTxn(vfdDocument, "button", insertionPoint.parent, insertionPoint.index);
    var txShell = focusedWindow.vxVFD.mTxMgrShell.doTransaction(buttontxn);
  }
};

_dd("read vxPalette.js");

