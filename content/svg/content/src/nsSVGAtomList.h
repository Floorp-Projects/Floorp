/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: Rod Spears (rods@netscape.com)
 *
 * Contributor(s): 
 */

/******

  This file contains the list of all SVG nsIAtoms and their values
  
  It is designed to be used as inline input to nsSVGAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro SVG_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to SVG_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/

// tags
SVG_ATOM(circle, "circle")
SVG_ATOM(ellipse, "ellipse")
SVG_ATOM(foreignObject, "foreignObject")
SVG_ATOM(g, "g")
SVG_ATOM(generic, "generic")
SVG_ATOM(line, "line")
SVG_ATOM(path, "path")
SVG_ATOM(polygon, "polygon")
SVG_ATOM(polyline, "polyline")
SVG_ATOM(rect, "rect")
SVG_ATOM(svg, "svg")

// properties
SVG_ATOM(cx, "cx")
SVG_ATOM(cy, "cy")
SVG_ATOM(d, "d")
SVG_ATOM(fill, "fill")
SVG_ATOM(height, "height")
SVG_ATOM(id, "id")
SVG_ATOM(pathLength, "pathLength")
SVG_ATOM(points, "points")
SVG_ATOM(r, "r")
SVG_ATOM(rx, "rx")
SVG_ATOM(ry, "ry")
SVG_ATOM(style, "style")
SVG_ATOM(transform, "transform")
SVG_ATOM(viewBox, "viewBox")
SVG_ATOM(width, "width")
SVG_ATOM(x, "x")
SVG_ATOM(y, "y")
SVG_ATOM(x1, "x1")
SVG_ATOM(y1, "y1")
SVG_ATOM(x2, "x2")
SVG_ATOM(y2, "y2")
  
// transformation keywords
SVG_ATOM(rotate, "rotate")
SVG_ATOM(scale, "scale")
SVG_ATOM(skewX, "skewX")
SVG_ATOM(skewY, "skewY")
SVG_ATOM(translate, "translate")

// length units
SVG_ATOM(cm, "cm")
SVG_ATOM(ems, "em")
SVG_ATOM(exs, "ex")
SVG_ATOM(in, "in")
SVG_ATOM(mm, "mm")
SVG_ATOM(pc, "pc")
SVG_ATOM(percentage, "%")
SVG_ATOM(pt, "pt")
SVG_ATOM(px, "px")

