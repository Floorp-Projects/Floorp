/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Terry Weissman <terry@netscape.com>, 25 Aug 1997.
 */


package grendel.view;

import javax.mail.Message;

/** Represents a view on messages.  There is no implication that these
    messages all come from the same Folder, or even from the same kind of
    Folder. <p>

    The messages are presented in a particular sorted order, and have
    also possibly been threaded.
    */

public interface MessageSetView {
  // The various ways we know to sort messages.
  final static int NUMBER  = 1;
  final static int DATE    = 2;
  final static int SUBJECT = 3;
  final static int AUTHOR  = 4;
  final static int READ    = 5;
  final static int FLAGGED = 6;
  final static int SIZE    = 7;
  final static int DELETED = 8;

//  public SearchTerm getSearchTerm();
//  public void setSearchTerm(SearchTerm);
  /** Gets the root of the tree of messages that are being viewed. */
  public ViewedMessage getMessageRoot();

  /** Set the sort order for display.  This is an array of the sort keys listed
    above.  It is an array so that the caller can define secondary sorts.
    Duplicates will be removed and other optimizations possibly made, so
    getSortOrder() may not necessarily return the same thing.  Note that
    the messages won't actually get resorted until reThread() is called. */
  public void setSortOrder(int order[]);

  /** Prepend a new sort order.  This will become the new primary sort;
    whatever sort was being done becomes secondary sorts.  Note that
    the messages won't actually get resorted until reThread() is called. */
  public void prependSortOrder(int order);

  public int[] getSortOrder();

  /** Set whether to do threading on these messages.  If false, then things
    just get sorted into one long list.  Note that the messages won't actually
    get resorted or threaded until reThread() is called. */
  public void setIsThreaded(boolean b);

  public boolean isThreaded();

  /** Cause the messages to get resorted and possibly threaded, according to
   previous calls to setSortOrder(), prependSortOrder(), and setIsThreaded().
   */
  public void reThread();

  public void addObserver(MessageSetViewObserver obs);
  public void removeObserver(MessageSetViewObserver obs);
}
