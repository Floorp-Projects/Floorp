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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#ifndef nsXFormsModelElement_h_
#define nsXFormsModelElement_h_

#include "nsIXTFGenericElement.h"
#include "nsIXTFPrivate.h"
#include "nsIXFormsModelElement.h"
#include "nsISchema.h"
#include "nsCOMArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMDocument.h"
#include "nsXFormsMDG.h"
#include "nsXFormsElement.h"

#include "nsISchemaLoader.h"
#include "nsISchema.h"

class nsIDOMElement;
class nsIDOMNode;
class nsIDOMXPathEvaluator;
class nsXFormsControl;
class nsXFormsInstanceElement;

enum nsXFormsModelEvent {
  eEvent_ModelConstruct,
  eEvent_ModelConstructDone,
  eEvent_Ready,
  eEvent_ModelDestruct,
  eEvent_Rebuild,
  eEvent_Refresh,
  eEvent_Revalidate,
  eEvent_Recalculate,
  eEvent_Reset,
  eEvent_BindingException,
  eEvent_LinkException,
  eEvent_LinkError,
  eEvent_ComputeException
};

class nsXFormsModelElement : public nsXFormsElement,
                             public nsIXTFGenericElement,
                             public nsIXTFPrivate,
                             public nsIXFormsModelElement,
                             public nsISchemaLoadListener,
                             public nsIDOMLoadListener
{
public:
  nsXFormsModelElement() NS_HIDDEN;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIXTFELEMENT
  NS_DECL_NSIXTFGENERICELEMENT
  NS_DECL_NSIXTFPRIVATE
  NS_DECL_NSIXFORMSMODELELEMENT
  NS_DECL_NSISCHEMALOADLISTENER
  NS_DECL_NSIWEBSERVICEERRORHANDLER
  NS_DECL_NSIDOMEVENTLISTENER

  // nsIDOMLoadListener
  NS_IMETHOD Load(nsIDOMEvent* aEvent);
  NS_IMETHOD BeforeUnload(nsIDOMEvent* aEvent);
  NS_IMETHOD Unload(nsIDOMEvent* aEvent);
  NS_IMETHOD Abort(nsIDOMEvent* aEvent);
  NS_IMETHOD Error(nsIDOMEvent* aEvent);

  NS_HIDDEN_(nsresult) DispatchEvent(nsXFormsModelEvent aEvent);

  NS_HIDDEN_(already_AddRefed<nsISchemaType>)
    GetTypeForControl(nsXFormsControl *aControl);

  NS_HIDDEN_(void) AddFormControl(nsXFormsControl *aControl);
  NS_HIDDEN_(void) RemoveFormControl(nsXFormsControl *aControl);

  NS_HIDDEN_(void) AddPendingInstance() { ++mPendingInstanceCount; }
  NS_HIDDEN_(void) RemovePendingInstance();

  NS_HIDDEN_(nsIDOMDocument*) FindInstanceDocument(const nsAString &aID);

  // Called after nsXFormsAtoms is registered
  static NS_HIDDEN_(void) Startup();

private:
  NS_HIDDEN_(nsresult) FinishConstruction();
  NS_HIDDEN_(PRBool)   ProcessBind(nsIDOMXPathEvaluator *aEvaluator,
                                   nsIDOMNode           *aContextNode,
                                   nsIDOMElement        *aBindElement);

  NS_HIDDEN_(void)     RemoveModelFromDocument();

  PRBool IsComplete() const { return (mSchemas.Count() == mSchemaCount
                                      && mPendingInstanceCount == 0);  }

  nsIDOMElement           *mElement;
  nsCOMArray<nsISchema>    mSchemas;
  nsVoidArray              mFormControls;

  PRInt32 mSchemaCount;
  PRInt32 mPendingInstanceCount;
  
  nsXFormsMDG mMDG;
};

NS_HIDDEN_(nsresult) NS_NewXFormsModelElement(nsIXTFElement **aResult);

#endif
