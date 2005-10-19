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

#ifndef nsXFormsModelElement_h_
#define nsXFormsModelElement_h_

#include "nsXFormsStubElement.h"
#include "nsIModelElementPrivate.h"
#include "nsIDOMEventListener.h"
#include "nsISchema.h"
#include "nsCOMArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsIDOMDocument.h"
#include "nsXFormsMDGEngine.h"
#include "nsXFormsUtils.h"

#include "nsISchemaLoader.h"
#include "nsISchema.h"
#include "nsIXFormsContextControl.h"

class nsIDOMElement;
class nsIDOMNode;
class nsIXFormsXPathEvaluator;
class nsIDOMXPathResult;
class nsXFormsControl;

/**
 * Implementation of the XForms \<model\> element.
 *
 * This includes all of the code for loading the model's external resources and
 * initializing the model and its controls.
 *
 * @see http://www.w3.org/TR/xforms/slice3.html#structure-model
 */
class nsXFormsModelElement : public nsXFormsStubElement,
                             public nsIModelElementPrivate,
                             public nsISchemaLoadListener,
                             public nsIDOMEventListener,
                             public nsIXFormsContextControl
{
public:
  nsXFormsModelElement() NS_HIDDEN;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSMODELELEMENT
  NS_DECL_NSIMODELELEMENTPRIVATE
  NS_DECL_NSISCHEMALOADLISTENER
  NS_DECL_NSIWEBSERVICEERRORHANDLER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIXFORMSCONTEXTCONTROL

  // nsIXTFGenericElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD GetScriptingInterfaces(PRUint32 *aCount, nsIID ***aArray);
  NS_IMETHOD WillChangeDocument(nsIDOMDocument *aNewDocument);
  NS_IMETHOD DocumentChanged(nsIDOMDocument *aNewDocument);
  NS_IMETHOD DoneAddingChildren();
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled);
  NS_IMETHOD OnCreated(nsIXTFGenericElementWrapper *aWrapper);

  // nsIXFormsControlBase overrides
  NS_IMETHOD Bind() {
    // dummy method, so does nothing
    return NS_OK;
  };

  // Called after nsXFormsAtoms is registered
  static NS_HIDDEN_(void) Startup();

  /**
   * The models are not ready for binding, so defer the binding of the control
   * by storing it as a property on the document.  The models will run through
   * this list when they are ready for binding.
   *
   * @param aDoc              Document that contains aElement
   * @param aControl          XForms control waiting to be bound
   */
  static NS_HIDDEN_(nsresult) DeferElementBind(nsIDOMDocument    *aDoc,
                                               nsIXFormsControlBase  *aControl);

  static nsresult NeedsPostRefresh(nsIXFormsControl* aControl);
  static void CancelPostRefresh(nsIXFormsControl* aControl);
private:

  NS_HIDDEN_(already_AddRefed<nsIDOMDocument>)
    FindInstanceDocument(const nsAString &aID);
  NS_HIDDEN_(void)     Reset();
  NS_HIDDEN_(void)     Ready();
  NS_HIDDEN_(void)     BackupOrRestoreInstanceData(PRBool restore);

  /** Initializes the MIPs on all form controls */
  NS_HIDDEN_(nsresult) InitializeControls();

  NS_HIDDEN_(nsresult) ProcessBindElements();
  NS_HIDDEN_(nsresult) FinishConstruction();
  NS_HIDDEN_(nsresult) ConstructDone();
  NS_HIDDEN_(void)     MaybeNotifyCompletion();

  NS_HIDDEN_(nsresult) ProcessBind(nsIXFormsXPathEvaluator   *aEvaluator,
                                   nsIDOMNode                *aContextNode,
                                   PRInt32                    aContextPosition,
                                   PRInt32                    aContextSize,
                                   nsIDOMElement             *aBindElement);

  NS_HIDDEN_(void)     RemoveModelFromDocument();

  /**
   * Set the states on the control |aControl| bound to the instance data node
   * |aNode|. It dispatches the necessary events and sets the pseudo class
   * states. |aAllStates| determines whether all states should be set, or only
   * changed.
   *
   * @param aControl          The event target
   * @param aNode             The instance node
   * @param aAllStates        Set all states (PR_TRUE), or only changed
   */
  NS_HIDDEN_(nsresult) SetStatesInternal(nsIXFormsControl *aControl,
                                         nsIDOMNode       *aNode,
                                         PRBool            aAllStates = PR_FALSE);

  /**
   * Sets the state of a specific state.
   *
   * @param aElement          The element to dispatch events to and set states on
   * @param aState            The state value
   * @param aOnEvent          The on event for the state.
   */
  NS_HIDDEN_(void)     SetSingleState(nsIDOMElement *aElement,
                                      PRBool         aState,
                                      nsXFormsEvent  aOnEvent);

  /**
   * Call the Bind() and Refresh() on controls which was deferred because
   * the model was not ready.
   *
   * @param aDoc              Document that contains the XForms control
   */
  static NS_HIDDEN_(void) ProcessDeferredBinds(nsIDOMDocument *aDoc);

  // Returns true when all external documents have been loaded
  PRBool IsComplete() const { return (mSchemaTotal == mSchemaCount
                                      && mPendingInstanceCount == 0);  }

  nsIDOMElement            *mElement;
  nsCOMPtr<nsISchemaLoader> mSchemas;
  nsStringArray             mPendingInlineSchemas;
  nsVoidArray               mFormControls;

  PRInt32 mSchemaCount;
  PRInt32 mSchemaTotal;
  PRInt32 mPendingInstanceCount;

  /** The MDG for this model */
  nsXFormsMDGEngine mMDG;

  /**
   * List of changed nodes, ie. nodes that have not been informed about
   * changes yet
   */
  nsCOMArray<nsIDOMNode>    mChangedNodes;

  // This flag indicates whether or not the document fired DOMContentLoaded
  PRBool mDocumentLoaded;

  // This flag indicates whether a xforms-rebuild has been called, but no
  // xforms-revalidate yet
  PRBool mNeedsRefresh;

  /**
   * List of instance elements contained by this model, including lazy-authored
   * instance elements.
   */
  nsCOMArray<nsIInstanceElementPrivate>    mInstanceList;

  /** Indicates whether the model's instance was built by lazy authoring */
  PRBool mLazyModel;
};

/**
 * nsPostRefresh is needed by the UI Controls, which are implemented in
 * XBL and used inside \<repeat\>. It is needed to refresh the controls,
 * because XBL bindings are attached to XForms controls *after* refreshing
 * the \<repeat\>.
 */

class nsPostRefresh {
public:
  nsPostRefresh();
  ~nsPostRefresh();
  static const nsVoidArray* PostRefreshList();
};

NS_HIDDEN_(nsresult) NS_NewXFormsModelElement(nsIXTFElement **aResult);

#endif
