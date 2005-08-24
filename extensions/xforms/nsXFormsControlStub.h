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
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIXTFBindableElementWrapper.h"
#include "nsIXFormsControlBase.h"

class nsIDOMEvent;
class nsIDOMXPathResult;
class nsIXTFXMLVisualWrapper;

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
  NS_IMETHOD GetDependencies(nsCOMArray<nsIDOMNode> **aDependencies);
  NS_IMETHOD GetElement(nsIDOMElement **aElement);
  NS_IMETHOD ResetBoundNode(const nsString     &aBindAttribute,
                            PRUint16            aResultType,
                            nsIDOMXPathResult **aResult = nsnull);
  NS_IMETHOD Bind();
  NS_IMETHOD TryFocus(PRBool* aOK);
  NS_IMETHOD IsEventTarget(PRBool *aOK);

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
  PRBool                              mHasParent;

  /** State to prevent infinite loop when generating and handling xforms-next
   *  and xforms-previous events
   */
  PRBool                              mPreventLoop;

  /**
   * Array of repeat-elements of which the control uses repeat-index.
   */
  nsCOMArray<nsIXFormsRepeatElement>  mIndexesUsed;

  /** 
   * Used to keep track of whether this control has any single node binding
   * attributes.
   */
  PRInt32 mBindAttrsCount;

  /** Returns the read only state of the control (ie. mBoundNode) */
  PRBool GetReadOnlyState();
  
  /** Returns the relevant state of the control */
  PRBool GetRelevantState();

  /**
   * Processes the node binding of a control, get the current MDG (mMDG) and
   * binds the control to its model.
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
   * Removes all of the attributes that may have been added to the control due
   * to binding with an instance node.
   */
  void ResetProperties();

  /**
   * Causes Bind() and Refresh() to be called if aName is the atom of a
   * single node binding attribute for this control.  Called by AttributeSet
   * and AttributeRemoved.
   */
  void MaybeBindAndRefresh(nsIAtom *aName);

  /**
   * Removes this control from its model's list of controls if a single node
   * binding attribute is removed.  Called by WillSetAttribute and
   * WillRemoveAttribute.
   * 
   * @param aName  - atom of the attribute being changed
   * @param aValue - value that the attribute is being changed to.
   */
  void MaybeRemoveFromModel(nsIAtom *aName, const nsAString &aValue); 

  /** Removes the index change event listeners */
  void RemoveIndexListeners();

  /**
   * Shows an error dialog for the user the first time an
   * xforms-binding-exception event is received by the control.
   *
   * The dialog can be disabled via the |xforms.disablePopup| preference.
   *
   * @return                 Whether handling was successful
   */
  PRBool HandleBindingException();
};


/**
 * nsXFormsControlStub inherits from nsXFormsXMLVisualStub
 */
class nsXFormsControlStub : public nsXFormsControlStubBase,
                            public nsXFormsXMLVisualStub
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFXMLVisual overrides
  /** This sets the notification mask and initializes mElement */
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
  {
    nsresult rv = nsXFormsXMLVisualStub::OnCreated(aWrapper);
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

  NS_IMETHOD Bind()
  {
    return nsXFormsControlStubBase::Bind();
  }

  /** Constructor */
  nsXFormsControlStub() : nsXFormsControlStubBase() {};
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
