/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmsqrtFrame_h___
#define nsMathMLmsqrtFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLmencloseFrame.h"

//
// <msqrt> -- form a radical
//

/*
The MathML REC describes:

The <msqrt> element is used to display square roots.
The syntax for <msqrt> is:
  <msqrt> base </msqrt>

Attributes of <msqrt> and <mroot>:

None (except the attributes allowed for all MathML elements, listed in Section
2.3.4). 

The <mroot> element increments scriptlevel by 2, and sets displaystyle to
"false", within index, but leaves both attributes unchanged within base. The
<msqrt> element leaves both attributes unchanged within all its arguments.
These attributes are inherited by every element from its rendering environment,
but can be set explicitly only on <mstyle>. (See Section 3.3.4.) 
*/

class nsMathMLmsqrtFrame : public nsMathMLmencloseFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmsqrtFrame(nsIPresShell*   aPresShell,
                                          nsStyleContext* aContext);

  virtual void
  Init(nsIContent* aContent,
       nsIFrame*   aParent,
       nsIFrame*   aPrevInFlow) MOZ_OVERRIDE;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) MOZ_OVERRIDE;

  NS_IMETHOD
  AttributeChanged(int32_t         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   int32_t         aModType) MOZ_OVERRIDE;

protected:
  nsMathMLmsqrtFrame(nsStyleContext* aContext);
  virtual ~nsMathMLmsqrtFrame();
};

#endif /* nsMathMLmsqrtFrame_h___ */

