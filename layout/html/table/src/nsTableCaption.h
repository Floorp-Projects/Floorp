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
 * nsTableCaption is the content object that represents table captions 
 * (HTML tag CAPTION). This class cannot be reused
 * outside of an nsTablePart.  It assumes that its parent is an nsTablePart, and 
 * it has a single nsBodyPart child.
 * 
 * @see nsTablePart
 * @see nsTableRow
 * @see nsBodyPart
 *
 * @author  sclark
 */
class nsTableCaption : public nsTableContent
{
public:

  /** constructor
    * @param aImplicit  PR_TRUE if there is no actual input tag corresponding to
    *                   this caption.
    */
  nsTableCaption (PRBool aImplicit);

  /** constructor
    * @param aTag  the HTML tag causing this caption to get constructed.
    */
  nsTableCaption (nsIAtom* aTag);

  /** destructor, not responsible for any memory destruction itself */
  virtual ~nsTableCaption();

    // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  /** returns nsITableContent::kTableCaptionType */
  virtual int GetType();

  /** @see nsIHTMLContent::CreateFrame */
  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);
};

#endif



