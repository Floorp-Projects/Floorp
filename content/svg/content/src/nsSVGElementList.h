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

/******

  This file contains the list of all SVG elements.  It is designed
  to be used by the C preprocessor for the purposes of creating a
  table of support for various elements such as support for the
  requiredFeatures, requiredExtensions, and requiredLanguage
  conditionals
  
 ******/

// Define the LangSpace & Tests support
#define SUPPORTS_NONE 0
#define SUPPORTS_LANG 1
#define SUPPORTS_TEST 2
#define SUPPORTS_EXTERNAL 4
#define SUPPORTS_ALL 7
// tags
SVG_ELEMENT(a, SUPPORTS_LANG|SUPPORTS_TEST)
SVG_ELEMENT(altGlyph, SUPPORTS_ALL)
SVG_ELEMENT(altGlyphDef, SUPPORTS_NONE)
SVG_ELEMENT(altGlyphItem, SUPPORTS_NONE)
SVG_ELEMENT(animate, SUPPORTS_EXTERNAL|SUPPORTS_TEST)
SVG_ELEMENT(animateColor, SUPPORTS_EXTERNAL|SUPPORTS_TEST)
SVG_ELEMENT(animateMotion, SUPPORTS_EXTERNAL|SUPPORTS_TEST)
SVG_ELEMENT(animateTransform, SUPPORTS_EXTERNAL|SUPPORTS_TEST)
SVG_ELEMENT(circle, SUPPORTS_ALL)
SVG_ELEMENT(clipPath, SUPPORTS_ALL)
SVG_ELEMENT(colorProfile, SUPPORTS_NONE)
SVG_ELEMENT(cursor, SUPPORTS_ALL)
SVG_ELEMENT(definition_src, SUPPORTS_NONE)
SVG_ELEMENT(defs, SUPPORTS_ALL)
SVG_ELEMENT(desc, SUPPORTS_LANG)
SVG_ELEMENT(ellipse, SUPPORTS_ALL)
SVG_ELEMENT(feBlend, SUPPORTS_NONE)
SVG_ELEMENT(feColorMatrix, SUPPORTS_NONE)
SVG_ELEMENT(feComponentTransfer, SUPPORTS_NONE)
SVG_ELEMENT(feComposite, SUPPORTS_NONE)
SVG_ELEMENT(feConvolveMatrix, SUPPORTS_NONE)
SVG_ELEMENT(feDiffuseLighting, SUPPORTS_NONE)
SVG_ELEMENT(feDisplacementMap, SUPPORTS_NONE)
SVG_ELEMENT(feDistantLight, SUPPORTS_NONE)
SVG_ELEMENT(feFlood, SUPPORTS_NONE)
SVG_ELEMENT(feFuncR, SUPPORTS_NONE)
SVG_ELEMENT(feFuncG, SUPPORTS_NONE)
SVG_ELEMENT(feFuncB, SUPPORTS_NONE)
SVG_ELEMENT(feFuncA, SUPPORTS_NONE)
SVG_ELEMENT(feGaussianBlur, SUPPORTS_NONE)
SVG_ELEMENT(feImage, SUPPORTS_EXTERNAL|SUPPORTS_LANG)
SVG_ELEMENT(feMerge, SUPPORTS_NONE)
SVG_ELEMENT(feMergeNode, SUPPORTS_NONE)
SVG_ELEMENT(feMorphology, SUPPORTS_NONE)
SVG_ELEMENT(feOffset, SUPPORTS_NONE)
SVG_ELEMENT(fePointLight, SUPPORTS_NONE)
SVG_ELEMENT(feSpecularLighting, SUPPORTS_NONE)
SVG_ELEMENT(feSpotLight, SUPPORTS_NONE)
SVG_ELEMENT(feTile, SUPPORTS_NONE)
SVG_ELEMENT(feTurbulence, SUPPORTS_NONE)
SVG_ELEMENT(filter, SUPPORTS_LANG|SUPPORTS_EXTERNAL)
SVG_ELEMENT(font, SUPPORTS_EXTERNAL)
SVG_ELEMENT(font_face, SUPPORTS_NONE)
SVG_ELEMENT(font_face_format, SUPPORTS_NONE)
SVG_ELEMENT(font_face_name, SUPPORTS_NONE)
SVG_ELEMENT(font_face_src, SUPPORTS_NONE)
SVG_ELEMENT(font_face_uri, SUPPORTS_NONE)
#ifdef MOZ_SVG_FOREIGNOBJECT
SVG_ELEMENT(foreignObject, SUPPORTS_ALL)
#endif
SVG_ELEMENT(g, SUPPORTS_ALL)
SVG_ELEMENT(glyph, SUPPORTS_NONE)
SVG_ELEMENT(glyphRef, SUPPORTS_NONE)
SVG_ELEMENT(hkern, SUPPORTS_NONE)
SVG_ELEMENT(image, SUPPORTS_ALL)
SVG_ELEMENT(line, SUPPORTS_ALL)
SVG_ELEMENT(linearGradient, SUPPORTS_EXTERNAL)
SVG_ELEMENT(marker, SUPPORTS_EXTERNAL|SUPPORTS_LANG)
SVG_ELEMENT(mask, SUPPORTS_ALL)
SVG_ELEMENT(metadata, SUPPORTS_NONE)
SVG_ELEMENT(missingGlyph, SUPPORTS_NONE)
SVG_ELEMENT(mpath, SUPPORTS_EXTERNAL)
SVG_ELEMENT(path, SUPPORTS_ALL)
SVG_ELEMENT(pattern, SUPPORTS_ALL)
SVG_ELEMENT(polygon, SUPPORTS_ALL)
SVG_ELEMENT(polyline, SUPPORTS_ALL)
SVG_ELEMENT(radialGradient, SUPPORTS_EXTERNAL)
SVG_ELEMENT(rect, SUPPORTS_ALL)
SVG_ELEMENT(script, SUPPORTS_EXTERNAL)
SVG_ELEMENT(set, SUPPORTS_TEST|SUPPORTS_EXTERNAL)
SVG_ELEMENT(stop, SUPPORTS_NONE)
SVG_ELEMENT(style, SUPPORTS_NONE)
SVG_ELEMENT(svg, SUPPORTS_ALL)
SVG_ELEMENT(svgSwitch, SUPPORTS_ALL) // switch is a C++ keyword, hence svgSwitch
SVG_ELEMENT(symbol, SUPPORTS_LANG|SUPPORTS_EXTERNAL)
SVG_ELEMENT(text, SUPPORTS_ALL)
SVG_ELEMENT(textPath, SUPPORTS_ALL)
SVG_ELEMENT(title, SUPPORTS_LANG)
SVG_ELEMENT(tref, SUPPORTS_ALL)
SVG_ELEMENT(tspan, SUPPORTS_ALL)
SVG_ELEMENT(use, SUPPORTS_ALL)
SVG_ELEMENT(view, SUPPORTS_EXTERNAL)
SVG_ELEMENT(vkern, SUPPORTS_NONE)
