/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JSONFormatter.h"

#include <algorithm>
#include <cassert>
#include <string.h>

static std::string replaceAll(std::string Mangled, std::string Pattern,
                              std::string Replacement) {
  size_t Pos = 0;
  while ((Pos = Mangled.find(Pattern, Pos)) != std::string::npos) {
    Mangled = Mangled.replace(Pos, Pattern.length(), Replacement);
    Pos += Replacement.length();
  }
  return Mangled;
}

/**
 * Hacky escaping logic with the goal of not upsetting the much more thorough
 * rust JSON parsing library that actually understands UTF-8.  Double-quote
 * and (escaping) backslash are escaped, as is tab (\t), with newlines (\r\n
 * and \n) normalized to escaped \n.
 *
 * Additionally, everything that's not printable ASCII is simply erased.  The
 * motivating file is media/openmax_il/il112/OMX_Other.h#93 which has a
 * corrupted apostrophe as <92> in there.  The better course of action would
 * be a validating UTF-8 parse that discards corrupt/non-printable characters.
 * Since this is motivated by a commenting proof-of-concept and builds are
 * already slow, I'm punting on that.
 */
std::string JSONFormatter::escape(std::string Input) {
  bool NeedsEscape = false;
  for (char C : Input) {
    if (C == '\\' || C == '"' || C < 32 || C > 126) {
      NeedsEscape = true;
      break;
    }
  }

  if (!NeedsEscape) {
    return Input;
  }

  std::string Cur = Input;
  Cur = replaceAll(Cur, "\\", "\\\\");
  Cur = replaceAll(Cur, "\"", "\\\"");
  Cur = replaceAll(Cur, "\t", "\\t");
  Cur = replaceAll(Cur, "\r\n", "\\n");
  Cur = replaceAll(Cur, "\n", "\\n");
  Cur.erase(std::remove_if(Cur.begin(), Cur.end(),
                           [](char C){ return C < 32 || C > 126; }),
            Cur.end());
  return Cur;
}

void JSONFormatter::add(const char *Name, const char *Value) {
  assert(PropertyCount < kMaxProperties);
  Properties[PropertyCount] = Property(Name, std::string(Value));
  PropertyCount++;

  // `"Name":"Value",`
  Length += strlen(Name) + 3 + strlen(Value) + 2 + 1;
}

void JSONFormatter::add(const char *Name, std::string Value) {
  std::string Escaped = escape(std::move(Value));

  // `"Name":"Escaped",`
  Length += strlen(Name) + 3 + Escaped.length() + 2 + 1;

  assert(PropertyCount < kMaxProperties);
  Properties[PropertyCount] = Property(Name, std::move(Escaped));
  PropertyCount++;
}

void JSONFormatter::add(const char *Name, int Value) {
  // 1 digit
  assert(Value >= 0 && Value < 10);

  assert(PropertyCount < kMaxProperties);
  Properties[PropertyCount] = Property(Name, Value);
  PropertyCount++;

  // `"Name":V,`
  Length += strlen(Name) + 3 + 2;
}

void JSONFormatter::format(std::string &Result) {
  Result.reserve(Length + 2);

  Result.push_back('{');
  for (int I = 0; I < PropertyCount; I++) {
    Result.push_back('"');
    Result.append(Properties[I].Name);
    Result.push_back('"');
    Result.push_back(':');

    if (Properties[I].IsString) {
      Result.push_back('"');
      Result.append(Properties[I].StringValue);
      Result.push_back('"');
    } else {
      Result.push_back(Properties[I].IntValue + '0');
    }

    if (I + 1 != PropertyCount) {
      Result.push_back(',');
    }
  }

  Result.push_back('}');
  Result.push_back('\n');

  assert(Result.length() == Length + 2);
}
