/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsMathMLsemanticsFrame.h"
#include "nsMimeTypes.h"
#include "mozilla/gfx/2D.h"

//
// <semantics> -- associate annotations with a MathML expression
//

nsIFrame*
NS_NewMathMLsemanticsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLsemanticsFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLsemanticsFrame)

nsMathMLsemanticsFrame::~nsMathMLsemanticsFrame()
{
}

nsIFrame*
nsMathMLsemanticsFrame::GetSelectedFrame()
{
  // By default, we will display the first child of the <semantics> element.
  nsIFrame* childFrame = mFrames.FirstChild();
  mSelectedFrame = childFrame;

  // An empty <semantics> is invalid
  if (!childFrame) {
    mInvalidMarkup = true;
    return mSelectedFrame;
  }
  mInvalidMarkup = false;

  // Using <annotation> or <annotation-xml> as a first child is invalid.
  // However some people use this syntax so we take care of this case too.
  bool firstChildIsAnnotation = false;
  nsIContent* childContent = childFrame->GetContent();
  if (childContent->IsAnyOfMathMLElements(nsGkAtoms::annotation_,
                                          nsGkAtoms::annotation_xml_)) {
    firstChildIsAnnotation = true;
  }

  // If the first child is a presentation MathML element other than
  // <annotation> or <annotation-xml>, we are done.
  if (!firstChildIsAnnotation &&
      childFrame->IsFrameOfType(nsIFrame::eMathML)) {
    nsIMathMLFrame* mathMLFrame = do_QueryFrame(childFrame);
    if (mathMLFrame) {
      TransmitAutomaticData();
      return mSelectedFrame;
    }
    // The first child is not an annotation, so skip it.
    childFrame = childFrame->GetNextSibling();
  }

  // Otherwise, we read the list of annotations and select the first one that
  // could be displayed in place of the first child of <semantics>. If none is
  // found, we fallback to this first child.
  for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
    nsIContent* childContent = childFrame->GetContent();

    if (childContent->IsMathMLElement(nsGkAtoms::annotation_)) {

      // If the <annotation> element has an src attribute we ignore it.
      // XXXfredw Should annotation images be supported? See the related
      // bug 297465 for mglyph.
      if (childContent->HasAttr(kNameSpaceID_None, nsGkAtoms::src)) continue;

      // Otherwise, we assume it is a text annotation that can always be
      // displayed and stop here.
      mSelectedFrame = childFrame;
      break;
    }

    if (childContent->IsMathMLElement(nsGkAtoms::annotation_xml_)) {

      // If the <annotation-xml> element has an src attribute we ignore it.
      if (childContent->HasAttr(kNameSpaceID_None, nsGkAtoms::src)) continue;

      // If the <annotation-xml> element has an encoding attribute
      // describing presentation MathML, SVG or HTML we assume the content
      // can be displayed and stop here.
      //
      // We recognize the following encoding values:
      //
      // - "MathML-Presentation", which is mentioned in the MathML3 REC
      // - "SVG1.1" which is mentioned in the W3C note
      //                   http://www.w3.org/Math/Documents/Notes/graphics.xml
      // - Other mime Content-Types for SVG and HTML
      //
      // We exclude APPLICATION_MATHML_XML = "application/mathml+xml" which
      // is ambiguous about whether it is Presentation or Content MathML.
      // Authors must use a more explicit encoding value.
      nsAutoString value;
      childContent->GetAttr(kNameSpaceID_None, nsGkAtoms::encoding, value);
      if (value.EqualsLiteral("application/mathml-presentation+xml") ||
          value.EqualsLiteral("MathML-Presentation") ||
          value.EqualsLiteral(IMAGE_SVG_XML) ||
          value.EqualsLiteral("SVG1.1") ||
          value.EqualsLiteral(APPLICATION_XHTML_XML) ||
          value.EqualsLiteral(TEXT_HTML)) {
        mSelectedFrame = childFrame;
        break;
      }
    }
  }

  TransmitAutomaticData();
  return mSelectedFrame;
}
