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
#include "nsXFormsMDG.h"

#include "nsIDOMNodeList.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMXPathResult.h"
#include "nsDeque.h"

#ifdef DEBUG
// #define DEBUG_XF_MDG
#endif

/* ------------------------------------ */
/* --------- nsXFormsMDGNode ---------- */
/* ------------------------------------ */
MOZ_DECL_CTOR_COUNTER(nsXFormsMDGNode)

nsXFormsMDGNode::nsXFormsMDGNode(nsIDOMNode* aNode, const ModelItemPropName aType)
  : mDirty (PR_TRUE), mHasExpr(PR_FALSE), mContextNode(aNode), mCount(0), mType(aType),
    mContextSize(0), mContextPosition(0), mDynFunc(PR_FALSE), mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsXFormsMDGNode);
}

nsXFormsMDGNode::~nsXFormsMDGNode()
{
  MOZ_COUNT_DTOR(nsXFormsMDGNode);
}

void
nsXFormsMDGNode::SetExpression(nsIDOMXPathExpression* aExpression, PRBool aDynFunc,
                               PRInt32 aContextPosition, PRInt32 aContextSize)
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
MOZ_DECL_CTOR_COUNTER(nsXFormsMDGEngine)

nsXFormsMDGEngine::nsXFormsMDGEngine(nsXFormsMDG* aOwner)
: mNodesInGraph(0), mOwner(aOwner)
{
  MOZ_COUNT_CTOR(nsXFormsMDGEngine);
}

nsXFormsMDGEngine::~nsXFormsMDGEngine()
{
  mNodeToMDG.Enumerate(DeleteLinkedNodes, nsnull);
  
  MOZ_COUNT_DTOR(nsXFormsMDGEngine);
}

PLDHashOperator
nsXFormsMDGEngine::DeleteLinkedNodes(nsISupports *aKey, nsAutoPtr<nsXFormsMDGNode>& aNode, void* aArg)
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

nsresult
nsXFormsMDGEngine::Init()
{
  nsresult rv = NS_ERROR_FAILURE;
  if (mNodeToFlag.Init() && mNodeToMDG.Init()) {
    rv = NS_OK;
  }
  
  return rv;
}

nsXFormsMDGNode*
nsXFormsMDGEngine::GetNode(nsIDOMNode* aDomNode, ModelItemPropName aType, PRBool aCreate)
{
  nsIDOMNode* nodeKey = aDomNode;
  nsXFormsMDGNode* nd = nsnull;

#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::GetNode(aDomNode=%p, aType=%d, aCreate=%d)\n", (void*) nodeKey, aType, aCreate);
#endif
  

  // Find correct type
  if (mNodeToMDG.Get(nodeKey, &nd)) {
    while (nd && nd->mType != aType) {
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
      nodeKey->AddRef();
      if (!mNodeToMDG.Put(nodeKey, nd)) {
        delete nd;
        NS_ERROR("Could not insert new node in HashTable!");
        return nsnull;
      }
    }

    mNodesInGraph++;
  }
  return nd;
}

nsresult
nsXFormsMDGEngine::Insert(nsIDOMNode* aContextNode, nsIDOMXPathExpression* aExpression,
                          const nsXFormsMDGSet* aDependencies, PRBool aDynFunc,
                          PRInt32 aContextPos, PRInt32 aContextSize,
                          ModelItemPropName aType, ModelItemPropName aDepType)
{
  NS_ENSURE_ARG(aContextNode);
  NS_ENSURE_ARG(aExpression);
  
#ifdef DEBUG_XF_MDG
  nsAutoString nodename;
  aContextNode->GetNodeName(nodename);
  printf("nsXFormsMDGEngine::Insert(aContextNode=%s, aExpression=n/a, aDependencies=|%d|,\n",
         NS_ConvertUCS2toUTF8(nodename).get(),
         aDependencies->Count());
  printf("                          aContextPos=%d, aContextSize=%d, aType=%d, aDepType=%d,\n",
         aContextPos, aContextSize, aType, aDepType);
  printf("                          aDynFunc=%d)\n",
         aDynFunc);
#endif
  nsXFormsMDGNode* newnode = GetNode(aContextNode, aType);
  
  if (!newnode) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  if (newnode->HasExpr()) {
    // MIP already in the graph. That is. there is already a MIP of the same
    // type for this node, which is illegal.
    // Example: <bind nodeset="x" required="true()" required="false()"/>
    return NS_ERROR_ABORT;
  }
  
  newnode->SetExpression(aExpression, aDynFunc, aContextPos, aContextSize);
  
  // Add dependencies
  if (aDependencies) {
    nsCOMPtr<nsIDOMNode> dep_domnode;
    nsXFormsMDGNode* dep_gnode;
    for (PRInt32 i = 0; i < aDependencies->Count(); ++i) {
      dep_domnode = aDependencies->GetNode(i);
      if (!dep_domnode) {
        return NS_ERROR_NULL_POINTER;
      }
      
      dep_gnode = GetNode(dep_domnode, aDepType);
      if (!dep_gnode) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
  
      if (aType == aDepType && dep_gnode->mContextNode == aContextNode) {
        // Reference to itself, ignore
        continue;
      }
  
      dep_gnode->mSuc.AppendElement(newnode);
      newnode->mCount++;
    }
  }

  return NS_OK;
}

