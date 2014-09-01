/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginIdentifierChild_h
#define dom_plugins_PluginIdentifierChild_h

#include "mozilla/plugins/PPluginIdentifierChild.h"
#include "npapi.h"
#include "npruntime.h"

#include "nsString.h"

namespace mozilla {
namespace plugins {

class PluginModuleChild;

/**
 * Plugin identifiers may be "temporary", see the comment on the
 * PPluginIdentifier constructor for details. This means that any IPDL method
 * which receives a PPluginIdentifierChild* parameter must use StackIdentifier
 * to track it.
 */
class PluginIdentifierChild : public PPluginIdentifierChild
{
  friend class PluginModuleChild;
public:
  bool IsString()
  {
    return mIsString;
  }

  NPIdentifier ToNPIdentifier()
  {
    if (mCanonicalIdentifier) {
      return mCanonicalIdentifier;
    }

    NS_ASSERTION(mHashed, "Handing out an unhashed identifier?");
    return this;
  }

  void MakePermanent();

  class MOZ_STACK_CLASS StackIdentifier
  {
  public:
    explicit StackIdentifier(PPluginIdentifierChild* actor)
      : mIdentifier(static_cast<PluginIdentifierChild*>(actor))
    {
      if (mIdentifier)
        mIdentifier->StartTemporary();
    }

    ~StackIdentifier() {
      if (mIdentifier)
        mIdentifier->FinishTemporary();
    }

    PluginIdentifierChild* operator->() { return mIdentifier; }

  private:
    PluginIdentifierChild* mIdentifier;
  };

protected:
  explicit PluginIdentifierChild(bool aIsString)
    : mCanonicalIdentifier(nullptr)
    , mHashed(false)
    , mTemporaryRefs(0)
    , mIsString(aIsString)
  {
    MOZ_COUNT_CTOR(PluginIdentifierChild);
  }

  virtual ~PluginIdentifierChild()
  {
    MOZ_COUNT_DTOR(PluginIdentifierChild);
  }

  // The following functions are implemented by the subclasses for their
  // identifier maps.
  virtual PluginIdentifierChild* GetCanonical() = 0;
  virtual void Hash() = 0;
  virtual void Unhash() = 0;

private:
  void StartTemporary();
  void FinishTemporary();

  // There's a possibility that we already have an actor that wraps the same
  // string or int because we do all this identifier construction
  // asynchronously. In this case we need to hand out the canonical version
  // created by the child side.
  //
  // In order to deal with temporary identifiers which appear on the stack,
  // identifiers use the following state invariants:
  //
  // * mCanonicalIdentifier is non-nullptr: this is a duplicate identifier, no
  //   further information is necessary.
  // * mHashed is false: this identifier is a newborn, non-permanent identifier
  // * mHashed is true, mTemporaryRefs is 0: this identifier is permanent
  // * mHashed is true, mTemporaryRefs is non-0: this identifier is temporary;
  //   if NPN_GetFooIdentifier is called for it, we need to retain it. If
  //   all stack references are lost, unhash it because it will soon be 
  //   deleted.

  PluginIdentifierChild* mCanonicalIdentifier;
  bool mHashed;
  unsigned int mTemporaryRefs;
  bool mIsString;
};

class PluginIdentifierChildString : public PluginIdentifierChild
{
  friend class PluginModuleChild;
public:
  NPUTF8* ToString()
  {
    return ToNewCString(mString);
  }

protected:
  explicit PluginIdentifierChildString(const nsCString& aString)
    : PluginIdentifierChild(true),
      mString(aString)
  { }

  virtual PluginIdentifierChild* GetCanonical();
  virtual void Hash();
  virtual void Unhash();

  nsCString mString;
};

class PluginIdentifierChildInt : public PluginIdentifierChild
{
  friend class PluginModuleChild;
public:
  int32_t ToInt()
  {
    return mInt;
  }

protected:
  explicit PluginIdentifierChildInt(int32_t aInt)
    : PluginIdentifierChild(false),
      mInt(aInt)
  { }

  virtual PluginIdentifierChild* GetCanonical();
  virtual void Hash();
  virtual void Unhash();

  int32_t mInt;
};

} // namespace plugins
} // namespace mozilla

#endif // dom_plugins_PluginIdentifierChild_h
