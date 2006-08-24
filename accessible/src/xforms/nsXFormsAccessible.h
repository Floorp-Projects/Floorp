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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef _nsXFormsAccessible_H_
#define _nsXFormsAccessible_H_

#include "nsAccessibleWrap.h"
#include "nsIXFormsUtilityService.h"

#define NS_NAMESPACE_XFORMS "http://www.w3.org/2002/xforms"

/**
 * The class is base for accessible objects of all XForms elements.
 */
class nsXFormsAccessible : public nsAccessibleWrap
{
public:
  nsXFormsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);

  // Returns value of instance node that xforms element is bound to.
  NS_IMETHOD GetValue(nsAString& aValue);

  // Returns state of xforms element taking into account state of instance node
  // that it is bound to.
  NS_IMETHOD GetState(PRUint32 *aState);

  // Returns value of child xforms 'label' element.
  NS_IMETHOD GetName(nsAString& aName);

  // Returns value of child xforms 'hint' element.
  NS_IMETHOD GetDescription(nsAString& aDescription);

  // Denies accessible nodes in anonymous content of xforms element by
  // always returning PR_FALSE value.
  NS_IMETHOD GetAllowsAnonChildAccessibles(PRBool *aAllowsAnonChildren);

protected:
  // Returns value of first child xforms element by tagname that is bound to
  // instance node.
  nsresult GetBoundChildElementValue(const nsAString& aTagName,
                                     nsAString& aValue);

private:
  // Service allows to get some xforms functionality.
  static nsIXFormsUtilityService *sXFormsService;
};

#endif

