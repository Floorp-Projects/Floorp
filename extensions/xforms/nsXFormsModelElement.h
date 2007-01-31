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
#include "nsIXFormsNSModelElement.h"
#include "nsIDOMEventListener.h"
#include "nsISchema.h"
#include "nsCOMArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsXFormsMDGEngine.h"
#include "nsXFormsUtils.h"
#include "nsIClassInfo.h"

#include "nsISchemaLoader.h"
#include "nsISchema.h"
#include "nsIXFormsContextControl.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"

class nsIDOMElement;
class nsIDOMNode;
class nsIDOMXPathEvaluator;
class nsIDOMXPathResult;
class nsXFormsControl;
class nsXFormsModelInstanceDocuments;

/**
 * Implementation of the instance node list returned by
 * nsIXFormsModel::getInstanceDocuments.
 *
 * Manages the list of all instance elements that belong to a given
 * nsXFormsModelElement.
 */
class nsXFormsModelInstanceDocuments : public nsIDOMNodeList,
                                       public nsIClassInfo
{
public:
  nsXFormsModelInstanceDocuments();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIDOMNODELIST

  /**
   * Add an instance element
   *
   * @param aInstance         The new instance element
   */
  void AddInstance(nsIInstanceElementPrivate *aInstance);

  /**
   * Remove an instance element
   *
   * @param aInstance         The instance element
   */
  void RemoveInstance(nsIInstanceElementPrivate *aInstance);

  /**
   * Get the instance document at a given index
   *
   * @note Does NOT addref the returned element!
   *
   * @param aIndex            The index
   * @return                  The instance element (or nsnull if not found)
   */
  nsIInstanceElementPrivate* GetInstanceAt(PRUint32 aIndex);

  /**
   * Instructs the class to drop references to all instance elements
   */
  void DropReferences();

protected:
  /** The array holding the instance elements */
  nsCOMArray<nsIInstanceElementPrivate>    mInstanceList;
};

/**
 * A class for storing pointers to XForms controls added to an XForms
 * model. Organized as a tree, with pointers to first child and next sibling.
 *
 * Notes:
 * 1) The root node is special; only has children and a nsnull mNode.
 * 2) All functions operate on the node they are called on, and its
 * subtree. Functions will never go up (as in: nearer the root) in the tree.
 */
class nsXFormsControlListItem
{
  nsAutoRefCnt                      mRefCnt;

  /** The XForms control itself */
  nsCOMPtr<nsIXFormsControl>        mNode;

  /** The next sibling of the node */
  nsRefPtr<nsXFormsControlListItem> mNextSibling;

  /** The first child of the node */
  nsRefPtr<nsXFormsControlListItem> mFirstChild;

  nsRefPtrHashtable<nsISupportsHashKey, nsXFormsControlListItem> *mControlListHash;

public:

  /** The constructor takes a hashtable pointer, which needs to point to the
   *  model's hashtable.  This is so that each item in the control list has
   *  access to the same hashtable and can add/remove items from it and find
   *  items in it.
   */
  nsXFormsControlListItem(
    nsIXFormsControl* aControl,
    nsRefPtrHashtable<nsISupportsHashKey,nsXFormsControlListItem> *aHash);
  nsXFormsControlListItem();
  ~nsXFormsControlListItem();
  nsXFormsControlListItem(const nsXFormsControlListItem& aCopy);

