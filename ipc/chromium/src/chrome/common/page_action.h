// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAGE_ACTION_H_
#define CHROME_COMMON_PAGE_ACTION_H_

#include <string>
#include <map>

#include "base/file_path.h"
#include "googleurl/src/gurl.h"

class PageAction {
 public:
  PageAction();
  virtual ~PageAction();

  typedef enum {
    PERMANENT = 0,
    TAB = 1,
  } PageActionType;

  std::string id() const { return id_; }
  void set_id(std::string id) { id_ = id; }

  PageActionType type() const { return type_; }
  void set_type(PageActionType type) { type_ = type; }

  std::string extension_id() const { return extension_id_; }
  void set_extension_id(std::string extension_id) {
    extension_id_ = extension_id;
  }

  std::string name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  FilePath icon_path() const { return icon_path_; }
  void set_icon_path(const FilePath& icon_path) {
    icon_path_ = icon_path;
  }

  std::string tooltip() const { return tooltip_; }
  void set_tooltip(const std::string& tooltip) {
    tooltip_ = tooltip;
  }

 private:
  // The id for the PageAction, for example: "RssPageAction".
  std::string id_;

  // The type of the PageAction.
  PageActionType type_;

  // The id for the extension this PageAction belongs to (as defined in the
  // extension manifest).
  std::string extension_id_;

  // The name of the PageAction.
  std::string name_;

  // The icon that represents the PageIcon.
  FilePath icon_path_;

  // The tooltip to show when the mouse hovers over the icon of the page action.
  std::string tooltip_;
};

typedef std::map<std::string, PageAction*> PageActionMap;

#endif  // CHROME_COMMON_PAGE_ACTION_H_
