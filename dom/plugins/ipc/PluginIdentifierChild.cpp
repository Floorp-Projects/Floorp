/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugins.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
