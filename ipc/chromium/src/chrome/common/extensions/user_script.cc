// Copyright 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/user_script.h"

#include "base/pickle.h"
#include "base/string_util.h"

bool UserScript::MatchesUrl(const GURL& url) {
  for (std::vector<std::string>::const_iterator glob = globs_.begin();
       glob != globs_.end(); ++glob) {
    if (MatchPattern(url.spec(), *glob))
      return true;
  }

  for (std::vector<URLPattern>::iterator pattern = url_patterns_.begin();
       pattern != url_patterns_.end(); ++pattern) {
    if (pattern->MatchesUrl(url))
      return true;
  }

  return false;
}

void UserScript::File::Pickle(::Pickle* pickle) const {
  pickle->WriteString(url_.spec());
  // Do not write path. It's not needed in the renderer.
  // Do not write content. It will be serialized by other means.
}

void UserScript::File::Unpickle(const ::Pickle& pickle, void** iter) {
  // Read url.
  std::string url;
  CHECK(pickle.ReadString(iter, &url));
  set_url(GURL(url));
}

void UserScript::Pickle(::Pickle* pickle) const {
  // Write the run location.
  pickle->WriteInt(run_location());

  // Write the extension id.
  pickle->WriteString(extension_id());

  // Write globs.
  pickle->WriteSize(globs_.size());
  for (std::vector<std::string>::const_iterator glob = globs_.begin();
       glob != globs_.end(); ++glob) {
    pickle->WriteString(*glob);
  }

  // Write url patterns.
  pickle->WriteSize(url_patterns_.size());
  for (std::vector<URLPattern>::const_iterator pattern = url_patterns_.begin();
       pattern != url_patterns_.end(); ++pattern) {
    pickle->WriteString(pattern->GetAsString());
  }

  // Write js scripts.
  pickle->WriteSize(js_scripts_.size());
  for (FileList::const_iterator file = js_scripts_.begin();
    file != js_scripts_.end(); ++file) {
    file->Pickle(pickle);
  }

  // Write css scripts.
  pickle->WriteSize(css_scripts_.size());
  for (FileList::const_iterator file = css_scripts_.begin();
    file != css_scripts_.end(); ++file) {
    file->Pickle(pickle);
  }
}

void UserScript::Unpickle(const ::Pickle& pickle, void** iter) {
  // Read the run location.
  int run_location = 0;
  CHECK(pickle.ReadInt(iter, &run_location));
  CHECK(run_location >= 0 && run_location < RUN_LOCATION_LAST);
  run_location_ = static_cast<RunLocation>(run_location);

  // Read the extension ID.
  CHECK(pickle.ReadString(iter, &extension_id_));

  // Read globs.
  size_t num_globs = 0;
  CHECK(pickle.ReadSize(iter, &num_globs));

  globs_.clear();
  for (size_t i = 0; i < num_globs; ++i) {
    std::string glob;
    CHECK(pickle.ReadString(iter, &glob));
    globs_.push_back(glob);
  }

  // Read url patterns.
  size_t num_patterns = 0;
  CHECK(pickle.ReadSize(iter, &num_patterns));

  url_patterns_.clear();
  for (size_t i = 0; i < num_patterns; ++i) {
    std::string pattern_str;
    URLPattern pattern;
    CHECK(pickle.ReadString(iter, &pattern_str));
    CHECK(pattern.Parse(pattern_str));
    url_patterns_.push_back(pattern);
  }

  // Read js scripts.
  size_t num_js_files = 0;
  CHECK(pickle.ReadSize(iter, &num_js_files));
  js_scripts_.clear();
  for (size_t i = 0; i < num_js_files; ++i) {
    File file;
    file.Unpickle(pickle, iter);
    js_scripts_.push_back(file);
  }

  // Read css scripts.
  size_t num_css_files = 0;
  CHECK(pickle.ReadSize(iter, &num_css_files));
  css_scripts_.clear();
  for (size_t i = 0; i < num_css_files; ++i) {
    File file;
    file.Unpickle(pickle, iter);
    css_scripts_.push_back(file);
  }
}
