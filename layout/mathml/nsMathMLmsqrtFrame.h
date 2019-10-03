/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmsqrtFrame_h___
#define nsMathMLmsqrtFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLmencloseFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

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

// XXXfredw: This class should share its layout logic with nsMathMLmrootFrame
// when the menclose "radical" notation is removed.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1548522
class nsMathMLmsqrtFrame final : public nsMathMLmencloseFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmsqrtFrame)

  friend nsIFrame* NS_NewMathMLmsqrtFrame(mozilla::PresShell* aPresShell,
                                          ComputedStyle* aStyle);

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual bool IsMrowLike() override {
    return mFrames.FirstChild() != mFrames.LastChild() || !mFrames.FirstChild();
  }

 protected:
  explicit nsMathMLmsqrtFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext);
  virtual ~nsMathMLmsqrtFrame();
};

#endif /* nsMathMLmsqrtFrame_h___ */
