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
#ifndef nsIWebFrame_h___
#define nsIWebFrame_h___

#include "nsCom.h"
#include "nsISupports.h"
#include "nsIWebWidget.h"

// IID for the nsIWebFrame interface
#define NS_IWEBFRAME_IID      \
{ 0xecace1b0, 0x541, 0x11d2, \
   { 0x80, 0x34, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

/**
 * Interface used for distinguishing frames that have web widgets associated
 * with them (i.e. <iframe>, <frame>)
 */
class nsIWebFrame : public nsISupports {
public:
  /**
    * Get the associated web widget and increment the ref count.
    */
  virtual nsIWebWidget* GetWebWidget() = 0;
};


#endif /* nsIWebWidget_h___ */
