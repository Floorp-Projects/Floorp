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
 * Created: Jamie Zawinski <jwz@netscape.com>,  1 Dec 1997.
 */

package grendel.storage;

import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;
import calypso.util.Assert;

import java.util.Vector;

import java.io.InputStream;
import java.io.IOException;

import javax.mail.FetchProfile;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Store;
import javax.mail.event.MessageChangedEvent;
import javax.mail.event.MessageCountEvent;


/** This class implements a Folder representing a newsgroup.
 */
class NewsFolder extends FolderBase {

  static final boolean DEBUG = false;

  private Folder parent;
  private String name;
  private NewsStore store;

  // Whether the fMessages vector has been initialized.
  private boolean loaded = false;

  // Cached responses of the GROUP command.
  private int high_message = -1;
  private int low_message = -1;
  private int estimated_message_count = -1;

  // If true, GROUP gave us an error; this group is probably nonexistant.
  private boolean bogus_group_p = false;


  /** The maximum portion of the elements in an NNTP XOVER request which
      are allowed to be superfluous.  (Some amount of superfluity is good,
      since it reduces the number of client-server round-trips, at the cost
      of sending slightly more data per request.)
   */
  static final double ALLOWABLE_XOVER_WASTE = 0.25;

  /** The maximum number of articles which may be requested per NNTP XOVER
      request.  Larger values reduce the number of client-server round-trips;
      smaller values increase client responsiveness on slow connections.
   */
  static final int MAX_XOVER_REQUEST_SIZE = 50;


  NewsFolder(Store s, Folder parent, String name) {
    super(s);
    this.parent = parent;
    this.name = name;
    this.store = (NewsStore) s;
  }

  public char getSeparator() {
    // #### If we're in "all groups" mode, this will be '.'.
    // Otherwise, there is no hierarchy, and therefore no separator.
    return '\000';
  }

  public int getType() {
    return HOLDS_MESSAGES;
  }

  public String getName() {
    return name;
  }

  public String getFullName() {
    return name;
  }

  public Folder getParent() {
    return parent;
  }

  public Folder[] list(String pattern) {
    return null;
  }

  public Folder getFolder(String subfolder) {
    return null;
  }

  public boolean create(int type) {
    return false;
  }

  public boolean exists() {
    return !bogus_group_p;
  }
 
  protected void getMessageCounts() {
    int counts[] = store.getGroupCounts(this);
    if (counts == null) {
      bogus_group_p = true;
    } else {
      estimated_message_count = counts[0];
      low_message = counts[1];
      high_message = counts[2];
    }
  }

  // #### Is "unread" the right sense of "new" for NNTP?
  public boolean hasNewMessages() {
    return (getUnreadMessageCount() > 0);
  }

  /** Returns the total number of messages in the folder, or -1 if unknown.
      This includes deleted and unread messages.
    */
  public int getMessageCount() {
    if (bogus_group_p)
      return -1;
    if (estimated_message_count == -1)
      getMessageCounts();
    return estimated_message_count;
  }

  /** Returns the number of non-deleted messages in the folder, or -1 if
      unknown.  This includes unread messages.
    */
  public int getUndeletedMessageCount() {
    return getMessageCount();
  }

  /** Returns the number of unread messages in the folder, or -1 if unknown.
      This does not include unread messages that are also deleted.
    */
  public int getUnreadMessageCount() {
    if (bogus_group_p)
      return -1;
    if (estimated_message_count == -1)
      getMessageCounts();

    if (low_message == -1 || high_message == -1)
      return -1;

    NewsRC rc = store.getNewsRC();
    NewsRCLine nl = rc.getNewsgroup(getFullName());
    long unread = nl.countMissingInRange(low_message, high_message+1);
    return (int) unread;
  }


  /** Returns the number of bytes consumed by deleted but not expunged
      messages in the folder, or -1 if unknown.
    */
  long deletedMessageBytes() {
    return 0;
  }


  public void appendMessages(Message msgs[]) throws MessagingException {
    throw new MessagingException("can't append messages to newsgroups");
  }

  public void fetch(Message msgs[], FetchProfile fp) {
  }

  public Message[] expunge() throws MessagingException {
    return null;
  }

  public boolean delete(boolean value) {
    // #### this should do cancel
    Assert.NotYetImplemented("NewsFolder.delete()");
    return false;
  }

  public boolean renameTo(Folder f) {
    // Can't rename newsgroups.
    return false;
  }

