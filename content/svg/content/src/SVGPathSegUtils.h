/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGPATHSEGUTILS_H__
#define MOZILLA_SVGPATHSEGUTILS_H__

#include "gfxPoint.h"
#include "mozilla/gfx/Point.h"
#include "nsDebug.h"
#include "nsMemory.h"

namespace mozilla {

// Path Segment Types
static const unsigned short PATHSEG_UNKNOWN                      = 0;
static const unsigned short PATHSEG_CLOSEPATH                    = 1;
static const unsigned short PATHSEG_MOVETO_ABS                   = 2;
static const unsigned short PATHSEG_MOVETO_REL                   = 3;
static const unsigned short PATHSEG_LINETO_ABS                   = 4;
static const unsigned short PATHSEG_LINETO_REL                   = 5;
static const unsigned short PATHSEG_CURVETO_CUBIC_ABS            = 6;
static const unsigned short PATHSEG_CURVETO_CUBIC_REL            = 7;
static const unsigned short PATHSEG_CURVETO_QUADRATIC_ABS        = 8;
static const unsigned short PATHSEG_CURVETO_QUADRATIC_REL        = 9;
static const unsigned short PATHSEG_ARC_ABS                      = 10;
static const unsigned short PATHSEG_ARC_REL                      = 11;
static const unsigned short PATHSEG_LINETO_HORIZONTAL_ABS        = 12;
static const unsigned short PATHSEG_LINETO_HORIZONTAL_REL        = 13;
static const unsigned short PATHSEG_LINETO_VERTICAL_ABS          = 14;
static const unsigned short PATHSEG_LINETO_VERTICAL_REL          = 15;
static const unsigned short PATHSEG_CURVETO_CUBIC_SMOOTH_ABS     = 16;
static const unsigned short PATHSEG_CURVETO_CUBIC_SMOOTH_REL     = 17;
static const unsigned short PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS = 18;
static const unsigned short PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL = 19;

#define NS_SVG_PATH_SEG_MAX_ARGS         7
#define NS_SVG_PATH_SEG_FIRST_VALID_TYPE mozilla::PATHSEG_CLOSEPATH
#define NS_SVG_PATH_SEG_LAST_VALID_TYPE  mozilla::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL
#define NS_SVG_PATH_SEG_TYPE_COUNT       (NS_SVG_PATH_SEG_LAST_VALID_TYPE + 1)

/**
 * Code that works with path segments can use an instance of this class to
 * store/provide information about the start of the current subpath and the
 * last path segment (if any).
 */
struct SVGPathTraversalState
{
  typedef gfx::Point Point;

  enum TraversalMode {
    eUpdateAll,
    eUpdateOnlyStartAndCurrentPos
  };

  SVGPathTraversalState()
    : start(0.0, 0.0)
    , pos(0.0, 0.0)
    , cp1(0.0, 0.0)
    , cp2(0.0, 0.0)
    , length(0.0)
    , mode(eUpdateAll)
  {}

  bool ShouldUpdateLengthAndControlPoints() { return mode == eUpdateAll; }

  Point start; // start point of current sub path (reset each moveto)

  Point pos;   // current position (end point of previous segment)

  Point cp1;   // quadratic control point - if the previous segment was a
               // quadratic bezier curve then this is set to the absolute
               // position of its control point, otherwise its set to pos

  Point cp2;   // cubic control point - if the previous segment was a cubic
               // bezier curve then this is set to the absolute position of
               // its second control point, otherwise it's set to pos

  float length;   // accumulated path length

  TraversalMode mode;  // indicates what to track while traversing a path
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
   * conversion by requiring uint32_t and float to be of the same size so we
   * can simply do a bitwise uint32_t<->float copy.
   */
  static float EncodeType(uint32_t aType) {
    static_assert(sizeof(uint32_t) == sizeof(float), "sizeof uint32_t and float must be the same");
    NS_ABORT_IF_FALSE(IsValidType(aType), "Seg type not recognized");
    return *(reinterpret_cast<float*>(&aType));
  }

