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
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsVoidArray.h"

#include "nsIDOMNode.h"

#include "nsXFormsTypes.h"
#include "nsXFormsMDGSet.h"

class nsIDOMXPathExpression;

/**
 * Flags, etc.
 * 
 * TODO: Convert to enum?
 */
const PRUint16 MDG_FLAG_READONLY                    = 1 << 1;
const PRUint16 MDG_FLAG_CONSTRAINT                  = 1 << 2;
const PRUint16 MDG_FLAG_RELEVANT                    = 1 << 3;
const PRUint16 MDG_FLAG_REQUIRED                    = 1 << 4;
const PRUint16 MDG_FLAG_SCHEMA_VALID                = 1 << 5;
const PRUint16 MDG_FLAG_VALID                       = 1 << 6;
const PRUint16 MDG_FLAG_INHERITED_RELEVANT          = 1 << 7;
const PRUint16 MDG_FLAG_INHERITED_READONLY          = 1 << 8;
const PRUint16 MDG_FLAG_DISPATCH_VALUE_CHANGED      = 1 << 9;
const PRUint16 MDG_FLAG_DISPATCH_READONLY_CHANGED   = 1 << 10;
const PRUint16 MDG_FLAG_DISPATCH_VALID_CHANGED      = 1 << 11;
const PRUint16 MDG_FLAG_DISPATCH_RELEVANT_CHANGED   = 1 << 12;
const PRUint16 MDG_FLAG_DISPATCH_REQUIRED_CHANGED   = 1 << 13;
const PRUint16 MDG_FLAG_DISPATCH_CONSTRAINT_CHANGED = 1 << 14;

const PRUint16 MDG_FLAGMASK_NOT_DISPATCH = ~(MDG_FLAG_DISPATCH_VALUE_CHANGED | \
                                             MDG_FLAG_DISPATCH_READONLY_CHANGED |\
                                             MDG_FLAG_DISPATCH_VALID_CHANGED |\
                                             MDG_FLAG_DISPATCH_RELEVANT_CHANGED |\
                                             MDG_FLAG_DISPATCH_REQUIRED_CHANGED |\
                                             MDG_FLAG_DISPATCH_CONSTRAINT_CHANGED);

const PRUint16 MDG_FLAG_DEFAULT = MDG_FLAG_CONSTRAINT | \
                                  MDG_FLAG_RELEVANT |\
                                  MDG_FLAG_INHERITED_RELEVANT;

const PRUint16 MDG_FLAG_INITIAL_DISPATCH = MDG_FLAG_DISPATCH_READONLY_CHANGED |\
                                           MDG_FLAG_DISPATCH_VALID_CHANGED |\
                                           MDG_FLAG_DISPATCH_RELEVANT_CHANGED |\
                                           MDG_FLAG_DISPATCH_REQUIRED_CHANGED;
                       
const PRUint16 MDG_FLAG_ALL_DISPATCH = MDG_FLAG_DISPATCH_READONLY_CHANGED |\
                                       MDG_FLAG_DISPATCH_VALID_CHANGED |\
                                       MDG_FLAG_DISPATCH_RELEVANT_CHANGED |\
                                       MDG_FLAG_DISPATCH_REQUIRED_CHANGED |\
                                       MDG_FLAG_DISPATCH_VALUE_CHANGED |\
                                       MDG_FLAG_DISPATCH_CONSTRAINT_CHANGED;
                       


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
  nsCOMPtr<nsIDOMXPathExpression> mExpression;
  
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
  
  /** Pointer to next nsXFormsMDGNode with same nsIDOMNode, but different type */
  nsXFormsMDGNode* mNext;

  /**
   * Constructor.
   * 
   * @param aNode            The context node
   * @param aType            The type of node (calculate, readonly, etc.)
   */ 
  nsXFormsMDGNode(nsIDOMNode* aNode, const ModelItemPropName aType);
  
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
  void SetExpression(nsIDOMXPathExpression* aExpression, PRBool aDynFunc,
                     PRInt32 aContextPosition, PRInt32 aContextSize);
                     
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
 */
class nsXFormsMDGEngine
{
protected:
  /**
   * Maps from nsIDOMNode to nsXFormsMDGNode(s)
   */
  nsClassHashtable<nsISupportsHashKey, nsXFormsMDGNode> mNodeToMDG; 
  