PLDHashOperator 
nsXFormsMDGEngine::AddStartNodes(nsISupports *aKey, nsXFormsMDGNode* aNode, void* aDeque)
{
#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::AddStartNodes(aKey=n/a, aNode=%p, aDeque=%p)\n", (void*) aNode, aDeque);
#endif

  nsDeque* deque = NS_STATIC_CAST(nsDeque*, aDeque);
  if (!deque) {
    NS_ERROR("nsXFormsMDGEngine::AddStartNodes called with NULL aDeque");
    return PL_DHASH_STOP;
  }
  
  while (aNode) {
    if (aNode->mCount == 0) {
      // Is it not possible to check error condition?
      deque->Push(aNode);
    }
    aNode = aNode->mNext;
  }
    
  return PL_DHASH_NEXT;
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
  mNodeToFlag.Clear();

  nsDeque sortedNodes(nsnull);

#ifdef DEBUG_XF_MDG
  printf("\tmNodesInGraph: %d\n", mNodesInGraph);
  printf("\tmNodeToMDG:    %d\n", mNodeToMDG.Count());
  printf("\tmNodeToFlag:   %d\n", mNodeToFlag.Count());
#endif

  // Initial scan for nsXFormsMDGNodes with no dependencies (count == 0)
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
      nsXFormsMDGNode* sucNode = NS_STATIC_CAST(nsXFormsMDGNode*, node->mSuc[i]);
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

  if (mGraph.Count() != mNodesInGraph) {
    NS_WARNING("XForms: There are loops in the MDG\n");
    rv = NS_ERROR_ABORT;
  }

  mNodesInGraph = 0;

  return rv;
}

void
nsXFormsMDGEngine::Clear() {
#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::Clear()\n");
#endif

  mNodeToMDG.Enumerate(DeleteLinkedNodes, nsnull);
  mNodeToMDG.Clear();

  mNodeToFlag.Clear();
  
  mGraph.Clear();
  
  mNodesInGraph = 0;
}

PLDHashOperator 
nsXFormsMDGEngine::AndFlag(nsISupports *aKey, PRUint16& aFlag, void* aMask)
{
  PRUint16* andMask = NS_STATIC_CAST(PRUint16*, aMask);
  if (!andMask) {
    return PL_DHASH_STOP;
  }
  aFlag &= *andMask;
  return PL_DHASH_NEXT;
}

PRBool
nsXFormsMDGEngine::AndFlags(PRUint16 aAndMask)
{
  PRUint32 entries = mNodeToFlag.Enumerate(AndFlag, &aAndMask);
  return (entries == mNodeToFlag.Count()) ? PR_TRUE : PR_FALSE;
}

PRUint16
nsXFormsMDGEngine::GetFlag(nsIDOMNode* aDomNode)
{
  PRUint16 flag = MDG_FLAG_DEFAULT | (mJustRebuilt && mFirstCalculate ? MDG_FLAG_INITIAL_DISPATCH : 0);

  // If node is found, flag is modified, if not flag is untouched
  mNodeToFlag.Get(aDomNode, &flag);

  return flag;
}

void
nsXFormsMDGEngine::SetFlag(nsIDOMNode* aDomNode, PRUint16 aFlag)
{
  nsIDOMNode *nodeKey = aDomNode;
  nodeKey->AddRef();
  mNodeToFlag.Put(nodeKey, aFlag);
}
    
