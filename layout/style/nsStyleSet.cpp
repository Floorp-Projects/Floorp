/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleRule.h"
#include "nsIStyleContext.h"
#include "nsISupportsArray.h"
#include "nsIFrame.h"
#include "nsHashtable.h"

static NS_DEFINE_IID(kIStyleSetIID, NS_ISTYLE_SET_IID);

class ContextKey : public nsHashKey {
public:
  ContextKey(nsIStyleContext* aContext);
  ContextKey(nsIStyleContext* aParent, nsISupportsArray* aRules);
  virtual ~ContextKey(void);

  void        SetContext(nsIStyleContext* aContext);

  PRBool      Equals(const nsHashKey* aOther) const;
  PRUint32    HashValue(void) const;
  nsHashKey*  Clone(void) const;

private:
  ContextKey(void);
  ContextKey(const ContextKey& aCopy);
  ContextKey& operator=(const ContextKey& aCopy) const;

protected:
  nsIStyleContext* mContext;
  nsIStyleContext* mParent;
  nsISupportsArray* mRules;
};

ContextKey::ContextKey(nsIStyleContext* aContext)
  : mContext(aContext),
    mParent(nsnull),
    mRules(nsnull)
{
  NS_IF_ADDREF(mContext);
}

ContextKey::ContextKey(nsIStyleContext* aParent, nsISupportsArray* aRules)
  : mContext(nsnull),
    mParent(aParent),
    mRules(aRules)
{
  NS_IF_ADDREF(mParent);
  NS_IF_ADDREF(mRules);
}

ContextKey::ContextKey(const ContextKey& aCopy)
  : mContext(aCopy.mContext),
    mParent(aCopy.mParent),
    mRules(aCopy.mRules)
{
  NS_IF_ADDREF(mContext);
  NS_IF_ADDREF(mParent);
  NS_IF_ADDREF(mRules);
}

ContextKey::~ContextKey(void)
{
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mParent);
  NS_IF_RELEASE(mRules);
}

void ContextKey::SetContext(nsIStyleContext* aContext)
{
  if (aContext != mContext) {
    NS_IF_RELEASE(mContext);
    mContext = aContext;
    NS_IF_ADDREF(mContext);
  }
  NS_IF_RELEASE(mParent);
  NS_IF_RELEASE(mRules);
}

PRBool ContextKey::Equals(const nsHashKey* aOther) const
{
  PRBool result = PR_TRUE;
  const ContextKey* other = (const ContextKey*)aOther;

  if (other != this) {
    if ((nsnull == mContext) || (nsnull == other->mContext) || (mContext != other->mContext)) {
      nsIStyleContext* otherParent = other->mParent;
      if ((nsnull == otherParent) && (nsnull != other->mContext)) {
        otherParent = other->mContext->GetParent();
      }
      else {
        NS_IF_ADDREF(otherParent);  // simulate the above addref
      }

      if (mParent == otherParent) {
        nsISupportsArray* otherRules = other->mRules;
        if ((nsnull == otherRules) && (nsnull != other->mContext)) {
          otherRules = other->mContext->GetStyleRules();
        }
        else {
          NS_IF_ADDREF(otherRules); // simulate the above addref
        }
        if ((nsnull != mRules) && (nsnull != otherRules)) {
          result = mRules->Equals(otherRules);
        }
        else {
          result = PRBool((nsnull == mRules) && (nsnull == otherRules));
        }
        NS_IF_RELEASE(otherRules);
      }
      else {
        result = PR_FALSE;
      }
      NS_IF_RELEASE(otherParent);
    }
  }
  return result;
}

PRUint32 ContextKey::HashValue(void) const
{
  if (nsnull != mContext) {
    return mContext->HashValue();
  }
  // we don't have a context yet, compute it like it would
  PRUint32 hashValue = ((nsnull != mParent) ? mParent->HashValue() : 0);
  if (nsnull != mRules) {
    PRInt32 index = mRules->Count();
    while (0 <= --index) {
      nsIStyleRule* rule = (nsIStyleRule*)mRules->ElementAt(index);
      PRUint32 hash = rule->HashValue();
      hashValue ^= (hash & 0x7FFFFFFF);
      NS_RELEASE(rule);
    }
  }
  return hashValue;
}

nsHashKey* ContextKey::Clone(void) const
{
  return new ContextKey(*this);
}


class StyleSetImpl : public nsIStyleSet {
public:
  StyleSetImpl();