  void ensureLoaded() {
    if (DEBUG)
      System.err.println("NF.ensureLoaded " + name);

    if (bogus_group_p)
      return;

    if (loaded)
      return;

    synchronized (this) {
      if (loaded)                   // double check
        return;

      if (estimated_message_count == -1)
        getMessageCounts();

      NewsRC rc = store.getNewsRC();
      NewsRCLine nl = rc.getNewsgroup(getFullName());

      long start = (int) nl.firstNonMember();
      long end = high_message;

      Assert.Assertion(start > 0);

      if (start < low_message) {
        // There is a message marked as unread in the newsrc file which
        // has expired from the news server (it is below the low water mark.)
        // Mark all messages up to (but not including) the low water mark as
        // read, to reduce the newsrc file size.
        //
        if (DEBUG)
          System.err.println("NEWSRC: low water mark is " + low_message +
                             " but first-unread is " + start + ".\n" +
                             "        marking 1-" + (low_message-1) +
                             " as read.");
        nl.insert(1, low_message);
        start = (int) nl.firstNonMember();
      }

      if (end < start)
        return;


      /* We want to get the data for *certain* message between `start'
         and `end' inclusive; but not necessarily *all* of them (there
         might be holes.)

         We can only do this by requesting ranges.

         * The fewer unneeded articles we request, the better.
         * The fewer range requests we make, the better.
         * But, keeping the length of the range requested relatively
           short is also good, to avoid hogging the pipe, and giving
           other users of the NNTP connection a chance to have their
           say.  In other words, there is a preferred range length.

         We can request a range that includes articles we don't
         specifically want (the holes.)

         Algorithm:

         - Move forward from start to end, accumulating ranges of
           articles to request.

         - Keep track of how many needed and unneeded articles we
           are requesting.

         - If adding a needed range (along with its preceeding
           unneeded range) to the set would push the ratio of
           needed/unneeded in the current request past a certain
           threshold, then break the request into two before this
           range: flush out what we've got, skip over the unneeded
           leading range (the unneeded messages before this range of
           needed messages), and start building a new request starting
           with the beginning of this range of needed messages.

         - Also, if adding a needed range would push the number of
           articles in the request past a certain number, break
           the request at that point: that is, break the current
           range in two.
       */

      long req_start = start;
      long total_in  = 0;
      long total_out = 0;

      if (DEBUG)
        System.err.println("NEWSRC: computing XOVER range for " +
                           start + "-" + end);

      long i = start;
      long last_read = nl.max();

      while (i <= end) {
        long range_start, range_end;    // this block of unread messages

        // Count up and skip over the following range of already-read
        // messages.  That is, the already-read messages that lie before
        // this range, and after the previous range.
        //
        range_start = (i >= last_read
                       ? i
                       : nl.firstNonMember(i, last_read+1));
        if (range_start == -1)
          range_start = i;

        if (range_start > end)
          break;

        // Count up and skip over the following range of unread messages.
        // That is, the unread messages lying after the read messages we
        // skipped above.
        //
        range_end = nl.nextMember(range_start) - 1;
        if (range_end < 0 || range_end > end)
          range_end = end;

        if ((range_end - req_start) >= MAX_XOVER_REQUEST_SIZE) {
          //
          // Adding this range, plus its leading junk, would push the
          // cumulative request over its maximum allowable size.

          long reduced_end = req_start + MAX_XOVER_REQUEST_SIZE - 1;
          Assert.Assertion(reduced_end < range_end);

          if (i == range_start) {
            //
            // This range has no leading junk: it is adjascent to the
            // previously-scheduled range.  Request only part of this
            // range, and start accumulating the next batch at the
            // point where we divided this range.
            //
            if (DEBUG)
              System.err.println("NEWSRC:   range " +
                                 range_start + "-" + range_end + " (" +
                                 (range_end - range_start + 1) + " + " +
                                 (range_start-i) + ") is large (+" +
                                 (i - req_start) + " = " +
                                 (range_end - req_start + 1) +
                                 ");\n          cont: " +
                                 req_start + "-" + reduced_end + " (" +
                                 (reduced_end - req_start + 1) + ")");

            store.openNewsgroup(this, req_start, reduced_end);
            i = reduced_end+1;
            req_start = i;
            total_in  = 0;
            total_out = 0;
            // (we know that req_start is an unread message.)
            Assert.Assertion(!nl.member(req_start));

          } else {
            //
            // Adding this range, plus its leading junk, would make the
            // cumulative request size be too large.  So ask for the
            // already-accumulated batch (not including this range);
            // and start accumulating the next batch at the beginning of
            // this range (after its leading junk.)
            //
            if (DEBUG)
              System.err.println("NEWSRC:   range " +
                                 range_start + "-" + range_end + " (" +
                                 (range_end - range_start + 1) + " + " +
                                 (range_start-i) + ") is large (+" +
                                 (i - req_start) + " = " +
                                 (range_end - req_start + 1) +
                                 ");\n          load: " +
                                 req_start + "-" + (i-1) + " (" +
                                 (i - req_start) + ")");

            store.openNewsgroup(this, req_start, (i-1));
            Assert.Assertion(i < range_start);
            i = range_start;
            req_start = i;
            total_in  = 0;
            total_out = 0;
            // (we know that req_start is an unread message, because
            // it points to the begining of this range.)
            Assert.Assertion(!nl.member(req_start));
          }

        } else {
          //
          // The range, plus its leading junk, satisfied the size constraint.
          // Now see if it satisfies the allowable-waste constraint.

          long range_in  = (range_end - range_start) + 1;
          long range_out = range_start - i;
          double waste;
          {
            long in  = (total_in  + range_in);
            long out = (total_out + range_out);
            waste = ((double) out) / ((double) (in + out));
            Assert.Assertion(waste >= 0.0 && waste <= 1.0);
          }

          if (waste > ALLOWABLE_XOVER_WASTE) {
            //
            // Adding this range, plus its leading junk, would push the
            // cumulative request over its "acceptable junk" ratio.
            // So ask for the already-accumulated batch (not including
            // this range); and start accumulating the next batch at the
            // beginning of this range (after its leading junk.)
            //
            if (DEBUG)
              System.err.println("NEWSRC:   range " +
                                 range_start + "-" + range_end +
                                 " plus leading waste (" +
                                 i + "-" + (range_start-1) +
                                 ") would mean " +
                                 (((int) (waste * 1000)) / 10.0) +
                                 "% waste; requesting " +
                                 req_start + "-" + i + " (" +
                                 (i - range_start + 1) + ")");

            store.openNewsgroup(this, req_start, i);
            req_start = range_start;
            total_in  = 0;
            total_out = 0;
            // (we know that req_start is an unread message.)
            Assert.Assertion(!nl.member(req_start));

          } else {
            //
            // Adding this range, plus its leading junk, is ok!
            //
            if (DEBUG)
              System.err.println("NEWSRC:   range " +
                                 range_start + "-" + range_end +
                                 " (" + (range_end - range_start + 1) + " + " +
                                 (total_out + range_out) + ") added; total " +
                                 (range_end - req_start + 1) +
                                 "; wasted " +
                                 (((int) (waste * 1000)) / 10.0) + "%");

            total_in  += range_in;
            total_out += range_out;

            i = range_end + 1;
            // If we're not done, then `i' points at a read message
            // (that is, it points at leading junk.)
            Assert.Assertion(i > end || nl.member(i));
          }
        }
      }

      // Got to end.  Is there still stuff in the request buffer?
      if (req_start <= end) {

        if (DEBUG)
          System.err.println("NEWSRC:   done: " +
                             req_start + "-" + end + " (" +
                             (end - req_start + 1) + ")");

        store.openNewsgroup(this, req_start, end);
      }
    }
  }


