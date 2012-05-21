/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a class that walks the lexicographic tree of rule nodes as style
 * rules are matched
 */

#ifndef nsRuleWalker_h_
#define nsRuleWalker_h_

#include "nsRuleNode.h"
#include "nsIStyleRule.h"
#include "StyleRule.h"

class nsRuleWalker {
public:
  nsRuleNode* CurrentNode() { return mCurrent; }
  void SetCurrentNode(nsRuleNode* aNode) {
    NS_ASSERTION(aNode, "Must have node here!");
    mCurrent = aNode;
  }

protected:
  void DoForward(nsIStyleRule* aRule) {
    mCurrent = mCurrent->Transition(aRule, mLevel, mImportance);
    NS_POSTCONDITION(mCurrent, "Transition messed up");
  }

public:
  void Forward(nsIStyleRule* aRule) {
    NS_PRECONDITION(!nsRefPtr<mozilla::css::StyleRule>(do_QueryObject(aRule)),
                    "Calling the wrong Forward() overload");
    DoForward(aRule);
  }
  void Forward(mozilla::css::StyleRule* aRule) {
    DoForward(aRule);
    mCheckForImportantRules =
      mCheckForImportantRules && !aRule->GetImportantRule();
  }
  // ForwardOnPossiblyCSSRule should only be used by callers that have
  // an explicit list of rules they need to walk, with the list
  // already containing any important rules they care about.
  void ForwardOnPossiblyCSSRule(nsIStyleRule* aRule) {
    DoForward(aRule);
  }

  void Reset() { mCurrent = mRoot; }

  bool AtRoot() { return mCurrent == mRoot; }

  void SetLevel(PRUint8 aLevel, bool aImportance,
                bool aCheckForImportantRules) {
    NS_ASSERTION(!aCheckForImportantRules || !aImportance,
                 "Shouldn't be checking for important rules while walking "
                 "important rules");
    mLevel = aLevel;
    mImportance = aImportance;
    mCheckForImportantRules = aCheckForImportantRules;
  }
  PRUint8 GetLevel() const { return mLevel; }
  bool GetImportance() const { return mImportance; }
  bool GetCheckForImportantRules() const { return mCheckForImportantRules; }

  // We define the visited-relevant link to be the link that is the
  // nearest self-or-ancestor to the node being matched.
  enum VisitedHandlingType {
    // Do rule matching as though all links are unvisited.
    eRelevantLinkUnvisited,
    // Do rule matching as though the relevant link is visited and all
    // other links are unvisited.
    eRelevantLinkVisited,
    // Do rule matching as though a rule should match if it would match
    // given any set of visitedness states.  (used by users other than
    // nsRuleWalker)
    eLinksVisitedOrUnvisited
  };

private:
  nsRuleNode* mCurrent; // Our current position.  Never null.
  nsRuleNode* mRoot; // The root of the tree we're walking.
  PRUint8 mLevel; // an nsStyleSet::sheetType
  bool mImportance;
  bool mCheckForImportantRules; // If true, check for important rules as
                                        // we walk and set to false if we find
                                        // one.

public:
  nsRuleWalker(nsRuleNode* aRoot)
    : mCurrent(aRoot)
    , mRoot(aRoot)
  {
    NS_ASSERTION(mCurrent, "Caller screwed up and gave us null node");
    MOZ_COUNT_CTOR(nsRuleWalker);
  }
  ~nsRuleWalker() { MOZ_COUNT_DTOR(nsRuleWalker); }
};

#endif /* !defined(nsRuleWalker_h_) */
