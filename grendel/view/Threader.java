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

package grendel.view;

import java.util.Hashtable;
import java.util.Enumeration;

import calypso.util.Assert;

/**
   This arranges a set of messages into a thread hierarchy, by references.
   The sets of messages are accessed via the IThreadable interface.

   @see IThreadable
   @see Sorter
 */

class Threader {
  private ThreadContainer root_node;     // has kids, and no next
  private Hashtable id_table;        // maps message IDs to ThreadContainers
  private int bogus_id_count = 0;    // tick of how many dup IDs we've seen

  /** Threads the set of messages indicated by threadable_root.
      The IThreadable returned is the new first element of the root set.
      @param    threadable_root     The start of the list.
    */
  public IThreadable thread(IThreadable threadable_root) {
    if (threadable_root == null) return null;
    id_table = new Hashtable();

    for (Enumeration e = threadable_root.allElements(); e.hasMoreElements();) {
      IThreadable t = (IThreadable) e.nextElement();
      if (!t.isDummy())
        buildContainer(t);
    }

    root_node = findRootSet();
    id_table.clear();
    id_table = null;

    pruneEmptyContainers(root_node);

    // We do this so to avoid flipping the input order each time through.
    root_node.reverseChildren();

    gatherSubjects();

    if (root_node.next != null)
      throw new Error("root node has a next?" + root_node);

    for (ThreadContainer r = root_node.child; r != null; r = r.next) {
      // If this direct child of the root node has no threadable in it,
      // manufacture a dummy container to bind its children together.
      // Note that these dummies can only ever occur as elements of
      // the root set.
      if (r.threadable == null)
        r.threadable = r.child.threadable.makeDummy();
    }

    IThreadable result = (root_node.child == null
                          ? null
                          : root_node.child.threadable);

    // Flush the tree structure of each element of the root set down into
    // their underlying threadables.
    root_node.flush();
    root_node = null;
    return result;
  }


  // buildContainer() does three things:
  //
  //   = It walks the tree of threadables, and wraps each in a
  //     ThreadContainer object.
  //   = It indexes each ThreadContainer object in the id_table, under
  //     the message ID of the contained IThreadable.
  //   = For each of the IThreadable's references, it ensures that there
  //     is a ThreadContainer in the table (an empty one, if necessary.)
  //
  private void buildContainer(IThreadable threadable) {

    Object id = threadable.messageThreadID();
    Assert.Assertion(id != null);
    ThreadContainer c = (ThreadContainer) id_table.get (id);

    if (c != null) {
      // There is already a ThreadContainer in the table for this ID.
      // Under normal circumstances, there will be no IThreadable in it
      // (since it was a forward reference from a References field.)
      //
      // If there is already a threadable in it, then that means there
      // are two IThreadables with the same ID.  Generate a new ID for
      // this one, sigh...  This ID is only used to cause the two entries
      // in the hash table to not stomp each other.
      //
      if (c.threadable != null) {
        id = "<Bogus-id:" + (bogus_id_count++) + ">";
        c = null;
      } else {
        c.threadable = threadable;
      }
    }

    // Create a ThreadContainer for this IThreadable, and index it in
    // the hash table.
    //
    if (c == null) {
      c = new ThreadContainer();
      c.threadable = threadable;
//      c.debug_id = id;
      id_table.put (id, c);
    }

    // Create ThreadContainers for each of the references which don't
    // have them.  Link each of the referenced messages together in the
    // order implied by the references field, unless they are already
    // linked.
    ThreadContainer parent_ref = null;
    {
      Object refs[] = threadable.messageThreadReferences();
      int L = (refs == null ? 0 : refs.length);
      for (int i = 0; i < L; i++) {
        Object ref_string = refs[i];
        ThreadContainer ref = (ThreadContainer) id_table.get (ref_string);

        if (ref == null) {
          ref = new ThreadContainer();
//          ref.debug_id = ref_string;
          id_table.put (ref_string, ref);
        }

        // If we have references A B C D, make D be a child of C, etc,
        // except if they have parents already.
        //
        if (parent_ref != null &&               // there is a parent
            ref.parent == null &&               // don't have a parent already
            parent_ref != ref &&                // not a tight loop
            !parent_ref.find_child(ref)) {      // not a wide loop
          // Ok, link it into the parent's child list.
          ref.parent = parent_ref;
          ref.next = parent_ref.child;
          parent_ref.child = ref;
        }
        parent_ref = ref;
      }
    }

    // At this point `parent_ref' is set to the container of the last element
    // in the references field.  Make that be the parent of this container,
    // unless doing so would introduce a circularity.
    //
    if (parent_ref != null &&
        (parent_ref == c ||
         c.find_child (parent_ref)))
      parent_ref = null;

    if (c.parent != null) {
      // If it has a parent already, that's there because we saw this message
      // in a references field, and presumed a parent based on the other
      // entries in that field.  Now that we have the actual message, we can
      // be more definitive, so throw away the old parent and use this new one.
      // Find this container in the parent's child-list, and unlink it.
      //
      // Note that this could cause this message to now have no parent, if it
      // has no references field, but some message referred to it as the
      // non-first element of its references.  (Which would have been some
      // kind of lie...)
      //
      ThreadContainer rest, prev;
      for (prev = null, rest = c.parent.child;
           rest != null;
           prev = rest, rest = rest.next) {
        if (rest == c)
          break;
      }
      if (rest == null)
        throw new Error("didn't find " + c + " in parent " + c.parent);

      if (prev == null)
        c.parent.child = c.next;
      else
        prev.next = c.next;

      c.next = null;
      c.parent = null;
    }

    // If we have a parent, link c into the parent's child list.
    if (parent_ref != null) {
      c.parent = parent_ref;
      c.next = parent_ref.child;
      parent_ref.child = c;
    }
  }


