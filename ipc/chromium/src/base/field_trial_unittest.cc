// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of FieldTrial class

#include "base/field_trial.h"

#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class FieldTrialTest : public testing::Test {
 public:
  FieldTrialTest() : trial_list_() { }

 private:
  FieldTrialList trial_list_;
};

// Test registration, and also check that destructors are called for trials
// (and that Purify doesn't catch us leaking).
TEST_F(FieldTrialTest, Registration) {
  const char* name1 = "name 1 test";
  const char* name2 = "name 2 test";
  EXPECT_FALSE(FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial1 = new FieldTrial(name1, 10);
  EXPECT_EQ(trial1->group(), FieldTrial::kNotParticipating);
  EXPECT_EQ(trial1->name(), name1);
  EXPECT_EQ(trial1->group_name(), "");

  trial1->AppendGroup("", 7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial2 = new FieldTrial(name2, 10);
  EXPECT_EQ(trial2->group(), FieldTrial::kNotParticipating);
  EXPECT_EQ(trial2->name(), name2);
  EXPECT_EQ(trial2->group_name(), "");

  trial2->AppendGroup("a first group", 7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_EQ(trial2, FieldTrialList::Find(name2));
  // Note: FieldTrialList should delete the objects at shutdown.
}

TEST_F(FieldTrialTest, AbsoluteProbabilities) {
  char always_true[] = " always true";
  char always_false[] = " always false";
  for (int i = 1; i < 250; ++i) {
    // Try lots of names, by changing the first character of the name.
    always_true[0] = i;
    always_false[0] = i;

    FieldTrial* trial_true = new FieldTrial(always_true, 10);
    const std::string winner = "_TheWinner";
    int winner_group = trial_true->AppendGroup(winner, 10);

    EXPECT_EQ(trial_true->group(), winner_group);
    EXPECT_EQ(trial_true->group_name(), winner);

    FieldTrial* trial_false = new FieldTrial(always_false, 10);
    int loser_group = trial_false->AppendGroup("ALoser", 0);

    EXPECT_NE(trial_false->group(), loser_group);
  }
}

TEST_F(FieldTrialTest, MiddleProbabilities) {
  char name[] = " same name";
  bool false_event_seen = false;
  bool true_event_seen = false;
  for (int i = 1; i < 250; ++i) {
    name[0] = i;
    FieldTrial* trial = new FieldTrial(name, 10);
    int might_win = trial->AppendGroup("MightWin", 5);

    if (trial->group() == might_win) {
      true_event_seen = true;
    } else {
      false_event_seen = true;
    }
    if (false_event_seen && true_event_seen)
      return;  // Successful test!!!
  }
  // Very surprising to get here. Probability should be around 1 in 2 ** 250.
  // One of the following will fail.
  EXPECT_TRUE(false_event_seen);
  EXPECT_TRUE(true_event_seen);
}

TEST_F(FieldTrialTest, OneWinner) {
  char name[] = "Some name";
  int group_count(10);

  FieldTrial* trial = new FieldTrial(name, group_count);
  int winner_index(-2);
  std::string winner_name;

  for (int i = 1; i <= group_count; ++i) {
    int might_win = trial->AppendGroup("", 1);

    if (trial->group() == might_win) {
      EXPECT_EQ(winner_index, -2);
      winner_index = might_win;
      StringAppendF(&winner_name, "_%d", might_win);
      EXPECT_EQ(winner_name, trial->group_name());
    }
  }
  EXPECT_GE(winner_index, 0);
  EXPECT_EQ(trial->group(), winner_index);
  EXPECT_EQ(winner_name, trial->group_name());
}
