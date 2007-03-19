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

#include "nsXFormsMDGEngine.h"

#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMText.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsIDOMXPathResult.h"
#include "nsDeque.h"
#include "nsXFormsUtils.h"
#include "nsDOMError.h"
#include "nsIDOMElement.h"
#include "nsXFormsModelElement.h"

#ifdef DEBUG
//#  define DEBUG_XF_MDG
  const char* gMIPNames[] = {"type",
                             "r/o",
                             "req",
                             "rel",
                             "calc",
                             "const",
                             "p3ptype"
  };
#endif

/* ------------------------------------ */
/* --------- nsXFormsMDGNode ---------- */
/* ------------------------------------ */

nsXFormsMDGNode::nsXFormsMDGNode(nsIDOMNode             *aNode,
                                 const ModelItemPropName aType)
  : mDirty (PR_TRUE), mHasExpr(PR_FALSE), mContextNode(aNode),
    mCount(0), mType(aType), mContextSize(0), mContextPosition(0),
    mDynFunc(PR_FALSE), mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsXFormsMDGNode);
}

nsXFormsMDGNode::~nsXFormsMDGNode()
{
  MOZ_COUNT_DTOR(nsXFormsMDGNode);
}

void
nsXFormsMDGNode::SetExpression(nsIDOMNSXPathExpression *aExpression,
                               PRBool                   aDynFunc,
                               PRInt32                  aContextPosition,
                               PRInt32                  aContextSize)
{
  mHasExpr = PR_TRUE;
  mDynFunc = aDynFunc;
  mExpression = aExpression;
  mContextPosition = aContextPosition;
  mContextSize = aContextSize;
}

PRBool
nsXFormsMDGNode::HasExpr() const
{
  return mHasExpr;
}

PRBool
nsXFormsMDGNode::IsDirty() const
{
  // A node is always dirty, if it depends on a dynamic function.
  return mDirty || mDynFunc;
}

void
nsXFormsMDGNode::MarkDirty()
{
  mDirty = PR_TRUE;
}

void
nsXFormsMDGNode::MarkClean()
{
  mDirty = PR_FALSE;
}


/* ------------------------------------ */
/* -------- nsXFormsMDGEngine --------- */
/* ------------------------------------ */

nsXFormsMDGEngine::nsXFormsMDGEngine()
: mNodesInGraph(0)
{
  MOZ_COUNT_CTOR(nsXFormsMDGEngine);
}

nsXFormsMDGEngine::~nsXFormsMDGEngine()
{
  mNodeToMDG.Enumerate(DeleteLinkedNodes, nsnull);

  MOZ_COUNT_DTOR(nsXFormsMDGEngine);
}

nsresult
nsXFormsMDGEngine::Init(nsXFormsModelElement *aModel)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (mNodeStates.Init() && mNodeToMDG.Init()) {
    rv = NS_OK;
  }

  mModel = aModel;

  return rv;
}

nsresult
nsXFormsMDGEngine::AddMIP(ModelItemPropName         aType,
                          nsIDOMNSXPathExpression  *aExpression,
                          nsCOMArray<nsIDOMNode>   *aDependencies,
                          PRBool                    aDynFunc,
                          nsIDOMNode               *aContextNode,
                          PRInt32                   aContextPos,
                          PRInt32                   aContextSize)
{
  NS_ENSURE_ARG(aContextNode);
  
#ifdef DEBUG_XF_MDG
  nsAutoString nodename;
  aContextNode->GetNodeName(nodename);
  printf("nsXFormsMDGEngine::AddMIP(aContextNode=%s, aExpression=%p, aDependencies=|%d|,\n",
         NS_ConvertUTF16toUTF8(nodename).get(),
         (void*) aExpression,
         aDependencies ? aDependencies->Count() : 0);
  printf("                          aContextPos=%d, aContextSize=%d, aType=%s, aDynFunc=%d)\n",
         aContextPos, aContextSize, gMIPNames[aType], aDynFunc);
#endif
  nsXFormsMDGNode* newnode = GetNode(aContextNode,
                                     aType,
                                     PR_TRUE);
  
  if (!newnode) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  if (newnode->HasExpr()) {
    // MIP already in the graph. That is, there is already a MIP of the same
    // type for this node, which is illegal.
    // Example: <bind nodeset="x" required="true()" required="false()"/>
    return NS_ERROR_ABORT;
  }

  if (aExpression) {
    newnode->SetExpression(aExpression, aDynFunc, aContextPos, aContextSize);
  }
  
  // Add dependencies
  if (aDependencies) {
    nsCOMPtr<nsIDOMNode> dep_domnode;
    nsXFormsMDGNode* dep_gnode;
    for (PRInt32 i = 0; i < aDependencies->Count(); ++i) {
      dep_domnode = aDependencies->ObjectAt(i);
      if (!dep_domnode) {
        return NS_ERROR_NULL_POINTER;
      }

#ifdef DEBUG_XF_MDG
      printf("\tDependency #%2d: %p\n", i, (void*) dep_domnode);
#endif
      
      // Get calculate graph node for the dependency (only a calculate
      // property can influence another MIP).
      dep_gnode = GetNode(dep_domnode, eModel_calculate);
      if (!dep_gnode) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
  
      if (dep_gnode->mType == aType && dep_gnode->mContextNode == aContextNode) {
        // Reference to itself, ignore
        continue;
      }
  
      // Add this node as a successor to the dependency (ie. the dependency
      // should be (re-)calculated before this node)
      dep_gnode->mSuc.AppendElement(newnode);
      newnode->mCount++;
    }
  }

  return NS_OK;
}