  NS_DECL_ISUPPORTS

  virtual void AppendOverrideStyleSheet(nsIStyleSheet* aSheet);
  virtual void InsertOverrideStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet);
  virtual void InsertOverrideStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet);
  virtual void RemoveOverrideStyleSheet(nsIStyleSheet* aSheet);
  virtual PRInt32 GetNumberOfOverrideStyleSheets();
  virtual nsIStyleSheet* GetOverrideStyleSheetAt(PRInt32 aIndex);

  virtual void AppendDocStyleSheet(nsIStyleSheet* aSheet);
  virtual void InsertDocStyleSheetAfter(nsIStyleSheet* aSheet,
                                        nsIStyleSheet* aAfterSheet);
  virtual void InsertDocStyleSheetBefore(nsIStyleSheet* aSheet,
                                         nsIStyleSheet* aBeforeSheet);
  virtual void RemoveDocStyleSheet(nsIStyleSheet* aSheet);
  virtual PRInt32 GetNumberOfDocStyleSheets();
  virtual nsIStyleSheet* GetDocStyleSheetAt(PRInt32 aIndex);

  virtual void AppendBackstopStyleSheet(nsIStyleSheet* aSheet);
  virtual void InsertBackstopStyleSheetAfter(nsIStyleSheet* aSheet,
                                             nsIStyleSheet* aAfterSheet);
  virtual void InsertBackstopStyleSheetBefore(nsIStyleSheet* aSheet,
                                              nsIStyleSheet* aBeforeSheet);
  virtual void RemoveBackstopStyleSheet(nsIStyleSheet* aSheet);
  virtual PRInt32 GetNumberOfBackstopStyleSheets();
  virtual nsIStyleSheet* GetBackstopStyleSheetAt(PRInt32 aIndex);

  virtual nsIStyleContext* ResolveStyleFor(nsIPresContext* aPresContext,
                                           nsIContent* aContent,
                                           nsIFrame* aParentFrame,
                                           PRBool aForceUnique = PR_FALSE);

  virtual nsIStyleContext* ResolvePseudoStyleFor(nsIPresContext* aPresContext,
                                                 nsIAtom* aPseudoTag,
                                                 nsIFrame* aParentFrame,
                                                 PRBool aForceUnique = PR_FALSE);

  virtual nsIStyleContext* ProbePseudoStyleFor(nsIPresContext* aPresContext,
                                               nsIAtom* aPseudoTag,
                                               nsIFrame* aParentFrame,
                                               PRBool aForceUnique = PR_FALSE);

  // xxx style rules enumeration

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0);

private:
  // These are not supported and are not implemented!
  StyleSetImpl(const StyleSetImpl& aCopy);
  StyleSetImpl& operator=(const StyleSetImpl& aCopy);

protected:
  virtual ~StyleSetImpl();
  PRBool EnsureArray(nsISupportsArray** aArray);
  nsIStyleContext* GetContext(nsIPresContext* aPresContext, nsIFrame* aParentFrame, 
                              nsIStyleContext* aParentContext, nsISupportsArray* aRules,
                              PRBool aForceUnique);
  PRInt32 RulesMatching(nsISupportsArray* aSheets,
                        nsIPresContext* aPresContext,
                        nsIContent* aContent,
                        nsIFrame* aParentFrame,
                        nsISupportsArray* aResults);
  PRInt32 RulesMatching(nsISupportsArray* aSheets,
                        nsIPresContext* aPresContext,
                        nsIAtom* aPseudoTag,
                        nsIFrame* aParentFrame,
                        nsISupportsArray* aResults);
  void  List(FILE* out, PRInt32 aIndent, nsISupportsArray* aSheets);
  void  ListContexts(FILE* out, PRInt32 aIndent);

  nsISupportsArray* mOverrideSheets;
  nsISupportsArray* mDocSheets;
  nsISupportsArray* mBackstopSheets;
  nsHashtable mStyleContexts;
};


StyleSetImpl::StyleSetImpl()
  : mOverrideSheets(nsnull),
    mDocSheets(nsnull),
    mBackstopSheets(nsnull)
{
  NS_INIT_REFCNT();
}

PRBool ReleaseContext(nsHashKey *aKey, void *aData)
{
  ((nsIStyleContext*)aData)->Release();
  return PR_TRUE;
}

