/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsObjectContent_h___
#define nsObjectContent_h___

#include "nsHTMLContainer.h"
#include "nsFrame.h"

#define nsObjectContentSuper nsHTMLContainer

class nsObjectContent : public nsObjectContentSuper {
public:
  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  void SetAttribute(nsIAtom* aAttribute, const nsString& aString);

  virtual void MapAttributesInto(nsIStyleContext* aContext, 
                                 nsIPresContext* aPresContext);

protected:
  nsObjectContent(nsIAtom* aTag);
  virtual ~nsObjectContent();

  nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                  nsHTMLValue& aValue,
                                  nsString& aResult) const;
};

#endif /* nsObjectContent_h___ */