  public void open(int mode) {
    if (DEBUG)
      System.err.println("NF.open " + name);
    ensureLoaded();
  }

  public void close(boolean doExpunge) throws MessagingException {
    NewsRC rc = store.getNewsRC();
    if (rc != null) {
      try {
        rc.save();
      } catch (IOException e) {
        throw new MessagingException("error saving newsrc file", e);
      }
    }
    fMessages = new Vector();
  }

  public boolean isOpen() {
    return true;        // #### Wrong!
  }

  InputStream getMessageStream(NewsMessage message, boolean headers_too)
    throws IOException {
    Assert.Assertion(this == message.getFolder());
    return store.getMessageStream(message, headers_too);
  }


  /* #### combine this with BerkeleyFolder via MessageBase?
   */
  /** Assert whether any messages have had their flags changed.
      A child message should call this on its parent when any persistent
      flag value is changed.  If a non-null message is provided, and
      flags_dirty is true, then notify any observers that this message
      has changed.

      @param flags_dirty  Whether the flags should currently be considered
                          to be dirty.

      @param message      If flags_dirty, the Message (BerkeleyMessage) that
                          has become dirty.

      @param old_flags    If message is non-null, the previous value of its
                          flags (Message.flags should be the new, dirty value.)
    */
  void setFlagsDirty(boolean flags_dirty, Message message, long old_flags) {

    // If the read-ness of a message has changed, flush the new counts down
    // into the newsrc object.
    //
    if (flags_dirty && message != null) {
      long new_flags = ((MessageBase)message).flags;
      long interesting = (MessageBase.FLAG_READ);
      if ((old_flags & interesting) != (new_flags & interesting)) {
        NewsRC rc = store.getNewsRC();
        NewsRCLine nl = rc.getNewsgroup(getFullName());
        int n = ((NewsMessage)message).getStorageFolderIndex();
        if (n < 0)
          ;  // Eh?
        else if ((new_flags & MessageBase.FLAG_READ) != 0)
          nl.insert(n);
        else
          nl.delete(n);
      }
    }

    if (flags_dirty && message != null) {
      notifyMessageChangedListeners(MessageChangedEvent.FLAGS_CHANGED,
                                    message);
    }
  }

}