  /**
   * Maps from nsIDOMNode to flag (MDG_FLAG_*)
   */
  nsDataHashtable<nsISupportsHashKey, PRUint16> mNodeToFlag;
  
  /**
   * True when Rebuild() has been run, but not ClearDispatchFlags()
   */
  PRBool mJustRebuilt;
  
  /**
   * True when last Calculate() was run when mJustRebuilded was true.
   */
  PRBool mFirstCalculate;
  
  /** The actual MDG */
  nsVoidArray mGraph;
  
  /** Set of nodes that are marked as changed, and should be included in recalculation */
  nsXFormsMDGSet mMarkedNodes;
  
  /** Number of nodes in the graph */
  PRInt32 mNodesInGraph;
  
  /** Hidden copy constructor */
  nsXFormsMDGEngine(nsXFormsMDGEngine&) {}
  
  /**
   * Used by Clear() and ~ to delete the linked nodes in mNodeToMDG, the hash table
   * itself handles the main nodes.
   */
  static PLDHashOperator PR_CALLBACK DeleteLinkedNodes(nsISupports *aKey, nsAutoPtr<nsXFormsMDGNode>& aNode, void* aArg);
  
  /**
   * Used by Rebuild() to find the start nodes for mGraph, that is nodes
   * where mCount == 0.
   */
  static PLDHashOperator PR_CALLBACK AddStartNodes(nsISupports *aKey, nsXFormsMDGNode* aNode, void* aDeque);
  
  /**
   * Used by AndFlags() to boolean AND all flags with aMask.
   */
  static PLDHashOperator PR_CALLBACK AndFlag(nsISupports *aKey, PRUint16& aFlag, void* aMask);

  /**
   * Inserts a new text child for aContextNode.
   * 
   * @param aContextNode     The node to create a child for
   * @param aNodeValue       The value of the new node
   * @param aBeforeNode      If non-null, insert new node before this node
   */
  nsresult CreateNewChild(nsIDOMNode* aContextNode, const nsAString& aNodeValue, nsIDOMNode* aBeforeNode = nsnull);

  /**
   * Retrieve a node from the graph.
   * 
   * @param aDomNode         The DOM node to retrieve
   * @param aType            The type to retrieve (readonly, calculate, etc)
   * @param aCreate          Create the node and insert it into the graph if it does not exist?
   * @return                 The node, nsnull if not found and aCreate != PR_TRUE
   */
  nsXFormsMDGNode* GetNode(nsIDOMNode* aDomNode, ModelItemPropName aType, PRBool aCreate = PR_TRUE);
  
  /**
   * Sets the flag for a node.
   * 
   * @param aDomNode         The node
   * @param aFlag            The flag
   */
  void     SetFlag(nsIDOMNode* aDomNode, PRUint16 aFlag);
  
  /**
   * Boolean OR the existing flag on the node with a new flag
   * 
   * @param aDomNode         The node
   * @param aFlag            The new flag
   */
  void     OrFlag(nsIDOMNode* aDomNode, PRInt16 aFlag);
  
  /**
   * Retrieve the flag for a node.
   * 
   * @param aDomNode         The node
   * @return                 The flag
   */
  PRUint16 GetFlag(nsIDOMNode* aDomNode);

  /**
   * Sets a bit in the flag for a node
   * 
   * @param aDomNode         The node
   * @param aBit             The bit position to set
   * @param aOn              The bit value
   */
  void     SetFlagBits(nsIDOMNode* aDomNode, PRUint16 aBit, PRBool aOn);
  
  /**
   * Boolean AND all flags with a mask.
   * 
   * @param aAndMask         The mask
   * @return                 Did operation succeed?
   */
  PRBool     AndFlags(PRUint16 aAndMask);
  
  /**
   * Evaluates the expression for the given node and returns the boolean result.
   * 
   * @param aNode            The node to evaluate
   * @param res              The result of the evaluation
   */
  nsresult BooleanExpression(nsXFormsMDGNode* aNode, PRBool& res);
  
  /**
   * Compute MIP value for node types with boolean result (all except calculate)
   * 
   * @param aStateFlag       The flag for the type of MIP
   * @param aDispatchFlag    The dispatch flag
   * @param aNode            The context node
   * @param aDidChange       Was the node changed?
   */
  nsresult ComputeMIP(PRUint16 aStateFlag, PRUint16 aDispatchFlag, nsXFormsMDGNode* aNode, PRBool& aDidChange);
  
