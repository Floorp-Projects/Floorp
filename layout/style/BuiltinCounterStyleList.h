/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all builtin counter styles */

/* Each entry is defined as a BUILTIN_COUNTER_STYLE macro with the
 * following parameters:
 * - 'value_' which is final part of name of NS_STYLE_LIST_STYLE_* macros
 * - 'atom_' which is the corresponding atom in nsGkAtoms table
 *
 * Users of this list should define the following macro before including
 * this file: BUILTIN_COUNTER_STYLE(value_, atom_)
 */

// none and decimal are not redefinable, so they have to be builtin.
BUILTIN_COUNTER_STYLE(None, none)
BUILTIN_COUNTER_STYLE(Decimal, decimal)
// the following graphic styles are processed in a different way.
BUILTIN_COUNTER_STYLE(Disc, disc)
BUILTIN_COUNTER_STYLE(Circle, circle)
BUILTIN_COUNTER_STYLE(Square, square)
BUILTIN_COUNTER_STYLE(DisclosureClosed, disclosure_closed)
BUILTIN_COUNTER_STYLE(DisclosureOpen, disclosure_open)
// the following counter styles require specific algorithms to generate.
BUILTIN_COUNTER_STYLE(Hebrew, hebrew)
BUILTIN_COUNTER_STYLE(JapaneseInformal, japanese_informal)
BUILTIN_COUNTER_STYLE(JapaneseFormal, japanese_formal)
BUILTIN_COUNTER_STYLE(KoreanHangulFormal, korean_hangul_formal)
BUILTIN_COUNTER_STYLE(KoreanHanjaInformal, korean_hanja_informal)
BUILTIN_COUNTER_STYLE(KoreanHanjaFormal, korean_hanja_formal)
BUILTIN_COUNTER_STYLE(SimpChineseInformal, simp_chinese_informal)
BUILTIN_COUNTER_STYLE(SimpChineseFormal, simp_chinese_formal)
BUILTIN_COUNTER_STYLE(TradChineseInformal, trad_chinese_informal)
BUILTIN_COUNTER_STYLE(TradChineseFormal, trad_chinese_formal)
BUILTIN_COUNTER_STYLE(EthiopicNumeric, ethiopic_numeric)
