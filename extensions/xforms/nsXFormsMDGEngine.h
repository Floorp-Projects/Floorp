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
 *  David Landwehr <dlandwehr@novell.com>
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

#ifndef __NSXFORMSMDGENGINE__
#define __NSXFORMSMDGENGINE__

#include "nscore.h"
#include "nsClassHashtable.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"

#include "nsIDOMNode.h"

#include "nsXFormsTypes.h"
#include "nsXFormsNodeState.h"

class nsIDOMNSXPathExpression;
class nsXFormsModelElement;

/**
 * Data structure for nodes in the graph.
 * 
 * There is one node per type (calculate, readonly, etc) for each nsIDOMNode.
 * All nsXFormsMDGNodes for same nsIDOMNode are linked together via 'next'.
 */
class nsXFormsMDGNode {
private:
  /** Dirty flag */
  PRBool mDirty;

  /** Does this node have an XPath expression attached to it */
  PRBool mHasExpr;
  
public:
  /** Pointer to the nsIDOMNode */
  nsCOMPtr<nsIDOMNode> mContextNode;
  
  /** The XPath expression for this node */
  nsCOMPtr<nsIDOMNSXPathExpression> mExpression;
  
  /** List of nodes that depend on this node */
  nsVoidArray mSuc;
  
  /** Number of nodes that this node depends on */
  unsigned int mCount;
  
  /** The type */
  ModelItemPropName mType;
  
  /** (XPath) Context size for this node */
  PRInt32 mContextSize;
  
  /** (XPath) Position for this node */
  PRInt32 mContextPosition;
  
  /** Does expression use dynamic functions */
  PRBool mDynFunc;

  /**
   * Pointer to next nsXFormsMDGNode with same nsIDOMNode, but different
   * MIP type (mType)
   */
  nsXFormsMDGNode* mNext;

  /**
   * Constructor.
   * 
   * @param aNode            The context node
   * @param aType            The type of node (calculate, readonly, etc.)
   */ 
  nsXFormsMDGNode(nsIDOMNode             *aNode,
                  const ModelItemPropName aType);
  
  /** Destructor */
  ~nsXFormsMDGNode();
  
  /**
   * Sets the XPath expression for the node.
   * 
   * @param aExpression      The XPath expression
   * @param aDynFunc         Whether expression uses dynamic functions
   * @param aContextPosition The context position for the expression
   * @param aContextSize     The context size for the expression
   */
  void SetExpression(nsIDOMNSXPathExpression *aExpression,
                     PRBool                   aDynFunc,
                     PRInt32                  aContextPosition,
                     PRInt32                  aContextSize);
                     
  /** Does node have an expression? */
  PRBool HasExpr() const;
  
  /** Is node dirty? */
  PRBool IsDirty() const;
  
  /* Mark node clean */
  void MarkClean();

  /* Mark node dirty */
  void MarkDirty();
};


/**
 * The Multi Dependency Graph (MDG) Engine.
 * 
 * This class handles all the necessary logic to create and maintain the MDG,
 * and update the instance data accordingly.
 * 
 * A bit about the graph:
 * As specified in the spec., one node (nsXFormsMDGNode) is created in the graph for
 * each of the attributes (readonly, calculate, etc.) for a nsIDOMNode. These graph
 * nodes are owned by the mNodeToMDG hash table, which maps from a nsIDOMNode to
 * the first node in a single-linked list (mNext) of nodes for the same nsIDOMNode.
 *
 * @todo Merge SetNodeValue() with nsXFormsUtils::SetNodeValue() (XXX)
 */
class nsXFormsMDGEngine
{
protected:
  /** Maps from nsIDOMNode to nsXFormsMDGNode */
  nsClassHashtable<nsISupportsHashKey, nsXFormsMDGNode> mNodeToMDG; 
  
  /** Maps from nsIDOMNode to nsXFormsNodeState */
  nsClassHashtable<nsISupportsHashKey, nsXFormsNodeState> mNodeStates;

  /** Default node state */
  nsXFormsNodeState mDefaultState;
  
  /** True when Rebuild() has been run, but not ClearDispatchFlags() */
  PRBool mJustRebuilt;
  
  /** True when last Calculate() was run when mJustRebuilt was true. */
  PRBool mFirstCalculate;
  
  /** The actual MDG */
  nsVoidArray mGraph;

  /** The model that created the MDG */
  nsXFormsModelElement *mModel;
  
