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


/*
 * nsIDOMSelectionListener: an interface to be implemented by classes
 * that want to get notifications when selection changes.
 *
 * This interface is meant to be IDL'ed and XPCOMified eventually.
 */


#ifndef nsIDOMSelectionListener_h___
#define nsIDOMSelectionListener_h___

#include "nsISupports.h"

// IID for the nsIDOMSelectionListener interface
/* a6cf90e2-15b3-11d2-932e-00805f8add32 */
#define NS_IDOMSELECTIONLISTENER_IID      \
{ 0xa6cf90e2, 0x15b3, 0x11d2,             \
{ 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// Selection interface
class nsIDOMSelectionListener : public nsISupports
{
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMSELECTIONLISTENER_IID; return iid; }


  NS_IMETHOD NotifySelectionChanged() = 0;
  
};



#endif // nsIDOMSelectionListener_h___