nsresult
nsXFormsMDGEngine::MarkNodeAsChanged(nsIDOMNode* aContextNode)
{
  return mMarkedNodes.AppendObject(aContextNode);
}

nsresult
nsXFormsMDGEngine::HandleMarkedNodes(nsCOMArray<nsIDOMNode> *aArray)
{
  NS_ENSURE_ARG_POINTER(aArray);

  // Handle nodes marked as changed
  for (PRInt32 i = 0; i < mMarkedNodes.Count(); ++i) {
    nsCOMPtr<nsIDOMNode> node = mMarkedNodes.ObjectAt(i);
    nsXFormsNodeState* ns = GetNCNodeState(node);
    NS_ENSURE_TRUE(ns, NS_ERROR_FAILURE);

    ns->Set(kFlags_ALL_DISPATCH, PR_TRUE);

    // Get the node, eMode_type == get any type of node
    nsXFormsMDGNode* n = GetNode(node, eModel_type, PR_FALSE);
    if (n) {
      while (n) {
        n->MarkDirty();
        n = n->mNext;
      }
    } else {
      // Add constraint to trigger validation of node
      n = GetNode(node, eModel_constraint, PR_TRUE);
      if (!n) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      n->MarkDirty();
      NS_ENSURE_TRUE(mGraph.AppendElement(n), NS_ERROR_OUT_OF_MEMORY);
    }

    NS_ENSURE_TRUE(aArray->AppendObjects(mMarkedNodes),
                   NS_ERROR_OUT_OF_MEMORY);

  }

  mMarkedNodes.Clear();

  return NS_OK;
}


#ifdef DEBUG_beaufour
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void
nsXFormsMDGEngine::PrintDot(const char* aFile)
{
  FILE *FD = stdout;
  if (aFile) {
    FD = fopen(aFile, "w");
  }
  fprintf(FD, "digraph {\n");
  for (PRInt32 i = 0; i < mGraph.Count(); ++i) {
    nsXFormsMDGNode* g = NS_STATIC_CAST(nsXFormsMDGNode*, mGraph[i]);
    if (g) {
      nsAutoString domNodeName;
      g->mContextNode->GetNodeName(domNodeName);
      
      if (g->IsDirty()) {
        fprintf(FD, "\t%s [color=red];\n",
                NS_ConvertUTF16toUTF8(domNodeName).get());
      }

      for (PRInt32 j = 0; j < g->mSuc.Count(); ++j) {
        nsXFormsMDGNode* sucnode = NS_STATIC_CAST(nsXFormsMDGNode*,
                                                  g->mSuc[j]);
        if (sucnode) {
          nsAutoString sucName;
          sucnode->mContextNode->GetNodeName(sucName);
          fprintf(FD, "\t%s -> %s [label=\"%s\"];\n",
                  NS_ConvertUTF16toUTF8(sucName).get(),
                  NS_ConvertUTF16toUTF8(domNodeName).get(),
                  gMIPNames[sucnode->mType]);
        }
      }
    }
  }
  fprintf(FD, "}\n");
  if (FD) {
    fclose(FD);
  }
}
#endif

nsresult
nsXFormsMDGEngine::Recalculate(nsCOMArray<nsIDOMNode> *aChangedNodes)
{
  NS_ENSURE_ARG(aChangedNodes);

#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::Recalculcate(aChangedNodes=|%d|)\n",
         aChangedNodes->Count());
#endif

  // XXX: There's something wrong with the marking of nodes, as we assume that
  // recalculate will always be called first. bug 338146
  nsresult rv = HandleMarkedNodes(aChangedNodes);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool res = PR_TRUE;

  mFirstCalculate = mJustRebuilt;

#ifdef DEBUG_XF_MDG
  printf("\taChangedNodes: %d\n", aChangedNodes->Count());
  printf("\tmNodeToMDG:    %d\n", mNodeToMDG.Count());
  printf("\tmNodeStates:   %d\n", mNodeStates.Count());
  printf("\tGraph nodes:   %d\n", mGraph.Count());
