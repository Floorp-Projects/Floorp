/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef RDFGLOBAL_H
#define RDFGLOBAL_H

#include "htrdf.h"

// Translates drop actions from HT into Windows drop actions
DROPEFFECT RDFGLOBAL_TranslateDropAction(HT_Resource dropTarget, COleDataObject* pDataObject, 
										 int position);

// Translate drop position (before, after, on, or parent)
enum DropPosition { DROP_BEFORE, DROP_AFTER, DROP_ON, DROP_ON_PARENT };
DropPosition RDFGLOBAL_TranslateDropPosition(HT_Resource dropTarget, int position);
void RDFGLOBAL_PerformDrop(COleDataObject* pDataObject, HT_Resource theNode, int dragFraction);

// More stuff for all HT views. Ability to drag selected items in a view.
void RDFGLOBAL_BeginDrag(COleDataSource* pDataSource, HT_View pView);

// Used all over the product to drag a title/url.
void RDFGLOBAL_DragTitleAndURL( COleDataSource *pDataSource, LPCSTR title, LPCSTR url );

#endif // RDFGLOBAL_H
