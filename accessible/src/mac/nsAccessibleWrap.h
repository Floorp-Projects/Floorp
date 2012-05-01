/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Håkan Waara <hwaara@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* For documentation of the accessibility architecture, 
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef _nsAccessibleWrap_H_
#define _nsAccessibleWrap_H_

#include <objc/objc.h>

#include "nsAccessible.h"
#include "nsAccUtils.h"
#include "States.h"

#include "nsCOMPtr.h"
#include "nsRect.h"

#include "nsTArray.h"
#include "nsAutoPtr.h"

#if defined(__OBJC__)
@class mozAccessible;
#endif

class nsAccessibleWrap : public nsAccessible
{
public: // construction, destruction
  nsAccessibleWrap(nsIContent* aContent, nsDocAccessible* aDoc);
  virtual ~nsAccessibleWrap();
    
  /**
   * Get the native Obj-C object (mozAccessible).
   */
  NS_IMETHOD GetNativeInterface (void** aOutAccessible);
  
  /**
   * The objective-c |Class| type that this accessible's native object
   * should be instantied with.   used on runtime to determine the
   * right type for this accessible's associated native object.
   */
  virtual Class GetNativeType ();

  virtual void Shutdown ();
  virtual void InvalidateChildren();

  virtual bool AppendChild(nsAccessible* aAccessible);
  virtual bool RemoveChild(nsAccessible* aAccessible);

  virtual nsresult HandleAccEvent(AccEvent* aEvent);

  /**
   * Ignored means that the accessible might still have children, but is not
   * displayed to the user. it also has no native accessible object represented
   * for it.
   */
  bool IsIgnored();
  
  inline bool HasPopup () 
    { return (NativeState() & mozilla::a11y::states::HASPOPUP); }
  
  /**
   * Returns this accessible's all children, adhering to "flat" accessibles by 
   * not returning their children.
   */
  void GetUnignoredChildren(nsTArray<nsAccessible*>* aChildrenArray);
  nsAccessible* GetUnignoredParent() const;

protected:

  virtual nsresult FirePlatformEvent(AccEvent* aEvent);

  /**
   * Return true if the parent doesn't have children to expose to AT.
   */
  bool AncestorIsFlat();

  /**
   * Get the native object. Create it if needed.
   */
#if defined(__OBJC__)
  mozAccessible* GetNativeObject();
#else
  id GetNativeObject();
#endif

private:

  /**
   * Our native object. Private because its creation is done lazily.
   * Don't access it directly. Ever. Unless you are GetNativeObject() or 
   * Shutdown()
   */
#if defined(__OBJC__)
  // if we are in Objective-C, we use the actual Obj-C class.
  mozAccessible* mNativeObject;
#else
  id mNativeObject;
#endif

  /**
   * We have created our native. This does not mean there is one.
   * This can never go back to false.
   * We need it because checking whether we need a native object cost time.
   */
  bool mNativeInited;  
};

// Define unsupported wrap classes here
typedef class nsHTMLTableCellAccessible    nsHTMLTableCellAccessibleWrap;
typedef class nsHTMLTableAccessible        nsHTMLTableAccessibleWrap;

#endif
