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
// Updated 2014.01 for dual table lookup
//

#include "scoreonescriptspan.h"

#include "cldutil.h"
#include "debug.h"
#include "lang_script.h"

#include <stdio.h>

using namespace std;

namespace CLD2 {

static const int kUnreliablePercentThreshold = 75;

void AddLangProb(uint32 langprob, Tote* chunk_tote) {
  ProcessProbV2Tote(langprob, chunk_tote);
}

void ZeroPSLang(uint32 langprob, Tote* chunk_tote) {
  uint8 top1 = (langprob >> 8) & 0xff;
  chunk_tote->SetScore(top1, 0);
}

bool SameCloseSet(uint16 lang1, uint16 lang2) {
  int lang1_close_set = LanguageCloseSet(static_cast<Language>(lang1));
  if (lang1_close_set == 0) {return false;}
  int lang2_close_set = LanguageCloseSet(static_cast<Language>(lang2));
  return (lang1_close_set == lang2_close_set);
}

bool SameCloseSet(Language lang1, Language lang2) {
  int lang1_close_set = LanguageCloseSet(lang1);
  if (lang1_close_set == 0) {return false;}
  int lang2_close_set = LanguageCloseSet(lang2);
  return (lang1_close_set == lang2_close_set);
}


// Needs expected score per 1KB in scoring context
void SetChunkSummary(ULScript ulscript, int first_linear_in_chunk,
                     int offset, int len,
                     const ScoringContext* scoringcontext,
                     const Tote* chunk_tote,
                     ChunkSummary* chunksummary) {
  int key3[3];
  chunk_tote->CurrentTopThreeKeys(key3);
  Language lang1 = FromPerScriptNumber(ulscript, key3[0]);
  Language lang2 = FromPerScriptNumber(ulscript, key3[1]);

  int actual_score_per_kb = 0;
  if (len > 0) {
    actual_score_per_kb = (chunk_tote->GetScore(key3[0]) << 10) / len;
  }
  int expected_subscr = lang1 * 4 + LScript4(ulscript);
  int expected_score_per_kb =
     scoringcontext->scoringtables->kExpectedScore[expected_subscr];

  chunksummary->offset = offset;
  chunksummary->chunk_start = first_linear_in_chunk;
  chunksummary->lang1 = lang1;
  chunksummary->lang2 = lang2;
  chunksummary->score1 = chunk_tote->GetScore(key3[0]);
  chunksummary->score2 = chunk_tote->GetScore(key3[1]);
  chunksummary->bytes = len;
  chunksummary->grams = chunk_tote->GetScoreCount();
  chunksummary->ulscript = ulscript;
  chunksummary->reliability_delta = ReliabilityDelta(chunksummary->score1,
                                                     chunksummary->score2,
                                                     chunksummary->grams);
  // If lang1/lang2 in same close set, set delta reliability to 100%
  if (SameCloseSet(lang1, lang2)) {
    chunksummary->reliability_delta = 100;
  }
  chunksummary->reliability_score =
     ReliabilityExpected(actual_score_per_kb, expected_score_per_kb);
}

// Return true if just lang1 is there: lang2=0 and lang3=0
bool IsSingleLang(uint32 langprob) {
  // Probably a bug -- which end is lang1? But only used to call empty Boost1
  return ((langprob & 0x00ffff00) == 0);
}

// Update scoring context distinct_boost for single language quad
void AddDistinctBoost1(uint32 langprob, ScoringContext* scoringcontext) {
  // Probably keep this empty -- not a good enough signal
}

// Update scoring context distinct_boost for distinct octagram
// Keep last 4 used. Since these are mostly (except at splices) in
// hitbuffer, we might be able to just use a subscript and splice
void AddDistinctBoost2(uint32 langprob, ScoringContext* scoringcontext) {
// this is called 0..n times per chunk with decoded hitbuffer->distinct...
  LangBoosts* distinct_boost = &scoringcontext->distinct_boost.latn;
  if (scoringcontext->ulscript != ULScript_Latin) {
    distinct_boost = &scoringcontext->distinct_boost.othr;
  }
  int n = distinct_boost->n;
  distinct_boost->langprob[n] = langprob;
  distinct_boost->n = distinct_boost->wrap(n + 1);
}

// For each chunk, add extra weight for language priors (from content-lang and
// meta lang=xx) and distinctive tokens
void ScoreBoosts(const ScoringContext* scoringcontext, Tote* chunk_tote) {
  // Get boosts for current script
  const LangBoosts* langprior_boost = &scoringcontext->langprior_boost.latn;
  const LangBoosts* langprior_whack = &scoringcontext->langprior_whack.latn;
  const LangBoosts* distinct_boost = &scoringcontext->distinct_boost.latn;
  if (scoringcontext->ulscript != ULScript_Latin) {
    langprior_boost = &scoringcontext->langprior_boost.othr;
    langprior_whack = &scoringcontext->langprior_whack.othr;
    distinct_boost = &scoringcontext->distinct_boost.othr;
  }

  for (int k = 0; k < kMaxBoosts; ++k) {
    uint32 langprob = langprior_boost->langprob[k];
    if (langprob > 0) {AddLangProb(langprob, chunk_tote);}
  }
  for (int k = 0; k < kMaxBoosts; ++k) {
    uint32 langprob = distinct_boost->langprob[k];
    if (langprob > 0) {AddLangProb(langprob, chunk_tote);}
  }
  // boost has a packed set of per-script langs and probabilites
  // whack has a packed set of per-script lang to be suppressed (zeroed)
  // When a language in a close set is given as an explicit hint, others in
  //  that set will be whacked here.
  for (int k = 0; k < kMaxBoosts; ++k) {
    uint32 langprob = langprior_whack->langprob[k];
    if (langprob > 0) {ZeroPSLang(langprob, chunk_tote);}
  }
}



// At this point, The chunk is described by
//  hitbuffer->base[cspan->chunk_base .. cspan->chunk_base + cspan->base_len)
//  hitbuffer->delta[cspan->chunk_delta ... )
//  hitbuffer->distinct[cspan->chunk_distinct ... )
// Scored text is in text[lo..hi) where
//  lo is 0 or the min of first base/delta/distinct hitbuffer offset and
//  hi is the min of next base/delta/distinct hitbuffer offset after
//  base_len, etc.
void GetTextSpanOffsets(const ScoringHitBuffer* hitbuffer,
                        const ChunkSpan* cspan, int* lo, int* hi) {
  // Front of this span
  int lo_base = hitbuffer->base[cspan->chunk_base].offset;
  int lo_delta = hitbuffer->delta[cspan->chunk_delta].offset;
  int lo_distinct = hitbuffer->distinct[cspan->chunk_distinct].offset;
  // Front of next span
  int hi_base = hitbuffer->base[cspan->chunk_base +
    cspan->base_len].offset;
  int hi_delta = hitbuffer->delta[cspan->chunk_delta +
    cspan->delta_len].offset;
  int hi_distinct = hitbuffer->distinct[cspan->chunk_distinct +
    cspan->distinct_len].offset;

  *lo = 0;
//  if (cspan->chunk_base > 0) {
//    *lo = minint(minint(lo_base, lo_delta), lo_distinct);
//  }
  *lo = minint(minint(lo_base, lo_delta), lo_distinct);
  *hi = minint(minint(hi_base, hi_delta), hi_distinct);
}


int DiffScore(const CLD2TableSummary* obj, int indirect,
              uint16 lang1, uint16 lang2) {
  if (indirect < static_cast<int>(obj->kCLDTableSizeOne)) {
    // Up to three languages at indirect
    uint32 langprob = obj->kCLDTableInd[indirect];
    return GetLangScore(langprob, lang1) - GetLangScore(langprob, lang2);
  } else {
    // Up to six languages at start + 2 * (indirect - start)
    indirect += (indirect - obj->kCLDTableSizeOne);
    uint32 langprob = obj->kCLDTableInd[indirect];
    uint32 langprob2 = obj->kCLDTableInd[indirect + 1];
    return (GetLangScore(langprob, lang1) + GetLangScore(langprob2, lang1)) -
      (GetLangScore(langprob, lang2) + GetLangScore(langprob2, lang2));
  }

}

// Score all the bases, deltas, distincts, boosts for one chunk into chunk_tote
// After last chunk there is always a hitbuffer entry with an offset just off
// the end of the text.
// Sets delta_len, and distinct_len
void ScoreOneChunk(const char* text, ULScript ulscript,
                   const ScoringHitBuffer* hitbuffer,
                   int chunk_i,
                   ScoringContext* scoringcontext,
                   ChunkSpan* cspan, Tote* chunk_tote,
                   ChunkSummary* chunksummary) {
  int first_linear_in_chunk = hitbuffer->chunk_start[chunk_i];
  int first_linear_in_next_chunk = hitbuffer->chunk_start[chunk_i + 1];

  chunk_tote->Reinit();
  cspan->delta_len = 0;
  cspan->distinct_len = 0;
  if (scoringcontext->flags_cld2_verbose) {
    fprintf(scoringcontext->debug_file, "<br>ScoreOneChunk[%d..%d) ",
            first_linear_in_chunk, first_linear_in_next_chunk);
  }

  // 2013.02.05 linear design: just use base and base_len for the span
  cspan->chunk_base = first_linear_in_chunk;
  cspan->base_len = first_linear_in_next_chunk - first_linear_in_chunk;
  for (int i = first_linear_in_chunk; i < first_linear_in_next_chunk; ++i) {
    uint32 langprob = hitbuffer->linear[i].langprob;
    AddLangProb(langprob, chunk_tote);
    if (hitbuffer->linear[i].type <= QUADHIT) {
      chunk_tote->AddScoreCount();      // Just count quads, not octas
    }
    if (hitbuffer->linear[i].type == DISTINCTHIT) {
      AddDistinctBoost2(langprob, scoringcontext);
    }
  }

  // Score language prior boosts
  // Score distinct word boost
  ScoreBoosts(scoringcontext, chunk_tote);

  int lo = hitbuffer->linear[first_linear_in_chunk].offset;
  int hi = hitbuffer->linear[first_linear_in_next_chunk].offset;

  // Chunk_tote: get top langs, scores, etc. and fill in chunk summary
  SetChunkSummary(ulscript, first_linear_in_chunk, lo, hi - lo,
                  scoringcontext, chunk_tote, chunksummary);

  bool more_to_come = false;
  bool score_cjk = false;
  if (scoringcontext->flags_cld2_html) {
    // Show one chunk in readable output
    CLD2_Debug(text, lo, hi, more_to_come, score_cjk, hitbuffer,
               scoringcontext, cspan, chunksummary);
  }

  scoringcontext->prior_chunk_lang = static_cast<Language>(chunksummary->lang1);
}


// Score chunks of text described by hitbuffer, allowing each to be in a
// different language, and optionally adjusting the boundaries inbetween.
// Set last_cspan to the last chunkspan used
void ScoreAllHits(const char* text,  ULScript ulscript,
                  bool more_to_come, bool score_cjk,
                  const ScoringHitBuffer* hitbuffer,
                  ScoringContext* scoringcontext,
                  SummaryBuffer* summarybuffer, ChunkSpan* last_cspan) {
  ChunkSpan prior_cspan = {0, 0, 0, 0, 0, 0};
  ChunkSpan cspan = {0, 0, 0, 0, 0, 0};

  for (int i = 0; i < hitbuffer->next_chunk_start; ++i) {
    // Score one chunk
    // Sets delta_len, and distinct_len
    Tote chunk_tote;
    ChunkSummary chunksummary;
    ScoreOneChunk(text, ulscript,
                  hitbuffer, i,
                  scoringcontext, &cspan, &chunk_tote, &chunksummary);

    // Put result in summarybuffer
    if (summarybuffer->n < kMaxSummaries) {
      summarybuffer->chunksummary[summarybuffer->n] = chunksummary;
      summarybuffer->n += 1;
    }

    prior_cspan = cspan;
    cspan.chunk_base += cspan.base_len;
    cspan.chunk_delta += cspan.delta_len;
    cspan.chunk_distinct += cspan.distinct_len;
  }

  // Add one dummy off the end to hold first unused linear_in_chunk
  int linear_off_end = hitbuffer->next_linear;
  int offset_off_end = hitbuffer->linear[linear_off_end].offset;
  ChunkSummary* cs = &summarybuffer->chunksummary[summarybuffer->n];
  memset(cs, 0, sizeof(ChunkSummary));
  cs->offset = offset_off_end;
  cs->chunk_start = linear_off_end;
  *last_cspan = prior_cspan;
}


void SummaryBufferToDocTote(const SummaryBuffer* summarybuffer,
                            bool more_to_come, DocTote* doc_tote) {
  int cs_bytes_sum = 0;
  for (int i = 0; i < summarybuffer->n; ++i) {
    const ChunkSummary* cs = &summarybuffer->chunksummary[i];
    int reliability = minint(cs->reliability_delta, cs->reliability_score);
    // doc_tote uses full languages
    doc_tote->Add(cs->lang1, cs->bytes, cs->score1, reliability);
    cs_bytes_sum += cs->bytes;
  }
}

// Turn on for debugging vectors
static const bool kShowLettersOriginal = false;


// If next chunk language matches last vector language, extend last element
// Otherwise add new element to vector
void ItemToVector(ScriptScanner* scanner,
                  ResultChunkVector* vec, Language new_lang,
                  int mapped_offset, int mapped_len) {
  uint16 last_vec_lang = static_cast<uint16>(UNKNOWN_LANGUAGE);
  int last_vec_subscr = vec->size() - 1;
  if (last_vec_subscr >= 0) {
    ResultChunk* priorrc = &(*vec)[last_vec_subscr];
    last_vec_lang = priorrc->lang1;
    if (new_lang == last_vec_lang) {
      // Extend prior. Current mapped_offset may be beyond prior end, so do
      // the arithmetic to include any such gap
      priorrc->bytes = minint((mapped_offset + mapped_len) - priorrc->offset,
                              kMaxResultChunkBytes);
      if (kShowLettersOriginal) {
        // Optionally print the new chunk original text
        string temp2(&scanner->GetBufferStart()[priorrc->offset],
                     priorrc->bytes);
        fprintf(stderr, "Item[%d..%d) '%s'<br>\n",
                priorrc->offset, priorrc->offset + priorrc->bytes,
                GetHtmlEscapedText(temp2).c_str());
      }
      return;
    }
  }
  // Add new vector element
  ResultChunk rc;
  rc.offset = mapped_offset;
  rc.bytes = minint(mapped_len, kMaxResultChunkBytes);
  rc.lang1 = static_cast<uint16>(new_lang);
  vec->push_back(rc);
  if (kShowLettersOriginal) {
    // Optionally print the new chunk original text
    string temp2(&scanner->GetBufferStart()[rc.offset], rc.bytes);
    fprintf(stderr, "Item[%d..%d) '%s'<br>\n",
            rc.offset, rc.offset + rc.bytes,
            GetHtmlEscapedText(temp2).c_str());
  }
}

uint16 PriorVecLang(const ResultChunkVector* vec) {
  if (vec->empty()) {return static_cast<uint16>(UNKNOWN_LANGUAGE);}
  return (*vec)[vec->size() - 1].lang1;
}

uint16 NextChunkLang(const SummaryBuffer* summarybuffer, int i) {
  if ((i + 1) >= summarybuffer->n) {
    return static_cast<uint16>(UNKNOWN_LANGUAGE);
  }
  return summarybuffer->chunksummary[i + 1].lang1;
}



// Add n elements of summarybuffer to resultchunk vector:
// Each element is letters-only text [offset..offset+bytes)
// This maps back to original[Back(offset)..Back(offset+bytes))
//
// We go out of our way to minimize the variation in the ResultChunkVector,
// so that the caller has fewer but more meaningful spans in different
// lanaguges, for the likely purpose of translation or spell-check.
//
// The language of each chunk is lang1, but it might be unreliable for
// either of two reasons: its score is relatively too close to the score of
// lang2, or its score is too far away from the expected score of real text in
// the given language. Unreliable languages are mapped to Unknown.
//
void SummaryBufferToVector(ScriptScanner* scanner, const char* text,
                           const SummaryBuffer* summarybuffer,
                           bool more_to_come, ResultChunkVector* vec) {
  if (vec == NULL) {return;}

  if (kShowLettersOriginal) {
    fprintf(stderr, "map2original_ ");
    scanner->map2original_.DumpWindow();
    fprintf(stderr, "<br>\n");
    fprintf(stderr, "map2uplow_ ");
    scanner->map2uplow_.DumpWindow();
    fprintf(stderr, "<br>\n");
  }

  for (int i = 0; i < summarybuffer->n; ++i) {
    const ChunkSummary* cs = &summarybuffer->chunksummary[i];
    int unmapped_offset = cs->offset;
    int unmapped_len = cs->bytes;

    if (kShowLettersOriginal) {
      // Optionally print the chunk lowercase letters/marks text
      string temp(&text[unmapped_offset], unmapped_len);
      fprintf(stderr, "Letters [%d..%d) '%s'<br>\n",
              unmapped_offset, unmapped_offset + unmapped_len,
              GetHtmlEscapedText(temp).c_str());
    }

    int mapped_offset = scanner->MapBack(unmapped_offset);

    // Trim back a little to prefer splicing original at word boundaries
    if (mapped_offset > 0) {
      // Size of prior vector entry, if any
      int prior_size = 0;
      if (!vec->empty()) {
        ResultChunk* rc = &(*vec)[vec->size() - 1];
        prior_size = rc->bytes;
      }
      // Maximum back up size to leave at least 3 bytes in prior,
      // and not entire buffer, and no more than 12 bytes total backup
      int n_limit = minint(prior_size - 3, mapped_offset);
      n_limit = minint(n_limit, 12);

      // Backscan over letters, stopping if prior byte is < 0x41
      // There is some possibility that we will backscan over a different script
      const char* s = &scanner->GetBufferStart()[mapped_offset];
      const unsigned char* us = reinterpret_cast<const unsigned char*>(s);
      int n = 0;
      while ((n < n_limit) && (us[-n - 1] >= 0x41)) {++n;}
      if (n >= n_limit) {n = 0;} // New boundary not found within range

      // Also back up exactly one leading punctuation character if '"#@
      if (n < n_limit) {
        unsigned char c = us[-n - 1];
        if ((c == '\'') || (c == '"') || (c == '#') || (c == '@')) {++n;}
      }
      // Shrink the previous chunk slightly
      if (n > 0) {
        ResultChunk* rc = &(*vec)[vec->size() - 1];
        rc->bytes -= n;
        mapped_offset -= n;
        if (kShowLettersOriginal) {
          fprintf(stderr, "Back up %d bytes<br>\n", n);
          // Optionally print the prior chunk original text
          string temp2(&scanner->GetBufferStart()[rc->offset], rc->bytes);
          fprintf(stderr, "Prior   [%d..%d) '%s'<br>\n",
                  rc->offset, rc->offset + rc->bytes,
                  GetHtmlEscapedText(temp2).c_str());
        }
      }
    }

    int mapped_len =
      scanner->MapBack(unmapped_offset + unmapped_len) - mapped_offset;

    if (kShowLettersOriginal) {
      // Optionally print the chunk original text
      string temp2(&scanner->GetBufferStart()[mapped_offset], mapped_len);
      fprintf(stderr, "Original[%d..%d) '%s'<br>\n",
              mapped_offset, mapped_offset + mapped_len,
              GetHtmlEscapedText(temp2).c_str());
    }

    Language new_lang = static_cast<Language>(cs->lang1);
    bool reliability_delta_bad =
      (cs->reliability_delta < kUnreliablePercentThreshold);
    bool reliability_score_bad =
      (cs->reliability_score < kUnreliablePercentThreshold);

    // If the top language matches last vector, ignore reliability_delta
    uint16 prior_lang = PriorVecLang(vec);
    if (prior_lang == cs->lang1) {
      reliability_delta_bad = false;
    }
    // If the top language is in same close set as last vector, set up to merge
    if (SameCloseSet(cs->lang1, prior_lang)) {
      new_lang = static_cast<Language>(prior_lang);
      reliability_delta_bad = false;
    }
    // If the top two languages are in the same close set and the last vector
    // language is the second language, set up to merge
    if (SameCloseSet(cs->lang1, cs->lang2) &&
        (prior_lang == cs->lang2)) {
      new_lang = static_cast<Language>(prior_lang);
      reliability_delta_bad = false;
    }
    // If unreliable and the last and next vector languages are both
    // the second language, set up to merge
    uint16 next_lang = NextChunkLang(summarybuffer, i);
    if (reliability_delta_bad &&
        (prior_lang == cs->lang2) && (next_lang == cs->lang2)) {
      new_lang = static_cast<Language>(prior_lang);
      reliability_delta_bad = false;
    }

    if (reliability_delta_bad || reliability_score_bad) {
      new_lang = UNKNOWN_LANGUAGE;
    }
    ItemToVector(scanner, vec, new_lang, mapped_offset, mapped_len);
  }
}

// Add just one element to resultchunk vector:
// For RTypeNone or RTypeOne
void JustOneItemToVector(ScriptScanner* scanner, const char* text,
                         Language lang1, int unmapped_offset, int unmapped_len,
                         ResultChunkVector* vec) {
  if (vec == NULL) {return;}

  if (kShowLettersOriginal) {
    fprintf(stderr, "map2original_ ");
    scanner->map2original_.DumpWindow();
    fprintf(stderr, "<br>\n");
    fprintf(stderr, "map2uplow_ ");
    scanner->map2uplow_.DumpWindow();
    fprintf(stderr, "<br>\n");
  }

  if (kShowLettersOriginal) {
   // Optionally print the chunk lowercase letters/marks text
   string temp(&text[unmapped_offset], unmapped_len);
   fprintf(stderr, "Letters1 [%d..%d) '%s'<br>\n",
           unmapped_offset, unmapped_offset + unmapped_len,
           GetHtmlEscapedText(temp).c_str());
  }

  int mapped_offset = scanner->MapBack(unmapped_offset);
  int mapped_len =
    scanner->MapBack(unmapped_offset + unmapped_len) - mapped_offset;

  if (kShowLettersOriginal) {
    // Optionally print the chunk original text
    string temp2(&scanner->GetBufferStart()[mapped_offset], mapped_len);
    fprintf(stderr, "Original1[%d..%d) '%s'<br>\n",
            mapped_offset, mapped_offset + mapped_len,
            GetHtmlEscapedText(temp2).c_str());
  }

  ItemToVector(scanner, vec, lang1, mapped_offset, mapped_len);
}


// Debugging. Not thread safe. Defined in getonescriptspan
char* DisplayPiece(const char* next_byte_, int byte_length_);

// If high bit is on, take out high bit and add 2B to make table2 entries easy
inline int PrintableIndirect(int x) {
  if ((x & 0x80000000u) != 0) {
    return (x & ~0x80000000u) + 2000000000;
  }
  return x;
}
void DumpHitBuffer(FILE* df, const char* text,
                   const ScoringHitBuffer* hitbuffer) {
  fprintf(df,
          "<br>DumpHitBuffer[%s, next_base/delta/distinct %d, %d, %d)<br>\n",
          ULScriptCode(hitbuffer->ulscript),
          hitbuffer->next_base, hitbuffer->next_delta,
          hitbuffer->next_distinct);
  for (int i = 0; i < hitbuffer->maxscoringhits; ++i) {
    if (i < hitbuffer->next_base) {
      fprintf(df, "Q[%d]%d,%d,%s ",
              i, hitbuffer->base[i].offset,
              PrintableIndirect(hitbuffer->base[i].indirect),
              DisplayPiece(&text[hitbuffer->base[i].offset], 6));
    }
    if (i < hitbuffer->next_delta) {
      fprintf(df, "DL[%d]%d,%d,%s ",
              i, hitbuffer->delta[i].offset, hitbuffer->delta[i].indirect,
              DisplayPiece(&text[hitbuffer->delta[i].offset], 12));
    }
    if (i < hitbuffer->next_distinct) {
      fprintf(df, "D[%d]%d,%d,%s ",
              i, hitbuffer->distinct[i].offset, hitbuffer->distinct[i].indirect,
              DisplayPiece(&text[hitbuffer->distinct[i].offset], 12));
    }
    if (i < hitbuffer->next_base) {
      fprintf(df, "<br>\n");
    }
    if (i > 50) {break;}
  }
  if (hitbuffer->next_base > 50) {
    int i = hitbuffer->next_base;
    fprintf(df, "Q[%d]%d,%d,%s ",
            i, hitbuffer->base[i].offset,
            PrintableIndirect(hitbuffer->base[i].indirect),
            DisplayPiece(&text[hitbuffer->base[i].offset], 6));
  }
  if (hitbuffer->next_delta > 50) {
    int i = hitbuffer->next_delta;
    fprintf(df, "DL[%d]%d,%d,%s ",
            i, hitbuffer->delta[i].offset, hitbuffer->delta[i].indirect,
            DisplayPiece(&text[hitbuffer->delta[i].offset], 12));
  }
  if (hitbuffer->next_distinct > 50) {
    int i = hitbuffer->next_distinct;
    fprintf(df, "D[%d]%d,%d,%s ",
            i, hitbuffer->distinct[i].offset, hitbuffer->distinct[i].indirect,
            DisplayPiece(&text[hitbuffer->distinct[i].offset], 12));
  }
  fprintf(df, "<br>\n");
}


void DumpLinearBuffer(FILE* df, const char* text,
                      const ScoringHitBuffer* hitbuffer) {
  fprintf(df, "<br>DumpLinearBuffer[%d)<br>\n",
          hitbuffer->next_linear);
  // Include the dummy entry off the end
  for (int i = 0; i < hitbuffer->next_linear + 1; ++i) {
    if ((50 < i) && (i < (hitbuffer->next_linear - 1))) {continue;}
    fprintf(df, "[%d]%d,%c=%08x,%s<br>\n",
            i, hitbuffer->linear[i].offset,
            "UQLD"[hitbuffer->linear[i].type],
            hitbuffer->linear[i].langprob,
            DisplayPiece(&text[hitbuffer->linear[i].offset], 6));
  }
  fprintf(df, "<br>\n");

  fprintf(df, "DumpChunkStart[%d]<br>\n", hitbuffer->next_chunk_start);
  for (int i = 0; i < hitbuffer->next_chunk_start + 1; ++i) {
    fprintf(df, "[%d]%d\n", i, hitbuffer->chunk_start[i]);
  }
  fprintf(df, "<br>\n");
}

// Move this verbose debugging output to debug.cc eventually
void DumpChunkSummary(FILE* df, const ChunkSummary* cs) {
  // Print chunksummary
  fprintf(df, "%d lin[%d] %s.%d %s.%d %dB %d# %s %dRd %dRs<br>\n",
          cs->offset,
          cs->chunk_start,
          LanguageCode(static_cast<Language>(cs->lang1)),
          cs->score1,
          LanguageCode(static_cast<Language>(cs->lang2)),
          cs->score2,
          cs->bytes,
          cs->grams,
          ULScriptCode(static_cast<ULScript>(cs->ulscript)),
          cs->reliability_delta,
          cs->reliability_score);
}

void DumpSummaryBuffer(FILE* df, const SummaryBuffer* summarybuffer) {
  fprintf(df, "<br>DumpSummaryBuffer[%d]<br>\n", summarybuffer->n);
  fprintf(df, "[i] offset linear[chunk_start] lang.score1 lang.score2 "
              "bytesB ngrams# script rel_delta rel_score<br>\n");
  for (int i = 0; i <= summarybuffer->n; ++i) {
    fprintf(df, "[%d] ", i);
    DumpChunkSummary(df, &summarybuffer->chunksummary[i]);
  }
  fprintf(df, "<br>\n");
}



// Within hitbufer->linear[]
// <-- prior chunk --><-- this chunk -->
// |                  |                 |
// linear0            linear1           linear2
//     lang0              lang1
// The goal of sharpening is to move this_linear to better separate langs
int BetterBoundary(const char* text,
                   ScoringHitBuffer* hitbuffer,
                   ScoringContext* scoringcontext,
                   uint16 pslang0, uint16 pslang1,
                   int linear0, int linear1, int linear2) {
  // Degenerate case, no change
  if ((linear2 - linear0) <= 8) {return linear1;}

  // Each diff gives pslang0 score - pslang1 score
  // Running diff has four entries + + + + followed by four entries - - - -
  // so that this value is maximal at the sharpest boundary between pslang0
  // (positive diffs) and pslang1 (negative diffs)
  int running_diff = 0;
  int diff[8];    // Ring buffer of pslang0-pslang1 differences
  // Initialize with first 8 diffs
  for (int i = linear0; i < linear0 + 8; ++i) {
    int j = i & 7;
    uint32 langprob = hitbuffer->linear[i].langprob;
    diff[j] = GetLangScore(langprob, pslang0) -
       GetLangScore(langprob, pslang1);
    if (i < linear0 + 4) {
      // First four diffs pslang0 - pslang1
      running_diff += diff[j];
    } else {
      // Second four diffs -(pslang0 - pslang1)
      running_diff -= diff[j];
    }
  }

  // Now scan for sharpest boundary. j is at left end of 8 entries
  // To be a boundary, there must be both >0 and <0 entries in the window
  int better_boundary_value = 0;
  int better_boundary = linear1;
  for (int i = linear0; i < linear2 - 8; ++i) {
    int j = i & 7;
    if (better_boundary_value < running_diff) {
      bool has_plus = false;
      bool has_minus = false;
      for (int kk = 0; kk < 8; ++kk) {
        if (diff[kk] > 0) {has_plus = true;}
        if (diff[kk] < 0) {has_minus = true;}
      }
      if (has_plus && has_minus) {
        better_boundary_value = running_diff;
        better_boundary = i + 4;
      }
    }
    // Shift right one entry
    uint32 langprob = hitbuffer->linear[i + 8].langprob;
    int newdiff = GetLangScore(langprob, pslang0) -
       GetLangScore(langprob, pslang1);
    int middiff = diff[(i + 4) & 7];
    int olddiff = diff[j];
    diff[j] = newdiff;
    running_diff -= olddiff;                  // Remove left
    running_diff += 2 * middiff;              // Convert middle from - to +
    running_diff -= newdiff;                  // Insert right
  }

  if (scoringcontext->flags_cld2_verbose && (linear1 != better_boundary)) {
    Language lang0 = FromPerScriptNumber(scoringcontext->ulscript, pslang0);
    Language lang1 = FromPerScriptNumber(scoringcontext->ulscript, pslang1);
    fprintf(scoringcontext->debug_file, " Better lin[%d=>%d] %s^^%s <br>\n",
            linear1, better_boundary,
            LanguageCode(lang0), LanguageCode(lang1));
    int lin0_off = hitbuffer->linear[linear0].offset;
    int lin1_off = hitbuffer->linear[linear1].offset;
    int lin2_off = hitbuffer->linear[linear2].offset;
    int better_offm1 = hitbuffer->linear[better_boundary - 1].offset;
    int better_off = hitbuffer->linear[better_boundary].offset;
    int better_offp1 = hitbuffer->linear[better_boundary + 1].offset;
    string old0(&text[lin0_off], lin1_off - lin0_off);
    string old1(&text[lin1_off], lin2_off - lin1_off);
    string new0(&text[lin0_off], better_offm1 - lin0_off);
    string new0m1(&text[better_offm1], better_off - better_offm1);
    string new1(&text[better_off], better_offp1 - better_off);
    string new1p1(&text[better_offp1], lin2_off - better_offp1);
    fprintf(scoringcontext->debug_file, "%s^^%s => <br>\n%s^%s^^%s^%s<br>\n",
            GetHtmlEscapedText(old0).c_str(),
            GetHtmlEscapedText(old1).c_str(),
            GetHtmlEscapedText(new0).c_str(),
            GetHtmlEscapedText(new0m1).c_str(),
            GetHtmlEscapedText(new1).c_str(),
            GetHtmlEscapedText(new1p1).c_str());
    // Slow picture of differences per linear entry
    int d;
    for (int i = linear0; i < linear2; ++i) {
      if (i == better_boundary) {
        fprintf(scoringcontext->debug_file, "^^ ");
      }
      uint32 langprob = hitbuffer->linear[i].langprob;
      d = GetLangScore(langprob, pslang0) - GetLangScore(langprob, pslang1);
      const char* s = "=";
      //if (d > 2) {s = "\xc2\xaf";}    // Macron
      if (d > 2) {s = "#";}
      else if (d > 0) {s = "+";}
      else if (d < -2) {s = "_";}
      else if (d < 0) {s = "-";}
      fprintf(scoringcontext->debug_file, "%s ", s);
    }
    fprintf(scoringcontext->debug_file, " &nbsp;&nbsp;(scale: #+=-_)<br>\n");
  }
  return better_boundary;
}


// For all but the first summary, if its top language differs from
// the previous chunk, refine the boundary
// Linearized version
void SharpenBoundaries(const char* text,
                       bool more_to_come,
                       ScoringHitBuffer* hitbuffer,
                       ScoringContext* scoringcontext,
                       SummaryBuffer* summarybuffer) {

  int prior_linear = summarybuffer->chunksummary[0].chunk_start;
  uint16 prior_lang = summarybuffer->chunksummary[0].lang1;

  if (scoringcontext->flags_cld2_verbose) {
    fprintf(scoringcontext->debug_file, "<br>SharpenBoundaries<br>\n");
  }
  for (int i = 1; i < summarybuffer->n; ++i) {
    ChunkSummary* cs = &summarybuffer->chunksummary[i];
    uint16 this_lang = cs->lang1;
    if (this_lang == prior_lang) {
      prior_linear = cs->chunk_start;
      continue;
    }

    int this_linear = cs->chunk_start;
    int next_linear = summarybuffer->chunksummary[i + 1].chunk_start;

    // If this/prior in same close set, don't move boundary
    if (SameCloseSet(prior_lang, this_lang)) {
      prior_linear = this_linear;
      prior_lang = this_lang;
      continue;
    }


    // Within hitbuffer->linear[]
    // <-- prior chunk --><-- this chunk -->
    // |                  |                 |
    // prior_linear       this_linear       next_linear
    //     prior_lang         this_lang
    // The goal of sharpening is to move this_linear to better separate langs

    uint8 pslang0 = PerScriptNumber(scoringcontext->ulscript,
                                    static_cast<Language>(prior_lang));
    uint8 pslang1 = PerScriptNumber(scoringcontext->ulscript,
                                    static_cast<Language>(this_lang));
    int better_linear = BetterBoundary(text,
                                       hitbuffer,
                                       scoringcontext,
                                       pslang0, pslang1,
                                       prior_linear, this_linear, next_linear);

    int old_offset = hitbuffer->linear[this_linear].offset;
    int new_offset = hitbuffer->linear[better_linear].offset;
    cs->chunk_start = better_linear;
    cs->offset = new_offset;
    // If this_linear moved right, make bytes smaller for this, larger for prior
    // If this_linear moved left, make bytes larger for this, smaller for prior
    cs->bytes -= (new_offset - old_offset);
    summarybuffer->chunksummary[i - 1].bytes += (new_offset - old_offset);

    this_linear = better_linear;    // Update so that next chunk doesn't intrude

    // Consider rescoring the two chunks

    // Update for next round (note: using pre-updated boundary)
    prior_linear = this_linear;
    prior_lang = this_lang;
  }
}

// Make a langprob that gives small weight to the default language for ulscript
uint32 DefaultLangProb(ULScript ulscript) {
  Language default_lang = DefaultLanguage(ulscript);
  return MakeLangProb(default_lang, 1);
}

// Effectively, do a merge-sort based on text offsets
// Look up each indirect value in appropriate scoring table and keep
// just the resulting langprobs
void LinearizeAll(ScoringContext* scoringcontext, bool score_cjk,
                  ScoringHitBuffer* hitbuffer) {
  const CLD2TableSummary* base_obj;       // unigram or quadgram
  const CLD2TableSummary* base_obj2;      // quadgram dual table
  const CLD2TableSummary* delta_obj;      // bigram or octagram
  const CLD2TableSummary* distinct_obj;   // bigram or octagram
  uint16 base_hit;
  if (score_cjk) {
    base_obj = scoringcontext->scoringtables->unigram_compat_obj;
    base_obj2 = scoringcontext->scoringtables->unigram_compat_obj;
    delta_obj = scoringcontext->scoringtables->deltabi_obj;
    distinct_obj = scoringcontext->scoringtables->distinctbi_obj;
    base_hit = UNIHIT;
  } else {
    base_obj = scoringcontext->scoringtables->quadgram_obj;
    base_obj2 = scoringcontext->scoringtables->quadgram_obj2;
    delta_obj = scoringcontext->scoringtables->deltaocta_obj;
    distinct_obj = scoringcontext->scoringtables->distinctocta_obj;
    base_hit = QUADHIT;
  }

  int base_limit = hitbuffer->next_base;
  int delta_limit = hitbuffer->next_delta;
  int distinct_limit = hitbuffer->next_distinct;
  int base_i = 0;
  int delta_i = 0;
  int distinct_i = 0;
  int linear_i = 0;

  // Start with an initial base hit for the default language for this script
  // Inserting this avoids edge effects with no hits at all
  hitbuffer->linear[linear_i].offset = hitbuffer->lowest_offset;
  hitbuffer->linear[linear_i].type = base_hit;
  hitbuffer->linear[linear_i].langprob =
    DefaultLangProb(scoringcontext->ulscript);
  ++linear_i;

  while ((base_i < base_limit) || (delta_i < delta_limit) ||
         (distinct_i < distinct_limit)) {
    int base_off = hitbuffer->base[base_i].offset;
    int delta_off = hitbuffer->delta[delta_i].offset;
    int distinct_off = hitbuffer->distinct[distinct_i].offset;

    // Do delta and distinct first, so that they are not lost at base_limit
    if ((delta_i < delta_limit) &&
        (delta_off <= base_off) && (delta_off <= distinct_off)) {
      // Add delta entry
      int indirect = hitbuffer->delta[delta_i].indirect;
      ++delta_i;
      uint32 langprob = delta_obj->kCLDTableInd[indirect];
      if (langprob > 0) {
        hitbuffer->linear[linear_i].offset = delta_off;
        hitbuffer->linear[linear_i].type = DELTAHIT;
        hitbuffer->linear[linear_i].langprob = langprob;
        ++linear_i;
      }
    }
    else if ((distinct_i < distinct_limit) &&
             (distinct_off <= base_off) && (distinct_off <= delta_off)) {
      // Add distinct entry
      int indirect = hitbuffer->distinct[distinct_i].indirect;
      ++distinct_i;
      uint32 langprob = distinct_obj->kCLDTableInd[indirect];
      if (langprob > 0) {
        hitbuffer->linear[linear_i].offset = distinct_off;
        hitbuffer->linear[linear_i].type = DISTINCTHIT;
        hitbuffer->linear[linear_i].langprob = langprob;
        ++linear_i;
      }
    }
    else {
      // Add one or two base entries
      int indirect = hitbuffer->base[base_i].indirect;
      // First, get right scoring table
      const CLD2TableSummary* local_base_obj = base_obj;
      if ((indirect & 0x80000000u) != 0) {
        local_base_obj = base_obj2;
        indirect &= ~0x80000000u;
      }
      ++base_i;
      // One langprob in kQuadInd[0..SingleSize),
      // two in kQuadInd[SingleSize..Size)
      if (indirect < static_cast<int>(local_base_obj->kCLDTableSizeOne)) {
        // Up to three languages at indirect
        uint32 langprob = local_base_obj->kCLDTableInd[indirect];
        if (langprob > 0) {
          hitbuffer->linear[linear_i].offset = base_off;
          hitbuffer->linear[linear_i].type = base_hit;
          hitbuffer->linear[linear_i].langprob = langprob;
          ++linear_i;
        }
      } else {
        // Up to six languages at start + 2 * (indirect - start)
        indirect += (indirect - local_base_obj->kCLDTableSizeOne);
        uint32 langprob = local_base_obj->kCLDTableInd[indirect];
        uint32 langprob2 = local_base_obj->kCLDTableInd[indirect + 1];
        if (langprob > 0) {
          hitbuffer->linear[linear_i].offset = base_off;
          hitbuffer->linear[linear_i].type = base_hit;
          hitbuffer->linear[linear_i].langprob = langprob;
          ++linear_i;
        }
        if (langprob2 > 0) {
          hitbuffer->linear[linear_i].offset = base_off;
          hitbuffer->linear[linear_i].type = base_hit;
          hitbuffer->linear[linear_i].langprob = langprob2;
          ++linear_i;
        }
      }
    }
  }

  // Update
  hitbuffer->next_linear = linear_i;

  // Add a dummy entry off the end, just to capture final offset
  hitbuffer->linear[linear_i].offset =
  hitbuffer->base[hitbuffer->next_base].offset;
  hitbuffer->linear[linear_i].langprob = 0;
}

// Break linear array into chunks of ~20 quadgram hits or ~50 CJK unigram hits
void ChunkAll(int letter_offset, bool score_cjk, ScoringHitBuffer* hitbuffer) {
  int chunksize;
  uint16 base_hit;
  if (score_cjk) {
    chunksize = kChunksizeUnis;
    base_hit = UNIHIT;
  } else {
    chunksize = kChunksizeQuads;
    base_hit = QUADHIT;
  }

  int linear_i = 0;
  int linear_off_end = hitbuffer->next_linear;
  int text_i = letter_offset;               // Next unseen text offset
  int next_chunk_start = 0;
  int bases_left = hitbuffer->next_base;
  while (bases_left > 0) {
    // Linearize one chunk
    int base_len = chunksize;     // Default; may be changed below
    if (bases_left < (chunksize + (chunksize >> 1))) {
      // If within 1.5 chunks of the end, avoid runts by using it all
      base_len = bases_left;
    } else if (bases_left < (2 * chunksize)) {
      // Avoid runts by splitting 1.5 to 2 chunks in half (about 3/4 each)
      base_len = (bases_left + 1) >> 1;
    }

    hitbuffer->chunk_start[next_chunk_start] = linear_i;
    hitbuffer->chunk_offset[next_chunk_start] = text_i;
    ++next_chunk_start;

    int base_count = 0;
    while ((base_count < base_len) && (linear_i < linear_off_end)) {
      if (hitbuffer->linear[linear_i].type == base_hit) {++base_count;}
      ++linear_i;
    }
    text_i = hitbuffer->linear[linear_i].offset;    // Next unseen text offset
    bases_left -= base_len;
  }

  // If no base hits at all, make a single dummy chunk
  if (next_chunk_start == 0) {
     hitbuffer->chunk_start[next_chunk_start] = 0;
     hitbuffer->chunk_offset[next_chunk_start] = hitbuffer->linear[0].offset;
     ++next_chunk_start;
  }

  // Remember the linear array start of dummy entry
  hitbuffer->next_chunk_start = next_chunk_start;

  // Add a dummy entry off the end, just to capture final linear subscr
  hitbuffer->chunk_start[next_chunk_start] = hitbuffer->next_linear;
  hitbuffer->chunk_offset[next_chunk_start] = text_i;
}


// Merge-sort the individual hit arrays, go indirect on the scoring subscripts,
// break linear array into chunks.
//
// Input:
//  hitbuffer base, delta, distinct arrays
// Output:
//  linear array
//  chunk_start array
//
void LinearizeHitBuffer(int letter_offset,
                        ScoringContext* scoringcontext,
                        bool more_to_come, bool score_cjk,
                        ScoringHitBuffer* hitbuffer) {
  LinearizeAll(scoringcontext, score_cjk, hitbuffer);
  ChunkAll(letter_offset, score_cjk, hitbuffer);
}



// The hitbuffer is in an awkward form -- three sets of base/delta/distinct
// scores, each with an indirect subscript to one of six scoring tables, some
// of which can yield two langprobs for six languages, others one langprob for
// three languages. The only correlation between base/delta/distinct is their
// offsets into the letters-only text buffer.
//
// SummaryBuffer needs to be built to linear, giving linear offset of start of
// each chunk
//
// So we first do all the langprob lookups and merge-sort by offset to make
// a single linear vector, building a side vector of chunk beginnings as we go.
// The sharpening is simply moving the beginnings, scoring is a simple linear
// sweep, etc.

void ProcessHitBuffer(const LangSpan& scriptspan,
                      int letter_offset,
                      ScoringContext* scoringcontext,
                      DocTote* doc_tote,
                      ResultChunkVector* vec,
                      bool more_to_come, bool score_cjk,
                      ScoringHitBuffer* hitbuffer) {
  if (scoringcontext->flags_cld2_verbose) {
    fprintf(scoringcontext->debug_file, "Hitbuffer[) ");
    DumpHitBuffer(scoringcontext->debug_file, scriptspan.text, hitbuffer);
  }

  LinearizeHitBuffer(letter_offset, scoringcontext, more_to_come, score_cjk,
                     hitbuffer);

  if (scoringcontext->flags_cld2_verbose) {
    fprintf(scoringcontext->debug_file, "Linear[) ");
    DumpLinearBuffer(scoringcontext->debug_file, scriptspan.text, hitbuffer);
  }

  SummaryBuffer summarybuffer;
  summarybuffer.n = 0;
  ChunkSpan last_cspan;
  ScoreAllHits(scriptspan.text, scriptspan.ulscript,
                    more_to_come, score_cjk, hitbuffer,
                    scoringcontext,
                    &summarybuffer, &last_cspan);

  if (scoringcontext->flags_cld2_verbose) {
    DumpSummaryBuffer(scoringcontext->debug_file, &summarybuffer);
  }

  if (vec != NULL) {
    // Sharpen boundaries of summarybuffer
    // This is not a high-performance path
    SharpenBoundaries(scriptspan.text, more_to_come, hitbuffer, scoringcontext,
                      &summarybuffer);
    // Show after the sharpening
    // CLD2_Debug2(scriptspan.text, more_to_come, score_cjk,
    //             hitbuffer, scoringcontext, &summarybuffer);

    if (scoringcontext->flags_cld2_verbose) {
      DumpSummaryBuffer(scoringcontext->debug_file, &summarybuffer);
    }
  }

  SummaryBufferToDocTote(&summarybuffer, more_to_come, doc_tote);
  SummaryBufferToVector(scoringcontext->scanner, scriptspan.text,
                        &summarybuffer, more_to_come, vec);
}

void SpliceHitBuffer(ScoringHitBuffer* hitbuffer, int next_offset) {
  // Splice hitbuffer and summarybuffer for next round. With big chunks and
  // distinctive-word state carried across chunks, we might not need to do this.
  hitbuffer->next_base = 0;
  hitbuffer->next_delta = 0;
  hitbuffer->next_distinct = 0;
  hitbuffer->next_linear = 0;
  hitbuffer->next_chunk_start = 0;
  hitbuffer->lowest_offset = next_offset;
}


// Score RTypeNone or RTypeOne scriptspan into doc_tote and vec, updating
// scoringcontext
void ScoreEntireScriptSpan(const LangSpan& scriptspan,
                           ScoringContext* scoringcontext,
                           DocTote* doc_tote,
                           ResultChunkVector* vec) {
  int bytes = scriptspan.text_bytes;
  // Artificially set score to 1024 per 1KB, or 1 per byte
  int score = bytes;
  int reliability = 100;
  // doc_tote uses full languages
  Language one_one_lang = DefaultLanguage(scriptspan.ulscript);
  doc_tote->Add(one_one_lang, bytes, score, reliability);

  if (scoringcontext->flags_cld2_html) {
    ChunkSummary chunksummary = {
      1, 0,
      one_one_lang, UNKNOWN_LANGUAGE, score, 1,
      bytes, 0, scriptspan.ulscript, reliability, reliability
    };
    CLD2_Debug(scriptspan.text, 1, scriptspan.text_bytes,
               false, false, NULL,
               scoringcontext, NULL, &chunksummary);
  }

  // First byte is always a space
  JustOneItemToVector(scoringcontext->scanner, scriptspan.text,
                      one_one_lang, 1, bytes - 1, vec);

  scoringcontext->prior_chunk_lang = UNKNOWN_LANGUAGE;
}

// Score RTypeCJK scriptspan into doc_tote and vec, updating scoringcontext
void ScoreCJKScriptSpan(const LangSpan& scriptspan,
                        ScoringContext* scoringcontext,
                        DocTote* doc_tote,
                        ResultChunkVector* vec) {
  // Allocate three parallel arrays of scoring hits
  ScoringHitBuffer* hitbuffer = new ScoringHitBuffer;
  hitbuffer->init();
  hitbuffer->ulscript = scriptspan.ulscript;

  scoringcontext->prior_chunk_lang = UNKNOWN_LANGUAGE;
  scoringcontext->oldest_distinct_boost = 0;

  // Incoming scriptspan has a single leading space at scriptspan.text[0]
  // and three trailing spaces then NUL at scriptspan.text[text_bytes + 0/1/2/3]

  int letter_offset = 1;        // Skip initial space
  hitbuffer->lowest_offset = letter_offset;
  int letter_limit = scriptspan.text_bytes;
  while (letter_offset < letter_limit) {
    if (scoringcontext->flags_cld2_verbose) {
      fprintf(scoringcontext->debug_file, " ScoreCJKScriptSpan[%d,%d)<br>\n",
              letter_offset, letter_limit);
    }
    //
    // Fill up one hitbuffer, possibly splicing onto previous fragment
    //
    // NOTE: GetUniHits deals with close repeats
    // NOTE: After last chunk there is always a hitbuffer entry with an offset
    // just off the end of the text = next_offset.
    int next_offset = GetUniHits(scriptspan.text, letter_offset, letter_limit,
                                  scoringcontext, hitbuffer);
    // NOTE: GetBiHitVectors deals with close repeats,
    // does one hash and two lookups (delta and distinct) per word
    GetBiHits(scriptspan.text, letter_offset, next_offset,
                scoringcontext, hitbuffer);

    //
    // Score one hitbuffer in chunks to summarybuffer
    //
    bool more_to_come = next_offset < letter_limit;
    bool score_cjk = true;
    ProcessHitBuffer(scriptspan, letter_offset, scoringcontext, doc_tote, vec,
                     more_to_come, score_cjk, hitbuffer);
    SpliceHitBuffer(hitbuffer, next_offset);

    letter_offset = next_offset;
  }

  delete hitbuffer;
  // Context across buffers is not connected yet
  scoringcontext->prior_chunk_lang = UNKNOWN_LANGUAGE;
}



// Score RTypeMany scriptspan into doc_tote and vec, updating scoringcontext
// We have a scriptspan with all lowercase text in one script. Look up
// quadgrams and octagrams, saving the hits in three parallel vectors.
// Score from those vectors in chunks, toting each chunk to get a single
// language, and combining into the overall document score. The hit vectors
// in general are not big enough to handle and entire scriptspan, so
// repeat until the entire scriptspan is scored.
// Caller deals with minimizing numbr of runt scriptspans
// This routine deals with minimizing number of runt chunks.
//
// Returns updated scoringcontext
// Returns updated doc_tote
// If vec != NULL, appends to that vector of ResultChunk's
void ScoreQuadScriptSpan(const LangSpan& scriptspan,
                         ScoringContext* scoringcontext,
                         DocTote* doc_tote,
                         ResultChunkVector* vec) {
  // Allocate three parallel arrays of scoring hits
  ScoringHitBuffer* hitbuffer = new ScoringHitBuffer;
  hitbuffer->init();
  hitbuffer->ulscript = scriptspan.ulscript;

  scoringcontext->prior_chunk_lang = UNKNOWN_LANGUAGE;
  scoringcontext->oldest_distinct_boost = 0;

  // Incoming scriptspan has a single leading space at scriptspan.text[0]
  // and three trailing spaces then NUL at scriptspan.text[text_bytes + 0/1/2/3]

  int letter_offset = 1;        // Skip initial space
  hitbuffer->lowest_offset = letter_offset;
  int letter_limit = scriptspan.text_bytes;
  while (letter_offset < letter_limit) {
    //
    // Fill up one hitbuffer, possibly splicing onto previous fragment
    //
    // NOTE: GetQuadHits deals with close repeats
    // NOTE: After last chunk there is always a hitbuffer entry with an offset
    // just off the end of the text = next_offset.
    int next_offset = GetQuadHits(scriptspan.text, letter_offset, letter_limit,
                                  scoringcontext, hitbuffer);
    // If true, there is more text to process in this scriptspan
    // NOTE: GetOctaHitVectors deals with close repeats,
    // does one hash and two lookups (delta and distinct) per word
    GetOctaHits(scriptspan.text, letter_offset, next_offset,
                scoringcontext, hitbuffer);

    //
    // Score one hitbuffer in chunks to summarybuffer
    //
    bool more_to_come = next_offset < letter_limit;
    bool score_cjk = false;
    ProcessHitBuffer(scriptspan, letter_offset, scoringcontext, doc_tote, vec,
                     more_to_come, score_cjk, hitbuffer);
    SpliceHitBuffer(hitbuffer, next_offset);

    letter_offset = next_offset;
  }

  delete hitbuffer;
}


// Score one scriptspan into doc_tote and vec, updating scoringcontext
// Inputs:
//  One scriptspan of perhaps 40-60KB, all same script lower-case letters
//    and single ASCII spaces. First character is a space to allow simple
//    begining-of-word detect. End of buffer has three spaces and NUL to
//    allow easy scan-to-end-of-word.
//  Scoring context of
//    scoring tables
//    flags
//    running boosts
// Outputs:
//  Updated doc_tote giving overall languages and byte counts
//  Optional updated chunk vector giving offset, length, language
//
// Caller initializes flags, boosts, doc_tote and vec.
// Caller aggregates across multiple scriptspans
// Caller calculates final document result
// Caller deals with detecting and triggering suppression of repeated text.
//
// This top-level routine just chooses the recognition type and calls one of
// the next-level-down routines.
//
void ScoreOneScriptSpan(const LangSpan& scriptspan,
                        ScoringContext* scoringcontext,
                        DocTote* doc_tote,
                        ResultChunkVector* vec) {
  if (scoringcontext->flags_cld2_verbose) {
    fprintf(scoringcontext->debug_file, "<br>ScoreOneScriptSpan(%s,%d) ",
            ULScriptCode(scriptspan.ulscript), scriptspan.text_bytes);
    // Optionally print the chunk lowercase letters/marks text
    string temp(&scriptspan.text[0], scriptspan.text_bytes);
    fprintf(scoringcontext->debug_file, "'%s'",
            GetHtmlEscapedText(temp).c_str());
    fprintf(scoringcontext->debug_file, "<br>\n");
  }
  scoringcontext->prior_chunk_lang = UNKNOWN_LANGUAGE;
  scoringcontext->oldest_distinct_boost = 0;
  ULScriptRType rtype = ULScriptRecognitionType(scriptspan.ulscript);
  if (scoringcontext->flags_cld2_score_as_quads && (rtype != RTypeCJK)) {
    rtype = RTypeMany;
  }
  switch (rtype) {
  case RTypeNone:
  case RTypeOne:
    ScoreEntireScriptSpan(scriptspan, scoringcontext, doc_tote, vec);
    break;
  case RTypeCJK:
    ScoreCJKScriptSpan(scriptspan, scoringcontext, doc_tote, vec);
    break;
  case RTypeMany:
    ScoreQuadScriptSpan(scriptspan, scoringcontext, doc_tote, vec);
    break;
  }
}

}       // End namespace CLD2

