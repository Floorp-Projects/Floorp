/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginIdentifierChild.h"
#include "PluginModuleChild.h"

namespace mozilla {
namespace plugins {

void
PluginIdentifierChild::MakePermanent()
{
  if (mCanonicalIdentifier) {
    NS_ASSERTION(mCanonicalIdentifier->mHashed && mCanonicalIdentifier->mTemporaryRefs == 0,
                 "Canonical identifiers should always be permanent.");
    return; // nothing to do
  }

  if (!mHashed) {
    NS_ASSERTION(mTemporaryRefs == 0, "Not hashed, but temporary refs?");

    PluginIdentifierChild* c = GetCanonical();
    if (c) {
      NS_ASSERTION(c != this, "How did I get in the hash?");
      mCanonicalIdentifier = c;
      NS_ASSERTION(mCanonicalIdentifier->mHashed && mCanonicalIdentifier->mTemporaryRefs == 0,
                   "Canonical identifiers should always be permanent.");
      return;
    }

    Hash();
    mHashed = true;
    return;
  }

  if (mTemporaryRefs) {
    SendRetain();
    mTemporaryRefs = 0;
  }
}

void
PluginIdentifierChild::StartTemporary()
{
  if (mCanonicalIdentifier) {
    NS_ASSERTION(mCanonicalIdentifier->mHashed && mCanonicalIdentifier->mTemporaryRefs == 0,
                 "Canonical identifiers should always be permanent.");
    return; // nothing to do
  }

  if (!mHashed) {
    NS_ASSERTION(mTemporaryRefs == 0, "Not hashed, but temporary refs?");

    PluginIdentifierChild* c = GetCanonical();
    if (c) {
      NS_ASSERTION(c != this, "How did I get in the hash?");
      mCanonicalIdentifier = c;
      NS_ASSERTION(mCanonicalIdentifier->mHashed && mCanonicalIdentifier->mTemporaryRefs == 0,
                   "Canonical identifiers should always be permanent.");
      return;
    }

    Hash();
    mHashed = true;
    mTemporaryRefs = 1;
    return;
  }

  if (mTemporaryRefs)
    ++mTemporaryRefs;
}

void
PluginIdentifierChild::FinishTemporary()
{
  if (mCanonicalIdentifier)
    return;

  NS_ASSERTION(mHashed, "Finishing unhashed identifier?");
  if (!mTemporaryRefs)
    return;

  --mTemporaryRefs;
  if (mTemporaryRefs)
    return;

  Unhash();
  mHashed = false;
}

PluginIdentifierChild*
PluginIdentifierChildString::GetCanonical()
{
  PluginModuleChild* module = static_cast<PluginModuleChild*>(Manager());
  return module->mStringIdentifiers.Get(mString);
}

void
PluginIdentifierChildString::Hash()
{
  PluginModuleChild* module = static_cast<PluginModuleChild*>(Manager());
  NS_ASSERTION(module->mStringIdentifiers.Get(mString) == NULL, "Replacing Hash?");
  module->mStringIdentifiers.Put(mString, this);
}

void
PluginIdentifierChildString::Unhash()
{
  PluginModuleChild* module = static_cast<PluginModuleChild*>(Manager());
  NS_ASSERTION(module->mStringIdentifiers.Get(mString) == this, "Incorrect identifier hash?");
  module->mStringIdentifiers.Remove(mString);
}

PluginIdentifierChild*
PluginIdentifierChildInt::GetCanonical()
{
  PluginModuleChild* module = static_cast<PluginModuleChild*>(Manager());
  return module->mIntIdentifiers.Get(mInt);
}

void
PluginIdentifierChildInt::Hash()
{
  PluginModuleChild* module = static_cast<PluginModuleChild*>(Manager());
  NS_ASSERTION(module->mIntIdentifiers.Get(mInt) == NULL, "Replacing Hash?");
  module->mIntIdentifiers.Put(mInt, this);
}

void
PluginIdentifierChildInt::Unhash()
{
  PluginModuleChild* module = static_cast<PluginModuleChild*>(Manager());
  NS_ASSERTION(module->mIntIdentifiers.Get(mInt) == this, "Incorrect identifier hash?");
  module->mIntIdentifiers.Remove(mInt);
}

} // namespace mozilla::plugins
} // namespace mozilla