StyleSetImpl::~StyleSetImpl()
{
  NS_IF_RELEASE(mOverrideSheets);
  NS_IF_RELEASE(mDocSheets);
  NS_IF_RELEASE(mBackstopSheets);
  mStyleContexts.Enumerate(ReleaseContext);
}

NS_IMPL_ISUPPORTS(StyleSetImpl, kIStyleSetIID)

PRBool StyleSetImpl::EnsureArray(nsISupportsArray** aArray)
{
  if (nsnull == *aArray) {
    if (NS_OK != NS_NewISupportsArray(aArray)) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

// ----- Override sheets

void StyleSetImpl::AppendOverrideStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mOverrideSheets)) {
    mOverrideSheets->AppendElement(aSheet);
  }
}

void StyleSetImpl::InsertOverrideStyleSheetAfter(nsIStyleSheet* aSheet,
                                                 nsIStyleSheet* aAfterSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mOverrideSheets)) {
    PRInt32 index = mOverrideSheets->IndexOf(aAfterSheet);
    mOverrideSheets->InsertElementAt(aSheet, ++index);
  }
}

void StyleSetImpl::InsertOverrideStyleSheetBefore(nsIStyleSheet* aSheet,
                                                  nsIStyleSheet* aBeforeSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mOverrideSheets)) {
    PRInt32 index = mOverrideSheets->IndexOf(aBeforeSheet);
    mOverrideSheets->InsertElementAt(aSheet, ((-1 < index) ? index : 0));
  }
}

void StyleSetImpl::RemoveOverrideStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (nsnull != mOverrideSheets) {
    mOverrideSheets->RemoveElement(aSheet);
  }
}

PRInt32 StyleSetImpl::GetNumberOfOverrideStyleSheets()
{
  if (nsnull != mOverrideSheets) {
    return mOverrideSheets->Count();
  }
  return 0;
}

nsIStyleSheet* StyleSetImpl::GetOverrideStyleSheetAt(PRInt32 aIndex)
{
  nsIStyleSheet* sheet = nsnull;
  if (nsnull == mOverrideSheets) {
    sheet = (nsIStyleSheet*)mOverrideSheets->ElementAt(aIndex);
  }
  return sheet;
}

// -------- Doc Sheets

void StyleSetImpl::AppendDocStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mDocSheets)) {
    mDocSheets->AppendElement(aSheet);
  }
}

void StyleSetImpl::InsertDocStyleSheetAfter(nsIStyleSheet* aSheet,
                                         nsIStyleSheet* aAfterSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mDocSheets)) {
    PRInt32 index = mDocSheets->IndexOf(aAfterSheet);
    mDocSheets->InsertElementAt(aSheet, ++index);
  }
}

void StyleSetImpl::InsertDocStyleSheetBefore(nsIStyleSheet* aSheet,
                                          nsIStyleSheet* aBeforeSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mDocSheets)) {
    PRInt32 index = mDocSheets->IndexOf(aBeforeSheet);
    mDocSheets->InsertElementAt(aSheet, ((-1 < index) ? index : 0));
  }
}

void StyleSetImpl::RemoveDocStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (nsnull != mDocSheets) {
    mDocSheets->RemoveElement(aSheet);
  }
}

PRInt32 StyleSetImpl::GetNumberOfDocStyleSheets()
{
  if (nsnull != mDocSheets) {
    return mDocSheets->Count();
  }
  return 0;
}

nsIStyleSheet* StyleSetImpl::GetDocStyleSheetAt(PRInt32 aIndex)
{
  nsIStyleSheet* sheet = nsnull;
  if (nsnull == mDocSheets) {
    sheet = (nsIStyleSheet*)mDocSheets->ElementAt(aIndex);
  }
  return sheet;
}

// ------ backstop sheets

void StyleSetImpl::AppendBackstopStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mBackstopSheets)) {
    mBackstopSheets->AppendElement(aSheet);
  }
}

void StyleSetImpl::InsertBackstopStyleSheetAfter(nsIStyleSheet* aSheet,
                                                 nsIStyleSheet* aAfterSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mBackstopSheets)) {
    PRInt32 index = mBackstopSheets->IndexOf(aAfterSheet);
    mBackstopSheets->InsertElementAt(aSheet, ++index);
  }
}

