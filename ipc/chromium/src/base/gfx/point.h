// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_POINT_H__
#define BASE_GFX_POINT_H__

#include "build/build_config.h"

#include <ostream>

#if defined(OS_WIN)
typedef struct tagPOINT POINT;
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

//
// A point has an x and y coordinate.
//
class Point {
 public:
  Point();
  Point(int x, int y);
#if defined(OS_WIN)
  explicit Point(const POINT& point);
  Point& operator=(const POINT& point);
#elif defined(OS_MACOSX)
  explicit Point(const CGPoint& point);
#endif

  ~Point() {}

  int x() const { return x_; }
  int y() const { return y_; }

  void SetPoint(int x, int y) {
    x_ = x;
    y_ = y;
  }

  void set_x(int x) { x_ = x; }
  void set_y(int y) { y_ = y; }

  void Offset(int delta_x, int delta_y) {
    x_ += delta_x;
    y_ += delta_y;
  }

  bool operator==(const Point& rhs) const {
    return x_ == rhs.x_ && y_ == rhs.y_;
  }

  bool operator!=(const Point& rhs) const {
    return !(*this == rhs);
  }

#if defined(OS_WIN)
  POINT ToPOINT() const;
#elif defined(OS_MACOSX)
  CGPoint ToCGPoint() const;
#endif

 private:
  int x_;
  int y_;
};

}  // namespace gfx

inline std::ostream& operator<<(std::ostream& out, const gfx::Point& p) {
  return out << p.x() << "," << p.y();
}

#endif // BASE_GFX_POINT_H__
