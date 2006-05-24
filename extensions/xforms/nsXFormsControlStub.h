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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef nsXFormsControlStub_h_
#define nsXFormsControlStub_h_

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIXTFElement.h"
#include "nsXFormsAtoms.h"
#include "nsIDOMEventListener.h"

#include "nsIModelElementPrivate.h"
#include "nsIXFormsControl.h"
#include "nsIXFormsRepeatElement.h"
#include "nsXFormsStubElement.h"
#include "nsXFormsUtils.h"
#include "nsIXTFBindableElementWrapper.h"
#include "nsIXFormsControlBase.h"

class nsIDOMEvent;
class nsIDOMXPathResult;

/**
 * Common stub for all XForms controls that inherit from nsIXFormsControl and
 * is bound to an instance node.
 */
class nsXFormsControlStubBase : public nsIXFormsControl
{
public:

  /** The standard notification flags set on nsIXTFElement */
  const PRUint32 kStandardNotificationMask;

  /**
   * The element flags for the controls passed to
   * nsXFormsUtils:EvaluateNodeBinding()
   */
  const PRUint32 kElementFlags;

  // nsIXFormsControl
  NS_IMETHOD GetBoundNode(nsIDOMNode **aBoundNode);
  NS_IMETHOD BindToModel(PRBool aSetBoundNode = PR_FALSE);
  NS_IMETHOD GetDependencies(nsCOMArray<nsIDOMNode> **aDependencies);
  NS_IMETHOD GetElement(nsIDOMElement **aElement);
  NS_IMETHOD ResetBoundNode(const nsString &aBindAttribute,
                            PRUint16        aResultType,
                            PRBool         *aContextChanged);
  NS_IMETHOD Bind(PRBool *aContextChanged);
  NS_IMETHOD Refresh();
  NS_IMETHOD GetOnDeferredBindList(PRBool *onList);
  NS_IMETHOD SetOnDeferredBindList(PRBool putOnList);
  NS_IMETHOD TryFocus(PRBool* aOK);
  NS_IMETHOD IsEventTarget(PRBool *aOK);
  NS_IMETHOD GetUsesModelBinding(PRBool *aRes);
  NS_IMETHOD GetDefaultIntrinsicState(PRInt32 *aRes);
  NS_IMETHOD GetDisabledIntrinsicState(PRInt32 *aRes);

  nsresult Create(nsIXTFElementWrapper *aWrapper);
  // for nsIXTFElement
  nsresult HandleDefault(nsIDOMEvent *aEvent,
                         PRBool      *aHandled);
  nsresult OnDestroyed();
  nsresult DocumentChanged(nsIDOMDocument *aNewDocument);
  nsresult ParentChanged(nsIDOMElement *aNewParent);
  nsresult WillSetAttribute(nsIAtom *aName, const nsAString &aValue);
  nsresult AttributeSet(nsIAtom *aName, const nsAString &aValue);
  nsresult WillRemoveAttribute(nsIAtom *aName);
  nsresult AttributeRemoved(nsIAtom *aName);

  /**
   * This function manages the mBindAttrsCount value.  mBindAttrsCount will
   * be incremented if a single node binding attribute is specified that isn't
   * currently on the control AND aValue isn't empty.  Otherwise, if the control
   * already contains this attribute with a non-empty value and aValue is empty,
   * then mBindAttrsCount is decremented.
   *
   *   aName -  Atom of the single node binding attribute ('bind', 'ref', etc.).
   *            Using an atom to make it a tad harder to misuse by passing in
   *            any old string, for instance.  Since different controls may have
   *            different binding attrs, we do NO validation on the attr.  We
   *            assume that the caller is smart enough to only send us a
   *            binding attr atom.
   *   aValue - The string value that the SNB attribute is being set to.
   *
   */
  void AddRemoveSNBAttr(nsIAtom *aName, const nsAString &aValue);

  // nsIXFormsContextControl
  NS_DECL_NSIXFORMSCONTEXTCONTROL

#ifdef DEBUG_smaug
  virtual const char* Name() { return "[undefined]"; }
#endif
  

  /** Constructor */
  nsXFormsControlStubBase() :
    kStandardNotificationMask(nsIXTFElement::NOTIFY_WILL_SET_ATTRIBUTE |
                              nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                              nsIXTFElement::NOTIFY_WILL_REMOVE_ATTRIBUTE | 
                              nsIXTFElement::NOTIFY_ATTRIBUTE_REMOVED | 
                              nsIXTFElement::NOTIFY_DOCUMENT_CHANGED |
                              nsIXTFElement::NOTIFY_PARENT_CHANGED |
                              nsIXTFElement::NOTIFY_HANDLE_DEFAULT),
    kElementFlags(nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR),
    mHasParent(PR_FALSE),
    mPreventLoop(PR_FALSE),
    mUsesModelBinding(PR_FALSE),
    mAppearDisabled(PR_FALSE),
    mOnDeferredBindList(PR_FALSE),
    mBindAttrsCount(0)
    {};

protected:
  /** The nsIXTFXMLVisualWrapper */
  nsIDOMElement*                      mElement;

  /** The node that the control is bound to. */
  nsCOMPtr<nsIDOMNode>                mBoundNode;

  /** Array of nsIDOMNodes that the control depends on. */
  nsCOMArray<nsIDOMNode>              mDependencies;

  /** The model for the control */
  nsCOMPtr<nsIModelElementPrivate>    mModel;