void StyleSetImpl::InsertBackstopStyleSheetBefore(nsIStyleSheet* aSheet,
                                                  nsIStyleSheet* aBeforeSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  if (EnsureArray(&mBackstopSheets)) {
    PRInt32 index = mBackstopSheets->IndexOf(aBeforeSheet);
    mBackstopSheets->InsertElementAt(aSheet, ((-1 < index) ? index : 0));
  }
}

void StyleSetImpl::RemoveBackstopStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");

  if (nsnull != mBackstopSheets) {
    mBackstopSheets->RemoveElement(aSheet);
  }
}

PRInt32 StyleSetImpl::GetNumberOfBackstopStyleSheets()
{
  if (nsnull != mBackstopSheets) {
    return mBackstopSheets->Count();
  }
  return 0;
}

nsIStyleSheet* StyleSetImpl::GetBackstopStyleSheetAt(PRInt32 aIndex)
{
  nsIStyleSheet* sheet = nsnull;
  if (nsnull == mBackstopSheets) {
    sheet = (nsIStyleSheet*)mBackstopSheets->ElementAt(aIndex);
  }
  return sheet;
}

PRInt32 StyleSetImpl::RulesMatching(nsISupportsArray* aSheets,
                                    nsIPresContext* aPresContext,
                                    nsIContent* aContent,
                                    nsIFrame* aParentFrame,
                                    nsISupportsArray* aResults)
{
  PRInt32 ruleCount = 0;

  if (nsnull != aSheets) {
    PRInt32 sheetCount = aSheets->Count();
    PRInt32 index;
    for (index = 0; index < sheetCount; index++) {
      nsIStyleSheet* sheet = (nsIStyleSheet*)aSheets->ElementAt(index);
      ruleCount += sheet->RulesMatching(aPresContext, aContent, aParentFrame,
                                        aResults);
      NS_RELEASE(sheet);
    }
  }
  return ruleCount;
}

nsIStyleContext* StyleSetImpl::GetContext(nsIPresContext* aPresContext, nsIFrame* aParentFrame, 
                                          nsIStyleContext* aParentContext, nsISupportsArray* aRules,
                                          PRBool aForceUnique)
{
  nsIStyleContext* result;

  if ((PR_FALSE == aForceUnique) && 
      (nsnull != aParentContext) && (0 == aRules->Count()) && 
      (0 == aParentContext->GetStyleRuleCount())) {
    // this and parent are empty
    result = aParentContext;
    NS_ADDREF(result);  // add ref for the caller
//fprintf(stdout, ".");
  }
  else {
#if USE_CONTEXT_HASH
    // check for cached ruleSet to context or create
    ContextKey tempKey(aParentContext, aRules);
    if (PR_FALSE == aForceUnique) {
      result = (nsIStyleContext*)mStyleContexts.Get(&tempKey);
    } else {
      result = nsnull;
    }
    if (nsnull == result) {
      if (NS_OK == NS_NewStyleContext(&result, aParentContext, aRules, aPresContext)) {
        if (PR_TRUE == aForceUnique) {
          result->ForceUnique();
        }
        tempKey.SetContext(result);
        mStyleContexts.Put(&tempKey, result);  // hashtable clones key, so this is OK (table gets first ref)
//fprintf(stdout, "+");
      }
    }
    else {
//fprintf(stdout, "-");
    }
    NS_ADDREF(result);  // add ref for the caller
#else
    if ((PR_FALSE == aForceUnique) && (nsnull != aParentContext)) {
      result = aParentContext->FindChildWithRules(aRules);
    }
    else {
      result = nsnull;
    }
    if (nsnull == result) {
      if (NS_OK == NS_NewStyleContext(&result, aParentContext, aRules, aPresContext)) {
        if (PR_TRUE == aForceUnique) {
          result->ForceUnique();
        }
      }
//fprintf(stdout, "+");
    }
    else {
//fprintf(stdout, "-");
    }
#endif
  }
  return result;
}

nsIStyleContext* StyleSetImpl::ResolveStyleFor(nsIPresContext* aPresContext,
                                               nsIContent* aContent,
                                               nsIFrame* aParentFrame,
                                               PRBool aForceUnique)
{
  nsIStyleContext*  result = nsnull;
  nsIStyleContext*  parentContext = nsnull;
  
  if (nsnull != aParentFrame) {
    aParentFrame->GetStyleContext(aPresContext, parentContext);
    NS_ASSERTION(nsnull != parentContext, "parent must have style context");
  }

  // want to check parent frame's context for cached child context first

  // then do a brute force rule search

  nsISupportsArray*  rules = nsnull;
  if (NS_OK == NS_NewISupportsArray(&rules)) {
    PRInt32 ruleCount = RulesMatching(mOverrideSheets, aPresContext, aContent, aParentFrame, rules);
    ruleCount += RulesMatching(mDocSheets, aPresContext, aContent, aParentFrame, rules);
    ruleCount += RulesMatching(mBackstopSheets, aPresContext, aContent, aParentFrame, rules);

    result = GetContext(aPresContext, aParentFrame, parentContext, rules, aForceUnique);

    NS_RELEASE(rules);
  }

  NS_IF_RELEASE(parentContext);

  return result;
}