  /**
   * Nodes that are marked as changed, and should be included in recalculation
   */
  nsCOMArray<nsIDOMNode> mMarkedNodes;
  
  /** Number of nodes in the graph */
  PRInt32 mNodesInGraph;
  
  /** Hidden copy constructor */
  nsXFormsMDGEngine(nsXFormsMDGEngine&) {}
  
  /**
   * Used by Clear() and ~ to delete the linked nodes in mNodeToMDG, the hash table
   * itself handles the main nodes.
   */
  static PLDHashOperator PR_CALLBACK
    DeleteLinkedNodes(nsISupports                *aKey,
                      nsAutoPtr<nsXFormsMDGNode> &aNode,
                      void                       *aArg);
  
  /**
   * Used by Rebuild() to find the start nodes for mGraph, that is nodes
   * where mCount == 0.
   */
  static PLDHashOperator PR_CALLBACK
    AddStartNodes(nsISupports     *aKey,
                  nsXFormsMDGNode *aNode,
                  void            *aDeque);
  
  /**
   * Used by AndFlags() to boolean AND _all_ flags with aMask.
   */
  static PLDHashOperator PR_CALLBACK
    AndFlag(nsISupports                  *aKey,
            nsAutoPtr<nsXFormsNodeState> &aState,
            void                         *aMask);

  /**
   * Get non-const (NC) node state for a node, create a new node state if
   * necessary.
   *
   * @param aContextNode      The node to get the state for
   * @return                  The state (owned by nsXFormsMDGEngine)
   */
  nsXFormsNodeState* GetNCNodeState(nsIDOMNode *aContextNode);

  /**
   * Inserts a new text child for aContextNode.
   * 
   * @param aContextNode     The node to create a child for
   * @param aNodeValue       The value of the new node
   * @param aBeforeNode      If non-null, insert new node before this node
   */
  nsresult CreateNewChild(nsIDOMNode      *aContextNode,
                          const nsAString &aNodeValue,
                          nsIDOMNode      *aBeforeNode = nsnull);

  /**
   * Retrieve a node from the graph.
   * 
   * @param aDomNode         The DOM node to retrieve
   * @param aType            The type to retrieve (readonly, calculate, etc)
   * @param aCreate          Create the node and insert it into the graph
   *                         if it does not exist?
   * @return                 The node, nsnull if not found and aCreate != PR_TRUE
   *
   * @note aType == eModel_type means "any type", as we do not store type
   * information in the MDG.
   */
  nsXFormsMDGNode* GetNode(nsIDOMNode       *aDomNode,
                           ModelItemPropName aType,
                           PRBool            aCreate = PR_TRUE);

  /**
   * Boolean AND _all_ flags with a mask.
   * 
   * @param aAndMask         The mask
   * @return                 Did operation succeed?
   */
  PRBool   AndFlags(PRUint16 aAndMask);
  
  /**
   * Evaluates the expression for the given node and returns the boolean result.
   * 
   * @param aNode            The node to evaluate
   * @param aRes             The result of the evaluation
   */
  nsresult BooleanExpression(nsXFormsMDGNode *aNode,
                             PRBool          &aRes);
  
  /**
   * Compute MIP value for node types with boolean result (all except calculate)
   * 
   * @param aStateFlag       The flag for the type of MIP
   * @param aDispatchFlag    The dispatch flag
   * @param aNode            The context node
   * @param aDidChange       Was the node changed?
   */
  nsresult ComputeMIP(eFlag_t          aStateFlag,
                      eFlag_t          aDispatchFlag,
                      nsXFormsMDGNode *aNode,
                      PRBool          &aDidChange);
  
  /**
   * Same as ComputeMIP(), but also handles any inheritance of attributes.
   * 
   * @param aStateFlag       The flag for the type of MIP
   * @param aDispatchFlag    The dispatch flag
   * @param aInheritanceFlag The inheritance flag for the type of MIP
   * @param aNode            The context node
   * @param aSet             Set of the nodes influenced by operation
   */
  nsresult ComputeMIPWithInheritance(eFlag_t                 aStateFlag,
                                     eFlag_t                 aDispatchFlag,
                                     eFlag_t                 aInheritanceFlag,
                                     nsXFormsMDGNode        *aNode,
                                     nsCOMArray<nsIDOMNode> *aSet);

  /**
   * Attaches inheritance to all children of a given node
   * 
   * @param aSet             Set of the nodes influenced by operation
   * @param aSrc             The node
   * @param aState           The state of the flag
   * @param aStateFlag       The flag
   */
  nsresult AttachInheritance(nsCOMArray<nsIDOMNode> *aSet,
                             nsIDOMNode             *aSrc,
                             PRBool                 aState,
                             eFlag_t                aStateFlag);

