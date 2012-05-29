/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsHTMLWin32ObjectAccessible_H_
#define _nsHTMLWin32ObjectAccessible_H_

#include "nsBaseWidgetAccessible.h"

struct IAccessible;

class nsHTMLWin32ObjectOwnerAccessible : public AccessibleWrap
{
public:
  // This will own the nsHTMLWin32ObjectAccessible. We create this where the
  // <object> or <embed> exists in the tree, so that get_accNextSibling() etc.
  // will still point to Gecko accessible sibling content. This is necessary
  // because the native plugin accessible doesn't know where it exists in the
  // Mozilla tree, and returns null for previous and next sibling. This would
  // have the effect of cutting off all content after the plugin.
  nsHTMLWin32ObjectOwnerAccessible(nsIContent* aContent,
                                   DocAccessible* aDoc, void* aHwnd);
  virtual ~nsHTMLWin32ObjectOwnerAccessible() {}

  // nsAccessNode
  virtual void Shutdown();

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

protected:

  // Accessible
  virtual void CacheChildren();

  void* mHwnd;
  nsRefPtr<Accessible> mNativeAccessible;
};

/**
  * This class is used only internally, we never! send out an IAccessible linked
  *   back to this object. This class is used to represent a plugin object when
  *   referenced as a child or sibling of another Accessible node. We need only
  *   a limited portion of the nsIAccessible interface implemented here. The
  *   in depth accessible information will be returned by the actual IAccessible
  *   object returned by us in Accessible::NewAccessible() that gets the IAccessible
  *   from the windows system from the window handle.
  */
class nsHTMLWin32ObjectAccessible : public nsLeafAccessible
{
public:

  nsHTMLWin32ObjectAccessible(void *aHwnd);
  virtual ~nsHTMLWin32ObjectAccessible() {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetNativeInterface(void** aNativeAccessible) MOZ_OVERRIDE;

protected:
  void* mHwnd;
};

#endif  
