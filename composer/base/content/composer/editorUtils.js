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
 * The Original Code is Composer.
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

  /********** PUBLIC **********/

  getCurrentEditorElement: function getCurrentEditorElement()
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
  
  getCurrentEditor: function getCurrentEditor()
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
  
  getCurrentCommandManager: function getCurrentCommandManager()
  {
    try {
      return this.getCurrentEditorElement().commandManager;
    } catch (e) { dump (e)+"\n"; }

    return null;
  },
  
  newCommandParams: function newCommandParams()
  {
    try {
      const contractId = "@mozilla.org/embedcomp/command-params;1";
      const nsICommandParams = Components.interfaces.nsICommandParams;

      return Components.classes[contractId].createInstance(nsICommandParams);
    }
    catch(e) { dump("error thrown in newCommandParams: "+e+"\n"); }
    return null;
  },

  getCurrentEditingSession: function getCurrentEditingSession()
  {
    try {
      return this.getCurrentEditorElement().editingSession;
    } catch (e) { dump (e)+"\n"; }

    return null;
  },

  getCurrentEditorType: function getCurrentEditorType()
  {
    try {
      return this.getCurrentEditorElement().editortype;
    } catch (e) { dump (e)+"\n"; }

    return "";
  },

  isAlreadyEdited: function isAlreadyEdited(aURL)
  {
    // blank documents are never "already edited"...
    if (UrlUtils.isUrlAboutBlank(aURL))
      return null;
  
    var url = UrlUtils.newURI(aURL).spec;
  
    var windowManager = Components.classes[kWINDOWMEDIATOR_CID].getService();
    var windowManagerInterface = windowManager.QueryInterface(nsIWindowMediator);
    var enumerator = windowManagerInterface.getEnumerator( "composer" );
    while ( enumerator.hasMoreElements() )
    {
      var win = enumerator.getNext().QueryInterface(nsIDOMWindowInternal);
      try {
        var mixed = win.gDialog.tabeditor.isAlreadyEdited(url);
        if (mixed)
          return {window: win, editor: mixed.editor, index: mixed.index};
      }
      catch(e) {}
    }
    return null;
  },

  isDocumentEditable: function isDocumentEditable()
  {
    try {
      return this.getCurrentEditor().isDocumentEditable;
    } catch (e) { dump (e)+"\n"; }
    return false;
  },

  isDocumentModified: function isDocumentModified()
  {
    try {
      return this.getCurrentEditor().documentModified;
    } catch (e) { dump (e)+"\n"; }
    return false;
  },

  isDocumentEmpty: function isDocumentEmpty()
  {
    try {
      return this.getCurrentEditor().documentIsEmpty;
    } catch (e) { dump (e)+"\n"; }
    return false;
  },

  getDocumentTitle: function getDocumentTitle()
  {
    try {
      return this.getCurrentEditor().document.title;
    } catch (e) { dump (e)+"\n"; }

    return "";
  },

  setDocumentTitle: function setDocumentTitle(title)
  {
    try {
      this.getCurrentEditor().setDocumentTitle(title);

      // Update window title (doesn't work if called from a dialog)
      if (typeof window.UpdateWindowTitle == "function")
        window.UpdateWindowTitle();
    } catch (e) { dump (e)+"\n"; }
  },

  getSelectionContainer: function getSelectionContainer()
  {
    var editor = this.getCurrentEditor();
    if (!editor) return null;

    try {
      var selection = editor.selection;
      if (!selection) return null;
    }
    catch (e) { return null; }

    var result = { oneElementSelected:false };

    if (selection.isCollapsed) {
      result.node = selection.focusNode;
    }
    else {
      var rangeCount = selection.rangeCount;
      if (rangeCount == 1) {
        result.node = editor.getSelectedElement("");
        var range = selection.getRangeAt(0);

        // check for a weird case : when we select a piece of text inside
        // a text node and apply an inline style to it, the selection starts
        // at the end of the text node preceding the style and ends after the
        // last char of the style. Assume the style element is selected for
        // user's pleasure
        if (!result.node &&
            range.startContainer.nodeType == Node.TEXT_NODE &&
            range.startOffset == range.startContainer.length &&
            range.endContainer.nodeType == Node.TEXT_NODE &&
            range.endOffset == range.endContainer.length &&
            range.endContainer.nextSibling == null &&
            range.startContainer.nextSibling == range.endContainer.parentNode)
          result.node = range.endContainer.parentNode;

        if (!result.node) {
          // let's rely on the common ancestor of the selection
          result.node = range.commonAncestorContainer;
        }
        else {
          result.oneElementSelected = true;
        }
      }
      else {
        // assume table cells !
        var i, container = null;
        for (i = 0; i < rangeCount; i++) {
          range = selection.getRangeAt(i);
          if (!container) {
            container = range.startContainer;
          }
          else if (container != range.startContainer) {
            // all table cells don't belong to same row so let's
            // select the parent of all rows
            result.node = container.parentNode;
            break;
          }
          result.node = container;
        }
      }
    }

    // make sure we have an element here
    while (result.node.nodeType != Node.ELEMENT_NODE)
      result.node = result.node.parentNode;

    // and make sure the element is not a special editor node like
    // the <br> we insert in blank lines
    // and don't select anonymous content !!! (fix for bug 190279)
    while (result.node.hasAttribute("_moz_editor_bogus_node") ||
           editor.isAnonymousElement(result.node))
      result.node = result.node.parentNode;

    return result;
  }

};