  static uint32_t DecodeType(float aType) {
    static_assert(sizeof(uint32_t) == sizeof(float), "sizeof uint32_t and float must be the same");
    uint32_t type = *(reinterpret_cast<uint32_t*>(&aType));
    NS_ABORT_IF_FALSE(IsValidType(type), "Seg type not recognized");
    return type;
  }

  static PRUnichar GetPathSegTypeAsLetter(uint32_t aType) {
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
    static_assert(NS_ARRAY_LENGTH(table) == NS_SVG_PATH_SEG_TYPE_COUNT, "Unexpected table size");

    return table[aType];
  }

  static uint32_t ArgCountForType(uint32_t aType) {
    NS_ABORT_IF_FALSE(IsValidType(aType), "Seg type not recognized");

    static const uint8_t table[] = {
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
    static_assert(NS_ARRAY_LENGTH(table) == NS_SVG_PATH_SEG_TYPE_COUNT, "Unexpected table size");

    return table[aType];
  }

  /**
   * Convenience so that callers can pass a float containing an encoded type
   * and have it decoded implicitly.
   */
  static uint32_t ArgCountForType(float aType) {
    return ArgCountForType(DecodeType(aType));
  }

  static bool IsValidType(uint32_t aType) {
    return aType >= NS_SVG_PATH_SEG_FIRST_VALID_TYPE &&
           aType <= NS_SVG_PATH_SEG_LAST_VALID_TYPE;
  }

  static bool IsCubicType(uint32_t aType) {
    return aType == PATHSEG_CURVETO_CUBIC_REL ||
           aType == PATHSEG_CURVETO_CUBIC_ABS ||
           aType == PATHSEG_CURVETO_CUBIC_SMOOTH_REL ||
           aType == PATHSEG_CURVETO_CUBIC_SMOOTH_ABS;
  }

  static bool IsQuadraticType(uint32_t aType) {
    return aType == PATHSEG_CURVETO_QUADRATIC_REL ||
           aType == PATHSEG_CURVETO_QUADRATIC_ABS ||
           aType == PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL ||
           aType == PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS;
  }

  static bool IsArcType(uint32_t aType) {
    return aType == PATHSEG_ARC_ABS ||
           aType == PATHSEG_ARC_REL;
  }

  static bool IsRelativeOrAbsoluteType(uint32_t aType) {
    NS_ABORT_IF_FALSE(IsValidType(aType), "Seg type not recognized");

    // When adding a new path segment type, ensure that the returned condition
    // below is still correct.
    static_assert(NS_SVG_PATH_SEG_LAST_VALID_TYPE == PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL,
                  "Unexpected type");

    return aType >= PATHSEG_MOVETO_ABS;
  }

  static bool IsRelativeType(uint32_t aType) {
    NS_ABORT_IF_FALSE
      (IsRelativeOrAbsoluteType(aType),
       "IsRelativeType called with segment type that does not come in relative and absolute forms");

    // When adding a new path segment type, ensure that the returned condition
    // below is still correct.
    static_assert(NS_SVG_PATH_SEG_LAST_VALID_TYPE == PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL,
                  "Unexpected type");

    return aType & 1;
  }

  static uint32_t RelativeVersionOfType(uint32_t aType) {
    NS_ABORT_IF_FALSE
      (IsRelativeOrAbsoluteType(aType),
       "RelativeVersionOfType called with segment type that does not come in relative and absolute forms");

    // When adding a new path segment type, ensure that the returned condition
    // below is still correct.
    static_assert(NS_SVG_PATH_SEG_LAST_VALID_TYPE == PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL,
                  "Unexpected type");

    return aType | 1;
  }

  static uint32_t SameTypeModuloRelativeness(uint32_t aType1, uint32_t aType2) {
    if (!IsRelativeOrAbsoluteType(aType1)) {
      return aType1 == aType2;
    }

    return RelativeVersionOfType(aType1) == RelativeVersionOfType(aType2);
  }

  /**
   * Traverse the given path segment and update the SVGPathTraversalState
   * object.
   */
  static void TraversePathSegment(const float* aData,
                                  SVGPathTraversalState& aState);
};

} // namespace mozilla

#endif // MOZILLA_SVGPATHSEGUTILS_H__
