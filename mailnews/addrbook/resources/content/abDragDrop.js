/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var abResultsPaneObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
{
      aXferData.data = new TransferData();
      var selectedRows = GetSelectedRows();
      var selectedAddresses = GetSelectedAddresses();

      aXferData.data.addDataForFlavour("moz/abcard", selectedRows);
      aXferData.data.addDataForFlavour("text/x-moz-address", selectedAddresses);
    },

  onDrop: function (aEvent, aXferData, aDragSession)
    {
    },

  onDragExit: function (aEvent, aDragSession)
    {
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
    },

  getSupportedFlavours: function ()
	{
     return null;
  }
};

var abDirTreeObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
    },

  onDrop: function (aEvent, aXferData, aDragSession)
    {
	    var xferData = aXferData.data.split("\n");

      var row = {}, col = {}, obj = {};
      dirTree.treeBoxObject.getCellAt(aEvent.clientX, aEvent.clientY, row, col, obj);
      if (row.value >= dirTree.view.rowCount || row.value < 0) return;
      
      var item = dirTree.contentView.getItemAtIndex(row.value);
      var targetURI = item.id;
      var directory = GetDirectoryFromURI(targetURI);

      var abView = GetAbView();
        
      var rows = xferData[0].split(",");
      var numrows = rows.length;
      var srcURI = GetAbViewURI();

      if (srcURI == targetURI) {
        // should not be here
        return;
      }

      var result;
      var needToCopyCard = true;
      if (srcURI.length > targetURI.length) {
        result = srcURI.split(targetURI);
        if (result[0] != srcURI) {
          // src directory is a mailing list on target directory, no need to copy card
          needToCopyCard = false;
        }
      }
      else {
        result = targetURI.split(srcURI);
        if (result[0] != targetURI) {
          // target directory is a mailing list on src directory, no need to copy card
          needToCopyCard = false;
        }
	    }

      // if we still think we have to copy the card,
      // check if srcURI and targetURI are mailing lists on same directory
      // if so, we don't have to copy the card
      if (needToCopyCard) {
        var targetParentURI = GetParentDirectoryFromMailingListURI(targetURI);
        if (targetParentURI && (targetParentURI == GetParentDirectoryFromMailingListURI(srcURI)))
          needToCopyCard = false;
      }

      for (var i=0;i<numrows;i++) {
        var card = abView.getCardFromRow(rows[i]);
        directory.dropCard(card, needToCopyCard);
      }
      var statusText = document.getElementById("statusText");
      // XXX get this approved, move it to a string bundle
      statusText.setAttribute("label", i + " Card(s) Copied");
    },

  onDragExit: function (aEvent, aDragSession)
    {
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      if (aEvent.target.localName != "treechildren") {
         aDragSession.canDrop = false;
        return false;
      }
      
      var row = {}, col = {}, obj = {};
      dirTree.view.treeBoxObject.getCellAt(aEvent.clientX, aEvent.clientY, row, col, obj);
      if (row.value >= dirTree.view.rowCount || row.value < 0) return;
      
      var item = dirTree.contentView.getItemAtIndex(row.value);
      var targetURI = item.id;
      var srcURI = GetAbViewURI();

      // you can't drop a card onto the directory it comes from
      if (targetURI == srcURI) {
        aDragSession.canDrop = false;
        return false;
      }

      // determine if we dragging from a mailing list on a directory x to the parent (directory x).
      // if so, don't allow the drop
      var result = srcURI.split(targetURI);
      if (result != srcURI) {
        aDragSession.canDrop = false;
        return false;
      }
      return true;
    },

  getSupportedFlavours: function ()
  {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("moz/abcard");
      return flavourSet;
  }
};

