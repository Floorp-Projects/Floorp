/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMXULTreeElement_h__
#define nsIDOMXULTreeElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"

class nsIDOMXULElement;
class nsIDOMNodeList;

#define NS_IDOMXULTREEELEMENT_IID \
 { 0xa6cf90ec, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} } 

class nsIDOMXULTreeElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULTREEELEMENT_IID; return iid; }

  NS_IMETHOD    GetSelectedItems(nsIDOMNodeList** aSelectedItems)=0;

  NS_IMETHOD    GetSelectedCells(nsIDOMNodeList** aSelectedCells)=0;

  NS_IMETHOD    SelectItem(nsIDOMXULElement* aTreeItem)=0;

  NS_IMETHOD    SelectCell(nsIDOMXULElement* aTreeCell)=0;

  NS_IMETHOD    ClearSelection()=0;

  NS_IMETHOD    AddItemToSelection(nsIDOMXULElement* aTreeItem)=0;

  NS_IMETHOD    RemoveItemFromSelection(nsIDOMXULElement* aTreeItem)=0;

  NS_IMETHOD    AddCellToSelection(nsIDOMXULElement* aTreeCell)=0;

  NS_IMETHOD    RemoveCellFromSelection(nsIDOMXULElement* aTreeCell)=0;

  NS_IMETHOD    SelectItemRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem)=0;

  NS_IMETHOD    SelectCellRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem)=0;

  NS_IMETHOD    SelectAll()=0;

  NS_IMETHOD    InvertSelection()=0;
};


#define NS_DECL_IDOMXULTREEELEMENT   \
  NS_IMETHOD    GetSelectedItems(nsIDOMNodeList** aSelectedItems);  \
  NS_IMETHOD    GetSelectedCells(nsIDOMNodeList** aSelectedCells);  \
  NS_IMETHOD    SelectItem(nsIDOMXULElement* aTreeItem);  \
  NS_IMETHOD    SelectCell(nsIDOMXULElement* aTreeCell);  \
  NS_IMETHOD    ClearSelection();  \
  NS_IMETHOD    AddItemToSelection(nsIDOMXULElement* aTreeItem);  \
  NS_IMETHOD    RemoveItemFromSelection(nsIDOMXULElement* aTreeItem);  \
  NS_IMETHOD    AddCellToSelection(nsIDOMXULElement* aTreeCell);  \
  NS_IMETHOD    RemoveCellFromSelection(nsIDOMXULElement* aTreeCell);  \
  NS_IMETHOD    SelectItemRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem);  \
  NS_IMETHOD    SelectCellRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem);  \
  NS_IMETHOD    SelectAll();  \
  NS_IMETHOD    InvertSelection();  \



#define NS_FORWARD_IDOMXULTREEELEMENT(_to)  \
  NS_IMETHOD    GetSelectedItems(nsIDOMNodeList** aSelectedItems) { return _to GetSelectedItems(aSelectedItems); } \
  NS_IMETHOD    GetSelectedCells(nsIDOMNodeList** aSelectedCells) { return _to GetSelectedCells(aSelectedCells); } \
  NS_IMETHOD    SelectItem(nsIDOMXULElement* aTreeItem) { return _to SelectItem(aTreeItem); }  \
  NS_IMETHOD    SelectCell(nsIDOMXULElement* aTreeCell) { return _to SelectCell(aTreeCell); }  \
  NS_IMETHOD    ClearSelection() { return _to ClearSelection(); }  \
  NS_IMETHOD    AddItemToSelection(nsIDOMXULElement* aTreeItem) { return _to AddItemToSelection(aTreeItem); }  \
  NS_IMETHOD    RemoveItemFromSelection(nsIDOMXULElement* aTreeItem) { return _to RemoveItemFromSelection(aTreeItem); }  \
  NS_IMETHOD    AddCellToSelection(nsIDOMXULElement* aTreeCell) { return _to AddCellToSelection(aTreeCell); }  \
  NS_IMETHOD    RemoveCellFromSelection(nsIDOMXULElement* aTreeCell) { return _to RemoveCellFromSelection(aTreeCell); }  \
  NS_IMETHOD    SelectItemRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem) { return _to SelectItemRange(aStartItem, aEndItem); }  \
  NS_IMETHOD    SelectCellRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem) { return _to SelectCellRange(aStartItem, aEndItem); }  \
  NS_IMETHOD    SelectAll() { return _to SelectAll(); }  \
  NS_IMETHOD    InvertSelection() { return _to InvertSelection(); }  \


extern "C" NS_DOM nsresult NS_InitXULTreeElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULTreeElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULTreeElement_h__
