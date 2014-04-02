/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS Custom Property assignments for a Declaration at a given priority */

#ifndef mozilla_CSSVariableDeclarations_h
#define mozilla_CSSVariableDeclarations_h

#include "nsDataHashtable.h"

namespace mozilla {
class CSSVariableResolver;
}
class nsRuleData;

namespace mozilla {

class CSSVariableDeclarations
{
public:
  CSSVariableDeclarations();
  CSSVariableDeclarations(const CSSVariableDeclarations& aOther);
#ifdef DEBUG
  ~CSSVariableDeclarations();
#endif
  CSSVariableDeclarations& operator=(const CSSVariableDeclarations& aOther);

  /**
   * Returns whether this set of variable declarations includes a variable
   * with a given name.
   *
   * @param aName The variable name (not including any "--" prefix that would
   *   be part of the custom property name).
   */
  bool Has(const nsAString& aName) const;

  /**
   * Represents the type of a variable value.
   */
  enum Type {
    eTokenStream,  // a stream of CSS tokens (the usual type for variables)
    eInitial,      // 'initial'
    eInherit,      // 'inherit'
    eUnset         // 'unset'
  };

  /**
   * Gets the value of a variable in this set of variable declarations.
   *
   * @param aName The variable name (not including any "--" prefix that would
   *   be part of the custom property name).
   * @param aType Out parameter into which the type of the variable value will
   *   be stored.
   * @param aValue Out parameter into which the value of the variable will
   *   be stored.  If the variable is 'initial', 'inherit' or 'unset', this will
   *   be the empty string.
   * @return Whether a variable with the given name was found.  When false
   *   is returned, aType and aValue will not be modified.
   */
  bool Get(const nsAString& aName, Type& aType, nsString& aValue) const;

  /**
   * Adds or modifies an existing entry in this set of variable declarations
   * to have the value 'initial'.
   *
   * @param aName The variable name (not including any "--" prefix that would
   *   be part of the custom property name) whose value is to be set.
   */
  void PutInitial(const nsAString& aName);

  /**
   * Adds or modifies an existing entry in this set of variable declarations
   * to have the value 'inherit'.
   *
   * @param aName The variable name (not including any "--" prefix that would
   *   be part of the custom property name) whose value is to be set.
   */
  void PutInherit(const nsAString& aName);

  /**
   * Adds or modifies an existing entry in this set of variable declarations
   * to have the value 'unset'.
   *
   * @param aName The variable name (not including any "--" prefix that would
   *   be part of the custom property name) whose value is to be set.
   */
  void PutUnset(const nsAString& aName);

  /**
   * Adds or modifies an existing entry in this set of variable declarations
   * to have a token stream value.
   *
   * @param aName The variable name (not including any "--" prefix that would
   *   be part of the custom property name) whose value is to be set.
   * @param aTokenStream The CSS token stream.
   */
  void PutTokenStream(const nsAString& aName, const nsString& aTokenStream);

  /**
   * Removes an entry in this set of variable declarations.
   *
   * @param aName The variable name (not including any "--" prefix that would
   *   be part of the custom property name) whose entry is to be removed.
   */
  void Remove(const nsAString& aName);

  /**
   * Returns the number of entries in this set of variable declarations.
   */
  uint32_t Count() const { return mVariables.Count(); }

  /**
   * Copies each variable value from this object into aRuleData, unless that
   * variable already exists on aRuleData.
   */
  void MapRuleInfoInto(nsRuleData* aRuleData);

  /**
   * Copies the variables from this object into aResolver, marking them as
   * specified values.
   */
  void AddVariablesToResolver(CSSVariableResolver* aResolver) const;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  /**
   * Adds all the variable declarations from aOther into this object.
   */
  void CopyVariablesFrom(const CSSVariableDeclarations& aOther);
  static PLDHashOperator EnumerateVariableForCopy(const nsAString& aName,
                                                  nsString aValue,
                                                  void* aData);
  static PLDHashOperator
    EnumerateVariableForMapRuleInfoInto(const nsAString& aName,
                                        nsString aValue,
                                        void* aData);
  static PLDHashOperator
    EnumerateVariableForAddVariablesToResolver(const nsAString& aName,
                                               nsString aValue,
                                               void* aData);

  nsDataHashtable<nsStringHashKey, nsString> mVariables;
};

} // namespace mozilla

#endif
