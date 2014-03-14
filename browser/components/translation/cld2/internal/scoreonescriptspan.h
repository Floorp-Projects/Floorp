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
//
// Terminology:
// Incoming original text has HTML tags and entities removed, all but letters
// removed, and letters lowercased. Strings of non-letters are mapped to a
// single ASCII space.
//
// One scriptspan has a run of letters/spaces  in a single script. This is the
// fundamental text unit that is scored. There is an optional backmap from
// scriptspan text to the original document text, so that the language ranges
// reported in ResultChunkVector refer to byte ranges inthe original text.
//
// Scripts come in two forms, the full Unicode scripts described by
//   http://www.unicode.org/Public/UNIDATA/Scripts.txt
// and a modified list used exclusively in CLD2. The modified form maps all
// the CJK scripts to one, Hani. The current version description is in
//  i18n/encodings/cld2/builddata/script_summary.txt
// In addition, all non-letters are mapped to the Common script.
//
// ULScript describes this Unicode Letter script.
//
// Scoring uses text nil-grams, uni-grams, bi-grams, quad-grams, and octa-grams.
// Nilgrams (no text lookup at all) are for script-based pseudo-languages and
// for languages that are 1:1 with a given script. Unigrams and bigrams are
// used to score the CJK languages, all in the Hani script. Quadgrams and
// octagrams are used to score all other languages.
//
// RType is the Recognition Type per ulscript.
//
// The scoring tables map various grams to language-probability scores.
// A given gram that hits in scoring table maps to an indirect subscript into
// a list of packed languages and log probabilities.
//
// Languages are stored in two forms: 10-bit values in the Languge enum, and
// shorter 8-bit per-ulscript values in the scoring tables.
//
// Language refers to the full 10-bit range.
// pslang refers to the per-ulscript shorter values.
//
// Log probabilities also come in two forms. The full range uses values 0..255
// to represent minus log base 10th-root-of-2, covering 1 .. 1/2**25.5 or about
// TODO BOGUS description, 24 vs 12
// 1/47.5M. The second form quantizes these into multiples of 8 that can be
// added together to represent probability products. The quantized form uses
// values 24..0 with 0 now least likely instead of most likely, thus making
// larger sums for more probable results. 24 maps to original 1/2**4.8 (~1/28)
// and 0 maps to original 1/2**24.0 (~1/16M).
//
// qprob refers to quantized log probabilities.
//
// langprob is a uint32 holding three 1-byte pslangs and a 1-byte subscript to
// a list of three qprobs. It always nees a companion ulscript
//
// A scriptspan is scored via one or more hitbuffers


#ifndef I18N_ENCODINGS_CLD2_INTERNAL_SCOREONESCRIPTSPAN_H__
#define I18N_ENCODINGS_CLD2_INTERNAL_SCOREONESCRIPTSPAN_H__

#include <stdio.h>

#include "integral_types.h"           // for uint8 etc.

#include "cld2tablesummary.h"
#include "compact_lang_det_impl.h"    // for ResultChunkVector
#include "getonescriptspan.h"
#include "langspan.h"
#include "tote.h"
#include "utf8statetable.h"

