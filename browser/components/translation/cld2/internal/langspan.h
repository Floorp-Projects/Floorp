// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// Author: dsites@google.com (Dick Sites)
//


#ifndef I18N_ENCODINGS_CLD2_INTERNAL_LANGSPAN_H_
#define I18N_ENCODINGS_CLD2_INTERNAL_LANGSPAN_H_

#include "generated_language.h"
#include "generated_ulscript.h"

namespace CLD2 {

typedef struct {
  char* text;             // Pointer to the span, somewhere
  int text_bytes;         // Number of bytes of text in the span
  int offset;             // Offset of start of span in original input buffer
  ULScript ulscript;      // Unicode Letters Script of this span
  Language lang;          // Language identified for this span
  bool truncated;         // true if buffer filled up before a
                          // different script or EOF was found
} LangSpan;

}  // namespace CLD2
#endif  // I18N_ENCODINGS_CLD2_INTERNAL_LANGSPAN_H_

