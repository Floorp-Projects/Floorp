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
// Compile this in instead of debug.cc to remove code for debug output
//

#include "debug.h"
#include <string>

using namespace std;

namespace CLD2 {

string GetPlainEscapedText(const string& txt) {return string("");}

string GetHtmlEscapedText(const string& txt) {return string("");}

string GetColorHtmlEscapedText(Language lang, const string& txt) {
  return string("");
}

string GetLangColorHtmlEscapedText(Language lang, const string& txt) {
  return string("");
}


// For showing one chunk
// Print debug output for one scored chunk
// Optionally print out per-chunk scoring information
// In degenerate cases, hitbuffer and cspan can be NULL
void CLD2_Debug(const char* text,
                int lo_offset,
                int hi_offset,
                bool more_to_come, bool score_cjk,
                const ScoringHitBuffer* hitbuffer,
                const ScoringContext* scoringcontext,
                const ChunkSpan* cspan,
                const ChunkSummary* chunksummary) {}

// For showing all chunks
void CLD2_Debug2(const char* text,
                 bool more_to_come, bool score_cjk,
                 const ScoringHitBuffer* hitbuffer,
                 const ScoringContext* scoringcontext,
                 const SummaryBuffer* summarybuffer) {}

void DumpResultChunkVector(FILE* f, const char* src,
                           ResultChunkVector* resultchunkvector) {}

}       // End namespace CLD2

