/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService().QueryInterface(Components.interfaces.nsIDragService);

var abDirTreeObserver = {
    canDrop: function(index, orientation)
    {
      if (orientation != Components.interfaces.nsITreeView.DROP_ON)
        return false;

      var targetResource = dirTree.builderView.getResourceAtIndex(index);
      var targetURI = targetResource.Value;

      var srcURI = GetSelectedDirectory();

      if (targetURI == srcURI)
        return false;

      // determine if we dragging from a mailing list on a directory x to the parent (directory x).
      // if so, don't allow the drop
      var result = srcURI.split(targetURI);
      if (result != srcURI) 
        return false;

      // check if we can write to the target directory 
      // LDAP is readonly
      var targetDirectory = GetDirectoryFromURI(targetURI);
      return (targetDirectory.isMailList ||
              (targetDirectory.operations & targetDirectory.opWrite));
    },

    onDrop: function(row, orientation)
    {
      var dragSession = dragService.getCurrentSession();
      if (!dragSession)
        return;
      
      var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
      trans.addDataFlavor("moz/abcard");

      for (var i = 0; i < dragSession.numDropItems; i++) {
        dragSession.getData(trans, i);
        var dataObj = new Object();
        var flavor = new Object();
        var len = new Object();
        trans.getAnyTransferData(flavor, dataObj, len);
        if (dataObj)
          dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
        else
          continue;
        
        var transData = dataObj.data.split("\n");
        var rows = transData[0].split(",");
      var numrows = rows.length;

        var targetResource = dirTree.builderView.getResourceAtIndex(row);
        var targetURI = targetResource.Value;

        var srcURI = GetSelectedDirectory();

        if (srcURI == targetURI)
          return; // should not be here

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

        var abView = GetAbView();
        var directory = GetDirectoryFromURI(targetURI);

      for (var j = 0; j < numrows; j++) {
        var card = abView.getCardFromRow(rows[j]);
        directory.dropCard(card, needToCopyCard);
      }

        var cardsCopiedText = numrows == 1 ? gAddressBookBundle.getString("cardCopied")
                                           : gAddressBookBundle.getFormattedString("cardsCopied", [numrows]);

        var statusText = document.getElementById("statusText");
        statusText.setAttribute("label", cardsCopiedText);        
      }
    },

    onToggleOpenState: function()
    {
    },

    onCycleHeader: function(colID, elt)
    {
    },
      
    onCycleCell: function(row, colID)
    {
    },
      
    onSelectionChanged: function()
    {
    },

    onPerformAction: function(action)
    {
    },

    onPerformActionOnRow: function(action, row)
    {
    },

    onPerformActionOnCell: function(action, row, colID)
  {
  }
}
