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
 
const kMenuHeight = 130;
const kPropertiesWidth = 170;
const kProjectWidth = 170;

var vxShell = 
{
  mFocusObserver: null,
  mFocusedWindow: null,

  startup: function ()
  {
    _dd("vx_Startup");
    
    // Shell Startup
    // This requires several steps. Probably more, in time.
  
    // position and initialise the application window
    vxShell.initAppWindow(); 
    
    // load the property inspector
    // loadPropertyInspector();
    
    // load document inspector window
    // loadDocumentInspector();
  
    // load the palette
    vxShell.loadPalette();
    
    // load the history window
    // vxShell.loadHistory();
    
    // load a blank form (until projects come online)
    vxShell.focusedWindow = vxShell.loadDocument(null);
  
    // initialise the document focus observer
    const kObserverServiceCONTRACTID = "@mozilla.org/observer-service;1";
    const kObserverServiceIID = "nsIObserverService";
    vxShell.mFocusObserver = nsJSComponentManager.getService(kObserverServiceCONTRACTID, kObserverServiceIID);
    
  },
  
  shutdown: function ()
  {
    _dd("vx_Shutdown");
  },
  
  initAppWindow: function ()
  {
    // size the window
    moveTo(0,0);
    outerWidth = screen.availWidth;
    outerHeight = kMenuHeight;
  },
  
  loadPalette: function ()
  {
    // open the palette window
    const features = "resizable=no,dependent,chrome,dialog=yes";
    var hPalette = openDialog("chrome://vixen/content/palette/vxPalette.xul", "", features);

    // size the palette window
    hPalette.moveTo(0, kMenuHeight + 5);
    hPalette.outerWidth = kPropertiesWidth;
    hPalette.outerHeight = screen.availHeight - kMenuHeight - 60;
  },
  
  loadHistory: function ()
  {
    // open the history window
    const features = "resizable=no,dependent,chrome,dialog=yes";
    var hHistory = openDialog("chrome://vixen/content/history/history.xul", "", features);
    
    // size the history window
    hHistory.moveTo(screen.availWidth - kPropertiesWidth - 10, kMenuHeight + 5);
    hHistory.outerWidth = kPropertiesWidth;
    hHistory.outerHeight = screen.availHeight - kMenuHeight - 60;
  },
  
  loadDocument: function (aURL)
  {
    _dd("vx_LoadDocument");
    // open a blank form designer window
    const features = "resizable=yes,dependent,chrome,dialog=yes";
    var params = {
      documentURL: aURL
    };
    var hVFD = window.openDialog("chrome://vixen/content/vfd/vfd.xul", "", features, params);
    hVFD.moveTo(kPropertiesWidth + 5, kMenuHeight + 5);
    hVFD.outerWidth = screen.availWidth - kPropertiesWidth - kProjectWidth - 10;
    hVFD.outerHeight = screen.availHeight - kMenuHeight - 20;
    return hVFD;
  },
  
  loadDocumentWithUI: function ()
  {
    const kFilePickerIID = "nsIFilePicker";
    const kFilePickerCONTRACTID = "@mozilla.org/filepicker;1";
    var filePicker = nsJSComponentManager.createInstance(kFilePickerCONTRACTID, kFilePickerIID);
    if (filePicker) {
      const FP = Components.interfaces.nsIFilePicker;
      filePicker.init(window, "Open", FP.modeOpen);
      // XUL VFDs
      filePicker.appendFilter("XUL Files", "*.xul");
      // XXX-TODO: add filters for other types, e.g. string tables
      
      var filePicked = filePicker.show();
      if (filePicked == FP.returnOK && filePicker.file) {
        var file = filePicker.file.QueryInterface(Components.interfaces.nsILocalFile);
        return this.loadDocument(file.path);
      }
    }
    return null;
  },

  /** 
   * Edit menu functions
   */
  undo: function ()
  {
    this.mFocusedWindow.vxVFD.mTxMgrShell.undoTransaction();
  },
   
  redo: function ()
  {
    this.mFocusedWindow.vxVFD.mTxMgrShell.redoTransaction();
  },


  appAbout: function ()
  {
    // XXX TEMP
    alert("ViXEn - The Visual XUL Environment.\nhttp://www.mozilla.org/projects/vixen/");
  }  
};

function XXX_dumpDOM()
{
  var focusedWindow = vxShell.mFocusedWindow;
  var insertionPoint = focusedWindow.vxVFD.getInsertionPoint();
  dumpDOM(insertionPoint.parent);
}