PRInt32 StyleSetImpl::RulesMatching(nsISupportsArray* aSheets,
                                    nsIPresContext* aPresContext,
                                    nsIAtom* aPseudoTag,
                                    nsIFrame* aParentFrame,
                                    nsISupportsArray* aResults)
{
  PRInt32 ruleCount = 0;

  if (nsnull != aSheets) {
    PRInt32 sheetCount = aSheets->Count();
    PRInt32 index;
    for (index = 0; index < sheetCount; index++) {
      nsIStyleSheet* sheet = (nsIStyleSheet*)aSheets->ElementAt(index);
      ruleCount += sheet->RulesMatching(aPresContext, aPseudoTag, aParentFrame,
                                        aResults);
      NS_RELEASE(sheet);
    }
  }
  return ruleCount;
}

nsIStyleContext* StyleSetImpl::ResolvePseudoStyleFor(nsIPresContext* aPresContext,
                                                     nsIAtom* aPseudoTag,
                                                     nsIFrame* aParentFrame,
                                                     PRBool aForceUnique)
{
  nsIStyleContext*  result = nsnull;
  nsIStyleContext*  parentContext = nsnull;
  
  if (nsnull != aParentFrame) {
    aParentFrame->GetStyleContext(aPresContext, parentContext);
    NS_ASSERTION(nsnull != parentContext, "parent must have style context");
  }

  // want to check parent frame's context for cached child context first

  // then do a brute force rule search

  nsISupportsArray*  rules = nsnull;
  if (NS_OK == NS_NewISupportsArray(&rules)) {
    PRInt32 ruleCount = RulesMatching(mOverrideSheets, aPresContext, aPseudoTag, aParentFrame, rules);
    ruleCount += RulesMatching(mDocSheets, aPresContext, aPseudoTag, aParentFrame, rules);
    ruleCount += RulesMatching(mBackstopSheets, aPresContext, aPseudoTag, aParentFrame, rules);

    result = GetContext(aPresContext, aParentFrame, parentContext, rules, aForceUnique);

    NS_RELEASE(rules);
  }

  NS_IF_RELEASE(parentContext);

  return result;
}

nsIStyleContext* StyleSetImpl::ProbePseudoStyleFor(nsIPresContext* aPresContext,
                                                   nsIAtom* aPseudoTag,
                                                   nsIFrame* aParentFrame,
                                                   PRBool aForceUnique)
{
  nsIStyleContext*  result = nsnull;
  nsIStyleContext*  parentContext = nsnull;
  
  if (nsnull != aParentFrame) {
    aParentFrame->GetStyleContext(aPresContext, parentContext);
    NS_ASSERTION(nsnull != parentContext, "parent must have style context");
  }

  // want to check parent frame's context for cached child context first

  // then do a brute force rule search

  nsISupportsArray*  rules = nsnull;
  if (NS_OK == NS_NewISupportsArray(&rules)) {
    PRInt32 ruleCount = RulesMatching(mOverrideSheets, aPresContext, aPseudoTag, aParentFrame, rules);
    ruleCount += RulesMatching(mDocSheets, aPresContext, aPseudoTag, aParentFrame, rules);
    ruleCount += RulesMatching(mBackstopSheets, aPresContext, aPseudoTag, aParentFrame, rules);

    if (0 < ruleCount) {
      result = GetContext(aPresContext, aParentFrame, parentContext, rules, aForceUnique);
    }

    NS_RELEASE(rules);
  }

  NS_IF_RELEASE(parentContext);

  return result;
}

// xxx style rules enumeration

void StyleSetImpl::List(FILE* out, PRInt32 aIndent, nsISupportsArray* aSheets)
{
  PRInt32 count = ((nsnull != aSheets) ? aSheets->Count() : 0);

  for (PRInt32 index = 0; index < count; index++) {
    nsIStyleSheet* sheet = (nsIStyleSheet*)aSheets->ElementAt(index);
    sheet->List(out, aIndent);
    fputs("\n", out);
    NS_RELEASE(sheet);
  }
}

