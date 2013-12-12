/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* computed CSS Variable values */

#ifndef mozilla_CSSVariableValues_h
#define mozilla_CSSVariableValues_h

#include "nsCSSScanner.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

namespace mozilla {

class CSSVariableResolver;

class CSSVariableValues
{
public:
  CSSVariableValues();
  CSSVariableValues(const CSSVariableValues& aOther);
#ifdef DEBUG
  ~CSSVariableValues();
#endif
  CSSVariableValues& operator=(const CSSVariableValues& aOther);

  bool operator==(const CSSVariableValues& aOther) const;
  bool operator!=(const CSSVariableValues& aOther) const
    { return !(*this == aOther); }

  /**
   * Gets the value of a variable in this set of computed variables.
   *
   * @param aName The variable name (not including any "var-" prefix that would
   *   be part of the custom property name).
   * @param aValue Out parameter into which the value of the variable will
   *   be stored.
   * @return Whether a variable with the given name was found.  When false
   *   is returned, aValue will not be modified.
   */
  bool Get(const nsAString& aName, nsString& aValue) const;

  /**
   * Gets the value of a variable in this set of computed variables, along
   * with information on the types of tokens that appear at the start and
   * end of the token stream.
   *
   * @param aName The variable name (not including any "var-" prefix that would
   *   be part of the custom property name).
   * @param aValue Out parameter into which the value of the variable will
   *   be stored.
   * @param aFirstToken The type of token at the start of the variable value.
   * @param aLastToken The type of token at the en of the variable value.
   * @return Whether a variable with the given name was found.  When false
   *   is returned, aValue, aFirstToken and aLastToken will not be modified.
   */
  bool Get(const nsAString& aName,
           nsString& aValue,
           nsCSSTokenSerializationType& aFirstToken,
           nsCSSTokenSerializationType& aLastToken) const;

  /**
   * Gets the name of the variable at the given index.
   *
   * Variables on this object are ordered, and that order is just determined
   * based on the order that they are added to the object.  A consistent
   * ordering is required for CSSDeclaration objects in the DOM.
   * CSSDeclarations expose property names as indexed properties, which need to
   * be stable.
   *
   * @param aIndex The index of the variable to get.
   * @param aName Out parameter into which the name of the variable will be
   *   stored.
   */
  void GetVariableAt(size_t aIndex, nsAString& aName) const;

  /**
   * Gets the number of variables stored on this object.
   */
  size_t Count() const;

  /**
   * Adds or modifies an existing entry in this set of variable values.
   *
   * @param aName The variable name (not including any "var-" prefix that would
   *   be part of the custom property name) whose value is to be set.
   * @param aValue The variable value.
   * @param aFirstToken The type of token at the start of the variable value.
   * @param aLastToken The type of token at the en of the variable value.
   */
  void Put(const nsAString& aName,
           nsString aValue,
           nsCSSTokenSerializationType aFirstToken,
           nsCSSTokenSerializationType aLastToken);

  /**
   * Copies the variables from this object into aResolver, marking them as
   * computed, inherited values.
   */
  void AddVariablesToResolver(CSSVariableResolver* aResolver) const;

private:
  struct Variable
  {
    Variable()
      : mFirstToken(eCSSTokenSerialization_Nothing)
      , mLastToken(eCSSTokenSerialization_Nothing)
    {}

    Variable(const nsAString& aVariableName,
             nsString aValue,
             nsCSSTokenSerializationType aFirstToken,
             nsCSSTokenSerializationType aLastToken)
      : mVariableName(aVariableName)
      , mValue(aValue)
      , mFirstToken(aFirstToken)
      , mLastToken(aLastToken)
    {}

    nsString mVariableName;
    nsString mValue;
    nsCSSTokenSerializationType mFirstToken;
    nsCSSTokenSerializationType mLastToken;
  };

  /**
   * Adds all the variables from aOther into this object.
   */
  void CopyVariablesFrom(const CSSVariableValues& aOther);

  /**
   * Map of variable names to IDs.  Variable IDs are indexes into
   * mVariables.
   */
  nsDataHashtable<nsStringHashKey, size_t> mVariableIDs;

  /**
   * Array of variables, indexed by variable ID.
   */
  nsTArray<Variable> mVariables;
};

} // namespace mozilla

#endif