#endif
  
  // Go through all dirty nodes in the graph
  nsXFormsMDGNode* g;
  for (PRInt32 i = 0; i < mGraph.Count(); ++i) {
    g = NS_STATIC_CAST(nsXFormsMDGNode*, mGraph[i]);

    if (!g) {
      NS_WARNING("nsXFormsMDGEngine::Calculcate(): Empty node in graph!!!");
      continue;
    }

    NS_ASSERTION(g->mCount == 0,
                 "nsXFormsMDGEngine::Calculcate(): Graph node with mCount != 0");

#ifdef DEBUG_XF_MDG
    nsAutoString domNodeName;
    g->mContextNode->GetNodeName(domNodeName);

    printf("\tNode #%d: This=%p, Dirty=%d, DynFunc=%d, Type=%d, Count=%d, Suc=%d, CSize=%d, CPos=%d, Next=%p, domnode=%s\n",
           i, (void*) g, g->IsDirty(), g->mDynFunc, g->mType,
           g->mCount, g->mSuc.Count(), g->mContextSize, g->mContextPosition,
           (void*) g->mNext, NS_ConvertUTF16toUTF8(domNodeName).get());
#endif

    // Ignore node if it is not dirty
    if (!g->IsDirty()) {
      continue;
    }

    nsXFormsNodeState* ns = GetNCNodeState(g->mContextNode);
    NS_ENSURE_TRUE(ns, NS_ERROR_FAILURE);
    
    PRBool constraint = PR_TRUE;
    PRBool conChanged;
    // Find MIP-type and handle it accordingly
    switch (g->mType) {
    case eModel_calculate:
      if (g->HasExpr()) {
        nsCOMPtr<nsISupports> result;
        rv = g->mExpression->EvaluateWithContext(g->mContextNode,
                                                 g->mContextPosition,
                                                 g->mContextSize,
                                                 nsIDOMXPathResult::STRING_TYPE,
                                                 nsnull,
                                                 getter_AddRefs(result));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIDOMXPathResult> xpath_res = do_QueryInterface(result);
        NS_ENSURE_STATE(xpath_res);
        
        nsAutoString nodeval;
        rv = xpath_res->GetStringValue(nodeval);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = SetNodeValueInternal(g->mContextNode,
                                  nodeval,
                                  PR_FALSE,
                                  PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          NS_ENSURE_TRUE(aChangedNodes->AppendObject(g->mContextNode),
                         NS_ERROR_FAILURE);
        }
      }

      ns->Set(eFlag_DISPATCH_VALUE_CHANGED, PR_TRUE);
      break;
      
    case eModel_constraint:
      if (g->HasExpr()) {
        rv = BooleanExpression(g, constraint);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      conChanged = ns->IsConstraint() != constraint;
        // On the first calculate after a rebuild (mFirstCalculate) we also
        // add constraints to the set of changed nodes to trigger validation
        // of type information if present.
      if (conChanged || mFirstCalculate) {
        if (conChanged) {
          ns->Set(eFlag_CONSTRAINT, constraint);
          ns->Set(eFlag_DISPATCH_VALID_CHANGED, PR_TRUE);
        }
        NS_ENSURE_TRUE(aChangedNodes->AppendObject(g->mContextNode),
                       NS_ERROR_FAILURE);
      }

      break;

    case eModel_readonly:
      if (g->HasExpr()) {
        rv = ComputeMIPWithInheritance(eFlag_READONLY,
                                       eFlag_DISPATCH_READONLY_CHANGED,
                                       eFlag_INHERITED_READONLY,
                                       g,
                                       aChangedNodes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
      
    case eModel_relevant:
      if (g->HasExpr()) {
        rv = ComputeMIPWithInheritance(eFlag_RELEVANT,
                                       eFlag_DISPATCH_RELEVANT_CHANGED,
                                       eFlag_INHERITED_RELEVANT,
                                       g,
                                       aChangedNodes);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
      
    case eModel_required:
      if (g->HasExpr()) {
        PRBool didChange;
        rv = ComputeMIP(eFlag_REQUIRED,
                        eFlag_DISPATCH_REQUIRED_CHANGED,
                        g,
                        didChange);
        NS_ENSURE_SUCCESS(rv, rv);
      
        if (didChange) {
          NS_ENSURE_TRUE(aChangedNodes->AppendObject(g->mContextNode),
                         NS_ERROR_FAILURE);
        }
      }
      break;
      
    default:
      NS_ERROR("There was no expression which matched\n");
      res = PR_FALSE;
      break;
    }

    // Mark successors dirty
    nsXFormsMDGNode* sucnode;
    for (PRInt32 j = 0; j < g->mSuc.Count(); ++j) {
      sucnode = NS_STATIC_CAST(nsXFormsMDGNode*, g->mSuc[j]);
      if (!sucnode) {
        NS_ERROR("nsXFormsMDGEngine::Calculate(): Node has NULL successor!");
        return NS_ERROR_FAILURE;
      }
      sucnode->MarkDirty();
    }

    g->MarkClean();
  }
  nsXFormsUtils::MakeUniqueAndSort(aChangedNodes);
  
#ifdef DEBUG_XF_MDG
  printf("\taChangedNodes: %d\n", aChangedNodes->Count());
  printf("\tmNodeToMDG:    %d\n", mNodeToMDG.Count());
  printf("\tmNodeStates:   %d\n", mNodeStates.Count());
  printf("\tGraph nodes:   %d\n", mGraph.Count());
#endif

  return res;
}

nsresult
nsXFormsMDGEngine::Revalidate(nsCOMArray<nsIDOMNode> *aNodes)
{
  NS_ENSURE_ARG(aNodes);
  NS_ENSURE_STATE(mModel);

  for (PRInt32 i = 0; i < aNodes->Count(); ++i) {
    nsCOMPtr<nsIDOMNode> node = aNodes->ObjectAt(i);
    nsXFormsNodeState* ns = GetNCNodeState(node);
    PRBool constraint;
    mModel->ValidateNode(node, &constraint);
    if (constraint != ns->IsConstraintSchema()) {
      ns->Set(eFlag_CONSTRAINT_SCHEMA, constraint);
      ns->Set(eFlag_DISPATCH_VALID_CHANGED, PR_TRUE);
    }
  }

  return NS_OK;
}

nsresult
nsXFormsMDGEngine::Rebuild()
{
#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::Rebuild()\n");
#endif
  nsresult rv = NS_OK;
  mJustRebuilt = PR_TRUE;
  mFirstCalculate = PR_FALSE;

  mGraph.Clear();
  mNodeStates.Clear();

  nsDeque sortedNodes(nsnull);

#ifdef DEBUG_XF_MDG
  printf("\tmNodesInGraph: %d\n", mNodesInGraph);
  printf("\tmNodeToMDG:    %d\n", mNodeToMDG.Count());
  printf("\tmNodeStates:   %d\n", mNodeStates.Count());
#endif

  // Initial scan for nsXFormsMDGNodes with no dependencies (mCount == 0)
  PRUint32 entries = mNodeToMDG.EnumerateRead(AddStartNodes, &sortedNodes);
  if (entries != mNodeToMDG.Count()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

#ifdef DEBUG_XF_MDG
  printf("\tStartNodes:    %d\n", sortedNodes.GetSize());
#endif
  
  
  nsXFormsMDGNode* node;
  while ((node = NS_STATIC_CAST(nsXFormsMDGNode*, sortedNodes.Pop()))) {
    for (PRInt32 i = 0; i < node->mSuc.Count(); ++i) {
      nsXFormsMDGNode* sucNode = NS_STATIC_CAST(nsXFormsMDGNode*,
                                                node->mSuc[i]);
      NS_ASSERTION(sucNode, "XForms: NULL successor node");

      sucNode->mCount--;
      if (sucNode->mCount == 0) {
        sortedNodes.Push(sucNode);
      }
    }

    node->MarkDirty();

    if (!mGraph.AppendElement(node)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

#ifdef DEBUG_XF_MDG
    printf("\tmGraph.Count() = %2d\n", mGraph.Count());
    printf("\tmNodesInGraph  = %2d\n", mNodesInGraph);
#endif

  if (mGraph.Count() != mNodesInGraph) {
    nsCOMPtr<nsIDOMElement> modelElement;
    if (mModel) {
      modelElement = mModel->GetDOMElement();
    }
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("MDGLoopError"), modelElement);
    rv = NS_ERROR_ABORT;
  }

  mNodesInGraph = 0;

  return rv;
}

nsresult
nsXFormsMDGEngine::ClearDispatchFlags()
{
  mJustRebuilt = PR_FALSE;
  return AndFlags(kFlags_NOT_DISPATCH) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsXFormsMDGEngine::Clear() {
#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::Clear()\n");
#endif

  nsresult rv = mNodeToMDG.Enumerate(DeleteLinkedNodes, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  mNodeToMDG.Clear();

  mNodeStates.Clear();
  
  mGraph.Clear();
  
  mNodesInGraph = 0;

  return NS_OK;
}

nsresult
nsXFormsMDGEngine::SetNodeValue(nsIDOMNode       *aContextNode,
                                const nsAString  &aNodeValue,
                                PRBool           *aNodeChanged)
{
  return SetNodeValueInternal(aContextNode,
                              aNodeValue,
                              PR_TRUE,
                              PR_FALSE,
                              aNodeChanged);
}

nsresult
nsXFormsMDGEngine::SetNodeValueInternal(nsIDOMNode       *aContextNode,
                                        const nsAString  &aNodeValue,
                                        PRBool            aMarkNode,
                                        PRBool            aIsCalculate,
                                        PRBool           *aNodeChanged)
{
  if (aNodeChanged) {
    *aNodeChanged = PR_FALSE;
  }

  const nsXFormsNodeState* ns = GetNodeState(aContextNode);
  NS_ENSURE_TRUE(ns, NS_ERROR_FAILURE);

  // If the node is read-only and not set by a @calculate MIP,
  // ignore the call
  if (ns->IsReadonly() && !aIsCalculate) {
    ///
    /// @todo Better feedback for readonly nodes? (XXX)
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> childNode;
  PRUint16 nodeType;
  nsresult rv = aContextNode->GetNodeType(&nodeType);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString oldValue;
  nsXFormsUtils::GetNodeValue(aContextNode, oldValue);
  if (oldValue.Equals(aNodeValue)) {
    return NS_OK;
  }

  switch(nodeType) {
  case nsIDOMNode::ATTRIBUTE_NODE:
  case nsIDOMNode::TEXT_NODE:
  case nsIDOMNode::CDATA_SECTION_NODE:
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
  case nsIDOMNode::COMMENT_NODE:
    rv = aContextNode->SetNodeValue(aNodeValue);
    NS_ENSURE_SUCCESS(rv, rv);

    break;

  case nsIDOMNode::ELEMENT_NODE:

    rv = aContextNode->GetFirstChild(getter_AddRefs(childNode));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!childNode) {
      rv = CreateNewChild(aContextNode, aNodeValue);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      PRUint16 childType;
      rv = childNode->GetNodeType(&childType);
      NS_ENSURE_SUCCESS(rv, rv);

      if (childType == nsIDOMNode::TEXT_NODE ||
          childType == nsIDOMNode::CDATA_SECTION_NODE) {
        rv = childNode->SetNodeValue(aNodeValue);
        NS_ENSURE_SUCCESS(rv, rv);

        // Remove all leading text child nodes except first one (see
        // nsXFormsUtils::GetNodeValue method for motivation).
        nsCOMPtr<nsIDOMNode> siblingNode;
        while (true) {
          rv = childNode->GetNextSibling(getter_AddRefs(siblingNode));
          NS_ENSURE_SUCCESS(rv, rv);
          if (!siblingNode)
            break;

          rv = siblingNode->GetNodeType(&childType);
          NS_ENSURE_SUCCESS(rv, rv);
          if (childType != nsIDOMNode::TEXT_NODE &&
              childType != nsIDOMNode::CDATA_SECTION_NODE) {
            break;
          }
          nsCOMPtr<nsIDOMNode> stubNode;
          rv = aContextNode->RemoveChild(siblingNode,
                                         getter_AddRefs(stubNode));
          NS_ENSURE_SUCCESS(rv, rv);
        }
      } else {
        // Not a text child, create a new one
        rv = CreateNewChild(aContextNode, aNodeValue, childNode);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    break;

  default:
    /// Unsupported nodeType
    /// @todo Should return more specific error? (XXX)
    return NS_ERROR_ILLEGAL_VALUE;
    break;
  }
  
  // NB: Never reached for Readonly nodes.  
  if (aNodeChanged) {
    *aNodeChanged = PR_TRUE;
  }
  if (aMarkNode) {
      MarkNodeAsChanged(aContextNode);
  }
  
  return NS_OK;
}

nsresult
nsXFormsMDGEngine::SetNodeContent(nsIDOMNode       *aContextNode,
                                  nsIDOMNode       *aContentEnvelope)
{
  NS_ENSURE_ARG(aContextNode);
  NS_ENSURE_ARG(aContentEnvelope);

  // ok, this is tricky.  This function will REPLACE the contents of
  // aContextNode with the a clone of the contents of aContentEnvelope.  If
  // aContentEnvelope has no contents, then any contents that aContextNode
  // has will still be removed.  

  const nsXFormsNodeState* ns = GetNodeState(aContextNode);
  NS_ENSURE_TRUE(ns, NS_ERROR_FAILURE);

  // If the node is read-only and not set by a @calculate MIP,
  // ignore the call
  if (ns->IsReadonly()) {
    ///
    /// @todo Better feedback for readonly nodes? (XXX)
    return NS_OK;
  }

  PRUint16 nodeType;
  nsresult rv = aContextNode->GetNodeType(&nodeType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nodeType != nsIDOMNode::ELEMENT_NODE) {
    // got to return something pretty unique that we can check down the road in
    // order to dispatch any error events
    return NS_ERROR_DOM_WRONG_TYPE_ERR;
  }

  // Need to determine if the contents of the context node and content envelope
  // are already the same.  If so, we can avoid some unnecessary work.

  PRBool hasChildren1, hasChildren2, contentsEqual = PR_FALSE;
  nsresult rv1 = aContextNode->HasChildNodes(&hasChildren1);
  nsresult rv2 = aContentEnvelope->HasChildNodes(&hasChildren2);
  if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2) && hasChildren1 == hasChildren2) {
    // First test passed.  Both have the same number of children nodes.
    if (hasChildren1) {
      nsCOMPtr<nsIDOMNodeList> children1, children2;
      PRUint32 childrenLength1, childrenLength2;
  
      rv1 = aContextNode->GetChildNodes(getter_AddRefs(children1));
      rv2 = aContentEnvelope->GetChildNodes(getter_AddRefs(children2));

      if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2) && children1 && children2) {

        // Both have child nodes.
        rv1 = children1->GetLength(&childrenLength1);
        rv2 = children2->GetLength(&childrenLength2);
        if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2) && 
            (childrenLength1 == childrenLength2)) {

          // both have the same number of child nodes.  Now checking to see if
          // each of the children are equal.
          for (PRUint32 i = 0; i < childrenLength1; ++i) {
            nsCOMPtr<nsIDOMNode> child1, child2;
      
            rv1 = children1->Item(i, getter_AddRefs(child1));
            rv2 = children2->Item(i, getter_AddRefs(child2));
            if (NS_FAILED(rv1) || NS_FAILED(rv2)) {
              // Unexpected error.  Not as many children in the list as we
              // were told.
              return NS_ERROR_UNEXPECTED;
            }
      
            contentsEqual = nsXFormsUtils::AreNodesEqual(child1, child2, PR_TRUE);
            if (!contentsEqual) {
              break;
            }
          }
        }
      }
    } else {
      // neither have children
      contentsEqual = PR_TRUE;
    }
  }

  if (contentsEqual) {
    return NS_OK;
  }

  // remove any child nodes that aContextNode already contains
  nsCOMPtr<nsIDOMNode> resultNode;
  nsCOMPtr<nsIDOMNodeList> childList;
  rv = aContextNode->GetChildNodes(getter_AddRefs(childList));
  NS_ENSURE_SUCCESS(rv, rv);
  if (childList) {
    PRUint32 length;
    rv = childList->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRInt32 i = length-1; i >= 0; i--) {
      nsCOMPtr<nsIDOMNode> childNode;
      rv = childList->Item(i, getter_AddRefs(childNode));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aContextNode->RemoveChild(childNode, getter_AddRefs(resultNode));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // add contents of the envelope under aContextNode
  nsCOMPtr<nsIDOMNode> childNode;
  rv = aContentEnvelope->GetFirstChild(getter_AddRefs(childNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> document;
  rv = aContextNode->GetOwnerDocument(getter_AddRefs(document));
  NS_ENSURE_STATE(document);

  while (childNode) {
    nsCOMPtr<nsIDOMNode> importedNode;
    rv = document->ImportNode(childNode, PR_TRUE, getter_AddRefs(importedNode));
    NS_ENSURE_STATE(importedNode);
    rv = aContextNode->AppendChild(importedNode, getter_AddRefs(resultNode));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = childNode->GetNextSibling(getter_AddRefs(resultNode));
    NS_ENSURE_SUCCESS(rv, rv);

    resultNode.swap(childNode);
  }

  // We already know that the contents have changed.  Mark the node so that
  // a xforms-value-changed can be dispatched.
  MarkNodeAsChanged(aContextNode);

  return NS_OK;
}

const nsXFormsNodeState*
nsXFormsMDGEngine::GetNodeState(nsIDOMNode *aContextNode)
{
  return GetNCNodeState(aContextNode);
}

/**********************************************/
/*              Private functions             */
/**********************************************/
nsXFormsNodeState*
nsXFormsMDGEngine::GetNCNodeState(nsIDOMNode *aContextNode)
{
  nsXFormsNodeState* ns = nsnull;

  if (aContextNode && !mNodeStates.Get(aContextNode, &ns)) {
    ns = new nsXFormsNodeState(kFlags_DEFAULT |
                               ((mJustRebuilt && mFirstCalculate) ? kFlags_INITIAL_DISPATCH : 0));
    NS_ASSERTION(ns, "Could not create new nsXFormsNodeState");

    if (!mNodeStates.Put(aContextNode, ns)) {
      NS_ERROR("Could not insert new nsXFormsNodeState");
      delete ns;
      return nsnull;
    }    
    aContextNode->AddRef();

    // Do an initial type check, and set the validity state
    PRBool constraint;
    mModel->ValidateNode(aContextNode, &constraint);
    ns->Set(eFlag_CONSTRAINT_SCHEMA, constraint);
  }
  return ns;
}

nsresult
nsXFormsMDGEngine::CreateNewChild(nsIDOMNode      *aContextNode,
                                  const nsAString &aNodeValue,
                                  nsIDOMNode      *aBeforeNode)
{
  nsresult rv;

  nsCOMPtr<nsIDOMDocument> document;
  rv = aContextNode->GetOwnerDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMText> textNode;
  rv = document->CreateTextNode(aNodeValue, getter_AddRefs(textNode));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMNode> newNode;
  if (aBeforeNode) {
    rv = aContextNode->InsertBefore(textNode,
                                    aBeforeNode,
                                    getter_AddRefs(newNode));
  } else {
    rv = aContextNode->AppendChild(textNode,
                                   getter_AddRefs(newNode));
  }

  return rv;
}

PLDHashOperator
nsXFormsMDGEngine::DeleteLinkedNodes(nsISupports                *aKey,
                                     nsAutoPtr<nsXFormsMDGNode> &aNode,
                                     void                       *aArg)
{
  if (!aNode) {
    NS_WARNING("nsXFormsMDGEngine::DeleteLinkedNodes() called with aNode == nsnull!");
    return PL_DHASH_STOP;
  }
  nsXFormsMDGNode* temp;
  nsXFormsMDGNode* next = aNode->mNext;
  
  while (next) {
    temp = next;  
    next = next->mNext;
    delete temp;
  }

  return PL_DHASH_NEXT;
}

nsXFormsMDGNode*
nsXFormsMDGEngine::GetNode(nsIDOMNode       *aDomNode,
                           ModelItemPropName aType,
                           PRBool            aCreate)
{
  nsIDOMNode* nodeKey = aDomNode;
  nsXFormsMDGNode* nd = nsnull;

#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::GetNode(aDomNode=%p, aType=%s, aCreate=%d)\n",
         (void*) nodeKey, gMIPNames[aType], aCreate);
#endif
  

  // Find correct type
  if (mNodeToMDG.Get(nodeKey, &nd) && aType != eModel_type) {
    while (nd && aType != nd->mType) {
      nd = nd->mNext;
    }
  } 
  
  // Eventually create node
  if (!nd && aCreate){
    nd = new nsXFormsMDGNode(nodeKey, aType);
    if (!nd) {
      NS_ERROR("Could not allocate room for new nsXFormsMDGNode");
      return nsnull;
    }
    
#ifdef DEBUG_XF_MDG
    printf("\tNode not found, create new MDGNode '%p'\n", (void*) nd);
#endif
    // Link to existing node
    nsXFormsMDGNode* nd_exists;
    if (mNodeToMDG.Get(nodeKey, &nd_exists)) {
      while (nd_exists->mNext) {
        nd_exists = nd_exists->mNext;
      }
#ifdef DEBUG_XF_MDG
      printf("\tLinking to existing MDGNode '%p'\n", (void*) nd_exists);
#endif
      nd_exists->mNext = nd;
    } else {
      if (!mNodeToMDG.Put(nodeKey, nd)) {
        delete nd;
        NS_ERROR("Could not insert new node in HashTable!");
        return nsnull;
      }
      nodeKey->AddRef();
    }

    mNodesInGraph++;
  }
  return nd;
}

PLDHashOperator 
nsXFormsMDGEngine::AddStartNodes(nsISupports     *aKey,
                                 nsXFormsMDGNode *aNode,
                                 void            *aDeque)
{
#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::AddStartNodes(aKey=n/a, aNode=%p, aDeque=%p)\n",
         (void*) aNode,
         aDeque);
#endif

  nsDeque* deque = NS_STATIC_CAST(nsDeque*, aDeque);
  if (!deque) {
    NS_ERROR("nsXFormsMDGEngine::AddStartNodes called with NULL aDeque");
    return PL_DHASH_STOP;
  }
  
  while (aNode) {
    if (aNode->mCount == 0) {
      ///
      /// @todo Is it not possible to check error condition? (XXX)
      deque->Push(aNode);
    }
    aNode = aNode->mNext;
  }
    
  return PL_DHASH_NEXT;
}

PLDHashOperator 
nsXFormsMDGEngine::AndFlag(nsISupports                  *aKey,
                           nsAutoPtr<nsXFormsNodeState> &aState,
                           void                         *aMask)
{
  PRUint16* andMask = NS_STATIC_CAST(PRUint16*, aMask);
  if (!andMask) {
    return PL_DHASH_STOP;
  }
  *aState &= *andMask;
  return PL_DHASH_NEXT;
}

PRBool
nsXFormsMDGEngine::AndFlags(PRUint16 aAndMask)
{
  PRUint32 entries = mNodeStates.Enumerate(AndFlag, &aAndMask);
  return (entries == mNodeStates.Count()) ? PR_TRUE : PR_FALSE;
}


nsresult
nsXFormsMDGEngine::BooleanExpression(nsXFormsMDGNode* aNode, PRBool& state)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_TRUE(aNode->mExpression, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsISupports> retval;
  nsresult rv;

  rv = aNode->mExpression->EvaluateWithContext(aNode->mContextNode,
                                               aNode->mContextPosition,
                                               aNode->mContextSize,
                                               nsIDOMXPathResult::BOOLEAN_TYPE,
                                               nsnull,
                                               getter_AddRefs(retval));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMXPathResult> xpath_res = do_QueryInterface(retval);
  NS_ENSURE_TRUE(xpath_res, NS_ERROR_FAILURE);
  
  rv = xpath_res->GetBooleanValue(&state);
  
  return rv;
}

nsresult
nsXFormsMDGEngine::ComputeMIP(eFlag_t          aStateFlag,
                              eFlag_t          aDispatchFlag,
                              nsXFormsMDGNode *aNode,
                              PRBool          &aDidChange)
{
  NS_ENSURE_ARG(aNode);

  aDidChange = PR_FALSE;

  if (!aNode->mExpression)
    return NS_OK;

  nsXFormsNodeState* ns = GetNCNodeState(aNode->mContextNode);
  NS_ENSURE_TRUE(ns, NS_ERROR_FAILURE);
  
  PRBool state;
  nsresult rv = BooleanExpression(aNode, state);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool cstate = ns->Test(aStateFlag);  
  
  ns->Set(aStateFlag, state);

  aDidChange = (state != cstate) ? PR_TRUE : PR_FALSE;
  if (aDidChange) {
    ns->Set(aDispatchFlag, PR_TRUE);
  }
  
  return NS_OK;
}

nsresult
nsXFormsMDGEngine::ComputeMIPWithInheritance(eFlag_t                aStateFlag,
                                             eFlag_t                aDispatchFlag,
                                             eFlag_t                aInheritanceFlag,
                                             nsXFormsMDGNode        *aNode,
                                             nsCOMArray<nsIDOMNode> *aSet)
{
  nsresult rv;
  PRBool didChange;
  rv = ComputeMIP(aStateFlag, aDispatchFlag, aNode, didChange);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (didChange) {
    nsXFormsNodeState* ns = GetNCNodeState(aNode->mContextNode);
    NS_ENSURE_TRUE(ns, NS_ERROR_FAILURE);
    if (   !(aStateFlag == eFlag_READONLY && ns->Test(aInheritanceFlag))
        ||  (aStateFlag == eFlag_RELEVANT && ns->Test(aInheritanceFlag)) )
    {
      NS_ENSURE_TRUE(aSet->AppendObject(aNode->mContextNode),
                     NS_ERROR_FAILURE);
      rv = AttachInheritance(aSet,
                             aNode->mContextNode,
                             ns->Test(aStateFlag),
                             aStateFlag);
    }
  }

  return rv;
}

nsresult
nsXFormsMDGEngine::AttachInheritance(nsCOMArray<nsIDOMNode> *aSet,
                                     nsIDOMNode             *aSrc,
                                     PRBool                  aState,
                                     eFlag_t                 aStateFlag)
{
  NS_ENSURE_ARG(aSrc);
  
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv;
  PRBool updateNode = PR_FALSE;

  nsCOMPtr<nsIDOMNodeList> childList;
  rv = aSrc->GetChildNodes(getter_AddRefs(childList));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount;
  rv = childList->GetLength(&childCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < childCount; i++) {
    rv = childList->Item(i, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

    nsXFormsNodeState *ns = GetNCNodeState(node);
    NS_ENSURE_TRUE(ns, NS_ERROR_FAILURE);

    PRBool curState = ns->Test(aStateFlag);    

    if (aStateFlag == eFlag_RELEVANT) {
      if (!aState) { // The nodes are getting irrelevant
        if (ns->Test(eFlag_INHERITED_RELEVANT) && curState) {
          ns->Set(eFlag_INHERITED_RELEVANT, PR_FALSE);
          ns->Set(eFlag_DISPATCH_RELEVANT_CHANGED, PR_TRUE);
          updateNode = PR_TRUE;
        }
      } else { // The nodes are becoming relevant
        if (curState) {
          // Relevant has changed from inheritance
          ns->Set(eFlag_DISPATCH_RELEVANT_CHANGED, PR_TRUE);
          ns->Set(eFlag_INHERITED_RELEVANT, PR_TRUE);
          updateNode = PR_TRUE;
        }
      }
    } else if (aStateFlag == eFlag_READONLY) {
      if (aState) { // The nodes are getting readonly
        if (!ns->Test(eFlag_INHERITED_READONLY) && curState == PR_FALSE) {
          ns->Set(eFlag_INHERITED_READONLY | eFlag_DISPATCH_READONLY_CHANGED,
                  PR_TRUE);
          updateNode = PR_TRUE;
        }
      } else { // The nodes are getting readwrite
        if (curState) {
          ns->Set(eFlag_DISPATCH_READONLY_CHANGED, PR_TRUE);
          ns->Set(eFlag_INHERITED_READONLY, PR_FALSE);
          updateNode = PR_TRUE;
        }
      }
    }
    
    if (updateNode) {
      rv = AttachInheritance(aSet, node, aState, aStateFlag);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(aSet->AppendObject(node), NS_ERROR_FAILURE);
      updateNode = PR_FALSE;
    }
  }

  return NS_OK;
}

nsresult
nsXFormsMDGEngine::Invalidate()
{
  nsXFormsMDGNode* g;
  for (PRInt32 i = 0; i < mGraph.Count(); ++i) {
    g = NS_STATIC_CAST(nsXFormsMDGNode*, mGraph[i]);
    NS_ENSURE_TRUE(g, NS_ERROR_FAILURE);
    g->MarkDirty();
  }
  return NS_OK;
}

