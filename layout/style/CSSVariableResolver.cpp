/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* object that resolves CSS variables using specified and inherited variable
 * values
 */

#include "CSSVariableResolver.h"

#include "CSSVariableDeclarations.h"
#include "CSSVariableValues.h"
#include "mozilla/PodOperations.h"
#include <algorithm>

namespace mozilla {

/**
 * Data used by the EnumerateVariableReferences callback.  Reset must be called
 * on it before it is used.
 */
class EnumerateVariableReferencesData
{
public:
  EnumerateVariableReferencesData(CSSVariableResolver& aResolver)
    : mResolver(aResolver)
    , mReferences(new bool[aResolver.mVariables.Length()])
  {
  }

  /**
   * Resets the data so that it can be passed to another call of
   * EnumerateVariableReferences for a different variable.
   */
  void Reset()
  {
    PodZero(mReferences.get(), mResolver.mVariables.Length());
    mReferencesNonExistentVariable = false;
  }

  void RecordVariableReference(const nsAString& aVariableName)
  {
    size_t id;
    if (mResolver.mVariableIDs.Get(aVariableName, &id)) {
      mReferences[id] = true;
    } else {
      mReferencesNonExistentVariable = true;
    }
  }

  bool HasReferenceToVariable(size_t aID) const
  {
    return mReferences[aID];
  }

  bool ReferencesNonExistentVariable() const
  {
   return mReferencesNonExistentVariable;
  }

private:
  CSSVariableResolver& mResolver;

  // Array of booleans, where each index is a variable ID.  If an element is
  // true, it indicates that the variable we have called
  // EnumerateVariableReferences for has a reference to the variable with
  // that ID.
  nsAutoArrayPtr<bool> mReferences;

