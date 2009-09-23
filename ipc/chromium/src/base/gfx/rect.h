// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple integer rectangle class.  The containment semantics
// are array-like; that is, the coordinate (x, y) is considered to be
// contained by the rectangle, but the coordinate (x + width, y) is not.
// The class will happily let you create malformed rectangles (that is,
// rectangles with negative width and/or height), but there will be assertions
// in the operations (such as contain()) to complain in this case.

#ifndef BASE_GFX_RECT_H__
#define BASE_GFX_RECT_H__

#include <iostream>

#include "base/gfx/point.h"
#include "base/gfx/size.h"

#if defined(OS_WIN)
typedef struct tagRECT RECT;
#elif defined(OS_LINUX)
typedef struct _GdkRectangle GdkRectangle;
#endif

namespace gfx {

class Rect {
 public:
  Rect();
  Rect(int width, int height);
  Rect(int x, int y, int width, int height);
#if defined(OS_WIN)
  explicit Rect(const RECT& r);
#elif defined(OS_MACOSX)
  explicit Rect(const CGRect& r);
#elif defined(OS_LINUX)
  explicit Rect(const GdkRectangle& r);
#endif
  Rect(const gfx::Point& origin, const gfx::Size& size);

  ~Rect() {}

#if defined(OS_WIN)
  Rect& operator=(const RECT& r);
#elif defined(OS_MACOSX)
  Rect& operator=(const CGRect& r);
#elif defined(OS_LINUX)
  Rect& operator=(const GdkRectangle& r);
#endif

  int x() const { return origin_.x(); }
  void set_x(int x) { origin_.set_x(x); }

  int y() const { return origin_.y(); }
  void set_y(int y) { origin_.set_y(y); }

  int width() const { return size_.width(); }
  void set_width(int width);

  int height() const { return size_.height(); }
  void set_height(int height);

  const gfx::Point& origin() const { return origin_; }
  void set_origin(const gfx::Point& origin) { origin_ = origin; }

  const gfx::Size& size() const { return size_; }

  int right() const { return x() + width(); }
  int bottom() const { return y() + height(); }

  void SetRect(int x, int y, int width, int height);

  // Shrink the rectangle by a horizontal and vertical distance on all sides.
  void Inset(int horizontal, int vertical) {
    Inset(horizontal, vertical, horizontal, vertical);
  }

  // Shrink the rectangle by the specified amount on each side.
  void Inset(int left, int top, int right, int bottom);

  // Move the rectangle by a horizontal and vertical distance.
  void Offset(int horizontal, int vertical);
  void Offset(const gfx::Point& point) {
    Offset(point.x(), point.y());
  }

  // Returns true if the area of the rectangle is zero.
  bool IsEmpty() const { return size_.IsEmpty(); }

  bool operator==(const Rect& other) const;

  bool operator!=(const Rect& other) const {
    return !(*this == other);
  }

#if defined(OS_WIN)
  // Construct an equivalent Win32 RECT object.
  RECT ToRECT() const;
#elif defined(OS_LINUX)
  GdkRectangle ToGdkRectangle() const;
#elif defined(OS_MACOSX)
  // Construct an equivalent CoreGraphics object.
  CGRect ToCGRect() const;
#endif

  // Returns true if the point identified by point_x and point_y falls inside
  // this rectangle.  The point (x, y) is inside the rectangle, but the
  // point (x + width, y + height) is not.
  bool Contains(int point_x, int point_y) const;

  // Returns true if the specified point is contained by this rectangle.
  bool Contains(const gfx::Point& point) const {
    return Contains(point.x(), point.y());
  }

  // Returns true if this rectangle contains the specified rectangle.
  bool Contains(const Rect& rect) const;

  // Returns true if this rectangle intersects the specified rectangle.
  bool Intersects(const Rect& rect) const;

  // Computes the intersection of this rectangle with the given rectangle.
  Rect Intersect(const Rect& rect) const;

  // Computes the union of this rectangle with the given rectangle.  The union
  // is the smallest rectangle containing both rectangles.
  Rect Union(const Rect& rect) const;

  // Computes the rectangle resulting from subtracting |rect| from |this|.  If
  // |rect| does not intersect completely in either the x- or y-direction, then
  // |*this| is returned.  If |rect| contains |this|, then an empty Rect is
  // returned.
  Rect Subtract(const Rect& rect) const;

  // Returns true if this rectangle equals that of the supplied rectangle.
  bool Equals(const Rect& rect) const {
    return *this == rect;
  }

  // Fits as much of the receiving rectangle into the supplied rectangle as
  // possible, returning the result. For example, if the receiver had
  // a x-location of 2 and a width of 4, and the supplied rectangle had
  // an x-location of 0 with a width of 5, the returned rectangle would have
  // an x-location of 1 with a width of 4.
  Rect AdjustToFit(const Rect& rect) const;

  // Returns the center of this rectangle.
  Point CenterPoint() const;

 private:
  gfx::Point origin_;
  gfx::Size size_;
};

}  // namespace gfx

inline std::ostream& operator<<(std::ostream& out, const gfx::Rect& r) {
  return out << r.origin() << " " << r.size();
}

#endif  // BASE_GFX_RECT_H__