void
nsXFormsMDGEngine::OrFlag(nsIDOMNode* aDomNode, PRInt16 aFlag)
{
  PRUint16 fl = GetFlag(aDomNode);
  fl |= aFlag;
  SetFlag(aDomNode, fl);
}

void
nsXFormsMDGEngine::SetFlagBits(nsIDOMNode* aDomNode, PRUint16 aBits, PRBool aOn)
{
  PRUint32 fl = GetFlag(aDomNode);
  if (aOn) {
    fl |= aBits;
  } else {
    fl &= ~aBits;
  }
  SetFlag(aDomNode, fl);
}


nsresult
nsXFormsMDGEngine::BooleanExpression(nsXFormsMDGNode* aNode, PRBool& state)
{
  NS_ENSURE_ARG_POINTER(aNode);
  
  // TODO: Use aNode->contextPosition and aNode->contextSize!
  nsISupports* retval;
  nsresult rv;

  rv = aNode->mExpression->Evaluate(aNode->mContextNode, nsIDOMXPathResult::BOOLEAN_TYPE, nsnull, &retval);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMXPathResult> xpath_res = do_QueryInterface(retval);
  NS_ENSURE_TRUE(xpath_res, NS_ERROR_FAILURE);
  
  rv = xpath_res->GetBooleanValue(&state);
  
  return rv;
}

nsresult
nsXFormsMDGEngine::ComputeMIP(PRUint16 aStateFlag, PRUint16 aDispatchFlag, nsXFormsMDGNode* aNode, PRBool& aDidChange)
{
  NS_ENSURE_ARG_POINTER(aNode);
  
  PRUint32 word = GetFlag(aNode->mContextNode);
  PRBool state;
  nsresult rv = BooleanExpression(aNode, state);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool cstate = ((word & aStateFlag) != 0) ? PR_TRUE : PR_FALSE;
  
  if (state) {
    word |= aStateFlag;
  } else {
    word &= ~aStateFlag;
  }

  aDidChange = (state != cstate) ? PR_TRUE : PR_FALSE;
  if (aDidChange) {
    word |= aDispatchFlag;
  }

  SetFlag(aNode->mContextNode, word);
  
  return NS_OK;
}

nsresult
nsXFormsMDGEngine::ComputeMIPWithInheritance(PRUint16 aStateFlag, PRUint16 aDispatchFlag, PRUint16 aInheritanceFlag, nsXFormsMDGNode* aNode, nsXFormsMDGSet& aSet)
{
  nsresult rv;
  PRBool didChange;
  rv = ComputeMIP(aStateFlag, aDispatchFlag, aNode, didChange);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (didChange) {
    PRUint16 flag = GetFlag(aNode->mContextNode);
    PRBool state = ((flag & aStateFlag) != 0) ? PR_TRUE : PR_FALSE;
    if (   (aStateFlag == MDG_FLAG_READONLY && (flag & aInheritanceFlag) == 0)
        || (aStateFlag == MDG_FLAG_RELEVANT && (flag & aInheritanceFlag) > 0)  )
    {
      NS_ENSURE_TRUE(aSet.AddNode(aNode->mContextNode), NS_ERROR_FAILURE);
      rv = AttachInheritance(aSet, aNode->mContextNode, state, aStateFlag);
    }
  }

  return NS_OK;
}

nsresult
nsXFormsMDGEngine::AttachInheritance(nsXFormsMDGSet& aSet, nsIDOMNode* aSrc, PRBool aState, PRUint16 aStateFlag)
{
  NS_ENSURE_ARG(aSrc);
  
  nsCOMPtr<nsIDOMNode> node;
  PRUint32 flag;
  PRBool cstate;
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
    
    flag = GetFlag(node);

    cstate = ((flag & aStateFlag) != 0) ? PR_TRUE : PR_FALSE;

    if (aStateFlag == MDG_FLAG_RELEVANT) {
      if (aState == PR_FALSE) { // The nodes are getting irrelevant
        if ((flag & MDG_FLAG_INHERITED_RELEVANT) != 0 && cstate) {
          flag &= ~MDG_FLAG_INHERITED_RELEVANT;
          flag |= MDG_FLAG_DISPATCH_RELEVANT_CHANGED;
          updateNode = PR_TRUE;
        }
      } else { // The nodes are becoming relevant
        if (cstate) {
          flag |= MDG_FLAG_DISPATCH_RELEVANT_CHANGED; // Relevant has changed from inheritance
          flag |= MDG_FLAG_INHERITED_RELEVANT; // Clear the flag for inheritance
          updateNode = PR_TRUE;
        }
      }
    } else if (aStateFlag == MDG_FLAG_READONLY) {
      if (aState) { // The nodes are getting readonly
        if ((flag & MDG_FLAG_INHERITED_READONLY) == 0 && cstate == PR_FALSE) {
          flag |= MDG_FLAG_INHERITED_READONLY | MDG_FLAG_DISPATCH_READONLY_CHANGED;
          updateNode = PR_TRUE;
        }
      } else { // The nodes are getting readwrite
        if (cstate) {
          flag |= MDG_FLAG_DISPATCH_READONLY_CHANGED;
          flag &= ~MDG_FLAG_INHERITED_READONLY;
          updateNode = PR_TRUE;
        }
      }
    }
    
    if (updateNode) {
      SetFlag(node, flag);
      rv = AttachInheritance(aSet, node, aState, aStateFlag);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(aSet.AddNode(node), NS_ERROR_FAILURE);
      updateNode = PR_FALSE;
    }
  }

  return NS_OK;
}

