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

#ifndef dom_plugins_PluginIdentifierChild_h
#define dom_plugins_PluginIdentifierChild_h

#include "mozilla/plugins/PPluginIdentifierChild.h"
#include "npapi.h"
#include "npruntime.h"

#include "nsStringGlue.h"

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

  class NS_STACK_CLASS StackIdentifier
  {
  public:
    StackIdentifier(PPluginIdentifierChild* actor)
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
  PluginIdentifierChild(bool aIsString)
    : mCanonicalIdentifier(NULL)
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
  // * mCanonicalIdentifier is non-NULL: this is a duplicate identifier, no
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
  PluginIdentifierChildString(const nsCString& aString)
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
  PluginIdentifierChildInt(int32_t aInt)
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
