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

#ifndef nsITextServicesDocument_h__
#define nsITextServicesDocument_h__

#include "nsISupports.h"

class nsIDOMDocument;
class nsIPresShell;
class nsIEditor;
class nsString;

/*
TextServicesDocument interface to outside world
*/

#define NS_ITEXTSERVICESDOCUMENT_IID            \
{ /* 019718E1-CDB5-11d2-8D3C-000000000000 */    \
0x019718e1, 0xcdb5, 0x11d2,                     \
{ 0x8d, 0x3c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


/**
 * A text services specific interface. 
 * <P>
 * It's implemented by an object that executes some behavior that must be
 * tracked by the transaction manager.
 */
class nsITextServicesDocument  : public nsISupports{
public:

  static const nsIID& GetIID() { static nsIID iid = NS_ITEXTSERVICESDOCUMENT_IID; return iid; }

  /**
   *
   */
  NS_IMETHOD Init(nsIDOMDocument *aDOMDocument, nsIPresShell *aPresShell) = 0;

  /**
   *
   */
  NS_IMETHOD SetEditor(nsIEditor *aEditor) = 0;

  /**
   *
   */

  NS_IMETHOD GetCurrentTextBlock(nsString *aStr) = 0;

  /**
   *
   */

  NS_IMETHOD FirstBlock() = 0;

  /**
   *
   */

  NS_IMETHOD LastBlock() = 0;

  /**
   *
   */

  NS_IMETHOD PrevBlock() = 0;

  /**
   *
   */

  NS_IMETHOD NextBlock() = 0;

  /**
   *
   */

  NS_IMETHOD IsDone() = 0;

  /**
   *
   */

  NS_IMETHOD SetSelection(PRInt32 aOffset, PRInt32 aLength) = 0;

  /**
   *
   */

  NS_IMETHOD DeleteSelection() = 0;

  /**
   *
   */

  NS_IMETHOD InsertText(const nsString *aText) = 0;
};

#endif // nsITextServicesDocument_h__

