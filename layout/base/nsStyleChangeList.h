/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a list of the recomputation that needs to be done in response to a
 * style change
 */

#ifndef nsStyleChangeList_h___
#define nsStyleChangeList_h___

#include "mozilla/Attributes.h"

#include "nsChangeHint.h"
#include "nsCOMPtr.h"

class nsIFrame;
class nsIContent;

struct nsStyleChangeData
{
  nsIFrame* mFrame; // weak
  nsCOMPtr<nsIContent> mContent;
  nsChangeHint mHint;
};

class nsStyleChangeList : private AutoTArray<nsStyleChangeData, 10>
{
  typedef AutoTArray<nsStyleChangeData, 10> base_type;
  nsStyleChangeList(const nsStyleChangeList&) = delete;

public:
  using base_type::begin;
  using base_type::end;
  using base_type::IsEmpty;
  using base_type::Clear;

  nsStyleChangeList() { MOZ_COUNT_CTOR(nsStyleChangeList); }
  ~nsStyleChangeList() { MOZ_COUNT_DTOR(nsStyleChangeList); }
  void AppendChange(nsIFrame* aFrame, nsIContent* aContent, nsChangeHint aHint);
};

#endif /* nsStyleChangeList_h___ */
