// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json_writer.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/string_escape.h"

const char kPrettyPrintLineEnding[] = "\r\n";

/* static */
void JSONWriter::Write(const Value* const node, bool pretty_print,
                       std::string* json) {
  json->clear();
  // Is there a better way to estimate the size of the output?
  json->reserve(1024);
  JSONWriter writer(pretty_print, json);
  writer.BuildJSONString(node, 0);
  if (pretty_print)
    json->append(kPrettyPrintLineEnding);
}

JSONWriter::JSONWriter(bool pretty_print, std::string* json)
    : json_string_(json),
      pretty_print_(pretty_print) {
  DCHECK(json);
}

void JSONWriter::BuildJSONString(const Value* const node, int depth) {
  switch(node->GetType()) {
    case Value::TYPE_NULL:
      json_string_->append("null");
      break;

    case Value::TYPE_BOOLEAN:
      {
        bool value;
        bool result = node->GetAsBoolean(&value);
        DCHECK(result);
        json_string_->append(value ? "true" : "false");
        break;
      }

    case Value::TYPE_INTEGER:
      {
        int value;
        bool result = node->GetAsInteger(&value);
        DCHECK(result);
        StringAppendF(json_string_, "%d", value);
        break;
      }

    case Value::TYPE_REAL:
      {
        double value;
        bool result = node->GetAsReal(&value);
        DCHECK(result);
        std::string real = DoubleToString(value);
        // Ensure that the number has a .0 if there's no decimal or 'e'.  This
        // makes sure that when we read the JSON back, it's interpreted as a
        // real rather than an int.
        if (real.find('.') == std::string::npos &&
            real.find('e') == std::string::npos &&
            real.find('E') == std::string::npos) {
          real.append(".0");
        }
        // The JSON spec requires that non-integer values in the range (-1,1)
        // have a zero before the decimal point - ".52" is not valid, "0.52" is.
        if (real[0] == '.') {
          real.insert(0, "0");
        } else if (real.length() > 1 && real[0] == '-' && real[1] == '.') {
          // "-.1" bad "-0.1" good
          real.insert(1, "0");
        }
        json_string_->append(real);
        break;
      }

    case Value::TYPE_STRING:
      {
        std::wstring value;
        bool result = node->GetAsString(&value);
        DCHECK(result);
        AppendQuotedString(value);
        break;
      }

    case Value::TYPE_LIST:
      {
        json_string_->append("[");
        if (pretty_print_)
          json_string_->append(" ");

        const ListValue* list = static_cast<const ListValue*>(node);
        for (size_t i = 0; i < list->GetSize(); ++i) {
          if (i != 0) {
            json_string_->append(",");
            if (pretty_print_)
              json_string_->append(" ");
          }

          Value* value = NULL;
          bool result = list->Get(i, &value);
          DCHECK(result);
          BuildJSONString(value, depth);
        }

        if (pretty_print_)
          json_string_->append(" ");
        json_string_->append("]");
        break;
      }

    case Value::TYPE_DICTIONARY:
      {
        json_string_->append("{");
        if (pretty_print_)
          json_string_->append(kPrettyPrintLineEnding);

        const DictionaryValue* dict =
          static_cast<const DictionaryValue*>(node);
        for (DictionaryValue::key_iterator key_itr = dict->begin_keys();
             key_itr != dict->end_keys();
             ++key_itr) {

          if (key_itr != dict->begin_keys()) {
            json_string_->append(",");
            if (pretty_print_)
              json_string_->append(kPrettyPrintLineEnding);
          }

          Value* value = NULL;
          bool result = dict->Get(*key_itr, &value);
          DCHECK(result);

          if (pretty_print_)
            IndentLine(depth + 1);
          AppendQuotedString(*key_itr);
          if (pretty_print_) {
            json_string_->append(": ");
          } else {
            json_string_->append(":");
          }
          BuildJSONString(value, depth + 1);
        }

        if (pretty_print_) {
          json_string_->append(kPrettyPrintLineEnding);
          IndentLine(depth);
          json_string_->append("}");
        } else {
          json_string_->append("}");
        }
        break;
      }

    default:
      // TODO(jhughes): handle TYPE_BINARY
      NOTREACHED() << "unknown json type";
  }
}

void JSONWriter::AppendQuotedString(const std::wstring& str) {
  string_escape::JavascriptDoubleQuote(WideToUTF16Hack(str), true,
                                       json_string_);
}

void JSONWriter::IndentLine(int depth) {
  // It may be faster to keep an indent string so we don't have to keep
  // reallocating.
  json_string_->append(std::string(depth * 3, ' '));
}
