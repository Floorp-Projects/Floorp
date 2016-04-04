/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLWin32ObjectAccessible_h_
#define mozilla_a11y_HTMLWin32ObjectAccessible_h_

#include "BaseAccessibles.h"

struct IAccessible;

namespace mozilla {
namespace a11y {

class HTMLWin32ObjectOwnerAccessible : public AccessibleWrap
{
public:
  // This will own the HTMLWin32ObjectAccessible. We create this where the
  // <object> or <embed> exists in the tree, so that get_accNextSibling() etc.
  // will still point to Gecko accessible sibling content. This is necessary
  // because the native plugin accessible doesn't know where it exists in the
  // Mozilla tree, and returns null for previous and next sibling. This would
  // have the effect of cutting off all content after the plugin.
  HTMLWin32ObjectOwnerAccessible(nsIContent* aContent,
                                 DocAccessible* aDoc, void* aHwnd);
  virtual ~HTMLWin32ObjectOwnerAccessible() {}

  // Accessible
  virtual void Shutdown();
  virtual mozilla::a11y::role NativeRole();
  virtual bool NativelyUnavailable() const;

protected:
  void* mHwnd;
  RefPtr<Accessible> mNativeAccessible;
};

/**
  * This class is used only internally, we never! send out an IAccessible linked
  *   back to this object. This class is used to represent a plugin object when
  *   referenced as a child or sibling of another Accessible node. We need only
  *   a limited portion of the Accessible interface implemented here. The
  *   in depth accessible information will be returned by the actual IAccessible
  *   object returned by us in Accessible::NewAccessible() that gets the IAccessible
  *   from the windows system from the window handle.
  */
class HTMLWin32ObjectAccessible : public DummyAccessible
{
public:
  HTMLWin32ObjectAccessible(void* aHwnd, DocAccessible* aDoc);
  virtual ~HTMLWin32ObjectAccessible() {}

  virtual void GetNativeInterface(void** aNativeAccessible) override;

protected:
  void* mHwnd;
};

} // namespace a11y
} // namespace mozilla

#endif