  // Find the root set of the ThreadContainers (and return a root node.)
  // A container is in the root set if it has no parents.
  //
  private ThreadContainer findRootSet() {
    ThreadContainer root = new ThreadContainer();
//    root.debug_id = "((root))";
    for (Enumeration e = id_table.elements(); e.hasMoreElements(); ) {
      ThreadContainer c = (ThreadContainer) e.nextElement();
      if (c.parent == null) {
        if (c.next != null)
          throw new Error("c.next is " + c.next.toString());
        c.next = root.child;
        root.child = c;
      }
    }
    return root;
  }


  // Walk through the threads and discard any empty container objects.
  // After calling this, there will only be any empty container objects
  // at depth 0, and those will all have at least two kids.
  //
  private void pruneEmptyContainers(ThreadContainer parent) {
    ThreadContainer container, prev, next;
    for (prev = null, container = parent.child, next = container.next;
         container != null;
         prev = container, container = next,
           next = (container == null ? null : container.next)) {

      if (container.threadable == null &&
          container.child == null) {
        // This is an empty container with no kids.  Nuke it.
        //
        // Normally such containers won't occur, but they can show up when
        // two messages have References lines that disagree.  For example,
        // assuming A and B are messages, and 1, 2, and 3 are references for
        // messages we haven't seen:
        //
        //        A has refs: 1 2 3
        //        B has refs: 1 3
        //
        // There is ambiguity as to whether 3 is a child of 1 or 2.  So,
        // depending on the processing order, we might end up with either
        //
        //        -- 1
        //           |-- 2
        //               |-- 3
        //                   |-- A
        //                   |-- B
        // or
        //        -- 1
        //           |-- 2            <--- non root childless container
        //           |-- 3
        //               |-- A
        //               |-- B
        //
        if (prev == null)
          parent.child = container.next;
        else
          prev.next = container.next;

        // Set container to prev so that prev keeps its same value
        // the next time through the loop.
        container = prev;

      } else if (container.threadable == null &&    // expired, and
                 container.child != null &&         // has kids, and
                 (container.parent != null ||       //   not at root, or
                  container.child.next == null)) {  //   only one kid

        // Expired message with kids.  Promote the kids to this level.
        // Don't do this if we would be promoting them to the root level,
        // unless there is only one kid.

        ThreadContainer tail;
        ThreadContainer kids = container.child;

        // Remove this container from the list, replacing it with `kids'.
        if (prev == null)
          parent.child = kids;
        else
          prev.next = kids;

        // make each child's parent be this level's parent.
        // make the last child's next be this container's next
        // (splicing `kids' into the list in place of `container'.)
        for (tail = kids; tail.next != null; tail = tail.next)
          tail.parent = container.parent;

        tail.parent = container.parent;
        tail.next = container.next;

        // Since we've inserted items in the chain, `next' currently points
        // to the item after them (tail.next); reset that so that we process
        // the newly promoted items the very next time around.
        next = kids;

        // Set container to prev so that prev keeps its same value
        // the next time through the loop.
        container = prev;

      } else if (container.child != null) {
        // A real message with kids.
        // Iterate over its children, and try to strip out the junk.

        pruneEmptyContainers(container);
      }
    }
  }


