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
#include "nsCSSContainerFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsIStyleContext.h"

nsCSSContainerFrame::nsCSSContainerFrame(nsIContent* aContent,
                                         nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

nsCSSContainerFrame::~nsCSSContainerFrame()
{
}

nsPlaceholderFrame*
nsCSSContainerFrame::CreatePlaceholderFrame(nsIPresContext* aPresContext,
                                            nsIFrame*       aFloatedFrame)
{
  nsIContent* content;
  aFloatedFrame->GetContent(content);

  nsPlaceholderFrame* placeholder;
  nsPlaceholderFrame::NewFrame((nsIFrame**)&placeholder, content, this, aFloatedFrame);
  NS_IF_RELEASE(content);

  // Let the placeholder share the same style context as the floated element
  nsIStyleContext*  kidSC;
  aFloatedFrame->GetStyleContext(aPresContext, kidSC);
  placeholder->SetStyleContext(aPresContext, kidSC);
  NS_RELEASE(kidSC);
  
  return placeholder;
}
