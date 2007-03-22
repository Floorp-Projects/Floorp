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

#include "nsCOMPtr.h"
#include "nsRect.h"

#include "nsTArray.h"
#include "nsAutoPtr.h"

#include "nsAccessible.h"

struct AccessibleWrapper;
struct objc_class;

class nsAccessibleWrap : public nsAccessible
{
  public: // construction, destruction
    nsAccessibleWrap(nsIDOMNode*, nsIWeakReference *aShell);
    virtual ~nsAccessibleWrap();
    
    // creates the native accessible connected to this one.
    NS_IMETHOD Init ();
    
    // get the native obj-c object (mozAccessible)
    NS_IMETHOD GetNativeInterface (void **aOutAccessible);
    
    // the objective-c |Class| type that this accessible's native object
    // should be instantied with.   used on runtime to determine the
    // right type for this accessible's associated native object.
    virtual objc_class* GetNativeType ();
    
    // returns a pointer to the native window for this accessible tree.
    void GetNativeWindow (void **aOutNativeWindow);
    
    virtual nsresult Shutdown ();
    virtual nsresult InvalidateChildren ();
    
    // we'll flatten buttons and checkboxes. usually they have a text node
    // child, that is their title. Works in conjunction with IsPruned() below.
    PRBool IsFlat() {
      PRUint32 role = Role(this);
      return (role == nsIAccessibleRole::ROLE_CHECKBUTTON ||
              role == nsIAccessibleRole::ROLE_PUSHBUTTON ||
              role == nsIAccessibleRole::ROLE_TOGGLE_BUTTON ||
              role == nsIAccessibleRole::ROLE_SPLITBUTTON ||
              role == nsIAccessibleRole::ROLE_ENTRY);
    }
    
    // ignored means that the accessible might still have children, but is not displayed
    // to the user. it also has no native accessible object represented for it.
    PRBool IsIgnored();
    
    PRInt32 GetUnignoredChildCount(PRBool aDeepCount);
    
    PRBool HasPopup () {
      PRUint32 state = 0;
      GetState(&state);
      return (state & nsIAccessibleStates::STATE_HASPOPUP);
    }
    
    // return this accessible's all children, adhering to "flat" accessibles by not returning their children.
    void GetUnignoredChildren(nsTArray<nsRefPtr<nsAccessibleWrap> > &aChildrenArray);
    virtual already_AddRefed<nsIAccessible> GetUnignoredParent();
    
  protected:
    
    PRBool AncestorIsFlat() {
      // we don't create a native object if we're child of a "flat" accessible; for example, on OS X buttons 
      // shouldn't have any children, because that makes the OS confused. 
      //
      // to maintain a scripting environment where the XPCOM accessible hierarchy look the same 
      // on all platforms, we still let the C++ objects be created though.
      
      nsCOMPtr<nsIAccessible> curParent = GetParent();
      while (curParent) {
        nsAccessibleWrap *ancestorWrap = NS_STATIC_CAST(nsAccessibleWrap*, (nsIAccessible*)curParent.get());
        if (ancestorWrap->IsFlat())
          return PR_TRUE;
        curParent = NS_STATIC_CAST(nsAccessibleWrap*, (nsIAccessible*)curParent.get())->GetParent();
      }
      // no parent was flat
      return PR_FALSE;
    }

    // Wrapper around our native object.
    AccessibleWrapper *mNativeWrapper;
};

// Define unsupported wrap classes here
typedef class nsHTMLTableCellAccessible    nsHTMLTableCellAccessibleWrap;
typedef class nsHTMLTableAccessible        nsHTMLTableAccessibleWrap;

#endif