nsresult
nsXFormsMDGEngine::Calculate(nsXFormsMDGSet& aValueChanged)
{
#ifdef DEBUG_XF_MDG
  printf("nsXFormsMDGEngine::Calculcate(aValueChanged=|%d|)\n", aValueChanged.Count());
#endif

  NS_ENSURE_TRUE(aValueChanged.AddSet(mMarkedNodes), NS_ERROR_OUT_OF_MEMORY);

  mMarkedNodes.Clear();
  
  PRBool res = PR_TRUE;

  mFirstCalculate = mJustRebuilt;

#ifdef DEBUG_XF_MDG
  printf("\taValueChanged: %d\n", aValueChanged.Count());
  printf("\tmNodeToMDG:    %d\n", mNodeToFlag.Count());
  printf("\tmNodeToFlag:   %d\n", mNodeToFlag.Count());
  printf("\tGraph nodes:   %d\n", mGraph.Count());
#endif
  
  // Go through all dirty nodes in the graph
  nsresult rv;
  nsXFormsMDGNode* g;
  for (PRInt32 i = 0; i < mGraph.Count(); ++i) {
    g = NS_STATIC_CAST(nsXFormsMDGNode*, mGraph[i]);

    if (!g) {
      NS_WARNING("nsXFormsMDGEngine::Calculcate(): Empty node in graph!!!");
      continue;
    }

#ifdef DEBUG_XF_MDG
    nsAutoString domNodeName;
    g->mContextNode->GetNodeName(domNodeName);

    printf("\tNode #%d: This=%p, Dirty=%d, DynFunc=%d, Type=%d, Count=%d, Suc=%d, CSize=%d, CPos=%d, Next=%p, domnode=%s\n",
           i, (void*) g, g->IsDirty(), g->mDynFunc, g->mType, g->mCount, g->mSuc.Count(),
           g->mContextSize, g->mContextPosition, (void*) g->mNext, NS_ConvertUCS2toUTF8(domNodeName).get());
#endif

    // Ignore node if it is not dirty
    if (!g->IsDirty()) {
      continue;
    }
    
    PRBool constraint = PR_TRUE;
    // Find MIP-type and handle it accordingly
    switch (g->mType) {
    case eModel_calculate:
      if (g->HasExpr()) {
        nsISupports* retval;
        rv = g->mExpression->Evaluate(g->mContextNode, nsIDOMXPathResult::STRING_TYPE, nsnull, &retval);
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsCOMPtr<nsIDOMXPathResult> xpath_res = do_QueryInterface(retval);
        NS_ENSURE_TRUE(xpath_res, NS_ERROR_OUT_OF_MEMORY);
        
        nsAutoString nodeval;
        rv = xpath_res->GetStringValue(nodeval);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mOwner->SetNodeValue(g->mContextNode, nodeval); 
        if (NS_SUCCEEDED(rv)) {
          NS_ENSURE_TRUE(aValueChanged.AddNode(g->mContextNode), NS_ERROR_FAILURE);
        }
      }

      OrFlag(g->mContextNode, MDG_FLAG_DISPATCH_VALUE_CHANGED);// | MDG_FLAG_DISPATCH_READONLY_CHANGED | MDG_FLAG_DISPATCH_VALID_CHANGED | MDG_FLAG_DISPATCH_RELEVANT_CHANGED);
      break;
      
    case eModel_constraint:
      if (g->HasExpr()) {
        rv = BooleanExpression(g, constraint);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // TODO: Schema validity should be checked here
            
      if ((GetFlag(g->mContextNode) & MDG_FLAG_CONSTRAINT) > 0 != constraint) {
        SetFlagBits(g->mContextNode, MDG_FLAG_CONSTRAINT, constraint);
        SetFlagBits(g->mContextNode, MDG_FLAG_DISPATCH_VALID_CHANGED, PR_TRUE);
        NS_ENSURE_TRUE(aValueChanged.AddNode(g->mContextNode), NS_ERROR_FAILURE);
      }
      break;
      
    case eModel_readonly:
      if (g->HasExpr()) {
        rv = ComputeMIPWithInheritance(MDG_FLAG_READONLY, MDG_FLAG_DISPATCH_READONLY_CHANGED, MDG_FLAG_INHERITED_READONLY, g, aValueChanged);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
      
    case eModel_relevant:
      if (g->HasExpr()) {
        rv = ComputeMIPWithInheritance(MDG_FLAG_RELEVANT, MDG_FLAG_DISPATCH_RELEVANT_CHANGED, MDG_FLAG_INHERITED_RELEVANT, g, aValueChanged);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
      
    case eModel_required:
      PRBool didChange;
      rv = ComputeMIP(MDG_FLAG_REQUIRED, MDG_FLAG_DISPATCH_REQUIRED_CHANGED, g, didChange);
      NS_ENSURE_SUCCESS(rv, rv);
      
      if (g->HasExpr() && didChange) {
        NS_ENSURE_TRUE(aValueChanged.AddNode(g->mContextNode), NS_ERROR_FAILURE);
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
  aValueChanged.MakeUnique();
  
#ifdef DEBUG_XF_MDG
  printf("\taValueChanged: %d\n", aValueChanged.Count());
  printf("\tmNodeToMDG:    %d\n", mNodeToFlag.Count());
  printf("\tmNodeToFlag:   %d\n", mNodeToFlag.Count());
  printf("\tGraph nodes:   %d\n", mGraph.Count());
#endif

  return res;
}

// TODO: This function needs to be called 
nsresult
nsXFormsMDGEngine::MarkNode(nsIDOMNode* aDomNode)
{
  SetFlagBits(aDomNode, MDG_FLAG_ALL_DISPATCH, PR_TRUE);

  nsXFormsMDGNode* n = GetNode(aDomNode, eModel_calculate);
  if (n) {
    n->MarkDirty();
  }

  // Add constraint to trigger validation of node 
  n = GetNode(aDomNode, eModel_constraint, PR_FALSE);
  if (!n) {
    n = GetNode(aDomNode, eModel_constraint, PR_TRUE);
    if (!n) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ENSURE_TRUE(mGraph.AppendElement(n), NS_ERROR_OUT_OF_MEMORY);
  }

  n->MarkDirty();

  NS_ENSURE_TRUE(mMarkedNodes.AddNode(aDomNode), NS_ERROR_FAILURE);

  return NS_OK;
}

PRBool
nsXFormsMDGEngine::ClearDispatchFlags()
{
  mJustRebuilt = PR_FALSE;
  return AndFlags(MDG_FLAGMASK_NOT_DISPATCH);
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

PRBool
nsXFormsMDGEngine::TestAndClear(nsIDOMNode* aDomNode, PRUint16 aFlag)
{
  PRUint16 fl = GetFlag(aDomNode);
  PRUint16 test = fl & aFlag;
  fl &= ~aFlag;
  SetFlag(aDomNode, fl);
  return (test != 0) ? PR_TRUE : PR_FALSE;
}

PRBool
nsXFormsMDGEngine::TestAndSet(nsIDOMNode* aDomNode, PRUint16 aFlag)
{
  PRUint16 fl = GetFlag(aDomNode);
  PRUint16 test = fl & aFlag;
  fl |= aFlag;
  SetFlag(aDomNode, fl);
  return (test != 0) ? PR_TRUE : PR_FALSE;
}

PRBool
nsXFormsMDGEngine::Test(nsIDOMNode* aDomNode, PRUint16 aFlag)
{
  PRUint16 fl = GetFlag(aDomNode);
  PRUint16 test = fl & aFlag;
  return (test != 0) ? PR_TRUE : PR_FALSE;
}
