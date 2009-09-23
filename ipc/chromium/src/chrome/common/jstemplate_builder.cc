// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper function for using JsTemplate. See jstemplate_builder.h for more
// info.

#include "chrome/common/jstemplate_builder.h"

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/json_value_serializer.h"

#include "grit/common_resources.h"

namespace jstemplate_builder {

std::string GetTemplateHtml(const StringPiece& html_template,
                            const DictionaryValue* json,
                            const StringPiece& template_id) {
  // fetch and cache the pointer of the jstemplate resource source text.
  static const StringPiece jstemplate_src(
    ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_JSTEMPLATE_JS));

  if (jstemplate_src.empty()) {
    NOTREACHED() << "Unable to get jstemplate src";
    return std::string();
  }

  // Convert the template data to a json string.
  DCHECK(json) << "must include json data structure";

  std::string jstext;
  JSONStringValueSerializer serializer(&jstext);
  serializer.Serialize(*json);
  // </ confuses the HTML parser because it could be a </script> tag.  So we
  // replace </ with <\/.  The extra \ will be ignored by the JS engine.
  ReplaceSubstringsAfterOffset(&jstext, 0, "</", "<\\/");

  std::string output(html_template.data(), html_template.size());
  output.append("<script>");
  output.append(jstemplate_src.data(), jstemplate_src.size());
  output.append("var tp = document.getElementById('");
  output.append(template_id.data(), template_id.size());
  output.append("'); var cx = new JsExprContext(");
  output.append(jstext);
  output.append("); jstProcess(cx, tp);</script>");

  return output;
}

}  // namespace jstemplate_builder
