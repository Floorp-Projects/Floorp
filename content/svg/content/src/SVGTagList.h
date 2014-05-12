/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/******

  This file contains the list of all SVG tags.

  It is designed to be used as inline input to SVGElementFactory.cpp
  *only* through the magic of C preprocessing.

  All entries must be enclosed in the macro SVG_TAG or SVG_FROM_PARSER_TAG
  which will have cruel and unusual things done to them.

  SVG_FROM_PARSER_TAG is used where the element creation method takes
  a FromParser argument, and SVG_TAG where it does not.

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order.

  The first argument to SVG_TAG is both the enum identifier of the
  property and the atom name. The second argument is the "creator"
  method of the form NS_New$TAGNAMEElement, that will be used by
  SVGElementFactory.cpp to create a content object for a tag of that
  type.

 ******/

SVG_TAG(a, A)
SVG_TAG(altGlyph, AltGlyph)
SVG_TAG(animate, Animate)
SVG_TAG(animateMotion, AnimateMotion)
SVG_TAG(animateTransform, AnimateTransform)
SVG_TAG(circle, Circle)
SVG_TAG(clipPath, ClipPath)
SVG_TAG(defs, Defs)
SVG_TAG(desc, Desc)
SVG_TAG(ellipse, Ellipse)
SVG_TAG(feBlend, FEBlend)
SVG_TAG(feColorMatrix, FEColorMatrix)
SVG_TAG(feComponentTransfer, FEComponentTransfer)
SVG_TAG(feComposite, FEComposite)
SVG_TAG(feConvolveMatrix, FEConvolveMatrix)
SVG_TAG(feDiffuseLighting, FEDiffuseLighting)
SVG_TAG(feDisplacementMap, FEDisplacementMap)
SVG_TAG(feDistantLight, FEDistantLight)
SVG_TAG(feDropShadow, FEDropShadow)
SVG_TAG(feFlood, FEFlood)
SVG_TAG(feFuncA, FEFuncA)
SVG_TAG(feFuncB, FEFuncB)
SVG_TAG(feFuncG, FEFuncG)
SVG_TAG(feFuncR, FEFuncR)
SVG_TAG(feGaussianBlur, FEGaussianBlur)
SVG_TAG(feImage, FEImage)
SVG_TAG(feMerge, FEMerge)
SVG_TAG(feMergeNode, FEMergeNode)
SVG_TAG(feMorphology, FEMorphology)
SVG_TAG(feOffset, FEOffset)
SVG_TAG(fePointLight, FEPointLight)
SVG_TAG(feSpecularLighting, FESpecularLighting)
SVG_TAG(feSpotLight, FESpotLight)
SVG_TAG(feTile, FETile)
SVG_TAG(feTurbulence, FETurbulence)
SVG_TAG(filter, Filter)
SVG_TAG(foreignObject, ForeignObject)
SVG_TAG(g, G)
SVG_FROM_PARSER_TAG(iframe, IFrame)
SVG_TAG(image, Image)
SVG_TAG(line, Line)
SVG_TAG(linearGradient, LinearGradient)
SVG_TAG(marker, Marker)
SVG_TAG(mask, Mask)
SVG_TAG(metadata, Metadata)
SVG_TAG(mpath, MPath)
SVG_TAG(path, Path)
SVG_TAG(pattern, Pattern)
SVG_TAG(polygon, Polygon)
SVG_TAG(polyline, Polyline)
SVG_TAG(radialGradient, RadialGradient)
SVG_TAG(rect, Rect)
SVG_FROM_PARSER_TAG(script, Script)
SVG_TAG(set, Set)
SVG_TAG(stop, Stop)
SVG_TAG(style, Style)
SVG_FROM_PARSER_TAG(svg, SVG)
SVG_TAG(svgSwitch, Switch)
SVG_TAG(symbol, Symbol)
SVG_TAG(text, Text)
SVG_TAG(textPath, TextPath)
SVG_TAG(title, Title)
SVG_TAG(tspan, TSpan)
SVG_TAG(use, Use)
SVG_TAG(view, View)
