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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Jonas Sicking <jonas@sicking.cc> (Original Author)
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

#include "nsNodeUtils.h"
#include "nsContentUtils.h"
#include "nsINode.h"
#include "nsIContent.h"
#include "nsTArray.h"
#include "nsIMutationObserver.h"
#include "nsIDocument.h"
#include "nsIDOMUserDataHandler.h"
#include "nsIEventListenerManager.h"

#define IMPL_MUTATION_NOTIFICATION(func_, content_, params_)      \
  PR_BEGIN_MACRO                                                  \
  nsINode* node = content_;                                       \
  nsINode* prev;                                                  \
  do {                                                            \
    nsINode::nsSlots* slots = node->GetExistingSlots();           \
    if (slots && !slots->mMutationObservers.IsEmpty()) {          \
      /* No need to explicitly notify the first observer first    \
         since that'll happen anyway. */                          \
      NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(                         \
        slots->mMutationObservers, nsIMutationObserver,           \
        func_, params_);                                          \
    }                                                             \
    prev = node;                                                  \
    node = node->GetNodeParent();                                 \
                                                                  \
    if (!node && prev->IsNodeOfType(nsINode::eXUL)) {             \
      /* XUL elements can have the in-document flag set, but      \
         still be in an orphaned subtree. In this case we         \
         need to notify the document */                           \
      node = NS_STATIC_CAST(nsIContent*, prev)->GetCurrentDoc();  \
    }                                                             \
  } while (node);                                                 \
  PR_END_MACRO


void
nsNodeUtils::CharacterDataChanged(nsIContent* aContent, PRBool aAppend)
{
  nsIDocument* doc = aContent->GetOwnerDoc();
  IMPL_MUTATION_NOTIFICATION(CharacterDataChanged, aContent,
                             (doc, aContent, aAppend));
}

void
nsNodeUtils::AttributeChanged(nsIContent* aContent,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType)
{
  nsIDocument* doc = aContent->GetOwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeChanged, aContent,
                             (doc, aContent, aNameSpaceID, aAttribute,
                              aModType));
}

void
nsNodeUtils::ContentAppended(nsIContent* aContainer,
                             PRInt32 aNewIndexInContainer)
{
  nsIDocument* document = aContainer->GetOwnerDoc();

  IMPL_MUTATION_NOTIFICATION(ContentAppended, aContainer,
                             (document, aContainer, aNewIndexInContainer));
}

void
nsNodeUtils::ContentInserted(nsINode* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer)
{
  NS_PRECONDITION(aContainer->IsNodeOfType(nsINode::eCONTENT) ||
                  aContainer->IsNodeOfType(nsINode::eDOCUMENT),
                  "container must be an nsIContent or an nsIDocument");
  nsIContent* container;
  nsIDocument* document;
  if (aContainer->IsNodeOfType(nsINode::eCONTENT)) {
    container = NS_STATIC_CAST(nsIContent*, aContainer);
    document = aContainer->GetOwnerDoc();
  }
  else {
    container = nsnull;
    document = NS_STATIC_CAST(nsIDocument*, aContainer);
  }

  IMPL_MUTATION_NOTIFICATION(ContentInserted, aContainer,
                             (document, container, aChild, aIndexInContainer));
}

void
nsNodeUtils::ContentRemoved(nsINode* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
  NS_PRECONDITION(aContainer->IsNodeOfType(nsINode::eCONTENT) ||
                  aContainer->IsNodeOfType(nsINode::eDOCUMENT),
                  "container must be an nsIContent or an nsIDocument");
  nsIContent* container;
  nsIDocument* document;
  if (aContainer->IsNodeOfType(nsINode::eCONTENT)) {
    container = NS_STATIC_CAST(nsIContent*, aContainer);
    document = aContainer->GetOwnerDoc();
  }
  else {
    container = nsnull;
    document = NS_STATIC_CAST(nsIDocument*, aContainer);
  }

  IMPL_MUTATION_NOTIFICATION(ContentRemoved, aContainer,
                             (document, container, aChild, aIndexInContainer));
}

void
nsNodeUtils::LastRelease(nsINode* aNode, PRBool aDelete)
{
  nsINode::nsSlots* slots = aNode->GetExistingSlots();
  if (slots && !slots->mMutationObservers.IsEmpty()) {
    NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(slots->mMutationObservers,
                                       nsIMutationObserver,
                                       NodeWillBeDestroyed, (aNode));
  }

  // Kill properties first since that may run external code, so we want to
  // be in as complete state as possible at that time.
  if (aNode->HasProperties()) {
    nsIDocument* document = aNode->GetOwnerDoc();
    if (document) {
      nsContentUtils::CallUserDataHandler(document,
                                          nsIDOMUserDataHandler::NODE_DELETED,
                                          aNode, nsnull, nsnull);
      document->PropertyTable()->DeleteAllPropertiesFor(aNode);
    }
    aNode->UnsetFlags(NODE_HAS_PROPERTIES);
  }

  if (aNode->HasFlag(NODE_HAS_RANGELIST)) {
#ifdef DEBUG
    if (!nsContentUtils::LookupRangeList(aNode) &&
        nsContentUtils::IsInitialized()) {
      NS_ERROR("Huh, our bit says we have a range list, but there's nothing "
               "in the hash!?!!");
    }
#endif

    nsContentUtils::RemoveRangeList(aNode);
    aNode->UnsetFlags(NODE_HAS_RANGELIST);
  }

  if (aNode->HasFlag(NODE_HAS_LISTENERMANAGER)) {
#ifdef DEBUG
    if (nsContentUtils::IsInitialized()) {
      nsCOMPtr<nsIEventListenerManager> manager;
      PRBool created;
      nsContentUtils::GetListenerManager(aNode, PR_FALSE,
                                         getter_AddRefs(manager), &created);
      if (!manager) {
        NS_ERROR("Huh, our bit says we have a listener manager list, "
                 "but there's nothing in the hash!?!!");
      }
    }
#endif

    nsContentUtils::RemoveListenerManager(aNode);
    aNode->UnsetFlags(NODE_HAS_LISTENERMANAGER);
  }

  if (aDelete) {
    delete aNode;
  }
}

