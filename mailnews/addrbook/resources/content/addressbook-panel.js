/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Seth Spitzer <sspitzer@netscape.com>
 */

function GetAbViewListener()
{
  // the ab panel doesn't care if the total changes, or if the selection changes
  return null;
}

function abPanelLoad() 
{
  InitCommonJS(); 

  var selectedAB = "moz-abmdbdirectory://abook.mab";

  // XXX duplicated code, see OnLoadMailList()
  // set popup with address book names
  var abPopup = document.getElementById('addressbookList');
  if ( abPopup )
  {
    var menupopup = document.getElementById('addressbookList-menupopup');

    if ( selectedAB && menupopup && menupopup.childNodes )
    {
      for ( var index = menupopup.childNodes.length - 1; index >= 0; index-- )
      {
        if ( menupopup.childNodes[index].getAttribute('value') == selectedAB )
        {
          abPopup.label = menupopup.childNodes[index].getAttribute('label');
          abPopup.value = menupopup.childNodes[index].getAttribute('value');
          break;
        }
      }
    }
  }

  ChangeDirectoryByDOMNode(abPopup.selectedItem);
}

function abPanelUnload()
{
  CloseAbView();
}

function AbPanelNewCard() 
{
  var abList = document.getElementById('addressbookList');
  goNewCardDialog(abList.selectedItem.getAttribute('id'));
}

function AbPanelNewList() 
{
  var abList = document.getElementById('addressbookList');
  goNewListDialog(abList.selectedItem.getAttribute('id'));
}

function ResultsPaneSelectionChanged() 
{
  // do nothing for ab panel
}

function OnClickedCard() 
{
  // do nothing for ab panel
}

function AbResultsPaneDoubleClick(card) 
{
  // double click for ab panel means "send mail to this person / list"
  AbNewMessage();
}

function UpdateCardView() 
{
  // do nothing for ab panel
}

