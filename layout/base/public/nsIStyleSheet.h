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
#ifndef nsIStyleSheet_h___
#define nsIStyleSheet_h___

#include <stdio.h>
#include "nsISupports.h"
class nsIAtom;
class nsIURL;
class nsIStyleRule;
class nsISupportsArray;
class nsIPresContext;
class nsIContent;
class nsIFrame;

// IID for the nsIStyleSheet interface {8c4a80a0-ad6a-11d1-8031-006008159b5a}
#define NS_ISTYLE_SHEET_IID     \
{0x8c4a80a0, 0xad6a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIStyleSheet : public nsISupports {
public:
  virtual nsIURL* GetURL(void) = 0;

  // populate void array with nsIStyleRule*
  virtual PRInt32 RulesMatching(nsIPresContext* aPresContext,
                                nsIContent* aContent,
                                nsIFrame* aParentFrame,
                                nsISupportsArray* aResults) = 0;

  // XXX style rule enumerations

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
};

#endif /* nsIStyleSheet_h___ */
