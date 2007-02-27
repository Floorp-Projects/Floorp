/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Mark Banner <mark@standard8.demon.co.uk>
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
      var selectedRows = GetSelectedRows();

      if (!selectedRows)
        return;

      var selectedAddresses = GetSelectedAddresses();

      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("moz/abcard", selectedRows);
      aXferData.data.addDataForFlavour("text/x-moz-address", selectedAddresses);
      aXferData.data.addDataForFlavour("text/unicode", selectedAddresses);

      var srcDirectory = GetDirectoryFromURI(GetSelectedDirectory());
      // The default allowable actions are copy, move and link, so we need
      // to restrict them here.
      if ((srcDirectory.operations & srcDirectory.opWrite))
        // Only allow copy & move from read-write directories.
        aDragAction.action = Components.interfaces.
                             nsIDragService.DRAGDROP_ACTION_COPY |
                             Components.interfaces.
                             nsIDragService.DRAGDROP_ACTION_MOVE;
      else
        // Only allow copy from read-only directories.
        aDragAction.action = Components.interfaces.
                             nsIDragService.DRAGDROP_ACTION_COPY;
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
  /**
   * canDrop - determine if the tree will accept the dropping of a item
   * onto it.
   *
   * Note 1: We don't allow duplicate mailing list names, therefore copy
   * is not allowed for mailing lists.
   * Note 2: Mailing lists currently really need a card in the parent
   * address book, therefore only moving to an address book is allowed.
   *
   * The possibilities:
   *
   *   anything          -> same place             = Not allowed
   *   anything          -> read only directory    = Not allowed
   *   mailing list      -> mailing list           = Not allowed
   *   (we currently do not support recursive lists)
   *   address book card -> different address book = MOVE or COPY
   *   address book card -> mailing list           = COPY only
   *   (cards currently have to exist outside list for list to work correctly)
   *   mailing list      -> different address book = MOVE only
   *   (lists currently need to have unique names)
   *   card in mailing list -> parent mailing list = Not allowed
   *   card in mailing list -> other mailing list  = MOVE or COPY
   *   card in mailing list -> other address book  = MOVE or COPY
   *   read only directory item -> anywhere        = COPY only
   */
  canDrop: function(index, orientation)
  {
    if (orientation != Components.interfaces.nsITreeView.DROP_ON)
      return false;

    var targetResource = dirTree.builderView.getResourceAtIndex(index);
    var targetURI = targetResource.Value;

    var srcURI = GetSelectedDirectory();

    // The same place case
    if (targetURI == srcURI)
      return false;

    // determine if we dragging from a mailing list on a directory x to the parent (directory x).
    // if so, don't allow the drop
    if (srcURI.substring(0, targetURI.length) == targetURI)
      return false

    // check if we can write to the target directory 
    // e.g. LDAP is readonly currently
    var targetDirectory = GetDirectoryFromURI(targetURI);

    if (!(targetDirectory.operations & targetDirectory.opWrite))
      return false;

    var dragSession = dragService.getCurrentSession();
    if (!dragSession)
      return false;

    // If target directory is a mailing list, then only allow copies.
    if (targetDirectory.isMailList &&
         dragSession.dragAction != Components.interfaces.
                                   nsIDragService.DRAGDROP_ACTION_COPY)
      return false;

    var srcDirectory = GetDirectoryFromURI(srcURI);

    // Only allow copy from read-only directories.
    if (!(srcDirectory.operations & srcDirectory.opWrite) &&
        dragSession.dragAction != Components.interfaces.
                                  nsIDragService.DRAGDROP_ACTION_COPY)
      return false;

    // Go through the cards checking to see if one of them is a mailing list
    // (if we are attempting a copy) - we can't copy mailing lists as
    // that would give us duplicate names which isn't allowed at the
    // moment.
    var draggingMailList = false;
    var abView = GetAbView();
    var trans = Components.classes["@mozilla.org/widget/transferable;1"].
                createInstance(Components.interfaces.nsITransferable);

    trans.addDataFlavor("moz/abcard");

    for (var i = 0; i < dragSession.numDropItems && !draggingMailList; i++)
    {
      dragSession.getData(trans, i);

      var dataObj = new Object();
      var flavor = new Object();
      var len = new Object();
      try {
        trans.getAnyTransferData(flavor, dataObj, len);
      }
      catch (ex) {
        dragSession.canDrop = false;
        return false;
      }

      dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);

      var transData = dataObj.data.split("\n");
      var rows = transData[0].split(",");

      for (var j = 0; j < rows.length; j++)
      {
        if (abView.getCardFromRow(rows[j]).isMailList)
        {
          draggingMailList = true;
          break;
        }
      }
    }

    // The rest of the cases - allow cards for copy or move, but only allow
    // move of mailing lists if we're not going into another mailing list.
    if (draggingMailList &&
        (targetDirectory.isMailList ||
         dragSession.dragAction == Components.interfaces.
                                   nsIDragService.DRAGDROP_ACTION_COPY))
    {
      return false;
    }

    dragSession.canDrop = true;
    return true;
  },

  /**
   * onDrop - we don't need to check again for correctness as the
   * tree view calls canDrop just before calling onDrop.
   *
   */
  onDrop: function(row, orientation)
  {
    var dragSession = dragService.getCurrentSession();
    if (!dragSession)
      return;
      
    var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
    trans.addDataFlavor("moz/abcard");

    var targetResource = dirTree.builderView.getResourceAtIndex(row);

    var targetURI = targetResource.Value;
    var srcURI = GetSelectedDirectory();

    for (var i = 0; i < dragSession.numDropItems; i++) {
      dragSession.getData(trans, i);
      var dataObj = new Object();
      var flavor = new Object();
      var len = new Object();
      try {
        trans.getAnyTransferData(flavor, dataObj, len);
        dataObj = 
          dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
      }
      catch (ex) {
        continue;
      }

      var transData = dataObj.data.split("\n");
      var rows = transData[0].split(",");
      var numrows = rows.length;

      var result;
      // needToCopyCard is used for whether or not we should be creating
      // copies of the cards in a mailing list in a different address book
      // - it's not for if we are moving or not.
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

      // Only move if we are not transferring to a mail list
      var actionIsMoving = (dragSession.dragAction & dragSession.DRAGDROP_ACTION_MOVE) && !directory.isMailList;

      for (j = 0; j < numrows; j++) {
        var card = abView.getCardFromRow(rows[j]);
        if (card.isMailList) {
          // This check ensures we haven't slipped through by mistake
          if (needToCopyCard && actionIsMoving) {
            directory.addMailList(GetDirectoryFromURI(card.mailListURI));
          }
        } else {
          directory.dropCard(card, needToCopyCard);
        }
      }

      var cardsTransferredText;

      // If we are moving, but not moving to a directory, then delete the
      // selected cards and display the appropriate text
      if (actionIsMoving) {
        // If we have moved the cards, then delete them as well.
        abView.deleteSelectedCards();
        cardsTransferredText = (numrows == 1 ? gAddressBookBundle.getString("cardMoved")
                                             : gAddressBookBundle.getFormattedString("cardsMoved", [numrows]));
      } else {
        cardsTransferredText = (numrows == 1 ? gAddressBookBundle.getString("cardCopied")
                                             : gAddressBookBundle.getFormattedString("cardsCopied", [numrows]));
      }

      document.getElementById("statusText").label = cardsTransferredText;
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

function DragAddressOverTargetControl(event)
{
  var dragSession = gDragService.getCurrentSession();

  if (!dragSession.isDataFlavorSupported("text/x-moz-address"))
     return;

  var trans = Components.classes["@mozilla.org/widget/transferable;1"]
                        .createInstance(Components.interfaces.nsITransferable);
  trans.addDataFlavor("text/x-moz-address");

  var canDrop = true;

  for ( var i = 0; i < dragSession.numDropItems; ++i )
  {
    dragSession.getData ( trans, i );
    var dataObj = new Object();
    var bestFlavor = new Object();
    var len = new Object();
    try
    {
      trans.getAnyTransferData ( bestFlavor, dataObj, len );
    }
    catch (ex)
    {
      canDrop = false;
      break;
    }
  }
  dragSession.canDrop = canDrop;
}

function DropAddressOverTargetControl(event)
{
  var dragSession = gDragService.getCurrentSession();
  
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
  trans.addDataFlavor("text/x-moz-address");

  for ( var i = 0; i < dragSession.numDropItems; ++i )
  {
    dragSession.getData ( trans, i );
    var dataObj = new Object();
    var bestFlavor = new Object();
    var len = new Object();

    // Ensure we catch any empty data that may have slipped through
    try
    {
      trans.getAnyTransferData ( bestFlavor, dataObj, len);
    }
    catch (ex)
    {
      continue;
    }
 
    if ( dataObj )  
      dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
    if ( !dataObj ) 
      continue;

    // pull the address out of the data object
    var address = dataObj.data.substring(0, len.value);
    if (!address)
      continue;

    DropRecipient(address);
  }
}
