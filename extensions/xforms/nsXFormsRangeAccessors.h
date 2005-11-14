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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
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

#include "nsIXFormsRangeAccessors.h"
#include "nsXFormsAccessors.h"

/**
 * Implementation for the accessors for a range element,
 * nsIXFormsRangeAccessors.
 *
 * @todo Support out-of/in-range events (XXX)
 */
class nsXFormsRangeAccessors : public nsIXFormsRangeAccessors,
                               public nsXFormsAccessors
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSRANGEACCESSORS
  NS_FORWARD_NSIXFORMSACCESSORS(nsXFormsAccessors::)

  // Constructor
  nsXFormsRangeAccessors(nsIDelegateInternal* aDelegate,
                         nsIDOMElement* aElement)
    : nsXFormsAccessors(aDelegate, aElement)
  {
  }

  // nsIClassInfo overrides
  NS_IMETHOD GetInterfaces(PRUint32 *aCount, nsIID * **aArray);

protected:
  /**
   * Gets the value of an attribute on the element (mElement).
   *
   * @param aAttr        The attribute
   * @param aVal         The returned value ("DOMNull"s it if it's not there or empty)
   */
  nsresult AttributeGetter(const nsAString &aAttr, nsAString &aVal);
};