  // If any two members of the root set have the same subject, merge them.
  // This is so that messages which don't have References headers at all
  // still get threaded (to the extent possible, at least.)
  //
  private void gatherSubjects() {

    int count = 0;
    for (ThreadContainer c = root_node.child; c != null; c = c.next)
      count++;

    // Make the hash table large enough to not need to be rehashed.
    Hashtable subj_table = new Hashtable((int) (count * 1.2), (float) 0.9);

    count = 0;
    for (ThreadContainer c = root_node.child; c != null; c = c.next) {
      IThreadable threadable = c.threadable;

      // If there is no threadable, this is a dummy node in the root set.
      // Only root set members may be dummies, and they always have at least
      // two kids.  Take the first kid as representative of the subject.
      if (threadable == null)
        threadable = c.child.threadable;

      String subj = threadable.simplifiedSubject();

      if (subj == null || subj == "")
        continue;

      ThreadContainer old = (ThreadContainer) subj_table.get(subj);

      // Add this container to the table if:
      //  - There is no container in the table with this subject, or
      //  - This one is a dummy container and the old one is not: the dummy
      //    one is more interesting as a root, so put it in the table instead.
      //  - The container in the table has a "Re:" version of this subject,
      //    and this container has a non-"Re:" version of this subject.
      //    The non-re version is the more interesting of the two.
      //
      if (old == null ||
          (c.threadable == null && old.threadable != null) ||
          (old.threadable != null && old.threadable.subjectIsReply() &&
           c.threadable   != null &&  !c.threadable.subjectIsReply())) {
        subj_table.put(subj, c);
        count++;
      }
    }

    if (count == 0)   // if the table is empty, we're done.
      return;

    // The subj_table is now populated with one entry for each subject which
    // occurs in the root set.  Now iterate over the root set, and gather
    // together the difference.
    //
    ThreadContainer prev, c, rest;
    for (prev = null, c = root_node.child, rest = c.next;
         c != null;
         prev = c, c = rest, rest = (rest == null ? null : rest.next)) {

      IThreadable threadable = c.threadable;
      if (threadable == null)  // might be a dummy -- see above
        threadable = c.child.threadable;

      String subj = threadable.simplifiedSubject();

      // Don't thread together all subjectless messages; let them dangle.
      if (subj == null || subj == "")
        continue;

      ThreadContainer old = (ThreadContainer) subj_table.get(subj);
      if (old == c)     // oops, that's us
        continue;

      // Ok, so now we have found another container in the root set with
      // the same subject.  There are a few possibilities:
      //
      // - If both are dummies, append one's children to the other, and remove
      //   the now-empty container.
      //
      // - If one container is a dummy and the other is not, make the non-dummy
      //   one be a child of the dummy, and a sibling of the other "real"
      //   messages with the same subject (the dummy's children.)
      //
      // - If that container is a non-dummy, and that message's subject does
      //   not begin with "Re:", but *this* message's subject does, then
      //   make this be a child of the other.
      //
      // - If that container is a non-dummy, and that message's subject begins
      //   with "Re:", but *this* message's subject does *not*, then make that
      //   be a child of this one -- they were misordered.  (This happens
      //   somewhat implicitly, since if there are two messages, one with Re:
      //   and one without, the one without will be in the hash table,
      //   regardless of the order in which they were seen.)
      //
      // - Otherwise, make a new dummy container and make both messages be a
      //   child of it.  This catches the both-are-replies and neither-are-
      //   replies cases, and makes them be siblings instead of asserting a
      //   hierarchical relationship which might not be true.
      //
      //   (People who reply to messages without using "Re:" and without using
      //   a References line will break this slightly.  Those people suck.)
      //
      // (It has occurred to me that taking the date or message number into
      // account would be one way of resolving some of the ambiguous cases,
      // but that's not altogether straightforward either.)


      // Remove the "second" message from the root set.
      if (prev == null)
        root_node.child = c.next;
      else
        prev.next = c.next;
      c.next = null;


      if (old.threadable == null && c.threadable == null) {
        // They're both dummies; merge them.
        ThreadContainer tail;
        for (tail = old.child;
             tail != null && tail.next != null;
             tail = tail.next)
          ;
        tail.next = c.child;
        for (tail = c.child; tail != null; tail = tail.next)
          tail.parent = old;
        c.child = null;

      } else if (old.threadable == null ||               // old is empty, or
                 (c.threadable != null &&
                  c.threadable.subjectIsReply() &&       //   c has Re, and
                  !old.threadable.subjectIsReply())) {   //   old does not.
        // Make this message be a child of the other.
        c.parent = old;
        c.next = old.child;
        old.child = c;

      } else {
        // Make the old and new messages be children of a new dummy container.
        // We do this by creating a new container object for old->msg and
        // transforming the old container into a dummy (by merely emptying it),
        // so that the hash table still points to the one that is at depth 0
        // instead of depth 1.

        ThreadContainer newc = new ThreadContainer();
        newc.threadable = old.threadable;
//        newc.debug_id = old.debug_id;
        newc.child = old.child;
        for (ThreadContainer tail = newc.child; tail != null; tail = tail.next)
          tail.parent = newc;

        old.threadable = null;
        old.child = null;
//        old.debug_id = null;

        c.parent = old;
        newc.parent = old;

        // old is now a dummy; make it have exactly two kids, c and newc.
        old.child = c;
        c.next = newc;
      }

      // we've done a merge, so keep the same `prev' next time around.
      c = prev;
    }

    subj_table.clear();
    subj_table = null;
  }
}


