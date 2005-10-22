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
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
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

#ifndef __NSXFORMSDELEGATESTUB_H__
#define __NSXFORMSDELEGATESTUB_H__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsIXFormsDelegate.h"
#include "nsXFormsControlStub.h"
#include "nsIXFormsUIWidget.h"

class nsIAtom;

/**
 * nsRepeatState is used to indicate whether the element
 * is inside \<repeat\>'s template. If it is, there is no need
 * to refresh the widget bound to the element.
 */
enum nsRepeatState {
  eType_Unknown,
  eType_Template,
  eType_GeneratedContent,
  eType_NotApplicable
};

/**
 * Stub implementation of the nsIXFormsDelegate interface.
 */
class nsXFormsDelegateStub : public nsXFormsBindableControlStub,
                             public nsIXFormsDelegate
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSDELEGATE

  NS_IMETHOD OnCreated(nsIXTFBindableElementWrapper *aWrapper);
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD WillChangeParent(nsIDOMElement *aNewParent);
  NS_IMETHOD WillChangeDocument(nsIDOMDocument *aNewDocument);

  // nsIXFormsControl
  NS_IMETHOD TryFocus(PRBool* aOK);
  NS_IMETHOD Refresh();

#ifdef DEBUG_smaug
  virtual const char* Name()
  {
    if (mControlType.IsEmpty()) {
      return "[undefined delegate]";
    }

    return NS_ConvertUTF16toUTF8(mControlType).get();
  }
#endif


  nsXFormsDelegateStub(const nsAString& aType = EmptyString())
    : mControlType(aType), mRepeatState(eType_Unknown) {}

protected:
  // Checks the status of the model item properties.
  nsresult GetState(PRInt32 aState, PRBool *aStateVal);
  
  // This is called when XBL widget is attached to the XForms control.
  // It checks the ancestors of the element and returns an nsRepeatState
  // depending on the elements place in the document.
  nsRepeatState UpdateRepeatState();

  // Sets/removes the moz:type attribute. The attribute can be used to detect the
  // type of the node, which is bound the the control.
  void SetMozTypeAttribute();

  nsString      mControlType;
  nsRepeatState mRepeatState;
};

#endif