void StyleSetImpl::List(FILE* out, PRInt32 aIndent)
{
  List(out, aIndent, mOverrideSheets);
  List(out, aIndent, mDocSheets);
  List(out, aIndent, mBackstopSheets);
}

struct ContextNode {
  nsIStyleContext*  mContext;
  ContextNode*      mNext;
  ContextNode*      mChild;
  ContextNode(nsIStyleContext* aContext)
  {
    mContext = aContext;
    mNext = nsnull;
    mChild = nsnull;
  }
  ~ContextNode(void)
  {
    if (nsnull != mNext) {
      delete mNext;
    }
    if (nsnull != mChild) {
      delete mChild;
    }
  }
};

static ContextNode*  gRootNode;

static ContextNode* FindNode(nsIStyleContext* aContext, ContextNode* aStart)
{
  ContextNode* node = aStart;
  while (nsnull != node) {
    if (node->mContext == aContext) {
      return node;
    }
    if (nsnull != node->mChild) {
      ContextNode* result = FindNode(aContext, node->mChild);
      if (nsnull != result) {
        return result;
      }
    }
    node = node->mNext;
  }
  return nsnull;
}

PRBool GatherContexts(nsHashKey *aKey, void *aData)
{
  PRBool  result = PR_TRUE;

  nsIStyleContext* context = (nsIStyleContext*)aData;
  nsIStyleContext* parent = context->GetParent();
  ContextNode* node = new ContextNode(context);

  if (nsnull == gRootNode) {
    gRootNode = node;
  }
  else {
    if (nsnull == parent) { // found real root, replace temp
      node->mNext = gRootNode;  // orphan the old root
      gRootNode = node;
    }
    else {
      ContextNode*  parentNode = FindNode(parent, gRootNode);
      if (nsnull == parentNode) { // hang orhpans off root
        node->mNext = gRootNode->mNext;
        gRootNode->mNext = node;
      }
      else {
        node->mNext = parentNode->mChild;
        parentNode->mChild = node;
      }
    }
  }
  // graft orphans
  ContextNode*  prevOrphan = nsnull;
  ContextNode*  orphan = gRootNode;
  while (nsnull != orphan) {
    nsIStyleContext*  orphanParent = orphan->mContext->GetParent();
    if (orphanParent == context) {  // found our child
      if (orphan == gRootNode) {
        gRootNode = orphan->mNext;
        orphan->mNext = node->mChild;
        node->mChild = orphan;
        orphan = gRootNode;
      }
      else {
        ContextNode* foundling = orphan;
        orphan = orphan->mNext;
        prevOrphan->mNext = orphan;
        foundling->mNext = node->mChild;
        node->mChild = foundling;
      }
    }
    else {
      prevOrphan = orphan;
      orphan = orphan->mNext;
    }
    NS_IF_RELEASE(orphanParent);
  }
  NS_IF_RELEASE(parent);
  return result;
}

static PRInt32 ListNode(ContextNode* aNode, FILE* out, PRInt32 aIndent) 
{
  PRInt32 count = 0;
  ContextNode* node = aNode;
  while (nsnull != node) {
    node->mContext->List(out, aIndent);
    count++;
    if (nsnull != node->mChild) {
      nsIStyleContext* childParent = node->mChild->mContext->GetParent();
      NS_ASSERTION(childParent == node->mContext, "broken graph");
      NS_RELEASE(childParent);
      count += ListNode(node->mChild, out, aIndent + 1);
    }
    node = node->mNext;
  }
  return count;
}

void StyleSetImpl::ListContexts(FILE* out, PRInt32 aIndent)
{
  mStyleContexts.Enumerate(GatherContexts);
  NS_ASSERTION(gRootNode->mNext == nsnull, "dangling orphan");

  PRInt32 listCount = ListNode(gRootNode, out, aIndent);
  NS_ASSERTION(listCount == mStyleContexts.Count(), "graph incomplete");

  delete gRootNode;
  gRootNode = nsnull;
}


NS_LAYOUT nsresult
NS_NewStyleSet(nsIStyleSet** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  StyleSetImpl  *it = new StyleSetImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIStyleSetIID, (void **) aInstancePtrResult);
}
