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
#include "nsCSSInlineFrame.h"
#include "nsInputFrame.h"

class nsIAtom;
class nsString;

// this class definition will move to nsInputFile.cpp

class nsInputFileFrame : public nsCSSInlineFrame {
public:
  nsInputFileFrame(nsIContent* aContent, nsIFrame* aParentFrame);
  NS_IMETHOD Reflow(nsIPresContext*      aCX,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);
  virtual void MouseClicked(nsIPresContext* aPresContext);
  NS_IMETHOD MoveTo(nscoord aX, nscoord aY);
  NS_IMETHOD SizeTo(nscoord aWidth, nscoord aHeight);
  static PRInt32 gSpacing;

protected:
  virtual ~nsInputFileFrame();
};

class nsInputFile : public nsInput {
public:
  typedef nsInput nsInputFileSuper;
  static nsString* gFILE_TYPE;

  nsInputFile (nsIAtom* aTag, nsIFormManager* aManager);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

protected:
  virtual ~nsInputFile();

  virtual void GetType(nsString& aResult) const;

  //PRInt32 mMaxLength;
};

#endif


