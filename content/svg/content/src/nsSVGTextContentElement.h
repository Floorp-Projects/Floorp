/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGTEXTCONTENTELEMENTBASE_H__
#define __NS_SVGTEXTCONTENTELEMENTBASE_H__

#include "DOMSVGTests.h"
#include "nsIDOMSVGTextContentElement.h"
#include "nsSVGElement.h"
#include "nsSVGTextContainerFrame.h"

typedef nsSVGElement nsSVGTextContentElementBase;

/**
 * Note that nsSVGTextElement does not inherit nsSVGTextPositioningElement, or
 * this class - it reimplements us instead (see its documenting comment). The
 * upshot is that any changes to this class also need to be made in
 * nsSVGTextElement.
 */
class nsSVGTextContentElement : public nsSVGTextContentElementBase,
                                public DOMSVGTests
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTEXTCONTENTELEMENT

protected:

  nsSVGTextContentElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGTextContentElementBase(aNodeInfo)
  {}

  nsSVGTextContainerFrame* GetTextContainerFrame() {
    return do_QueryFrame(GetPrimaryFrame(Flush_Layout));
  }
};

#endif
