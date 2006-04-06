/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Composer.
 *
 * The Initial Developer of the Original Code is
 * Disruptive Innovations SARL.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <daniel.glazman@disruptive-innovations.com>, Original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var EditorUtils = {

  getCurrentEditorElement: function ()
  {
    var tmpWindow = window;
    
    do {
      var tabeditor = tmpWindow.document.getElementById("tabeditor");
  
      if (tabeditor)
        return tabeditor.getCurrentEditorElement() ;
  
      tmpWindow = tmpWindow.opener;
    } 
    while (tmpWindow);
  
    return null;
  },
  
  getCurrentEditor: function()
  {
    // Get the active editor from the <editor> tag
    var editor;
    try {
      var editorElement = this.getCurrentEditorElement();
      editor = editorElement.getEditor(editorElement.contentWindow);
  
      // Do QIs now so editor users won't have to figure out which interface to use
      // Using "instanceof" does the QI for us.
      editor instanceof nsIPlaintextEditor;
      editor instanceof nsIHTMLEditor;
    } catch (e) { dump("Error in GetCurrentEditor: " + e); }
  
    return editor;
  },
  
  isAlreadyEdited: function(aURL)
  {
    // blank documents are never "already edited"...
    if (IsUrlAboutBlank(aURL))
      return  null;
  
    var url = NewURI(aURL).spec;
  
    var windowManager = Components.classes[kWINDOWMEDIATOR_CID].getService();
    var windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);
    var enumerator = windowManagerInterface.getEnumerator( "composer" );
    while ( enumerator.hasMoreElements() )
    {
      var win = enumerator.getNext().QueryInterface(nsIDOMWindowInternal);
      try {
        var editor = win.document.getElementById("tabeditor").IsDocumentAlreadyEdited(url);
        if (editor)
          return {window: win, editor: editor};
      }
      catch(e) {}
    }
    return null;
  },
  
  IsDocumentEditable: function()
  {
    try {
      return this.getCurrentEditor().isDocumentEditable;
    } catch (e) {}
    return false;
  }

};