  // Whether the variable we have called EnumerateVariableReferences for
  // references a variable that does not exist in the resolver.
  bool mReferencesNonExistentVariable;
};

static void
RecordVariableReference(const nsAString& aVariableName,
                        void* aData)
{
  static_cast<EnumerateVariableReferencesData*>(aData)->
    RecordVariableReference(aVariableName);
}

void
CSSVariableResolver::RemoveCycles(size_t v)
{
  mVariables[v].mIndex = mNextIndex;
  mVariables[v].mLowLink = mNextIndex;
  mVariables[v].mInStack = true;
  mStack.AppendElement(v);
  mNextIndex++;

  for (size_t i = 0, n = mReferences[v].Length(); i < n; i++) {
    size_t w = mReferences[v][i];
    if (!mVariables[w].mIndex) {
      RemoveCycles(w);
      mVariables[v].mLowLink = std::min(mVariables[v].mLowLink,
                                        mVariables[w].mLowLink);
    } else if (mVariables[w].mInStack) {
      mVariables[v].mLowLink = std::min(mVariables[v].mLowLink,
                                        mVariables[w].mIndex);
    }
  }

  if (mVariables[v].mLowLink == mVariables[v].mIndex) {
    if (mStack.LastElement() == v) {
      // A strongly connected component consisting of a single variable is not
      // necessarily invalid.  We handle variables that reference themselves
      // earlier, in CSSVariableResolver::Resolve.
      mVariables[mStack.LastElement()].mInStack = false;
      mStack.TruncateLength(mStack.Length() - 1);
    } else {
      size_t w;
      do {
        w = mStack.LastElement();
        mVariables[w].mValue.Truncate(0);
        mVariables[w].mInStack = false;
        mStack.TruncateLength(mStack.Length() - 1);
      } while (w != v);
    }
  }
}

void
CSSVariableResolver::ResolveVariable(size_t aID)
{
  if (mVariables[aID].mValue.IsEmpty() || mVariables[aID].mWasInherited) {
    // The variable is invalid or was inherited.   We can just copy the value
    // and its first/last token information across.
    mOutput->Put(mVariables[aID].mVariableName,
                 mVariables[aID].mValue,
                 mVariables[aID].mFirstToken,
                 mVariables[aID].mLastToken);
  } else {
    // Otherwise we need to resolve the variable references, after resolving
    // all of our dependencies first.  We do this even for variables that we
    // know do not reference other variables so that we can find their
    // first/last token.
    //
    // XXX We might want to do this first/last token finding during
    // EnumerateVariableReferences, so that we can avoid calling
    // ResolveVariableValue and parsing the value again.
    for (size_t i = 0, n = mReferences[aID].Length(); i < n; i++) {
      size_t j = mReferences[aID][i];
      if (aID != j && !mVariables[j].mResolved) {
        ResolveVariable(j);
      }
    }
    nsString resolvedValue;
    nsCSSTokenSerializationType firstToken, lastToken;
    if (!mParser.ResolveVariableValue(mVariables[aID].mValue, mOutput,
                                      resolvedValue, firstToken, lastToken)) {
      resolvedValue.Truncate(0);
    }
    mOutput->Put(mVariables[aID].mVariableName, resolvedValue,
                 firstToken, lastToken);
  }
  mVariables[aID].mResolved = true;
}

void
CSSVariableResolver::Resolve(const CSSVariableValues* aInherited,
                             const CSSVariableDeclarations* aSpecified)
{
  MOZ_ASSERT(!mResolved);

  // The only time we would be worried about having a null aInherited is
  // for the root, but in that case nsRuleNode::ComputeVariablesData will
  // happen to pass in whatever we're using as mOutput for aInherited,
  // which will initially be empty.
  MOZ_ASSERT(aInherited);
  MOZ_ASSERT(aSpecified);

  aInherited->AddVariablesToResolver(this);
  aSpecified->AddVariablesToResolver(this);

  // First we look at each variable's value and record which other variables
  // it references.
  size_t n = mVariables.Length();
  mReferences.SetLength(n);
  EnumerateVariableReferencesData data(*this);
  for (size_t id = 0; id < n; id++) {
    data.Reset();
    if (!mVariables[id].mWasInherited &&
        !mVariables[id].mValue.IsEmpty()) {
      if (mParser.EnumerateVariableReferences(mVariables[id].mValue,
                                              RecordVariableReference,
                                              &data)) {
        // Convert the boolean array of dependencies in |data| to a list
        // of dependencies.
        for (size_t i = 0; i < n; i++) {
          if (data.HasReferenceToVariable(i)) {
            mReferences[id].AppendElement(i);
          }
        }
        // If a variable references itself, it is invalid.  (RemoveCycles
        // does not check for cycles consisting of a single variable, so we
        // check here.)
        if (data.HasReferenceToVariable(id)) {
          mVariables[id].mValue.Truncate();
        }
        // Also record whether it referenced any variables that don't exist
        // in the resolver, so that we can ensure we still resolve its value
        // in ResolveVariable, even though its mReferences list is empty.
        mVariables[id].mReferencesNonExistentVariable =
          data.ReferencesNonExistentVariable();
      } else {
        MOZ_ASSERT(false, "EnumerateVariableReferences should not have failed "
                          "if we previously parsed the specified value");
        mVariables[id].mValue.Truncate(0);
      }
    }
  }

  // Next we remove any cycles in variable references using Tarjan's strongly
  // connected components finding algorithm, setting variables in cycles to
  // have an invalid value.
  mNextIndex = 1;
  for (size_t id = 0; id < n; id++) {
    if (!mVariables[id].mIndex) {
      RemoveCycles(id);
      MOZ_ASSERT(mStack.IsEmpty());
    }
  }

  // Finally we construct the computed value for the variable by substituting
  // any variable references.
  for (size_t id = 0; id < n; id++) {
    if (!mVariables[id].mResolved) {
      ResolveVariable(id);
    }
  }

#ifdef DEBUG
  mResolved = true;
#endif
}

void
CSSVariableResolver::Put(const nsAString& aVariableName,
                         nsString aValue,
                         nsCSSTokenSerializationType aFirstToken,
                         nsCSSTokenSerializationType aLastToken,
                         bool aWasInherited)
{
  MOZ_ASSERT(!mResolved);

  size_t id;
  if (mVariableIDs.Get(aVariableName, &id)) {
    MOZ_ASSERT(mVariables[id].mWasInherited && !aWasInherited,
               "should only overwrite inherited variables with specified "
               "variables");
    mVariables[id].mValue = aValue;
    mVariables[id].mFirstToken = aFirstToken;
    mVariables[id].mLastToken = aLastToken;
    mVariables[id].mWasInherited = aWasInherited;
  } else {
    id = mVariables.Length();
    mVariableIDs.Put(aVariableName, id);
    mVariables.AppendElement(Variable(aVariableName, aValue,
                                      aFirstToken, aLastToken, aWasInherited));
  }
}

}
