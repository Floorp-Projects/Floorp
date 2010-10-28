/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Mounir Lamouri <mounir.lamouri@mozilla.com> (Original author)
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

#ifndef nsIMutationObserver2_h___
#define nsIMutationObserver2_h___

#include "nsIMutationObserver.h"

class nsIContent;
class nsINode;

#define NS_IMUTATION_OBSERVER_2_IID \
{0x61ac1cfd, 0xf3ef, 0x4408, \
  {0x8a, 0x72, 0xee, 0xf0, 0x41, 0xbe, 0xc7, 0xe9 } }

/**
 * Mutation observer interface 2 is adding AttributeChildRemoved to
 * nsIMutationObserver.
 *
 * @see nsIMutationObserver.
 */
class nsIMutationObserver2 : public nsIMutationObserver
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IMUTATION_OBSERVER_2_IID)

  /**
   * Notification that an attribute's child has been removed.
   *
   * @param aContainer The attribute that had its child removed.
   * @param aChild     The child that was removed.
   *
   * @note Attributes can't have more than one child so it will be always the
   *       first one being removed.
   */
  virtual void AttributeChildRemoved(nsINode* aAttribute,
                                     nsIContent* aChild) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIMutationObserver2, NS_IMUTATION_OBSERVER_2_IID)

#define NS_DECL_NSIMUTATIONOBSERVER2_ATTRIBUTECHILDREMOVED                \
    virtual void AttributeChildRemoved(nsINode* aAttribute,               \
                                       nsIContent* aChild);

#define NS_DECL_NSIMUTATIONOBSERVER2                                      \
    NS_DECL_NSIMUTATIONOBSERVER                                           \
    NS_DECL_NSIMUTATIONOBSERVER2_ATTRIBUTECHILDREMOVED

#define NS_IMPL_NSIMUTATIONOBSERVER2_CONTENT(_class)                      \
NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(_class)                               \
void                                                                      \
_class::AttributeChildRemoved(nsINode* aAttribute, nsIContent *aChild)    \
{                                                                         \
}


#endif /* nsIMutationObserver2_h___ */

