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
* wsm-colorpicker ----------------------------------------------
* Quick script which adds support for the color picker widget
* to nsWidgetStateManager in the pref winodw.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

function AddColorPicker(aCallback)
{
  window.addEventListener("load", AddColorPicker_delayCheck, false);
  window.AddColorPicker_callback = aCallback;
}
    
function AddColorPicker_delayCheck()
{
  if (parent.hPrefWindow) 
    AddColorPicker_addColorHandlers()
  else
    setTimeout("AddColorPicker_delayCheck()", 1);
}

function AddColorPicker_addColorHandlers() 
{
  parent.hPrefWindow.wsm.handlers.colorpicker = {
    set: function (aElementID, aDataObject)
    {
      var wsm = parent.hPrefWindow.wsm;
      var element = wsm.contentArea.document.getElementById( aElementID );
      element.color = aDataObject.color;
    },
    
    get: function (aElementID)
    {
      var wsm = parent.hPrefWindow.wsm;
      var element = wsm.contentArea.document.getElementById( aElementID );
      var dataObject = wsm.generic_Get(element);
      if(dataObject) { 
        dataObject.color = element.color;
        return dataObject;
      }
      return null;
    }
  }

  window.AddColorPicker_callback();
}
