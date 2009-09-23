// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACCESSIBILITY_TYPES_H_
#define CHROME_COMMON_ACCESSIBILITY_TYPES_H_

////////////////////////////////////////////////////////////////////////////////
//
// AccessibilityTypes
//
// Provides enumerations used to preserve platform-independence in accessibility
// functions used in various Views, both in Browser\Views and Views.
//
////////////////////////////////////////////////////////////////////////////////
class AccessibilityTypes {
 public:
  // This defines an enumeration of the supported accessibility roles in our
  // Views (e.g. used in View::GetAccessibleRole). Any interface using roles
  // must provide a conversion to its own roles (see e.g.
  // ViewAccessibility::get_accRole and ViewAccessibility::MSAARole).
  enum Role {
    ROLE_APPLICATION,
    ROLE_BUTTONDROPDOWN,
    ROLE_CLIENT,
    ROLE_GROUPING,
    ROLE_PAGETAB,
    ROLE_PUSHBUTTON,
    ROLE_TEXT,
    ROLE_TOOLBAR
  };

  // This defines an enumeration of the supported accessibility roles in our
  // Views (e.g. used in View::GetAccessibleState). Any interface using roles
  // must provide a conversion to its own roles (see e.g.
  // ViewAccessibility::get_accState and ViewAccessibility::MSAAState).
  enum State {
    STATE_HASPOPUP,
    STATE_READONLY
  };

 private:
  // Do not instantiate this class.
  AccessibilityTypes() {}
  ~AccessibilityTypes() {}
};

#endif  // CHROME_COMMON_ACCESSIBILITY_TYPES_H_