  /**
   * Same as ComputeMIP(), but also handles any inheritance of attributes.
   * 
   * @param aStateFlag       The flag for the type of MIP
   * @param aDispatchFlag    The dispatch flag
   * @param aInheritanceFlag The inheritance flag for the type of MIP
   * @param aNode            The context node
   * @param aSet             Set of the nodes influenced by operation
   */
  nsresult ComputeMIPWithInheritance(PRUint16 aStateFlag, PRUint16 aDispatchFlag, PRUint16 aInheritanceFlag, nsXFormsMDGNode* aNode, nsXFormsMDGSet* aSet);

  /**
   * Attaches inheritance to all children of a given node
   * 
   * @param aSet             Set of the nodes influenced by operation
   * @param aSrc             The node
   * @param aState           The state of the flag
   * @param aStateFlag       The flag
   */
  nsresult AttachInheritance(nsXFormsMDGSet* aSet, nsIDOMNode* aSrc, PRBool aState, PRUint16 aStateFlag);

  /**
   * Invalidate the information, ie. mark all nodes as dirty.
   */
  nsresult Invalidate();
  
  /**
   * Get flag value for node and clear flag.
   * 
   * @param aDomNode         The node
   * @param aFlag            The flag
   * @return                 The flag value
   */
  PRBool TestAndClear(nsIDOMNode* aDomNode, PRUint16 aFlag);

  /**
   * Get flag value for node and set flag.
   * 
   * @param aDomNode         The node
   * @param aFlag            The flag
   * @return                 The flag value
   */
  PRBool TestAndSet(nsIDOMNode* aDomNode, PRUint16 aFlag);

  /**
   * Get flag value for node
   * 
   * @param aDomNode         The node
   * @param aFlag            The flag
   * @return                 The flag value
   */
  PRBool Test(nsIDOMNode* aDomNode, PRUint16 aFlag);

public:
  /**
   * Constructor
   */
  nsXFormsMDGEngine();
  
  /**
   * Destructor
   */
  ~nsXFormsMDGEngine();
  
  /**
   * Initializes the internal structures. Needs to be called before class is used!
   */
  nsresult Init();

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
  /* void addMIP (in long aType, in nsIDOMXPathExpression aExpression, in nsXFormsMDGSet aDependencies, in boolean aDynFunc, in nsIDOMNode aContextNode, in long aContextPos, in long aContextSize); */
  nsresult AddMIP(PRInt32 aType, nsIDOMXPathExpression *aExpression, nsXFormsMDGSet *aDependencies, PRBool aDynFunc, nsIDOMNode *aContextNode, PRInt32 aContextPos, PRInt32 aContextSize);

  /**
   * Recalculate the MDG.
   * 
   * @param aChangedNodes    Returns the nodes that was changed during recalculation.
   */
  nsresult Recalculate(nsXFormsMDGSet **aChangedNodes);

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
   * Set the value of a node. (used by nsXFormsMDG)

   * @param aContextNode     The node to set the value for
   * @param aNodeValue       The value
   * @param aMarkNode        Whether to mark node as changed
   * @param aNodeChanged     Was node changed?
   */
  nsresult SetNodeValue(nsIDOMNode *aContextNode, const nsAString & aNodeValue, PRBool aMarkNode, PRBool *aNodeChanged);

  /**
   * Get the value of a node. (used by nsXFormsMDG)

   * @param aContextNode     The node to get the value for
   * @param aNodeValue       The value of the node
   */
  nsresult GetNodeValue(nsIDOMNode *aContextNode, nsAString & aNodeValue);

  PRBool IsConstraint(nsIDOMNode *aContextNode);

  PRBool IsValid(nsIDOMNode *aContextNode);

  PRBool ShouldDispatchValid(nsIDOMNode *aContextNode);

  PRBool IsReadonly(nsIDOMNode *aContextNode);

  PRBool ShouldDispatchReadonly(nsIDOMNode *aContextNode);

  PRBool IsRelevant(nsIDOMNode *aContextNode);

  PRBool ShouldDispatchRelevant(nsIDOMNode *aContextNode);

  PRBool IsRequired(nsIDOMNode *aContextNode);

  PRBool ShouldDispatchRequired(nsIDOMNode *aContextNode);

  PRBool ShouldDispatchValueChanged(nsIDOMNode *aContextNode);
};

#endif
