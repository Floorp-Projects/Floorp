/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoLayoutPhase_h
#define nsAutoLayoutPhase_h

#ifdef DEBUG

#  include <stdint.h>

enum class nsLayoutPhase : uint8_t;
class nsPresContext;

struct nsAutoLayoutPhase {
  nsAutoLayoutPhase(nsPresContext* aPresContext, nsLayoutPhase aPhase);
  ~nsAutoLayoutPhase();

  void Enter();
  void Exit();

 private:
  nsPresContext* mPresContext;
  nsLayoutPhase mPhase;
  uint32_t mCount;
};

#  define AUTO_LAYOUT_PHASE_ENTRY_POINT(pc_, phase_) \
    nsAutoLayoutPhase autoLayoutPhase((pc_), (nsLayoutPhase::phase_))
#  define LAYOUT_PHASE_TEMP_EXIT() \
    PR_BEGIN_MACRO                 \
    autoLayoutPhase.Exit();        \
    PR_END_MACRO
#  define LAYOUT_PHASE_TEMP_REENTER() \
    PR_BEGIN_MACRO                    \
    autoLayoutPhase.Enter();          \
    PR_END_MACRO

#else  // DEBUG

#  define AUTO_LAYOUT_PHASE_ENTRY_POINT(pc_, phase_) PR_BEGIN_MACRO PR_END_MACRO
#  define LAYOUT_PHASE_TEMP_EXIT() PR_BEGIN_MACRO PR_END_MACRO
#  define LAYOUT_PHASE_TEMP_REENTER() PR_BEGIN_MACRO PR_END_MACRO

#endif  // DEBUG

#endif  // nsAutoLayoutPhase_h
