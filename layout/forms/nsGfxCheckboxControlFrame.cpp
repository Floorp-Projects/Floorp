/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGfxCheckboxControlFrame.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsLayoutUtils.h"
#include "nsRenderingContext.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsDisplayList.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;

//------------------------------------------------------------
nsIFrame*
NS_NewGfxCheckboxControlFrame(nsIPresShell* aPresShell,
                              nsStyleContext* aContext)
{
  return new (aPresShell) nsGfxCheckboxControlFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsGfxCheckboxControlFrame)


//------------------------------------------------------------
// Initialize GFX-rendered state
nsGfxCheckboxControlFrame::nsGfxCheckboxControlFrame(nsStyleContext* aContext)
: nsFormControlFrame(aContext)
{
}

nsGfxCheckboxControlFrame::~nsGfxCheckboxControlFrame()
{
}

#ifdef ACCESSIBILITY
a11y::AccType
nsGfxCheckboxControlFrame::AccessibleType()
{
  return a11y::eHTMLCheckboxType;
}
#endif
