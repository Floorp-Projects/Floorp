/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef MOZILLA_SVGPATHSEGUTILS_H__
#define MOZILLA_SVGPATHSEGUTILS_H__

#include "nsIDOMSVGPathSeg.h"
#include "nsIContent.h"
#include "nsAString.h"
#include "nsContentUtils.h"
#include "gfxPoint.h"

#define NS_SVG_PATH_SEG_MAX_ARGS 7

namespace mozilla {

/**
 * Code that works with path segments can use an instance of this class to
 * store/provide information about the start of the current subpath and the
 * last path segment (if any).
 */
struct SVGPathTraversalState
{
  SVGPathTraversalState()
    : start(0.0, 0.0)
    , pos(0.0, 0.0)
    , cp1(0.0, 0.0)
    , cp2(0.0, 0.0)
  {}

  gfxPoint start; // start point of current sub path (reset each moveto)

  gfxPoint pos;   // current position (end point of previous segment)

  gfxPoint cp1;   // quadratic control point - if the previous segment was a
                  // quadratic bezier curve then this is set to the absolute
                  // position of its control point, otherwise its set to pos

  gfxPoint cp2;   // cubic control point - if the previous segment was a cubic
                  // bezier curve then this is set to the absolute position of
                  // its second control point, otherwise it's set to pos
};


/**
 * This class is just a collection of static methods - it doesn't have any data
 * members, and it's not possible to create instances of this class. This class
 * exists purely as a convenient place to gather together a bunch of methods
 * related to manipulating and answering questions about path segments.
 * Internally we represent path segments purely as an array of floats. See the
 * comment documenting SVGPathData for more info on that.
 *
 * The DOM wrapper classes for encoded path segments (data contained in
 * instances of SVGPathData) is DOMSVGPathSeg and its sub-classes. Note that
 * there are multiple different DOM classes for path segs - one for each of the
 * 19 SVG 1.1 segment types.
 */
class SVGPathSegUtils
{
private:
  SVGPathSegUtils(){} // private to prevent instances

public:

  static void GetValueAsString(const float *aSeg, nsAString& aValue);

  /**
   * Encode a segment type enum to a float.
   *
   * At some point in the future we will likely want to encode other
   * information into the float, such as whether the command was explicit or
   * not. For now all this method does is save on int to float runtime
   * conversion by requiring PRUint32 and float to be of the same size so we
   * can simply do a bitwise PRUint32<->float copy.
   */
  static float EncodeType(PRUint32 aType) {
    PR_STATIC_ASSERT(sizeof(PRUint32) == sizeof(float));
    NS_ABORT_IF_FALSE(IsValidType(aType), "Seg type not recognized");
    return *(reinterpret_cast<float*>(&aType));
  }

  static PRUint32 DecodeType(float aType) {
    PR_STATIC_ASSERT(sizeof(PRUint32) == sizeof(float));
    PRUint32 type = *(reinterpret_cast<PRUint32*>(&aType));
    NS_ABORT_IF_FALSE(IsValidType(type), "Seg type not recognized");
    return type;
  }

  static PRUnichar GetPathSegTypeAsLetter(PRUint32 aType) {
    NS_ABORT_IF_FALSE(IsValidType(aType), "Seg type not recognized");

    static const PRUnichar table[] = {
      PRUnichar('x'),  //  0 == PATHSEG_UNKNOWN
      PRUnichar('z'),  //  1 == PATHSEG_CLOSEPATH
      PRUnichar('M'),  //  2 == PATHSEG_MOVETO_ABS
      PRUnichar('m'),  //  3 == PATHSEG_MOVETO_REL
      PRUnichar('L'),  //  4 == PATHSEG_LINETO_ABS
      PRUnichar('l'),  //  5 == PATHSEG_LINETO_REL
      PRUnichar('C'),  //  6 == PATHSEG_CURVETO_CUBIC_ABS
      PRUnichar('c'),  //  7 == PATHSEG_CURVETO_CUBIC_REL
      PRUnichar('Q'),  //  8 == PATHSEG_CURVETO_QUADRATIC_ABS
      PRUnichar('q'),  //  9 == PATHSEG_CURVETO_QUADRATIC_REL
      PRUnichar('A'),  // 10 == PATHSEG_ARC_ABS
      PRUnichar('a'),  // 11 == PATHSEG_ARC_REL
      PRUnichar('H'),  // 12 == PATHSEG_LINETO_HORIZONTAL_ABS
      PRUnichar('h'),  // 13 == PATHSEG_LINETO_HORIZONTAL_REL
      PRUnichar('V'),  // 14 == PATHSEG_LINETO_VERTICAL_ABS
      PRUnichar('v'),  // 15 == PATHSEG_LINETO_VERTICAL_REL
      PRUnichar('S'),  // 16 == PATHSEG_CURVETO_CUBIC_SMOOTH_ABS
      PRUnichar('s'),  // 17 == PATHSEG_CURVETO_CUBIC_SMOOTH_REL
      PRUnichar('T'),  // 18 == PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS
      PRUnichar('t')   // 19 == PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL
    };

    return table[aType];
  }

  static PRUint32 ArgCountForType(PRUint32 aType) {
    NS_ABORT_IF_FALSE(IsValidType(aType), "Seg type not recognized");

    static const PRUint8 table[] = {
      0,  //  0 == PATHSEG_UNKNOWN
      0,  //  1 == PATHSEG_CLOSEPATH
      2,  //  2 == PATHSEG_MOVETO_ABS
      2,  //  3 == PATHSEG_MOVETO_REL
      2,  //  4 == PATHSEG_LINETO_ABS
      2,  //  5 == PATHSEG_LINETO_REL
      6,  //  6 == PATHSEG_CURVETO_CUBIC_ABS
      6,  //  7 == PATHSEG_CURVETO_CUBIC_REL
      4,  //  8 == PATHSEG_CURVETO_QUADRATIC_ABS
      4,  //  9 == PATHSEG_CURVETO_QUADRATIC_REL
      7,  // 10 == PATHSEG_ARC_ABS
      7,  // 11 == PATHSEG_ARC_REL
      1,  // 12 == PATHSEG_LINETO_HORIZONTAL_ABS
      1,  // 13 == PATHSEG_LINETO_HORIZONTAL_REL
      1,  // 14 == PATHSEG_LINETO_VERTICAL_ABS
      1,  // 15 == PATHSEG_LINETO_VERTICAL_REL
      4,  // 16 == PATHSEG_CURVETO_CUBIC_SMOOTH_ABS
      4,  // 17 == PATHSEG_CURVETO_CUBIC_SMOOTH_REL
      2,  // 18 == PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS
      2   // 19 == PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL
    };

    return table[aType];
  }

  /**
   * Convenience so that callers can pass a float containing an encoded type
   * and have it decoded implicitly.
   */
  static PRUint32 ArgCountForType(float aType) {
    return ArgCountForType(DecodeType(aType));
  }

  static inline PRBool IsValidType(PRUint32 aType) {
    return aType >= nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH &&
           aType <= nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL;
  }

  static inline PRBool IsCubicType(PRUint32 aType) {
    return aType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL ||
           aType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS ||
           aType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL ||
           aType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS;
  }

  static inline PRBool IsQuadraticType(PRUint32 aType) {
    return aType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL ||
           aType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS ||
           aType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL ||
           aType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS;
  }

  /**
   * Returns the user unit length of tracing along the path segment.
   */
  static float GetLength(const float *aSeg, SVGPathTraversalState &aState);

  static void ToString(const float *aSeg, nsAString& aValue);
};

} // namespace mozilla

#endif // MOZILLA_SVGPATHSEGUTILS_H__