  nsrefcnt AddRef()
  {
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsXFormsControlListItem",
                  sizeof(nsXFormsControlListItem));
    return mRefCnt;
  }

  nsrefcnt Release()
  {
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsXFormsControlListItem");
    if (mRefCnt == 0) {
      mRefCnt = 1;
      delete this;
      return 0;
    }
    return mRefCnt;
  }

  /** Clear contents of current node, all siblings, and all children */
  void Clear();

  /**
   * Remove a control from the current (sub-) tree. Will search from that node
   * and down in the tree for the node.
   *
   * @param aControl     The control to remove
   * @param aRemoved     Was the control found and removed?
   */
  nsresult RemoveControl(nsIXFormsControl *aControl, PRBool &aRemoved);

  /**
   * Add a control to the (sub-) tree as a child to the given |aParent|. If
   * there is no |aParent|, it will insert it as a sibling to |this|. If
   * |aParent| is not found, it will bail.
   *
   * @param aControl     The control to insert
   * @param aParent      The (eventual) parent to insert it under
   */
  nsresult AddControl(nsIXFormsControl *aControl,
                      nsIXFormsControl *aParent);

  /**
   * Find a control in the (sub-) tree.
   *
   * @param aControl     The control to find
   */
  nsXFormsControlListItem* FindControl(nsIXFormsControl *aControl);

  /**
   * Return the nsIXFormsControl that this node contains.
   */
  already_AddRefed<nsIXFormsControl> Control();

  /** Return the first child of the node */
  nsXFormsControlListItem* FirstChild() { return mFirstChild; };

  /** Return the next sibling of the node */
  nsXFormsControlListItem* NextSibling() { return mNextSibling; };

  /**
   * An iterator implementation for the class.
   */
  class iterator
  {
  private:
    /** The control the iterator is currently pointing at */
    nsXFormsControlListItem  *mCur;

    /** A stack of non-visited nodes */
    nsVoidArray               mStack;

  public:
    iterator();
    iterator(const iterator&);
    iterator operator=(nsXFormsControlListItem*);
    bool operator!=(const nsXFormsControlListItem*);
    iterator operator++();
    nsXFormsControlListItem* operator*();
  };
  friend class nsXFormsControlListItem::iterator;

  /** The begining position for the node (itself) */
  nsXFormsControlListItem* begin();

  /** The end position for the node (nsnull) */
  nsXFormsControlListItem* end();
};

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
                             public nsIXFormsNSModelElement,
                             public nsISchemaLoadListener,
                             public nsIDOMEventListener,
                             public nsIXFormsContextControl
{
public:
  nsXFormsModelElement() NS_HIDDEN;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSMODELELEMENT
  NS_DECL_NSIMODELELEMENTPRIVATE
  NS_DECL_NSIXFORMSNSMODELELEMENT
  NS_DECL_NSISCHEMALOADLISTENER
  NS_DECL_NSIWEBSERVICEERRORHANDLER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIXFORMSCONTEXTCONTROL

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD GetScriptingInterfaces(PRUint32 *aCount, nsIID ***aArray);
  NS_IMETHOD WillChangeDocument(nsIDOMDocument *aNewDocument);
  NS_IMETHOD DocumentChanged(nsIDOMDocument *aNewDocument);
  NS_IMETHOD DoneAddingChildren();
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled);
  NS_IMETHOD OnCreated(nsIXTFElementWrapper *aWrapper);

  // nsIXFormsControlBase overrides
  NS_IMETHOD Bind(PRBool *aContextChanged) {
    // dummy method, so does nothing
    return NS_OK;
  };

  NS_IMETHOD GetOnDeferredBindList(PRBool *onList) {
    // dummy method, so does nothing
    return NS_OK;
  };

  NS_IMETHOD SetOnDeferredBindList(PRBool putOnList) {
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
   * @param aControl          XForms control waiting to be bound
   */
  static NS_HIDDEN_(nsresult) DeferElementBind(nsIXFormsControl  *aControl);

  static nsresult NeedsPostRefresh(nsIXFormsControl* aControl);

  // Sometimes a child that is on the post refresh list will want to force
  // its containing xforms control to refresh.  Like if an xf:item's label
  // refreshes, it wants to reflect any of its changes in the xf:select or
  // xf:select1 that contains it.  This function will try to minimize that by
  // allowing xforms controls that function as containers (like select/select1)
  // to defer their refresh until most or all of its children have refreshed.
  // Well, at least until every control on the sPostRefreshList has been
  // processed.  This will return PR_TRUE if the refresh will be deferred.
  static PRBool ContainerNeedsPostRefresh(nsIXFormsControl* aControl);
  static void CancelPostRefresh(nsIXFormsControl* aControl);

  /**
   * Returns the DOM element for the model.
   */
  already_AddRefed<nsIDOMElement> GetDOMElement();

private:

  NS_HIDDEN_(already_AddRefed<nsIDOMDocument>)
    FindInstanceDocument(const nsAString &aID);
  NS_HIDDEN_(void)     Reset();
  NS_HIDDEN_(void)     BackupOrRestoreInstanceData(PRBool restore);

  /** Initializes the MIPs on all form controls */
  NS_HIDDEN_(nsresult) InitializeControls();

  NS_HIDDEN_(nsresult) InitializeInstances();

  NS_HIDDEN_(nsresult) ProcessBindElements();
  NS_HIDDEN_(nsresult) FinishConstruction();
  NS_HIDDEN_(nsresult) ConstructDone();
  NS_HIDDEN_(void)     MaybeNotifyCompletion();

  NS_HIDDEN_(nsresult) ProcessBind(nsIDOMXPathEvaluator *aEvaluator,
                                   nsIDOMNode           *aContextNode,
                                   PRInt32               aContextPosition,
                                   PRInt32               aContextSize,
                                   nsIDOMElement        *aBindElement,
                                   PRBool                aIsOuter = PR_FALSE);

  NS_HIDDEN_(void)     RemoveModelFromDocument();

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
   * Handle controls bindings which was deferred because the model was not
   * ready.
   *
   * @note Only registers the controls with the model. Does not setup
   * bindings, etc.
   *
   * @param aDoc              Document that contains the XForms control
   */
  static NS_HIDDEN_(void) ProcessDeferredBinds(nsIDOMDocument *aDoc);

  // Returns true when all external documents have been loaded
  PRBool IsComplete() const { return (mSchemaTotal == mSchemaCount
                                      && mPendingInstanceCount == 0);  }

  /**
   * Called by HandleEvent.  Event handler for the 'DOMContentLoaded' event.
   */
  NS_HIDDEN_(nsresult) HandleLoad(nsIDOMEvent *aEvent);

  /**
   * Called by HandleEvent.  Event handler for the 'unload' event.
   */
  NS_HIDDEN_(nsresult) HandleUnload(nsIDOMEvent *aEvent);

  NS_HIDDEN_(nsresult) RefreshSubTree(nsXFormsControlListItem *aCurrent,
                                      PRBool                   aForceRebind);

  NS_HIDDEN_(nsresult) ValidateDocument(nsIDOMDocument *aInstanceDocument,
                                        PRBool         *aResult);

  /**
   * Request to send an update event to the model. If an update is already
   * running, the event will be queued, and sent after that. If multiple
   * events are queued, they will be dispatched FIFO order.
   *
   * @param aEvent            The requested event
   */
  NS_HIDDEN_(nsresult) RequestUpdateEvent(nsXFormsEvent aEvent);
  
  /**
   * Returns true if a schema has already been registered to address the same
   * namespace as aSchemaElement.  Per section 3.3.1 of the XForms spec:
   * Within each XForms Model, there is a limit of one Schema per namespace
   * declaration, including inline and linked Schemas.
   *
   * @param aSchemaElement The schema element
   */
  PRBool IsDuplicateSchema(nsIDOMElement *aSchemaElement);

  /**
   * Validate all the instance documents.
   */
  void ValidateInstanceDocuments();

  /**
   * Return the name of builitn type.
   *
   * @param aType The builtin type
   * @param aName The name of the given type
   */
  NS_HIDDEN_(nsresult) GetBuiltinTypeName(PRUint16   aType,
                                          nsAString& aName);

  /**
   * Starting with aType, walks through the builtin derived types back to the
   * builtin primative type, appending the datatype URIs to the string array as
   * it goes
   */
  NS_HIDDEN_(nsresult) GetBuiltinTypesNames(PRUint16       aType,
                                            nsStringArray *aNameArray);

  /**
   * Starting from aType, walks the chain of datatype extension/derivation to
   * gather information.
   *
   * @param aType            The type we are trying to find datatype information
   *                         for
   * @param aFindRootBuiltin If true then root builtin type will be returned
                             only
   * @param aBuiltinType     If non-null, we'll return the builtin (if
   *                         aFindRootBuiltin is false) or root builtin (if
   *                         aFindRootBuiltin is true) type of the given type
   * @param aTypeArray       If non-null and aFindRootBuiltin is false, we'll
   *                         build a string array of datatype URIs and put them
   *                         in aTypeArray.
   */
  NS_HIDDEN_(nsresult) WalkTypeChainInternal(nsISchemaType *aType,
                                             PRBool         aFindRootBuiltin,
                                             PRUint16      *aBuiltinType,
                                             nsStringArray *aTypeArray = nsnull);
  /**
   * Returns the primative type that aSchemaType is derived/extended from
   */
  NS_HIDDEN_(nsresult) BuiltinTypeToPrimative(nsISchemaBuiltinType *aSchemaType,
                                              PRUint16             *aPrimType);

  nsIDOMElement            *mElement;
  nsCOMPtr<nsISchemaLoader> mSchemas;
  nsStringArray             mPendingInlineSchemas;
  nsXFormsControlListItem   mFormControls;
  nsRefPtrHashtable<nsISupportsHashKey, nsXFormsControlListItem> mControlListHash;

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

  /**
   * This flag indicates whether or not the document has fired
   * DOMContentLoaded
   */
  PRPackedBool mDocumentLoaded;

  /**
   * Indicates whether all controls should be refreshed on the next Refresh()
   * run.
   */
  PRPackedBool mRebindAllControls;

  /**
   * Indicates whether instance elements have been initialized
   */
  PRPackedBool mInstancesInitialized;

  /**
   * Indicates whether the model has handled the xforms-ready event
   */
  PRPackedBool mReadyHandled;

  /**
   * Indicates whether the model's instance was built by lazy authoring
   */
  PRPackedBool mLazyModel;

  /**
   * Indicates whether the model has handled the xforms-model-construct-done
   * event
   */
  PRPackedBool mConstructDoneHandled;

  /**
   * Indicates whether the model is currently processing an update event,
   * ie. xforms-rebuild, xforms-recalculate, xforms-revalidate, or
   * xforms-refresh.
   */
  PRPackedBool mProcessingUpdateEvent;

  /**
   * A list of update events that have been queued, because they were
   * requested while another update was running.
   */
  nsVoidArray mUpdateEventQueue;

  /**
   * The maximum allowed number of iterations of queued event dispatching in
   * RequestUpdateEvent(), before there is believe to be a loop, and
   * processing stops.
   */
  PRInt32 mLoopMax;

  /**
   * All instance documents contained by this model, including lazy-authored
   * instance documents.
   */
  nsRefPtr<nsXFormsModelInstanceDocuments> mInstanceDocuments;

  /**
   * Type information for nodes, with their type set through \<xforms:bind\>.
   *
   * @see http://www.w3.org/TR/xforms/slice6.html#model-prop-type
   */
  nsClassHashtable<nsISupportsHashKey, nsString> mNodeToType;

  /**
   * P3P type information for nodes, with their type set through
   * \<xforms:bind\>.

   * @see http://www.w3.org/TR/xforms/slice6.html#model-prop-p3ptype
   */
  nsClassHashtable<nsISupportsHashKey, nsString> mNodeToP3PType;

  friend class Updating;
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
