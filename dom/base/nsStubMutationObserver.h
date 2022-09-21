/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsStubMutationObserver is an implementation of the nsIMutationObserver
 * interface (except for the methods on nsISupports) that is intended to be
 * used as a base class within the content/layout library.  All methods do
 * nothing.
 */

#ifndef nsStubMutationObserver_h_
#define nsStubMutationObserver_h_

#include "nsTHashMap.h"
#include "nsIMutationObserver.h"

/**
 * There are two advantages to inheriting from nsStubMutationObserver
 * rather than directly from nsIMutationObserver:
 *  1. smaller compiled code size (since there's no need for the code
 *     for the empty virtual function implementations for every
 *     nsIMutationObserver implementation)
 *  2. the performance of document's loop over observers benefits from
 *     the fact that more of the functions called are the same (which
 *     can reduce instruction cache misses and perhaps improve branch
 *     prediction)
 */
class nsStubMutationObserver : public nsIMutationObserver {
 public:
  NS_DECL_NSIMUTATIONOBSERVER
};

class MutationObserverWrapper;

/**
 * @brief Base class for MutationObservers that are used by multiple nodes.
 *
 * Mutation Observers are stored inside of a nsINode using a DoublyLinkedList,
 * restricting the number of nodes a mutation observer can be inserted to one.
 *
 * To allow a mutation observer to be used by several nodes, this class
 * provides a MutationObserverWrapper which implements the nsIMutationObserver
 * interface and forwards all method calls to this class. For each node this
 * mutation observer will be used for, a wrapper object is created.
 */
class nsMultiMutationObserver : public nsIMutationObserver {
 public:
  /**
   * Adds the mutation observer to aNode by creating a MutationObserverWrapper
   * and inserting it into aNode.
   * Does nothing if there already is a mutation observer for aNode.
   */
  void AddMutationObserverToNode(nsINode* aNode);

  /**
   * Removes the mutation observer from aNode.
   * Does nothing if there is no mutation observer for aNode.
   */
  void RemoveMutationObserverFromNode(nsINode* aNode);

  /**
   * Returns true if there is already a mutation observer for aNode.
   */
  bool ContainsNode(const nsINode* aNode) const;

 private:
  friend class MutationObserverWrapper;
  nsTHashMap<nsINode*, MutationObserverWrapper*> mWrapperForNode;
};

/**
 * Convenience class that provides support for multiple nodes and has
 * default implementations for nsIMutationObserver.
 */
class nsStubMultiMutationObserver : public nsMultiMutationObserver {
 public:
  NS_DECL_NSIMUTATIONOBSERVER
};

#endif /* !defined(nsStubMutationObserver_h_) */
