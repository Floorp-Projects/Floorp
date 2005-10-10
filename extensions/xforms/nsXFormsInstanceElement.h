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

#ifndef nsXFormsInstanceElement_h_
#define nsXFormsInstanceElement_h_

#include "nsXFormsStubElement.h"
#include "nsIDOMDocument.h"
#include "nsCOMPtr.h"
#include "nsIModelElementPrivate.h"
#include "nsIInstanceElementPrivate.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"

class nsIDOMElement;

/**
 * Implementation of the XForms \<instance\> element.
 * It creates an instance document by either cloning the inline instance data
 * or loading an external xml document given by the src attribute.
 */

class nsXFormsInstanceElement : public nsXFormsStubElement,
                                public nsIInstanceElementPrivate,
                                public nsIStreamListener,
                                public nsIChannelEventSink,
                                public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIINSTANCEELEMENTPRIVATE
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsIXTFGenericElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aNewValue);
  NS_IMETHOD AttributeRemoved(nsIAtom *aName);
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();
  NS_IMETHOD OnCreated(nsIXTFGenericElementWrapper *aWrapper);
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);

  nsXFormsInstanceElement() NS_HIDDEN;

private:
  NS_HIDDEN_(nsresult) CloneInlineInstance();
  NS_HIDDEN_(void) LoadExternalInstance(const nsAString &aSrc);
  NS_HIDDEN_(nsresult) CreateInstanceDocument();
  NS_HIDDEN_(already_AddRefed<nsIModelElementPrivate>) GetModel();

  nsCOMPtr<nsIDOMDocument>    mDocument;
  nsCOMPtr<nsIDOMDocument>    mOriginalDocument;
  nsIDOMElement              *mElement;
  nsCOMPtr<nsIStreamListener> mListener;
  PRBool                      mAddingChildren;
  PRBool                      mLazy;
  nsCOMPtr<nsIChannel>        mChannel;
};

NS_HIDDEN_(nsresult)
NS_NewXFormsInstanceElement(nsIXTFElement **aResult);

#endif
