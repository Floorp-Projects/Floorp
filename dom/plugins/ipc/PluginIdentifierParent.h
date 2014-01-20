/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginIdentifierParent_h
#define dom_plugins_PluginIdentifierParent_h

#include "mozilla/plugins/PPluginIdentifierParent.h"

#include "npapi.h"
#include "npruntime.h"

namespace mozilla {
namespace plugins {

class PluginInstanceParent;

class PluginIdentifierParent : public PPluginIdentifierParent
{
  friend class PluginModuleParent;

public:
  NPIdentifier ToNPIdentifier()
  {
    return mIdentifier;
  }

  bool IsTemporary() {
    return !!mTemporaryRefs;
  }

  /**
   * Holds a perhaps-temporary identifier for the current stack frame.
   */
  class MOZ_STACK_CLASS StackIdentifier
  {
  public:
    StackIdentifier(PluginInstanceParent* inst, NPIdentifier aIdentifier);
    StackIdentifier(NPObject* aObject, NPIdentifier aIdentifier);
    ~StackIdentifier();

    operator PluginIdentifierParent*() {
      return mIdentifier;
    }

  private:
    DISALLOW_COPY_AND_ASSIGN(StackIdentifier);

    PluginIdentifierParent* mIdentifier;
  };

protected:
  PluginIdentifierParent(NPIdentifier aIdentifier, bool aTemporary)
    : mIdentifier(aIdentifier)
    , mTemporaryRefs(aTemporary ? 1 : 0)
  {
    MOZ_COUNT_CTOR(PluginIdentifierParent);
  }

  virtual ~PluginIdentifierParent()
  {
    MOZ_COUNT_DTOR(PluginIdentifierParent);
  }

  virtual bool RecvRetain() MOZ_OVERRIDE;

  void AddTemporaryRef() {
    mTemporaryRefs++;
  }

  /**
   * @returns true if the last temporary reference was removed.
   */
  bool RemoveTemporaryRef() {
    --mTemporaryRefs;
    return !mTemporaryRefs;
  }

private:
  NPIdentifier mIdentifier;
  unsigned int mTemporaryRefs;
};

} // namespace plugins
} // namespace mozilla

#endif // dom_plugins_PluginIdentifierParent_h
