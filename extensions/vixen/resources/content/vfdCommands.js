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
 * Original Author: 
 *   Ben Goodger <ben@netscape.com>
 *
 * Contributor(s): 
 */
 
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
var numButtons = 0;

var nsVFD = {

  insertButtonElement: function (aButtonElementType) 
  {
    switch (aButtonElementType) {
    case "button":
      var button = document.createElementNS(XUL_NS, "button");
      numButtons++;
      button.setAttribute("value", "Button"+numButtons);
      button.setAttribute("flex", "1");
      button.setAttribute("crop", "right");

      this.genericInsertElement(button);
      break;
    case "toolbar-button":
      break;
    case "menu-button":
      break;
    }
  },

  genericInsertElement: function (aElement) 
  {
    var domDocument = getDocument();
    
    // get the focused element so we know where to insert
    
    // otherwise, just use the window
    var scratchWindow = getDocumentWindow(domDocument);
    
    scratchWindow.appendChild(aElement);
  },
};

function getDocumentWindow(aDocument)
{ 
  if (!aDocument) 
    aDocument = getDocument();
  
  for (var i = 0; i < aDocument.childNodes.length; i++)
    if (aDocument.childNodes[i].localName == "window")
      return aDocument.childNodes[i];
  return null;
}

function getDocument()
{ 
  const WM_PROGID = "component://netscape/rdf/datasource?name=window-mediator";
  var wm = nsJSComponentManager.getService(WM_PROGID, "nsIWindowMediator");
  return wm.getMostRecentWindow("xuledit:document").frames["vfView"].document;
}