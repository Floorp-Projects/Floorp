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
#ifndef nsTableCaption_h__
#define nsTableCaption_h__

#include "nscore.h"
#include "nsTableContent.h"

/**
 * nsTableCaption
 * data structure to maintain information about a single table caption
 *
 * @author  sclark
 */
class nsTableCaption : public nsTableContent
{
public:

  nsTableCaption (PRBool aImplicit);

  nsTableCaption (nsIAtom* aTag);

  virtual ~nsTableCaption();

    // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual int GetType();

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

};

#endif



