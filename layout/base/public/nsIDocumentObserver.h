/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIDocumentObserver_h___
#define nsIDocumentObserver_h___

#include "nsISupports.h"
class nsIContent;
class nsIPresShell;
class nsIStyleSheet;
class nsString;

#define NS_IDOCUMENT_OBSERVER_IID \
{ 0xb3f92460, 0x944c, 0x11d1, {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// Document observer interface
class nsIDocumentObserver : public nsISupports {
public:
  /**
   * This is called when the documents title has arrived.
   */
  NS_IMETHOD SetTitle(const nsString& aTitle) = 0;

  /**
   * Notify that a content model update is beginning. This call can be
   * nested.
   */
  NS_IMETHOD BeginUpdate() = 0;

  /**
   * Notify that a content model update is finished. This call can be
   * nested.
   */
  NS_IMETHOD EndUpdate() = 0;

  /**
   * Notify the observer that a document load is beginning.
   */
  NS_IMETHOD BeginLoad() = 0;

  /**
   * Notify the observer that a document load has finished. Note that
   * the associated reflow of the document will be done <b>before</b>
   * EndLoad is invoked, not after.
   */
  NS_IMETHOD EndLoad() = 0;

  /**
   * Notify the observer that the document is being reflowed in
   * the given presentation shell.
   */
  NS_IMETHOD BeginReflow(nsIPresShell* aShell) = 0;

  /**
   * Notify the observer that the document is done being reflowed in
   * the given presentation shell.
   */
  NS_IMETHOD EndReflow(nsIPresShell* aShell) = 0;

  /**
   * Notification that the content model has changed. This method is
   * called automatically by content objects when their state is changed
   * (therefore there is normally no need to invoke this method
   * directly).  The notification is passed to any
   * IDocumentObservers. The notification is passed on to all of the
   * document observers. <p>
   *
   * This notification is not sent when a piece of content is
   * added/removed from the document (the other notifications are used
   * for that).
   *
   * @param aContent the piece of content that changed
   * @param aSubContent subrange information about the piece of content
   *  that changed
   */
  NS_IMETHOD ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent) = 0;

  /**
   * Notifcation that the content model has had data appended to the
   * given content object. This method is called automatically by the
   * content container objects when a new content object is appended to
   * the container (therefore there is normally no need to invoke this
   * method directly). The notification is passed on to all of the
   * document observers.
   *
   * @param aContainer the container that had a new child appended
   */
  NS_IMETHOD ContentAppended(nsIContent* aContainer) = 0;

  /**
   * Notification that content has been inserted. This method is called
   * automatically by the content container objects when a new content
   * object is inserted in the container (therefore there is normally no
   * need to invoke this method directly). The notification is passed on
   * to all of the document observers.
   *
   * @param aContainer the container that now contains aChild
   * @param aChild the child that was inserted
   * @param aIndexInContainer the index of the child in the container
   */
  NS_IMETHOD ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) = 0;

  /**
   * Notification that content has been replaced. This method is called
   * automatically by the content container objects when a content object
   * is replaced in the container (therefore there is normally no need to
   * invoke this method directly). The notification is passed on to all
   * of the document observers.
   *
   * @param aContainer the container that now contains aChild
   * @param aOldChild the child that was replaced
   * @param aNewChild the child that replaced aOldChild
   * @param aIndexInContainer the index of the old and new child in the
   *  container
   */
  NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) = 0;

  /**
   * Content is going to be removed immediately after this call. This
   * method is called automatically by content container objects when a
   * content object is about to be removed from the container (therefore
   * there is normally no need to invoke this method directly). The
   * notification is passed on to all of the document observers.
   *
   * @param aContainer the container that contains aChild
   * @param aChild the child that will be removed
   * @param aIndexInContainer the index of the child in the container
   */
  NS_IMETHOD ContentWillBeRemoved(nsIContent* aContainer,
                                  nsIContent* aChild,
                                  PRInt32 aIndexInContainer) = 0;

  /**
   * Content has just been removed. This method is called automatically
   * by content container objects when a content object has just been
   * removed from the container (therefore there is normally no need to
   * invoke this method directly). The notification is passed on to all
   * of the document observers.
   *
   * @param aContainer the container that had a child removed
   * @param aChild the child that was just removed
   * @param aIndexInContainer the index of the child in the container
   *  before it was removed
   */
  NS_IMETHOD ContentHasBeenRemoved(nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer) = 0;

  /**
   * A StyleSheet has just been added to the document.
   * This method is called automatically when a StyleSheet gets added
   * to the document. The notification is passed on to all of the 
   * document observers.
   *
   * @param aStyleSheet the StyleSheet that has been added
   */
  NS_IMETHOD StyleSheetAdded(nsIStyleSheet* aStyleSheet) = 0;
};

#endif /* nsIDocumentObserver_h___ */