  /** This event listener is used to create xforms-hint and xforms-help events. */
  nsCOMPtr<nsIDOMEventListener>       mEventListener;

  /** State that tells whether control has a parent or not */
  PRPackedBool                        mHasParent;

  /** State to prevent infinite loop when generating and handling xforms-next
   *  and xforms-previous events
   */
  PRPackedBool                        mPreventLoop;

  /**
   * Does the control use a model bind? That is, does it have a @bind.
   */
  PRPackedBool                        mUsesModelBinding;

  /**
   * Should the control appear disabled. This is f.x. used when a valid single
   * node binding is not pointing to an instance data node.
   */
  PRPackedBool                        mAppearDisabled;

  /**
   * Indicates whether this control is already on the deferred bind list
   */
  PRPackedBool                        mOnDeferredBindList;

  /** 
   * Used to keep track of whether this control has any single node binding
   * attributes.
   */
  PRInt8 mBindAttrsCount;

  /**
   * List of repeats that the node binding depends on.  This happens when using
   * the index() function in the binding expression.
   */
  nsCOMArray<nsIXFormsRepeatElement>  mIndexesUsed;

  /**
   * Does control have a binding attribute?
   */
  PRBool HasBindingAttribute() const { return mBindAttrsCount != 0; };

  /** Returns the relevant state of the control */
  PRBool GetRelevantState();

  /**
   * Processes the node binding of a control, get the current Model (mModel)
   * and binds the control to it.
   *
   * @note Will return NS_OK_XFORMS_DEFERRED if the binding is being
   * deferred.
   *
   * @param aBindingAttr      The default binding attribute
   * @param aResultType       The XPath result type requested
   * @param aResult           The XPath result
   * @param aModel            The model
   */
  nsresult
    ProcessNodeBinding(const nsString          &aBindingAttr,
                       PRUint16                 aResultType,
                       nsIDOMXPathResult      **aResult,
                       nsIModelElementPrivate **aModel = nsnull);
  
  /**
   *  Reset (and reinitialize) the event listener, which is used to create 
   *  xforms-hint and xforms-help events.
   */
  void ResetHelpAndHint(PRBool aInitialize);

  /**
   * Checks whether an attribute is a binding attribute for the control. This
   * should be overriden by controls that have "non-standard" binding
   * attributes.
   *
   * @param aAttr        The attribute to check.
   */
  virtual PRBool IsBindingAttribute(const nsIAtom *aAttr) const;
 
  /**
   * Causes Bind() and Refresh() to be called if aName is the atom of a
   * single node binding attribute for this control.  Called by AttributeSet
   * and AttributeRemoved.
   */
  void AfterSetAttribute(nsIAtom *aName);

  /**
   * Removes this control from its model's list of controls if a single node
   * binding attribute is removed.  Called by WillSetAttribute and
   * WillRemoveAttribute.
   * 
   * @param aName  - atom of the attribute being changed
   * @param aValue - value that the attribute is being changed to.
   */
  void BeforeSetAttribute(nsIAtom *aName, const nsAString &aValue);

  /** Removes the index change event listeners */
  void RemoveIndexListeners();

  /**
   * Forces detaching from the model.
   *
   * @param aRebind           Try rebinding to a new model?
   */
  nsresult ForceModelDetach(PRBool aRebind);

  /**
   * Adds the form control to the model, if the model has changed.
   *
   * @param aOldModel         The previous model the control was bound to
   * @param aParent           The parent XForms control
   */
  nsresult MaybeAddToModel(nsIModelElementPrivate *aOldModel,
                           nsIXFormsControl       *aParent);
};

/**
 * nsXFormsBindableControlStub inherits from nsXFormsBindableStub
 */
class nsXFormsBindableControlStub : public nsXFormsControlStubBase,
                                    public nsXFormsBindableStub
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  // nsIXTFBindableElement overrides
  /** This sets the notification mask and initializes mElement */
  NS_IMETHOD OnCreated(nsIXTFBindableElementWrapper *aWrapper)
  {
    nsresult rv = nsXFormsBindableStub::OnCreated(aWrapper);
    return NS_SUCCEEDED(rv) ? nsXFormsControlStubBase::Create(aWrapper) : rv;
  }

  // nsIXTFElement overrides
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent,
                           PRBool      *aHandled)
  {
    return nsXFormsControlStubBase::HandleDefault(aEvent, aHandled);
  }

  NS_IMETHOD OnDestroyed() {
    return nsXFormsControlStubBase::OnDestroyed();
  }

  NS_IMETHOD DocumentChanged(nsIDOMDocument *aNewDocument)
  {
    return nsXFormsControlStubBase::DocumentChanged(aNewDocument);
  }

  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent)
  {
    return nsXFormsControlStubBase::ParentChanged(aNewParent);
  }

  NS_IMETHOD WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
  {
    return nsXFormsControlStubBase::WillSetAttribute(aName, aValue);
  }

  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue)
  {
    return nsXFormsControlStubBase::AttributeSet(aName, aValue);
  }

  NS_IMETHOD WillRemoveAttribute(nsIAtom *aName)
  {
    return nsXFormsControlStubBase::WillRemoveAttribute(aName);
  }

  NS_IMETHOD AttributeRemoved(nsIAtom *aName)
  {
    return nsXFormsControlStubBase::AttributeRemoved(aName);
  }
  
  /** Constructor */
  nsXFormsBindableControlStub() : nsXFormsControlStubBase() {};
};
#endif
