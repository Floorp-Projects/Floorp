/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* object that resolves CSS variables using specified and inherited variable
 * values
 */

#ifndef mozilla_CSSVariableResolver_h
#define mozilla_CSSVariableResolver_h

#include "mozilla/DebugOnly.h"
#include "nsCSSParser.h"
#include "nsCSSScanner.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

namespace mozilla {

class CSSVariableDeclarations;
class CSSVariableValues;
class EnumerateVariableReferencesData;

class CSSVariableResolver
{
  friend class CSSVariableDeclarations;
  friend class CSSVariableValues;
  friend class EnumerateVariableReferencesData;
public:
  /**
   * Creates a new CSSVariableResolver that will output a set of resolved,
   * computed variables into aOutput.
   */
  explicit CSSVariableResolver(CSSVariableValues* aOutput)
    : mOutput(aOutput)
#ifdef DEBUG
    , mResolved(false)
#endif
  {
    MOZ_ASSERT(aOutput);
  }

  /**
   * Resolves the set of inherited variables from aInherited and the
   * set of specified variables from aSpecified.  The resoled variables
   * are written in to mOutput.
   */
  void Resolve(const CSSVariableValues* aInherited,
               const CSSVariableDeclarations* aSpecified);

private:
  struct Variable
  {
    Variable(const nsAString& aVariableName,
             nsString aValue,
             nsCSSTokenSerializationType aFirstToken,
             nsCSSTokenSerializationType aLastToken,
             bool aWasInherited)
      : mVariableName(aVariableName)
      , mValue(aValue)
      , mFirstToken(aFirstToken)
      , mLastToken(aLastToken)
      , mWasInherited(aWasInherited)
      , mResolved(false)
      , mReferencesNonExistentVariable(false)
      , mInStack(false)
      , mIndex(0)
      , mLowLink(0) { }

    nsString mVariableName;
    nsString mValue;
    nsCSSTokenSerializationType mFirstToken;
    nsCSSTokenSerializationType mLastToken;

    // Whether this variable came from the set of inherited variables.
    bool mWasInherited;

    // Whether this variable has been resolved yet.
    bool mResolved;

    // Whether this variables includes any references to non-existent variables.
    bool mReferencesNonExistentVariable;

    // Bookkeeping for the cycle remover algorithm.
    bool mInStack;
    size_t mIndex;
    size_t mLowLink;
  };

  /**
   * Adds or modifies an existing entry in the set of variables to be resolved.
   * This is intended to be called by the AddVariablesToResolver functions on
   * the CSSVariableDeclarations and CSSVariableValues objects passed in to
   * Resolve.
   *
   * @param aName The variable name (not including any "--" prefix that would
   *   be part of the custom property name) whose value is to be set.
   * @param aValue The variable value.
   * @param aFirstToken The type of token at the start of the variable value.
   * @param aLastToken The type of token at the en of the variable value.
   * @param aWasInherited Whether this variable came from the set of inherited
   *   variables.
   */
  void Put(const nsAString& aVariableName,
           nsString aValue,
           nsCSSTokenSerializationType aFirstToken,
           nsCSSTokenSerializationType aLastToken,
           bool aWasInherited);

  // Helper functions for Resolve.
  void RemoveCycles(size_t aID);
  void ResolveVariable(size_t aID);

  // A mapping of variable names to an ID that indexes into mVariables
  // and mReferences.
  nsDataHashtable<nsStringHashKey, size_t> mVariableIDs;

  // The set of variables.
  nsTArray<Variable> mVariables;

  // The list of variables that each variable references.
  nsTArray<nsTArray<size_t> > mReferences;

  // The next index to assign to a variable found during the cycle removing
  // algorithm's traversal of the variable reference graph.
  size_t mNextIndex;

  // Stack of variable IDs that we push to as we traverse the variable reference
  // graph while looking for cycles.  Variable::mInStack reflects whether a
  // given variable has its ID in mStack.
  nsTArray<size_t> mStack;

  // CSS parser to use for parsing property values with variable references.
  nsCSSParser mParser;

  // The object to output the resolved variables into.
  CSSVariableValues* mOutput;

#ifdef DEBUG
  // Whether Resolve has been called.
  bool mResolved;
#endif
};

} // namespace mozilla

#endif
