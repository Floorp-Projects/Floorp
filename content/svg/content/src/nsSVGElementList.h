/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * This file is used to help create a mapping from a specified SVG element to
 * attributes supported by that element. This mapping can be used to help
 * ensure that we don't accidentally implement support for attributes like
 * requiredFeatures on elements for which the SVG specification does not
 * define support.
 *
 * To use, include this file into another file after defining the SVG_ELEMENT
 * C preprocessor macro as appropriate.
 *
 * The following constants represent the following attributes:
 *
 * ATTRS_CONDITIONAL
 *   The requiredFeatures, requiredExtensions, and systemLanguage attributes
 *
 * ATTRS_EXTERNAL
 *   The externalResourcesRequired attribute
 *
 * ATTRS_ALL
 *   A convenience value indicating support for all of the above
 *
 * ATTRS_NONE
 *   A convenience value indicating support for none of the above
 */

//This file must not have include guards.

#define ATTRS_NONE        0x00
#define ATTRS_CONDITIONAL 0x01
#define ATTRS_EXTERNAL    0x02
#define ATTRS_ALL         (ATTRS_CONDITIONAL | ATTRS_EXTERNAL)
// tags
SVG_ELEMENT(a, ATTRS_ALL)
SVG_ELEMENT(altGlyph, ATTRS_ALL)
SVG_ELEMENT(altGlyphDef, ATTRS_NONE)
SVG_ELEMENT(altGlyphItem, ATTRS_NONE)
SVG_ELEMENT(animate, ATTRS_ALL)
SVG_ELEMENT(animateColor, ATTRS_ALL)
SVG_ELEMENT(animateMotion, ATTRS_ALL)
SVG_ELEMENT(animateTransform, ATTRS_ALL)
SVG_ELEMENT(circle, ATTRS_ALL)
SVG_ELEMENT(clipPath, ATTRS_ALL)
SVG_ELEMENT(colorProfile, ATTRS_NONE)
SVG_ELEMENT(cursor, ATTRS_ALL)
SVG_ELEMENT(definition_src, ATTRS_NONE)
SVG_ELEMENT(defs, ATTRS_ALL)
SVG_ELEMENT(desc, ATTRS_NONE)
SVG_ELEMENT(ellipse, ATTRS_ALL)
SVG_ELEMENT(feBlend, ATTRS_NONE)
SVG_ELEMENT(feColorMatrix, ATTRS_NONE)
SVG_ELEMENT(feComponentTransfer, ATTRS_NONE)
SVG_ELEMENT(feComposite, ATTRS_NONE)
SVG_ELEMENT(feConvolveMatrix, ATTRS_NONE)
SVG_ELEMENT(feDiffuseLighting, ATTRS_NONE)
SVG_ELEMENT(feDisplacementMap, ATTRS_NONE)
SVG_ELEMENT(feDistantLight, ATTRS_NONE)
SVG_ELEMENT(feFlood, ATTRS_NONE)
SVG_ELEMENT(feFuncR, ATTRS_NONE)
SVG_ELEMENT(feFuncG, ATTRS_NONE)
SVG_ELEMENT(feFuncB, ATTRS_NONE)
SVG_ELEMENT(feFuncA, ATTRS_NONE)
SVG_ELEMENT(feGaussianBlur, ATTRS_NONE)
SVG_ELEMENT(feImage, ATTRS_EXTERNAL)
SVG_ELEMENT(feMerge, ATTRS_NONE)
SVG_ELEMENT(feMergeNode, ATTRS_NONE)
SVG_ELEMENT(feMorphology, ATTRS_NONE)
SVG_ELEMENT(feOffset, ATTRS_NONE)
SVG_ELEMENT(fePointLight, ATTRS_NONE)
SVG_ELEMENT(feSpecularLighting, ATTRS_NONE)
SVG_ELEMENT(feSpotLight, ATTRS_NONE)
SVG_ELEMENT(feTile, ATTRS_NONE)
SVG_ELEMENT(feTurbulence, ATTRS_NONE)
SVG_ELEMENT(filter, ATTRS_EXTERNAL)
SVG_ELEMENT(font, ATTRS_EXTERNAL)
SVG_ELEMENT(font_face, ATTRS_NONE)
SVG_ELEMENT(font_face_format, ATTRS_NONE)
SVG_ELEMENT(font_face_name, ATTRS_NONE)
SVG_ELEMENT(font_face_src, ATTRS_NONE)
SVG_ELEMENT(font_face_uri, ATTRS_NONE)
SVG_ELEMENT(foreignObject, ATTRS_ALL)
SVG_ELEMENT(g, ATTRS_ALL)
SVG_ELEMENT(glyph, ATTRS_NONE)
SVG_ELEMENT(glyphRef, ATTRS_NONE)
SVG_ELEMENT(hkern, ATTRS_NONE)
SVG_ELEMENT(image, ATTRS_ALL)
SVG_ELEMENT(line, ATTRS_ALL)
SVG_ELEMENT(linearGradient, ATTRS_EXTERNAL)
SVG_ELEMENT(marker, ATTRS_NONE)
SVG_ELEMENT(mask, ATTRS_ALL)
SVG_ELEMENT(metadata, ATTRS_NONE)
SVG_ELEMENT(missingGlyph, ATTRS_NONE)
SVG_ELEMENT(mpath, ATTRS_EXTERNAL)
SVG_ELEMENT(path, ATTRS_ALL)
SVG_ELEMENT(pattern, ATTRS_ALL)
SVG_ELEMENT(polygon, ATTRS_ALL)
SVG_ELEMENT(polyline, ATTRS_ALL)
SVG_ELEMENT(radialGradient, ATTRS_EXTERNAL)
SVG_ELEMENT(rect, ATTRS_ALL)
SVG_ELEMENT(script, ATTRS_EXTERNAL)
SVG_ELEMENT(set, ATTRS_ALL)
SVG_ELEMENT(stop, ATTRS_NONE)
SVG_ELEMENT(style, ATTRS_NONE)
SVG_ELEMENT(svg, ATTRS_ALL)
SVG_ELEMENT(svgSwitch, ATTRS_ALL) // switch is a C++ keyword, hence svgSwitch
SVG_ELEMENT(symbol, ATTRS_NONE)
SVG_ELEMENT(text, ATTRS_ALL)
SVG_ELEMENT(textPath, ATTRS_ALL)
SVG_ELEMENT(title, ATTRS_NONE)
SVG_ELEMENT(tref, ATTRS_ALL)
SVG_ELEMENT(tspan, ATTRS_ALL)
SVG_ELEMENT(use, ATTRS_ALL)
SVG_ELEMENT(view, ATTRS_EXTERNAL)
SVG_ELEMENT(vkern, ATTRS_NONE)
