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
