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

#include "nsString.h"
#include "nsIDOMNode.h"
#include "nsIDOMXPathExpression.h"
#include "nsXFormsMDGEngine.h"

/**
 * Class nsXFormsMDG.
 * 
 * This a wrapper of the more low-level nsXFormsMDGEngine. It handles all the Multi
 * Dependency Graph (MDG) functions for the Mozilla XForms implementation.
 */
class nsXFormsMDG {
protected:
  /**
   * The MDG engine used.
   */
  nsXFormsMDGEngine mEngine;
  
  /**
   * Inserts a new text child for aContextNode.
   * 
   * @param aContextNode     The node to create a child for
   * @param aNodeValue       The value of the new node
   * @param aBeforeNode      If non-null, insert new node before this node
   */
  nsresult CreateNewChild(nsIDOMNode* aContextNode, const nsAString& aNodeValue, nsIDOMNode* aBeforeNode = nsnull);
  
public:
  /**
   * Constructor. Be sure to call Init() before using the object.
   */
  nsXFormsMDG();
  
  /**
   * Destructor.
   */
  ~nsXFormsMDG();

  /**
   * Initializes internal objects. If it fails, the object is unusable.
   */
  nsresult Init();

  /**
   * Insert new MIP (Model Item Property) into graph.
   * 
   * @param aType            The type of MIP
   * @param aExpression      The XPath expression
   * @param aDeps            Set of nodes expression depends on
   * @param aDynFunc         True if expression uses dynamic functions
   * @param aContextNode     The context node for aExpression
   * @param aContextPos      The context positions of aExpression
   * @param aContextSize     The context size for aExpression
   */
  nsresult AddMIP(ModelItemPropName aType, nsIDOMXPathExpression* aExpression,
                  nsXFormsMDGSet* aDeps, PRBool aDynFunc,
                  nsIDOMNode* aContextNode,
                  PRInt32 aContextPos, PRInt32 aContextSize);

  /**
   * Recalculate the MDG.
   * 
   * @param aChangedNodes    Returns the nodes that was changed during recalculation.
   */
  nsresult Recalculate(nsXFormsMDGSet& aChangedNodes);
  
  /**
   * Rebuilds the MDG.
   */
  nsresult Rebuild();
  
  /**
   * Clears all information in the MDG.
   */
  void Clear();

  /**
   * Clears all Dispatch flags.
   */
  void ClearDispatchFlags();
  
  /**
   * Mark a node as changed.
   * 
   * @param aContextNode     The node to be marked.
   */
  nsresult MarkNodeAsChanged(nsIDOMNode* aContextNode);
  
  /**
   * Set the value of a node. (used by nsXFormsMDG)

   * @param aContextNode     The node to set the value for
   * @param aNodeValue       The value
   * @param aMarkNode        Mark node as changed?
   */
  nsresult SetNodeValue(nsIDOMNode* aContextNode, nsAString& aNodeValue, PRBool aMarkNode = PR_FALSE);

  /**
   * Get the value of a node. (used by nsXFormsMDG)

   * @param aContextNode     The node to get the value for
   * @param aNodeValue       The value of the node
   */
  nsresult GetNodeValue(nsIDOMNode* aContextNode, nsAString& aNodeValue);
  
  PRBool IsConstraint(nsIDOMNode* aContextNode);
  PRBool IsValid(nsIDOMNode* aContextNode);
  PRBool ShouldDispatchValid(nsIDOMNode* aContextNode);
  PRBool IsReadonly(nsIDOMNode* aContextNode);
  PRBool ShouldDispatchReadonly(nsIDOMNode* aContextNode);
  PRBool IsRelevant(nsIDOMNode* aContextNode);
  PRBool ShouldDispatchRelevant(nsIDOMNode* aContextNode);
  PRBool IsRequired(nsIDOMNode* aContextNode);
  PRBool ShouldDispatchRequired(nsIDOMNode* aContextNode);
  PRBool ShouldDispatchValueChanged(nsIDOMNode* aContextNode);
};
