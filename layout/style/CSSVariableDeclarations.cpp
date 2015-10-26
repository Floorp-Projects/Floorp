/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* CSS Custom Property assignments for a Declaration at a given priority */

#include "CSSVariableDeclarations.h"

#include "CSSVariableResolver.h"
#include "nsCSSScanner.h"
#include "nsRuleData.h"

// These three special string values are used to represent specified values of
// 'initial', 'inherit' and 'unset'.  (Note that none of these are valid
// variable values.)
#define INITIAL_VALUE "!"
#define INHERIT_VALUE ";"
#define UNSET_VALUE   ")"

namespace mozilla {

CSSVariableDeclarations::CSSVariableDeclarations()
{
  MOZ_COUNT_CTOR(CSSVariableDeclarations);
}

CSSVariableDeclarations::CSSVariableDeclarations(const CSSVariableDeclarations& aOther)
{
  MOZ_COUNT_CTOR(CSSVariableDeclarations);
  CopyVariablesFrom(aOther);
}

#ifdef DEBUG
CSSVariableDeclarations::~CSSVariableDeclarations()
{
  MOZ_COUNT_DTOR(CSSVariableDeclarations);
}
#endif

CSSVariableDeclarations&
CSSVariableDeclarations::operator=(const CSSVariableDeclarations& aOther)
{
  if (this == &aOther) {
    return *this;
  }

  mVariables.Clear();
  CopyVariablesFrom(aOther);
  return *this;
}

void
CSSVariableDeclarations::CopyVariablesFrom(const CSSVariableDeclarations& aOther)
{
  for (auto iter = aOther.mVariables.ConstIter(); !iter.Done(); iter.Next()) {
    mVariables.Put(iter.Key(), iter.UserData());
  }
}

bool
CSSVariableDeclarations::Has(const nsAString& aName) const
{
  nsString value;
  return mVariables.Get(aName, &value);
}

bool
CSSVariableDeclarations::Get(const nsAString& aName,
                             Type& aType,
                             nsString& aTokenStream) const
{
  nsString value;
  if (!mVariables.Get(aName, &value)) {
    return false;
  }
  if (value.EqualsLiteral(INITIAL_VALUE)) {
    aType = eInitial;
    aTokenStream.Truncate();
  } else if (value.EqualsLiteral(INHERIT_VALUE)) {
    aType = eInitial;
    aTokenStream.Truncate();
  } else if (value.EqualsLiteral(UNSET_VALUE)) {
    aType = eUnset;
    aTokenStream.Truncate();
  } else {
    aType = eTokenStream;
    aTokenStream = value;
  }
  return true;
}

void
CSSVariableDeclarations::PutTokenStream(const nsAString& aName,
                                        const nsString& aTokenStream)
{
  MOZ_ASSERT(!aTokenStream.EqualsLiteral(INITIAL_VALUE) &&
             !aTokenStream.EqualsLiteral(INHERIT_VALUE) &&
             !aTokenStream.EqualsLiteral(UNSET_VALUE));
  mVariables.Put(aName, aTokenStream);
}

void
CSSVariableDeclarations::PutInitial(const nsAString& aName)
{
  mVariables.Put(aName, NS_LITERAL_STRING(INITIAL_VALUE));
}

void
CSSVariableDeclarations::PutInherit(const nsAString& aName)
{
  mVariables.Put(aName, NS_LITERAL_STRING(INHERIT_VALUE));
}

void
CSSVariableDeclarations::PutUnset(const nsAString& aName)
{
  mVariables.Put(aName, NS_LITERAL_STRING(UNSET_VALUE));
}

void
CSSVariableDeclarations::Remove(const nsAString& aName)
{
  mVariables.Remove(aName);
}

void
CSSVariableDeclarations::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (!(aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Variables))) {
    return;
  }

  if (!aRuleData->mVariables) {
    aRuleData->mVariables = new CSSVariableDeclarations(*this);
  } else {
    for (auto iter = mVariables.Iter(); !iter.Done(); iter.Next()) {
      nsDataHashtable<nsStringHashKey, nsString>& variables =
        aRuleData->mVariables->mVariables;
      const nsAString& aName = iter.Key();
      if (!variables.Contains(aName)) {
        variables.Put(aName, iter.UserData());
      }
    }
  }
}

void
CSSVariableDeclarations::AddVariablesToResolver(
                                           CSSVariableResolver* aResolver) const
{
  for (auto iter = mVariables.ConstIter(); !iter.Done(); iter.Next()) {
    const nsAString& name = iter.Key();
    nsString value = iter.UserData();
    if (value.EqualsLiteral(INITIAL_VALUE)) {
      // Values of 'initial' are treated the same as an invalid value in the
      // variable resolver.
      aResolver->Put(name, EmptyString(),
                     eCSSTokenSerialization_Nothing,
                     eCSSTokenSerialization_Nothing,
                     false);
    } else if (value.EqualsLiteral(INHERIT_VALUE) ||
               value.EqualsLiteral(UNSET_VALUE)) {
      // Values of 'inherit' and 'unset' don't need any handling, since it means
      // we just need to keep whatever value is currently in the resolver.  This
      // is because the specified variable declarations already have only the
      // winning declaration for the variable and no longer have any of the
      // others.
    } else {
      // At this point, we don't know what token types are at the start and end
      // of the specified variable value.  These will be determined later during
      // the resolving process.
      aResolver->Put(name, value,
                     eCSSTokenSerialization_Nothing,
                     eCSSTokenSerialization_Nothing,
                     false);
    }
  }
}

size_t
CSSVariableDeclarations::SizeOfIncludingThis(
                                      mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mVariables.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mVariables.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += iter.Data().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  return n;
}

} // namespace mozilla
