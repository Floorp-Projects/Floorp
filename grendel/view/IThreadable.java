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
 * Created: Jamie Zawinski <jwz@netscape.com>, 13 Jun 1995.
 * Ported from C on 14 Aug 1997.
 */

/**
   This is an interface representing messages to be threaded by references.
   It is the interface to the Threader.

   @see Threader
   @see ISortable
   @see Sorter
 */

package grendel.view;

import java.util.Enumeration;


interface IThreadable {

  /** Returns each subsequent element in the set of messages of
      which this IThreadable is the root.  Order is unimportant.
   */
  Enumeration allElements();

  /** Returns an object identifying this message.
      Generally this will be a representation of the contents of the
      Message-ID header.  It may be a String, or it may be something else;
      the only constraint is that <TT>equals</TT> and <TT>hashCode</TT>
      work properly on it.  (That is, two ID objects are equal and hash the
      same if they represent the same underlying ID-string.)
   */
  Object messageThreadID();

  /** Returns the IDs of the set of messages referenced by this one.
      This list should be ordered from oldest-ancestor to youngest-ancestor.
      As for the <TT>messageThreadID</TT> method, the contents of this array
      may be any type of object, so long as <TT>equals</TT> and
      <TT>hashCode</TT> work properly on them.
   */
  Object[] messageThreadReferences();

  /** When no references are present, subjects will be used to thread together
      messages.  This method should return a threadable subject: two messages
      with the same simplifiedSubject will be considered to belong to the same
      thread.  This string should not have `Re:' on the front, and may have
      been simplified in whatever other ways seem appropriate.
      <P>
      This is a String of Unicode characters, and should have had any encodings
      (such as RFC 2047 charset encodings) removed first.
      <P>
      If you aren't interested in threading by subject at all, return null.
   */
  String simplifiedSubject();

  /** Whether the original subject was one that appeared to be a reply (that
      is, had a `Re:' or some other indicator.)  When threading by subject,
      this property is used to tell whether two messages appear to be siblings,
      or in a parent/child relationship.
   */
  boolean subjectIsReply();

  /** When the proper thread order has been computed, these two methods will
      be called on each IThreadable in the chain, to set up the proper tree
      structure.
   */
  void setNext(Object next);
  void setChild(Object kid);

  /** Creates a dummy parent object.
      <P>
      With some set of messages, the only way to achieve proper threading is
      to introduce an element into the tree which represents messages which are
      not present in the set: for example, when two messages share a common
      ancestor, but that ancestor is not in the set.  This method is used to
      make a placeholder for those sorts of ancestors.  It should return an
      object which is also a IThreadable.  The setNext() and setChild() methods
      will be used on this placeholder, as either the object or the argument,
      just as for other elements of the tree.
    */
  IThreadable makeDummy();

  /** This should return true of dummy messages, false otherwise.
      It is legal to pass dummy messages in with the list returned by
      elements(); the isDummy() method is the mechanism by which they are
      noted and ignored.
   */
  boolean isDummy();
}
