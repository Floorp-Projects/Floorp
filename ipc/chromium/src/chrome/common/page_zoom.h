// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAGE_ZOOM_H_
#define CHROME_COMMON_PAGE_ZOOM_H_

// This enum is the parameter to various text/page zoom commands so we know
// what the specific zoom command is.
class PageZoom {
 public:
  enum Function {
    SMALLER  = -1,
    STANDARD = 0,
    LARGER   = 1,
  };

 private:
  PageZoom() {}  // For scoping only.
};

#endif  // CHROME_COMMON_PAGE_ZOOM_H_