namespace CLD2 {

static const int kMaxBoosts = 4;              // For each of PerScriptLangBoosts
                                              // must be power of two for wrap()
static const int kChunksizeQuads = 20;        // For non-CJK
static const int kChunksizeUnis = 50;         // For CJK
static const int kMaxScoringHits = 1000;
static const int kMaxSummaries = kMaxScoringHits / kChunksizeQuads;


// The first four tables are for CJK languages,
// the next three for quadgram languages, and
// the last for expected scores.
typedef struct {
  const UTF8PropObj* unigram_obj;               // 80K CJK characters
  const CLD2TableSummary* unigram_compat_obj;   // 256 CJK lookup probabilities
  const CLD2TableSummary* deltabi_obj;
  const CLD2TableSummary* distinctbi_obj;

  const CLD2TableSummary* quadgram_obj;         // Primary quadgram lookup table
  const CLD2TableSummary* quadgram_obj2;        // Secondary  "
  const CLD2TableSummary* deltaocta_obj;
  const CLD2TableSummary* distinctocta_obj;

  const short* kExpectedScore;      // Expected base + delta + distinct score
                                    // per 1KB input
                                    // Subscripted by language and script4
} ScoringTables;

// Context for boosting several languages
typedef struct {
   int32 n;
   uint32 langprob[kMaxBoosts];
   int wrap(int32 n) {return n & (kMaxBoosts - 1);}
} LangBoosts;

typedef struct {
   LangBoosts latn;
   LangBoosts othr;
} PerScriptLangBoosts;



// ScoringContext carries state across scriptspans
// ScoringContext also has read-only scoring tables mapping grams to qprobs
typedef struct {
  FILE* debug_file;                   // Non-NULL if debug output wanted
  bool flags_cld2_score_as_quads;
  bool flags_cld2_html;
  bool flags_cld2_cr;
  bool flags_cld2_verbose;
  ULScript ulscript;        // langprobs below are with respect to this script
  Language prior_chunk_lang;          // Mostly for debug output
  // boost has a packed set of per-script langs and probabilites
  // whack has a per-script lang to be suppressed from ever scoring (zeroed)
  // When a language in a close set is given as an explicit hint, others in
  //  that set will be whacked.
  PerScriptLangBoosts langprior_boost;  // From http content-lang or meta lang=
  PerScriptLangBoosts langprior_whack;  // From http content-lang or meta lang=
  PerScriptLangBoosts distinct_boost;   // From distinctive letter groups
  int oldest_distinct_boost;          // Subscript in hitbuffer of oldest
                                      // distinct score to use
  const ScoringTables* scoringtables; // Probability lookup tables
  ScriptScanner* scanner;             // For ResultChunkVector backmap

  // Inits boosts
  void init() {
    memset(&langprior_boost, 0, sizeof(langprior_boost));
    memset(&langprior_whack, 0, sizeof(langprior_whack));
    memset(&distinct_boost, 0, sizeof(distinct_boost));
  };
} ScoringContext;



// Begin private

// Holds one scoring-table lookup hit. We hold indirect subscript instead of
// langprob to allow a single hit to use a variable number of langprobs.
typedef struct {
  int offset;         // First byte of quad/octa etc. in scriptspan
  int indirect;       // subscript of langprobs in scoring table
} ScoringHit;

typedef enum {
  UNIHIT                       = 0,
  QUADHIT                      = 1,
  DELTAHIT                     = 2,
  DISTINCTHIT                  = 3
} LinearHitType;

// Holds one scoring-table lookup hit resolved into a langprob.
typedef struct {
  uint16 offset;      // First byte of quad/octa etc. in scriptspan
  uint16 type;        // LinearHitType
  uint32 langprob;    // langprob from scoring table
} LangprobHit;

// Holds arrays of scoring-table lookup hits for (part of) a scriptspan
typedef struct {
  ULScript ulscript;        // langprobs below are with respect to this script
  int maxscoringhits;       // determines size of arrays below
  int next_base;            // First unused entry in each array
  int next_delta;           //   "
  int next_distinct;        //   "
  int next_linear;          //   "
  int next_chunk_start;     // First unused chunk_start entry
  int lowest_offset;        // First byte of text span used to fill hitbuffer
  // Dummy entry at the end of each giving offset of first unused text byte
  ScoringHit base[kMaxScoringHits + 1];         // Uni/quad hits
  ScoringHit delta[kMaxScoringHits + 1];        // delta-bi/delta-octa hits
  ScoringHit distinct[kMaxScoringHits + 1];     // distinct-word hits
  LangprobHit linear[4 * kMaxScoringHits + 1];  // Above three merge-sorted
                                                // (4: some bases => 2 linear)
  int chunk_start[kMaxSummaries + 1];           // First linear[] subscr of
                                                //  each scored chunk
  int chunk_offset[kMaxSummaries + 1];          // First text subscr of
                                                //  each scored chunk

  void init() {
    ulscript = ULScript_Common;
    maxscoringhits = kMaxScoringHits;
    next_base = 0;
    next_delta = 0;
    next_distinct = 0;
    next_linear = 0;
    next_chunk_start = 0;
    lowest_offset = 0;
    base[0].offset = 0;
    base[0].indirect = 0;
    delta[0].offset = 0;
    delta[0].indirect = 0;
    distinct[0].offset = 0;
    distinct[0].indirect = 0;
    linear[0].offset = 0;
    linear[0].langprob = 0;
    chunk_start[0] = 0;
    chunk_offset[0] = 0;
  };
} ScoringHitBuffer;

// TODO: Explain here why we need both ChunkSpan and ChunkSummary
typedef struct {
  int chunk_base;       // Subscript of first hitbuffer.base[] in chunk
  int chunk_delta;      // Subscript of first hitbuffer.delta[]
  int chunk_distinct;   // Subscript of first hitbuffer.distinct[]
  int base_len;         // Number of hitbuffer.base[] in chunk
  int delta_len;        // Number of hitbuffer.delta[] in chunk
  int distinct_len;     // Number of hitbuffer.distinct[] in chunk
} ChunkSpan;


// Packed into 20 bytes for space
typedef struct {
  uint16 offset;              // Text offset within current scriptspan.text
  uint16 chunk_start;         // Scoring subscr within hitbuffer->linear[]
  uint16 lang1;               // Top lang, mapped to full Language
  uint16 lang2;               // Second lang, mapped to full Language
  uint16 score1;              // Top lang raw score
  uint16 score2;              // Second lang raw score
  uint16 bytes;               // Number of lower letters bytes in chunk
  uint16 grams;               // Number of scored base quad- uni-grams in chunk
  uint16 ulscript;            // ULScript of chunk
  uint8 reliability_delta;    // Reliability 0..100, delta top:second scores
  uint8 reliability_score;    // Reliability 0..100, top:expected score
} ChunkSummary;


// We buffer up ~50 chunk summaries, corresponding to chunks of 20 quads in a
// 1000-quad hit buffer, so we can do boundary adjustment on them
// when adjacent entries are different languages. After that, we add them
// all into the document score
//
// About 50 * 20 = 1000 bytes. OK for stack alloc
typedef struct {
  int n;
  ChunkSummary chunksummary[kMaxSummaries + 1];
} SummaryBuffer;

// End private


// Score RTypeNone or RTypeOne scriptspan into doc_tote and vec, updating
// scoringcontext
void ScoreEntireScriptSpan(const LangSpan& scriptspan,
                           ScoringContext* scoringcontext,
                           DocTote* doc_tote,
                           ResultChunkVector* vec);

// Score RTypeCJK scriptspan into doc_tote and vec, updating scoringcontext
void ScoreCJKScriptSpan(const LangSpan& scriptspan,
                        ScoringContext* scoringcontext,
                        DocTote* doc_tote,
                        ResultChunkVector* vec);

// Score RTypeMany scriptspan into doc_tote and vec, updating scoringcontext
void ScoreQuadScriptSpan(const LangSpan& scriptspan,
                         ScoringContext* scoringcontext,
                         DocTote* doc_tote,
                         ResultChunkVector* vec);

// Score one scriptspan into doc_tote and vec, updating scoringcontext
void ScoreOneScriptSpan(const LangSpan& scriptspan,
                        ScoringContext* scoringcontext,
                        DocTote* doc_tote,
                        ResultChunkVector* vec);

}       // End namespace CLD2

#endif  // I18N_ENCODINGS_CLD2_INTERNAL_SCOREONESCRIPTSPAN_H__

