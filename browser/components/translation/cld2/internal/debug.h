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
// Produces debugging output for CLD2. See debug_empty.h for suppressing this.


#ifndef I18N_ENCODINGS_CLD2_INTERNAL_DEBUG_H_
#define I18N_ENCODINGS_CLD2_INTERNAL_DEBUG_H_

#include <string>
#include "scoreonescriptspan.h"

namespace CLD2 {

// For showing one chunk
void CLD2_Debug(const char* text,
                int lo_offset,
                int hi_offset,
                bool more_to_come, bool score_cjk,
                const ScoringHitBuffer* hitbuffer,
                const ScoringContext* scoringcontext,
                const ChunkSpan* cspan,
                const ChunkSummary* chunksummary);

// For showing all chunks
void CLD2_Debug2(const char* text,
                 bool more_to_come, bool score_cjk,
                 const ScoringHitBuffer* hitbuffer,
                 const ScoringContext* scoringcontext,
                 const SummaryBuffer* summarybuffer);

std::string GetPlainEscapedText(const std::string& txt);
std::string GetHtmlEscapedText(const std::string& txt);
std::string GetColorHtmlEscapedText(Language lang, const std::string& txt);
std::string GetLangColorHtmlEscapedText(Language lang, const std::string& txt);

void DumpResultChunkVector(FILE* f, const char* src,
                           ResultChunkVector* resultchunkvector);


}     // End namespace CLD2

#endif  // I18N_ENCODINGS_CLD2_INTERNAL_DEBUG_H_