  /**
   * Invalidate the information, ie. mark all nodes as dirty.
   */
  nsresult Invalidate();
  
  /**
   * Set the value of a node.

   * @param aContextNode     The node to set the value for
   * @param aNodeValue       The value
   * @param aMarkNode        Whether to mark node as changed
   * @param aNodeChanged     Was node changed?
   * @param aIsCalculate     Is it a @calculate setting the value?
   */
  nsresult SetNodeValueInternal(nsIDOMNode      *aContextNode,
                                const nsAString &aNodeValue,
                                PRBool           aMarkNode = PR_TRUE,
                                PRBool           aIsCalculate = PR_FALSE,
                                PRBool          *aNodeChanged = nsnull);

  /**
   * Handle nodes nodes marked as dirty, and insert into "changed nodes
   * array".
   *
   * @param aArray            The "changed nodes array" to insert into
   */
  nsresult HandleMarkedNodes(nsCOMArray<nsIDOMNode> *aArray);

public:
  /**
   * Constructor
   */
  nsXFormsMDGEngine();

  /*
   * Destructor
   */
  ~nsXFormsMDGEngine();
  
  /**
   * Initializes the internal structures. Needs to be called before class is used!
   *
   * @param aModel           The model that created this MDGEngine instance.
   */
  nsresult Init(nsXFormsModelElement *aModel);

  /**
   * Insert new MIP (Model Item Property) into graph.
   * 
   * @param aType            The type of MIP
   * @param aExpression      The XPath expression
   * @param aDependencies    Set of nodes expression depends on
   * @param aDynFunc         True if expression uses dynamic functions
   * @param aContextNode     The context node for aExpression
   * @param aContextPos      The context positions of aExpression
   * @param aContextSize     The context size for aExpression
   */
  nsresult AddMIP(ModelItemPropName         aType,
                  nsIDOMNSXPathExpression  *aExpression,
                  nsCOMArray<nsIDOMNode>   *aDependencies,
                  PRBool                    aDynFunc,
                  nsIDOMNode               *aContextNode,
                  PRInt32                   aContextPos,
                  PRInt32                   aContextSize);

  /**
   * Recalculate the MDG.
   * 
   * @param aChangedNodes    Returns the nodes that was changed during recalculation.
   */
  nsresult Recalculate(nsCOMArray<nsIDOMNode> *aChangedNodes);

  /**
   * Revalidate nodes
   * 
   * @param aNodes           The nodes to revalidate
   */
  nsresult Revalidate(nsCOMArray<nsIDOMNode> *aNodes);

  /**
   * Rebuilds the MDG.
   */
  nsresult Rebuild();

  /**
   * Clears all information in the MDG.
   */
  nsresult Clear();

  /**
   * Clears all Dispatch flags.
   */
  nsresult ClearDispatchFlags();

  /**
   * Mark a node as changed.
   * 
   * @param aContextNode     The node to be marked.
   */
  nsresult MarkNodeAsChanged(nsIDOMNode *aContextNode);

  /**
   * Set the value of a node -- the public version of SetNodeValueInternal().
   *
   * @param aContextNode     The node to set the value for
   * @param aNodeValue       The value
   * @param aNodeChanged     Was node changed?
   */
  nsresult SetNodeValue(nsIDOMNode      *aContextNode,
                        const nsAString &aNodeValue,
                        PRBool          *aNodeChanged = nsnull);

  /**
   * Set the contents of a node
   *
   * @param aContextNode     The node to set the contents of
   * @param aContentEnvelope The container of the contents that need to be
   *                         moved under aContextNode
   */
  nsresult SetNodeContent(nsIDOMNode      *aContextNode,
                          nsIDOMNode      *aContentEnvelope);

  /**
   * External interface of GetNCNodeState(), returns const pointer to the node
   * state.
   *
   * @param aContextNode      The node to get the state for
   * @return                  The state (owned by nsXFormsMDGEngine)
   */
  const nsXFormsNodeState* GetNodeState(nsIDOMNode *aContextNode);

#ifdef DEBUG_beaufour
  /**
   * Write MDG graph to a file in GraphViz dot format.
   *
   * @param aFile             Filename to write to (nsnull = stdout)
   */
  void PrintDot(const char* aFile = nsnull);
#endif
};

#endif
