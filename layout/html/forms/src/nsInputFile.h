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

#ifndef nsInputFile_h___
#define nsInputFile_h___

#include "nsInput.h"
class nsIAtom;
class nsString;

// this class definition will move to nsInputFile.cpp

class nsInputFile : public nsInput {
public:
  typedef nsInput nsInputFileSuper;
  nsInputFile (nsIAtom* aTag, nsIFormManager* aManager);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

protected:
  virtual ~nsInputFile();

  virtual void GetType(nsString& aResult) const;

  nsString* mValue;
  PRInt32 mSize;
  PRInt32 mMaxLength;
};

#endif