/*
   The ThreadContainer object is used to encapsulate an IThreadable object
   (it holds some intermediate state used while threading.)  This is a
   private class that doesn't escape from this module.
 */

class ThreadContainer {
  IThreadable threadable;
  ThreadContainer parent;
  ThreadContainer child;
  ThreadContainer next;

//  Object debug_id;
//  int id2;
//  static int id2_pool = 0;

//  ThreadContainer() { id2 = ++id2_pool; }

//  public String toString() { return "<cont " +
//                               id2 + " " + debug_id +
//                               " [" + threadable + "] >"; }

//  public void debug_print(int depth) {
//    for (int i = 0; i < depth; i++) System.out.print("  ");
//    System.out.println(this);
//    if (child != null) child.debug_print(depth+1);
//    if (next != null) next.debug_print(depth);
//  }

  // Copy the ThreadContainer tree structure down into the underlying
  // IThreadable objects (that is, make the IThreadable tree look like
  // the ThreadContainer tree.)
  //
  void flush() {
    if (parent != null && threadable == null)
      // Only the root_node is allowed to not have a threadable.
      throw new Error("no threadable in " + this.toString());

    parent = null;

    if (threadable != null)
      threadable.setChild(child == null ? null : child.threadable);

    if (child != null) {
      child.flush();
      child = null;
    }

    if (threadable != null)
      threadable.setNext(next == null ? null : next.threadable);

    if (next != null) {
      next.flush();
      next = null;
    }

    threadable = null;
  }

  // Returns true if child is under self's tree.  This is used for
  // detecting circularities in the references header.
  boolean find_child(ThreadContainer target) {
    if (child == null)
      return false;
    else if (child == target)
      return true;
    else
      return child.find_child(target);
  }

  void reverseChildren() {
    if (child != null) {
      // nreverse the children (child through child.next.next.next...)
      ThreadContainer kid, prev, rest;
      for (prev = null, kid = child, rest = kid.next;
           kid != null;
           prev = kid, kid = rest, rest = (rest == null ? null : rest.next))
        kid.next = prev;
      child = prev;

      // then do it for the kids
      for (kid = child; kid != null; kid = kid.next)
        kid.reverseChildren();
    }
  }
}
