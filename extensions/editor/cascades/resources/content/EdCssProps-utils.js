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
 * The Original Code is CaScadeS, a stylesheet editor for Composer.
 *
 * The Initial Developer of the Original Code is
 * Daniel Glazman.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original author: Daniel Glazman <daniel@glazman.org>
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

// * retrieves the index of the selected entry in a tree
//   param XULElement tree
//   return integer
function getSelectedItem(tree)
{
  if (tree.view.selection.count == 1)
    return tree.contentView.getItemAtIndex(tree.currentIndex);
  else
    return null;
}

// * retrieves the importance of the index-nth style declaration in a rule
//   param DOMCSSRule styleRule
//   param integer index
//   return String
function GetDeclarationImportance(styleRule, index)
{
  var pName = styleRule.style.item(index);
  return styleRule.style.getPropertyPriority(pName);
}

// * selects a entry in the tree
//   param XULElement aItem
function selectTreeItem(aItem)
{
  /* first make sure item's containers are open */
  if (!aItem) return;
  var tmp = aItem.parentNode;
  while (tmp && tmp.nodeName != "tree") {
    if (tmp.nodeName == "treeitem")
      tmp.setAttribute("open", "true");
    tmp = tmp.parentNode;
  }

  /* then select the item */
  var itemIndex = gDialog.sheetsTree.contentView.getIndexOfItem(aItem);
  gDialog.sheetsTree.view.selection.select(itemIndex);
  /* and make sure it is visible in the clipping area of the tree */
  gDialog.sheetsTree.treeBoxObject.ensureRowIsVisible(itemIndex);
}

// * gets a rule's index in its DOMCSSRuleList container
//   param DOMCSSRuleList rulelist
//   param DOMCSSRule rule
//   return integer
function getRuleIndexInRulesList(rulelist, rule)
{
  if (!rule || !rulelist)
    return -1;

  var i;
  for (i=0; i<rulelist.length; i++) {
    if (rulelist.item(i) == rule) {
      return i;
    }
  }
  return -1;
}

// * opens a file picker and returns a file: URL. If openOnly is true,
//   the filepicker's mode is "open"; it is "save" instead and that allows
//   to pick new filenames
//   param boolean openOnly
function getLocalFileURL(openOnly)
{
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  var fileType = "*";
  if (openOnly)
    fp.init(window, "Select a CSS file", nsIFilePicker.modeOpen);
  else
    fp.init(window, "Select a CSS file", nsIFilePicker.modeSave);
  
  // Default or last filter is "All Files"
  fp.appendFilters(nsIFilePicker.filterAll);

  // set the file picker's current directory to last-opened location saved in prefs
  SetFilePickerDirectory(fp, fileType);

  /* doesn't handle *.shtml files */
  try {
    var ret = fp.show();
    if (ret == nsIFilePicker.returnCancel)
      return null;
  }
  catch (ex) {
    dump("filePicker.chooseInputFile threw an exception\n");
    return null;
  }
  // SaveFilePickerDirectory(fp, fileType);
  
  return fp.file ? getURLSpecFromFile(fp.file) : null;
}

// blatantly stolen from Venkman
function getURLSpecFromFile (file)
{
    if (!file)
        return null;

    const IOS_CTRID = "@mozilla.org/network/io-service;1";
    const LOCALFILE_CTRID = "@mozilla.org/file/local;1";

    const nsIIOService = Components.interfaces.nsIIOService;
    const nsILocalFile = Components.interfaces.nsILocalFile;
    const nsIFileProtocolHandler = Components.interfaces.nsIFileProtocolHandler;
    
    if (typeof file == "string")
    {
        var fileObj =
            Components.classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
        fileObj.initWithPath(file);
        file = fileObj;
    }
    
    var service = Components.classes[IOS_CTRID].getService(nsIIOService);
    var fileHandler = service.getProtocolHandler("file")
                             .QueryInterface(nsIFileProtocolHandler);
    return fileHandler.getURLSpecFromFile(file);
}

// * Debug only...
function doDump(text, value) {
  dump("===> " + text + " : " + value + "\n");
}

function ClearTreeSelection(tree) {
  if (tree)
    tree.view.selection.clearSelection();
}
