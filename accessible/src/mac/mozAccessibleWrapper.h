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
 
#include "nsAccessibleWrap.h"

#include "nsObjCExceptions.h"

#import "mozAccessible.h"

/* Wrapper class.  

   This is needed because C++-only headers such as nsAccessibleWrap.h must not depend
   on Objective-C and Cocoa. Classes in accessible/src/base depend on them, and other modules in turn
   depend on them, so in the end all of Mozilla would end up having to link against Cocoa and be renamed .mm :-)

   In order to have a mozAccessible object wrapped, the user passes itself (nsAccessible*) and the subclass of
   mozAccessible that should be instantiated.

   In the header file, the AccessibleWrapper is used as the member, and is forward-declared (because this header
   cannot be #included directly.
*/

struct AccessibleWrapper {
  mozAccessible *object;
  AccessibleWrapper (nsAccessibleWrap *parent, Class classType) {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    object = (mozAccessible*)[[classType alloc] initWithAccessible:parent];

    NS_OBJC_END_TRY_ABORT_BLOCK;
  }

  ~AccessibleWrapper () {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    // if some third-party still holds on to the object, it's important that it is marked
    // as expired, so it can't do any harm (e.g., walk into an expired hierarchy of nodes).
    [object expire];
    
    [object release];

    NS_OBJC_END_TRY_ABORT_BLOCK;
  }

  mozAccessible* getNativeObject () {
    return object;
  }
 
  bool isIgnored () {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

    return (bool)[object accessibilityIsIgnored];

    NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(PR_FALSE);
  }
};
