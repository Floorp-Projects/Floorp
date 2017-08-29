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
BUILTIN_COUNTER_STYLE(NONE, none)
BUILTIN_COUNTER_STYLE(DECIMAL, decimal)
// the following graphic styles are processed in a different way.
BUILTIN_COUNTER_STYLE(DISC, disc)
BUILTIN_COUNTER_STYLE(CIRCLE, circle)
BUILTIN_COUNTER_STYLE(SQUARE, square)
BUILTIN_COUNTER_STYLE(DISCLOSURE_CLOSED, disclosure_closed)
BUILTIN_COUNTER_STYLE(DISCLOSURE_OPEN, disclosure_open)
// the following counter styles require specific algorithms to generate.
BUILTIN_COUNTER_STYLE(HEBREW, hebrew)
BUILTIN_COUNTER_STYLE(JAPANESE_INFORMAL, japanese_informal)
BUILTIN_COUNTER_STYLE(JAPANESE_FORMAL, japanese_formal)
BUILTIN_COUNTER_STYLE(KOREAN_HANGUL_FORMAL, korean_hangul_formal)
BUILTIN_COUNTER_STYLE(KOREAN_HANJA_INFORMAL, korean_hanja_informal)
BUILTIN_COUNTER_STYLE(KOREAN_HANJA_FORMAL, korean_hanja_formal)
BUILTIN_COUNTER_STYLE(SIMP_CHINESE_INFORMAL, simp_chinese_informal)
BUILTIN_COUNTER_STYLE(SIMP_CHINESE_FORMAL, simp_chinese_formal)
BUILTIN_COUNTER_STYLE(TRAD_CHINESE_INFORMAL, trad_chinese_informal)
BUILTIN_COUNTER_STYLE(TRAD_CHINESE_FORMAL, trad_chinese_formal)
BUILTIN_COUNTER_STYLE(ETHIOPIC_NUMERIC, ethiopic_numeric)
