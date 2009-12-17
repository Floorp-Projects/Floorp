// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides some helper methods for building and rendering an
// internal html page.  The flow is as follows:
// - instantiate a builder given a webframe that we're going to render content
//   into
// - load the template html and load the jstemplate javascript into the frame
// - given a json data object, run the jstemplate javascript which fills in
//   template values

#ifndef CHROME_COMMON_JSTEMPLATE_BUILDER_H_
#define CHROME_COMMON_JSTEMPLATE_BUILDER_H_

#include <string>

#include "base/values.h"

class StringPiece;

namespace jstemplate_builder {
  // A helper function that generates a string of HTML to be loaded.  The
  // string includes the HTML and the javascript code necessary to generate the
  // full page.
  std::string GetTemplateHtml(const StringPiece& html_template,
                              const DictionaryValue* json,
                              const StringPiece& template_id);
}  // namespace jstemplate_builder
#endif  // CHROME_COMMON_JSTEMPLATE_BUILDER_H_
