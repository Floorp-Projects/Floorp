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
 * Created: Terry Weissman <terry@netscape.com>, 27 Aug 1997.
 */

package grendel.storage;

import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;
import calypso.util.Assert;

import grendel.storage.intertwingle.Twingle;
import grendel.util.Constants;

import java.lang.Thread;
import java.lang.SecurityException;
import java.lang.InterruptedException;
import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;
import java.io.FilenameFilter;
import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.text.DecimalFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.Enumeration;
import java.util.GregorianCalendar;
import java.util.Vector;

import javax.mail.FetchProfile;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Store;
import javax.mail.event.MessageChangedEvent;
import javax.mail.event.MessageCountEvent;

import javax.mail.internet.InternetHeaders;


class BerkeleyFolder extends FolderBase implements FilenameFilter {

  File fFile;                           // the underlying disk file

  Folder parent;

  boolean fLoaded = false;              // whether the folder has been parsed
  private boolean fLoading = false;     // true while in the process of parsing

  private boolean summary_dirty = false; // whether changes have been made that
                                         // would necessitate updating the
                                         // summary file.

  private boolean flags_dirty = false;   // whether any of the messages in
                                         // this folder have the FLAG_DIRTY
                                         // bit set (meaning we need to flush
                                         // out some X-Mozilla-Status headers.)

  /* The object that knows how to read and (possibly) write the summary file.
     It will be null if there is no summary file, or if the summary file
     is in a format we don't understand.  If fLoaded is true, this will always
     be non-null.  If fLoaded is false, this might be null or might not be.
   */
  MailSummaryFile mailSummaryFile = null;

  BerkeleyFolder(Store s, File f) {
    super(s);
    fFile = f;
  }

  protected BerkeleyFolder(BerkeleyFolder parent, String name) {
    this(parent.getStore(), new File(parent.fFile, name));
  }

  protected BerkeleyFolder(BerkeleyFolder parent, File f) {
    super(parent.getStore());
    fFile = f;
  }

  public char getSeparator() {
    return File.separatorChar;
  }

  public int getType() {
    return HOLDS_MESSAGES | HOLDS_FOLDERS;
  }


  /** Returns the file name associated with this folder. */
  File getFile() {
    return fFile;
  }

  public String getName() {
    return fFile.getName();
  }

  public String getFullName() {
    return fFile.getAbsolutePath(); // ### This isn't quite right!
  }

  public Folder getParent() {
    return parent;
  }

  public boolean accept(File f, String name) {
    int L = name.length();
    if (L == 0) return false;
    if (name.charAt(0) == '.') return false;
    if (name.charAt(L-1) == '~') return false;
    if (L < 4) return true;
    if (name.regionMatches(true, L-4, ".SNM", 0, 4)) return false;
    if (name.regionMatches(true, L-4, ".DAT", 0, 4)) return false;
    if (name.regionMatches(true, L-4, ".LOG", 0, 4)) return false;
    return true;
  }

  public Folder[] list(String pattern) {
    if (pattern.equals("%")) {
      String list[] = fFile.list(this);
      if (list == null || list.length == 0) return null;
      Vector result = new Vector();
      for (int i=0 ; i<list.length ; i++) {
        File sub = new File(fFile, list[i]);
        if (sub.isDirectory()) continue;
        result.addElement(new BerkeleyFolder(this, sub));
      }
      Folder folders[] = new Folder[result.size()];
      result.copyInto(folders);
      return folders;
    }
    Assert.NotYetImplemented("BerkeleyFolder.list");
    return null;
  }

  public Folder getFolder(String subfolder) {
    return new BerkeleyFolder(this, subfolder);
  }

  public boolean create(int type) {
    Assert.NotYetImplemented("Folder.create");
    return false;
  }

  public boolean exists() {
    return fFile.exists();
  }

  public boolean hasNewMessages() {
    return getUnreadMessageCount() > 0;
  }

  /** Returns the total number of messages in the folder, or -1 if unknown.
      This includes deleted and unread messages.
    */
  public int getMessageCount() {
    if (fMessages != null)
      return fMessages.size();
    ensureSummaryFileHeaderParsed();
    if (mailSummaryFile != null)
      return mailSummaryFile.totalMessageCount();
    else if (!fFile.exists() || fFile.length() < 5)
      return 0;
    else
      return -1;
  }

  /** Returns the number of non-deleted messages in the folder, or -1 if
      unknown.  This includes unread messages.
    */
  public int getUndeletedMessageCount() {
    ensureSummaryFileHeaderParsed();
    if (mailSummaryFile != null)
      return mailSummaryFile.undeletedMessageCount();
    else if (!fFile.exists() || fFile.length() < 5)
      return 0;
    else
      return -1;
  }

  /** Returns the number of unread messages in the folder, or -1 if unknown.
      This does not include unread messages that are also deleted.
    */
  public int getUnreadMessageCount() {
    ensureSummaryFileHeaderParsed();
    if (mailSummaryFile != null)
      return mailSummaryFile.unreadMessageCount();
    else if (!fFile.exists() || fFile.length() < 5)
      return 0;
    else
      return -1;
  }


  /** Returns the number of bytes consumed by deleted but not expunged
      messages in the folder, or -1 if unknown.
    */
  long deletedMessageBytes() {
    ensureSummaryFileHeaderParsed();
    if (mailSummaryFile != null)
      return mailSummaryFile.deletedMessageBytes();
    else if (!fFile.exists() || fFile.length() < 5)
      return 0;
    else
      return -1;
  }


  /** If the messages are not known, parse the underlying files.
      If the folder file has changed on disk since it was parsed,
      re-parse it (replacing the existing message objects with new
      ones.)
    */
  void ensureLoaded() {
    Assert.Assertion(!fLoading);

    if (!fLoaded) {
      synchronized (this) {
        if (!fLoaded)                   // double check, to avoid race.
          loadMessages();
      }

    } else {    // it was already loaded once; see if we need to reload.

      if (verifyFileDate(false))        // file (probably) hasn't changed
        return;

      synchronized (this) {

        if (verifyFileDate(true))       // double check, to avoid race.
          return;

        System.err.println("warning: file " + fFile +
                           " has changed; re-loading!");

        // notify all the old messages as deleted
        Message[] oldlist = new Message[fMessages.size()];
        fMessages.copyInto(oldlist);
        notifyMessageRemovedListeners(true /*???*/, oldlist);

        fMessages.setSize(0);         // nuke the old messages
        fLoaded = false;
        summary_dirty = false;
        flags_dirty = false;
        loadMessages();               // read in the new messages

        // notify all the new messages as added
        Message[] newlist = new Message[fMessages.size()];
        fMessages.copyInto(newlist);
        notifyMessageAddedListeners(newlist);
      }
    }
  }

  private synchronized void loadMessages() {
    // Do the timebomb.
//    java.util.Date before = new Date(97, 9, 1, 0, 0);
//    java.util.Date now = new Date();
//    java.util.Date then = new Date(97, 11, 25, 12, 00);
//    if (now.before(before) || now.after(then)) {
//      System.err.println("This software has expired");
//      System.exit(-1);
//    }

    Assert.Assertion(!fLoading && !fLoaded);

    summary_dirty = false;
    try {    // make sure unlockFolderFile() gets called.
      fLoading = true;
      lockFolderFile();

      ParseBerkeleyFolderWithSummary parser =
        new ParseBerkeleyFolderWithSummary();
      RandomAccessFile in = null;
      try {
        try {
          in = new RandomAccessFile(fFile, "r");
          parser.mapOverMessages(in, this);

          // If there is no summary object, or an unwritable one, use Grendel.
          // Do this now so that the newly-created summary object gets the
          // proper date/size from the folder file (date/size that corresponds
          // to the time at which we parsed the folder.)
          //
          // Also mark it dirty, so that the background thread will write a
          // new file in Grendel format (creating a summary where there was
          // none, or auto-upgrading the old format to the new one.)
          //
          if (mailSummaryFile == null || !mailSummaryFile.writable()) {
            mailSummaryFile = new MailSummaryFileGrendel(this);
            setSummaryDirty(true);
          }

        } finally {
          if (in != null)
            in.close();
        }
      } catch (IOException e) {
        System.err.println("error parsing folder " + fFile + ": " + e);
        e.printStackTrace();
        throw new Error("Argh!  Couldn't open mailbox folder!");
      }
    } finally {
      unlockFolderFile();
      fLoading = false;
    }

    Assert.Assertion(!fLoaded);
    fLoaded = true;

    if (summary_dirty) {
      // Kludge to deal properly with dirtying that occurs during parsing:
      // delay the spawning of the background thread until after parsing
      // is complete.
      //
      // #### Perhaps instead of spawning the thread to write the summary,
      // we should just write it now?  If we have parsed the folder from
      // scratch, we've already done a lot of I/O, and a little more won't
      // hurt.
      summary_dirty = false;
      setSummaryDirty(true);
    }
  }


  /** Assert whether the folder's summary file should be regenerated.
    */
  void setSummaryDirty(boolean summary_dirty) {
    if (!summary_dirty ||
        (this.summary_dirty == summary_dirty)) {
      // no change, or being marked as non-dirty.
      this.summary_dirty = summary_dirty;

    } else if (fLoading) {
      // being marked as dirty while loading.  Mark it dirty, but don't spawn.
      this.summary_dirty = summary_dirty;

    } else if (!fLoaded) {
      // being marked as dirty, but not loaded (or loading.)  Ignore it.

    } else {
      // being marked as dirty.
      // Add this folder to the update-thread's list, but make sure we don't
      // add it more than once.
      synchronized (this) {
        if (this.summary_dirty) return;  // double-check that we didn't lose
                                         // a race.
        this.summary_dirty = summary_dirty;
        SummaryUpdateThread t = SummaryUpdateThread.Get();
        t.addDirtyFolder(this);
      }
    }
  }

  // The per-folder thread that flushes X-Mozilla-Status changes out to the
  // file.  Reads and writes of this variable must be synchronized.
  Thread statusFlagsUpdateThread = null;

  /** Assert whether any messages have the FLAGS_DIRTY bit set in them.
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

    if (flags_dirty && fLoaded && !fLoading)
      // This is as good a time as any to make sure the disk file hasn't
      // changed (note that this is lazy: the disk isn't pegged each time
      // ensureLoaded() is called.)
      ensureLoaded();

    // If the deleted-ness or read-ness of a message has changed, flush
    // the new counts down into the summary object.
    if (flags_dirty && message != null) {
      long new_flags = ((MessageBase)message).flags;
      long interesting = (MessageBase.FLAG_DELETED | MessageBase.FLAG_READ);
      if ((old_flags & interesting) != (new_flags & interesting)) {
        int msglen = ((BerkeleyMessage)message).fLength;
        updateSummaryMessageCounts(old_flags, new_flags, msglen);
      }
    }

    if (!flags_dirty ||
        (this.flags_dirty == flags_dirty)) {
      // no change, or being marked as non-dirty.
      this.flags_dirty = flags_dirty;

    } else if (fLoading) {
      // being marked as dirty while loading.  Mark it dirty, but don't spawn.
      this.flags_dirty = flags_dirty;

    } else if (!fLoaded) {
      // being marked as dirty, but not loaded (or loading.)  Ignore it.

    } else {
      // being marked as dirty.
      // Spawn a thread to update this folder's X-Mozilla-Status headers
      // in a few seconds, if there's not already one around.
      synchronized (this) {
        this.flags_dirty = flags_dirty;
        if (statusFlagsUpdateThread == null ||
            !statusFlagsUpdateThread.isAlive()) {
          statusFlagsUpdateThread = new StatusFlagsUpdateThread(this);
          statusFlagsUpdateThread.start();
        }
      }
    }
    if (flags_dirty && message != null) {
      notifyMessageChangedListeners(MessageChangedEvent.FLAGS_CHANGED,
                                    message);
    }
  }


  /** If the deleted-ness or read-ness of a message has changed, flush
      the new counts down into the summary object.
    */
  protected void updateSummaryMessageCounts(long old_flags, long new_flags,
                                            int msglen) {
    Assert.Assertion(msglen > 0);
    ensureSummaryFileHeaderParsed();
    if (mailSummaryFile == null)
      return;

    int total   = mailSummaryFile.totalMessageCount();
    int undel   = mailSummaryFile.undeletedMessageCount();
    int unread  = mailSummaryFile.unreadMessageCount();
    long dbytes = mailSummaryFile.deletedMessageBytes();

    if ((old_flags & MessageBase.FLAG_DELETED) !=
        (new_flags & MessageBase.FLAG_DELETED)) {
      //
      // deleted-ness of a message changed.
      //
      if ((new_flags & MessageBase.FLAG_DELETED) != 0) {    // became deleted
        Assert.Assertion(undel > 0);
        if (undel > 0) {
          undel--;
          dbytes += msglen;
        }
      } else {                                              // became undeleted
        Assert.Assertion(undel < total);
        if (undel < total) {
          undel++;
          Assert.Assertion(dbytes >= msglen);
          if (dbytes >= msglen)
            dbytes -= msglen;
        }
      }

    } else if (((new_flags & MessageBase.FLAG_DELETED) == 0) &&
               (old_flags & MessageBase.FLAG_READ) !=
               (new_flags & MessageBase.FLAG_READ)) {
      //
      // read-ness of a non-deleted message changed.
      //
      if ((new_flags & MessageBase.FLAG_READ) != 0) {          // became read
        Assert.Assertion(unread > 0);
        if (unread > 0)
          unread--;
      } else {                                                 // became unread
        Assert.Assertion(unread < total);
        if (unread < total)
          unread++;
      }
    } else {
      Assert.Assertion(false);
    }

    mailSummaryFile.setFolderMessageCounts(total, undel, unread, dbytes);
  }


  protected void ensureSummaryFileHeaderParsed() {
    // If there isn't a summary file object, presumably this is because the
    // messages of this folder aren't loaded.  Examine the summary file on
    // disk, if any, and make a summary object corresponding to it.
    //
    if (mailSummaryFile == null) {
      Assert.Assertion(!fLoaded);
      synchronized (this) {
        if (mailSummaryFile == null) {   // double-check
          try {  // ignore errors while reading the summary file.
            InputStream stream = null;
            try {    // be sure to close stream
              File name = summaryFileName();
              stream = new BufferedInputStream(new FileInputStream(name));
//              System.err.println("opened (unloaded) summary file " + name);
              mailSummaryFile =
                MailSummaryFileFactory.ParseFileHeader(this, stream);
            } finally {
              if (stream != null) stream.close();
            }
          } catch (IOException e) {
          } catch (SecurityException e) {
          }
        }
      }
    }
  }


  private long verify_file_tick = 0;

  /** Returns false if the disk file has changed unexpectedly.

      @param force   If true, guarentee that the disk will be checked, and
                     the returned value will be accurate.  If false, the
                     disk will not be checked more frequently than once
                     a second, so true might be returned without verification.

                     <P> That is, if `force' is false, and this method returns
                     true, the disk might or might not be in sync.  However,
                     regardless of the value of `force', if false is returned,
                     the disk is definitely out of sync.
    */
  protected boolean verifyFileDate(boolean force) {
    if (!fLoaded) return true;

    Assert.Assertion(mailSummaryFile != null);

    long now = System.currentTimeMillis();

    // If less than a second has passed, assume everything is ok.
    if (!force &&
        verify_file_tick + 1000 > now)
      return true;

    synchronized (this) {
      verify_file_tick = now;
      if (mailSummaryFile.folderDate() != fFile.lastModified() ||
          mailSummaryFile.folderSize() != fFile.length())
        return false;
      else
        return true;
    }
  }


  /** Call this to inform the summary file module that the folder file
      has just been modified.  (The code managing the summary file may wish
      to act on this information in some way.  With our current summary file
      format, failing to so notice it will cause the folder to be re-parsed
      the next time, since we won't be able to tell the difference between
      some *other* program modifying the file instead of us.)

      <P> Call this with the folder file still locked, to avoid a potential
      race.

      @see MailSummaryFile
      @see MailSummaryFileGrendel
   */
  protected void updateSummaryFile() {

    ensureSummaryFileHeaderParsed();

    if (mailSummaryFile != null &&
        mailSummaryFile.writable()) {
      synchronized (this) {
        try {
          mailSummaryFile.updateSummaryFile();
        } catch (IOException e) {
          // Ignore it, I guess...
        }
      }
    } else if (fLoaded) {
      // If there was no summary file, or if it was in an unknown format,
      // or if it was in a format that we don't know how to write, then
      // schedule a modern-format summary file to be written.  We can
      // do this only when the messages are loaded: if the messages aren't
      // loaded, we might have a MailSummaryFile object, but we won't
      // be able to write anything meaningful into it.
      setSummaryDirty(true);
    }
  }


  /** Returns the name of the mail summary file used for this folder.
      This name is OS-dependent; it is assumed that the data within the
      file will contain versioning information.
      <P>
      On all systems, the summary file resides in the same directory
      as the folder file.
      <P>
      On Unix, the summary file for <TT>FOO</TT> would be
      <TT>.FOO.summary</TT> (note leading dot.)
      <P>
      On Windows and Mac, the summary file for <TT>FOO.MBX</TT> would be
      <TT>FOO.SNM</TT>.
    */
  File summaryFileName() {
    File file = getFile();
    String dir = file.getParent();
    String name = file.getName();

    if (Constants.ISUNIX) {
      name = "." + name + ".summary";
    } else {
      int i = name.lastIndexOf(".");
      if (i > 0)
        name = name.substring(0, i);
      name = name + ".snm";
    }

    return new File(dir, name);
  }


  /** Returns the name of a non-existent file that can be used as a temporary
      file for this folder.  This name is OS-dependent.
      <P>
      On all systems, the summary file resides in the same directory
      as the folder file, and has the same "base" name.
      <P>
      On Unix, the summary file for <TT>FOO</TT> would be
      <TT>.FOO.ns_tmp</TT> (note leading dot.)
      <P>
      On Windows and Mac, the summary file for <TT>FOO.MBX</TT> would be
      <TT>FOO.TMP</TT>.
    */
  File tempFolderName() {
    File file = getFile();
    String dir = file.getParent();
    String name = file.getName();

    if (Constants.ISUNIX) {
      name = "." + name + ".ns_tmp";
    } else {
      int i = name.lastIndexOf(".");
      if (i > 0)
        name = name.substring(0, i);
      name = name + ".tmp";
    }

    return new File(dir, name);
  }





  /** Write a summary file for this folder.
      @see MailSummaryFile
      @see MailSummaryFileGrendel
    */
  synchronized void writeSummaryFile() throws IOException {
    Assert.Assertion(fLoaded);
    if (!fLoaded) return;
    if (!summary_dirty) return;
    setSummaryDirty(false);  // do this first
    mailSummaryFile.writeSummaryFile();
  }


  /** This causes the X-Mozilla-Status headers in the folder file itself
      to be updated.  (Not to be confused with the summary file: this
      modifies the <I>folder</I>.)  If a changed message's bits in the
      file include an X-Mozilla-Status header, it will be overwritten
      (random-access.)  If the header is not present, the message will be
      unchanged (and in that situation, the flags will only live in the
      summary file, not the folder itself; and if the summary file gets
      blown away, the flags will be lost.  That's why we try to have an
      X-Mozilla-Status header on every message.)
   */
  synchronized void flushMozillaStatus() throws IOException {

    Assert.Assertion(!fLoading);
    if (!fLoaded) return;
    if (!flags_dirty) return;

    // check file dates to make sure it hasn't changed from under us.
    ensureLoaded();

    if (!flags_dirty) return;   // double-check, in case reloaded

    setFlagsDirty(false, null, 0);
    boolean wrote_anything = false;

    try {    // make sure unlockFolderFile() gets called.
      lockFolderFile();

      ParseBerkeleyFolder parser = new ParseBerkeleyFolder();
      RandomAccessFile io = null;
      try {
        io = new RandomAccessFile(fFile, "rw");

        for (Enumeration e = fMessages.elements(); e.hasMoreElements();) {
          BerkeleyMessage m = (BerkeleyMessage) e.nextElement();
          if (m.flagsAreDirty()) {
            parser.flushMozillaStatus(io,
                                      m.getStorageFolderIndex(),
                                      m.getSize(),
                                      (short) m.makeMozillaFlags(m));
            m.setFlagsDirty(false);
            wrote_anything = true;
          }
        }

      } finally {
        if (io != null)
          io.close();
      }

      // At this point, the file has been written, and closed, without error.
      // Update the date/size that will be encoded into the summary file the
      // next time it is written.
      //
      // Don't call updateSummaryFile() now, which would update the dates in
      // the existing disk summary file.  Wait until writeSummaryFile() is
      // called before putting the above dates on disk.  The reason for this
      // is, if we *have* written the X-Mozilla-Status headers into the
      // folder, but have *not* written a new summary file, the folder has
      // more current flags than the summary, so it would be appropriate to
      // discard the summary file and re-parse the folder.
      //
      // However, this situation (re-parsing) can only occur if the program
      // dies after the StatusFlagsUpdateThread runs, but before the
      // SummaryUpdateThread runs.
      //
      mailSummaryFile.setFolderDateAndSize(fFile.lastModified(),
                                           fFile.length());

    } finally {
      unlockFolderFile();
    }

    if (wrote_anything)
      // Since flags changed, mark the summary file as needing (eventually)
      // to be regenerated as well.
      setSummaryDirty(true);
  }




  public void appendMessages(Message msgs[]) throws MessagingException {
    try {
      for (int i=0 ; i<msgs.length ; i++) {
        addMessage(msgs[i]);
      }
    } catch (IOException e) {
      throw new MessagingException(e.toString());
    }
  }



  /** Add a message to the end of this folder.
      The disk file will be updated.
    */
  void addMessage(Message m) throws IOException, MessagingException {
    MessageExtra mextra = MessageExtraFactory.Get(m);


    // If this folder is loaded, check the file dates and re-parse it
    // first, if necessary, to make sure it hasn't changed from under us.
    // But if it's not loaded, don't force a load.
    if (fLoaded)
      ensureLoaded();

    InputStream in = null;
    RandomAccessFile out = null;
    boolean file_existed = fFile.exists();
    long old_file_size = -1;
    boolean completed_normally = false;
    InternetHeaders headers = null;
    long bytes_written = 0;

    try {    // make sure unlockFolderFile() gets called.
      lockFolderFile();

      try {    // make sure in.close() and out.close() get called.

        in = mextra.getInputStreamWithHeaders();
        out = new RandomAccessFile(fFile, "rw");
        old_file_size = out.length();
        out.seek(old_file_size);

        headers = copyMessageBytes(m, in, out);       // do the deed

        bytes_written = out.length() - old_file_size;

        completed_normally = true;

      } finally {

        if (in != null)
          in.close();

        if (out != null) {

          if (completed_normally) {
            out.close();

            // Else, an error happened while appending to the file.  At this
            // point, we have written incomplete data to the file.  Try and
            // clean up after our damage.

          } else if (!file_existed) {
            // If the file didn't exist before we started, just delete it.
            out.close();
            out = null;
            fFile.delete();

          } else if (old_file_size < 0) {
            // never wrote anything.  No problem.
            out.close();

          } else {
            // Truncate the file back to the length it had before we started
            // appending to it.
            //
            // #### call out.ftruncate(old_file_size) here!
            // #### I don't know how to do this in Java.
            // #### in the meantime, let's append "\n\n" to make sure
            // #### we didn't write any partial lines that will cause
            // #### subsequently-appended messages to be swallowed.
            //
            ByteBuf buf = new ByteBuf();
            buf.setLength(0);
            buf.append(Constants.LINEBREAK);
            buf.append(">> error! truncated!");
            buf.append(Constants.LINEBREAK);
            buf.append(Constants.LINEBREAK);
            try {
              out.write(buf.toBytes(), 0, buf.length());
            } catch (IOException e) {
              // just ignore it; we're writing in service of error recovery.
            }

            out.close();
          }
        }
      }


      // Update the folder's statistics about what is contained in it.
      //
      ensureSummaryFileHeaderParsed();
      if (mailSummaryFile != null) {
        int total   = mailSummaryFile.totalMessageCount();
        int undel   = mailSummaryFile.undeletedMessageCount();
        int unread  = mailSummaryFile.unreadMessageCount();
        long dbytes = mailSummaryFile.deletedMessageBytes();

        total++;
        if (!mextra.isRead())
          unread++;

        if (mextra.isDeleted())
          dbytes += bytes_written;
        else if (undel != -1)
          undel++;

        // Inform the summary file of the new statistics.
        //
        mailSummaryFile.setFolderMessageCounts(total, undel, unread, dbytes);
      }

      // At this point, the file has been written, and closed, without error.
      // Update the date/size encoded into the summary file (without
      // rewriting the whole summary file itself.)
      //
      updateSummaryFile();

    } finally {
      unlockFolderFile();
    }

    // Everything has been written to disk without error (both folder
    // and summary file.)  Now, create an in-memory message representing
    // the new message, and let any observers know that this message has
    // been added to the folder.
    //
    BerkeleyMessage newmessage = new BerkeleyMessage(this, headers);

    // Add this message to the list of messages in this folder, *if* this
    // folder knows what messages are in it.  If we haven't yet parsed
    // the folder, don't make it appear that the folder has only one
    // message in it; rather, just wait until it gets parsed for real.
    // #### fix me wrt observers
    if (fLoaded) {
      fMessages.addElement(newmessage);
    }

    Twingle twingle = Twingle.GetSingleton();
    if (twingle != null) {
      twingle.add(headers, this);
    }

    Message list[] = { newmessage };
    notifyMessageAddedListeners(list);
  }


  public void fetch(Message msgs[], FetchProfile fp) {}

  /** Remove all messages that are marked as deleted from the folder's file.
      This is done by writing a new file, then renaming it.

      @throws IOException  if the file could not be expunged.  A likely
                           cause would be running out of disk space, since
                           the peak file usage can be up to 2x the size of
                           the original folder.
    */
  public Message[] expunge() throws MessagingException {
    Message[] result = null;
    try {
      result = realExpunge();
    } catch (IOException e) {
      throw new MessagingException(e.toString());
    }
    return result;
  }

  protected synchronized Message[] realExpunge() throws IOException {


    // To compact a folder, we must have its message list around.
    // This (probably) means parsing the summary file, which is important,
    // because the summary file might be the only place where the proper
    // flags live.  One of the things that expunging does is ensure that
    // every message in the folder has an X-Mozilla-Status header.  The
    // absence of that header is what can cause the situation that the
    // summary file contains non-recomputable data.
    //
    // The pessimal case here is where we're compacting a folder that doesn't
    // have a summary file: that will cause it to be fully parsed, and then
    // read again to compact it.  There's not a lot we can do about that.
    //
    ensureLoaded();
    Assert.Assertion(fLoaded);
    Assert.Assertion(mailSummaryFile != null);

    // Before compacting the folder, ensure that any in-memory changes to
    // the flags have been flushed out.
    if (flags_dirty)
      flushMozillaStatus();

    boolean everything_completed_normally = false;

    Vector old_msgs = fMessages;
    Assert.Assertion(old_msgs != null);

    try {       // if something goes wrong, restore the value of fMessages

      // We need to keep two copies of the folder's Message objects:
      //
      //  *  we need the old messages around in case there is an error,
      //     and we need to back out our changes;
      //
      //  *  we need the new messages so that we can correctly write a
      //     new summary file.
      //
      // Since the second *should* be a subset of the first, it would be
      // good if we could manage to share the data; but I don't see an easy
      // way to do that, and still be able to back out the changes if an error
      // occurs (the expunging process wants to record a new file-offset into
      // the message objects, among other things.)
      //
      // Another reason for keeping them is so that, at the end, we can tell
      // all the observers that the first set is gone and the new set exists.
      // We don't want to tell the observers anything if the attempt to
      // compact the disk file is abortive.
      //
      // This does not mean that we're using twice as much memory, since all
      // of these messages will be sharing strings and IDs by virtue of the
      // ByteStringTable and MessageIDTable.
      //
      fMessages = new Vector(old_msgs.size());


      try {    // make sure unlockFolderFile() gets called.
        lockFolderFile();

        File tmp_file = tempFolderName();

        try {    // make sure the temp file gets deleted, if there's an error

          RandomAccessFile in = null;
          RandomAccessFile out = null;
          boolean io_completed_normally = false;

          try {    // make sure in.close() and out.close() get called.

            in =  new RandomAccessFile(fFile, "r");

            // make sure the tmp file doesn't exist (we have the folder locked,
            // so if it does exist, it's probably left over from a crash.)
            tmp_file.delete();

            out = new RandomAccessFile(tmp_file, "rw");

            ParseBerkeleyFolderAndExpunge expungator =
              new ParseBerkeleyFolderAndExpunge();
            expungator.expungeDeletedMessages(this, in, out,    // do the deed
                                              old_msgs.elements());

            int total   = expungator.totalMessageCount();
            int undel   = expungator.undeletedMessageCount();
            int unread  = expungator.unreadMessageCount();
            long dbytes = expungator.deletedMessageBytes();
            Assert.Assertion(total == undel);
            Assert.Assertion(dbytes == 0);
            mailSummaryFile.setFolderMessageCounts(total, undel,
                                                   unread, dbytes);

            io_completed_normally = true;

          } finally {   // close in and out, and (if we didn't get an error)
            // move the temp-file.
            if (in != null)
              in.close();
            if (out != null)
              out.close();

            if (io_completed_normally) {
              // Move the temp file to the folder file.  This might get an
              // error; that's ok, it will be caught because tmp_file will
              // not be null.
              tmp_file.renameTo(fFile);
              tmp_file = null;
            }
          }

        } finally {
          // if temp_file is non-null, then an error occurred.
          if (tmp_file != null) {
            try {
              tmp_file.delete();
            } catch (SecurityException e) {
            }
          }
        }

        // At this point, the folder file has been successfully overwritten
        // with new data.  Write out a new summary file right now (we've
        // already done a ton of disk I/O, a little bit more won't hurt.)
        //
        Assert.Assertion(fLoaded);
        summary_dirty = true;
        mailSummaryFile.setFolderDateAndSize(fFile.lastModified(),
                                             fFile.length());
        writeSummaryFile();

      } finally {
        unlockFolderFile();
      }

      everything_completed_normally = true;

    } finally { // if something went wrong, restore the value of fMessages

      if (!everything_completed_normally)
        // something blew up; put back the old messages and pretend
        // we didn't do anything.
        fMessages = old_msgs;
    }

    // At this point, the new, compacted folder is on disk, and it has a
    // summary.  All that remains is to let the observers (if any) know
    // that the message objects have changed.
    //
    // #### We could concievably do something clever here, by "zippering" the
    // two message lists together.  What we have (above) in fMessages and
    // old_msgs looks something like
    //
    //      old_msgs:  A1 B1 C1 D1 E1 F1 G1
    //     fMessages:  A2    C2    E2 F2
    //
    // A1 and A2 are non-identical BerkeleyMessage objects that represent
    // the same message on disk.  There are two things we could do:
    //
    //  Option 1:
    //    * have an observable event that says "A2 replaces A1."
    //    * have an observable event that says "B1 has been expunged."
    //
    //  Option 2:
    //
    //    * copy the new contents of A2 into A1, and discard A2.
    //    * have an observable event that says "A1 has changed."
    //    * have an observable event that say "B1 has been expunged."
    //
    // I think I lean towards #2.  But what we do today is:
    //
    //    * say "A1 has been deleted."
    //    * say "A2 has been added".
    //
    // Actually, all of the comment above was written before we went to
    // javamail APIs.  Those APIs require approach #2 (well, I don't think
    // we need the "A1 has changed" part), and so I attempt to
    // implement that here... -- Terry

    int oldsize = old_msgs.size();
    int newsize = fMessages.size();
    int j=0;
    NEXTMESSAGE:
    for (int i=0 ; i<newsize ; i++) {
      BerkeleyMessage newmsg = (BerkeleyMessage) fMessages.elementAt(i);
      while (j < oldsize) {
        BerkeleyMessage old = (BerkeleyMessage) old_msgs.elementAt(j);
        if (old.getMessageID() == newmsg.getMessageID()
            // && Need more tests! ###
            ) {
          old.setStorageFolderIndex(newmsg.getStorageFolderIndex());
          old.setSize(newmsg.getSize());
          fMessages.setElementAt(old, i);
          old_msgs.removeElementAt(j);
          oldsize--;
          continue NEXTMESSAGE;
        }
        j++;
      }
      // Yikes!!!  Somehow, we got new messages that didn't line up with
      // the old ones.  Um, well, such is life.  Send a notification out that
      // new messages appeared...
      Vector newlist = new Vector();
      for ( ; i<newsize ; i++) {
        newlist.addElement(fMessages.elementAt(i));
      }
      Message[] tmp = new Message[newlist.size()];
      newlist.copyInto(tmp);
      notifyMessageAddedListeners(tmp);

      break;
    }

    for (j=0 ; j<oldsize ; j++) {
      ((BerkeleyMessage) old_msgs.elementAt(j)).setDeleted(true);
    }

    Message[] result = new Message[old_msgs.size()];
    old_msgs.copyInto(result);
    notifyMessageRemovedListeners(true/*???*/, result);
    return result;
  }


  private InternetHeaders copyMessageBytes(Message m,
                                           InputStream in,
                                           RandomAccessFile out)
    throws IOException {
    InternetHeaders headers = new InternetHeaders();
    ByteBuf buf = null;
    ByteBuf line = null;

    try {
      buf  = ByteBuf.Alloc();
      line = ByteBuf.Alloc();

      // buf.append(Constants.LINEBREAK);  // assume file ends in newline
      buf.append(Constants.LINEBREAK);
      makeDummyEnvelope(buf);
      out.write(buf.toBytes(), 0, buf.length());

      ByteLineBuffer linebuf = new ByteLineBuffer();
      linebuf.setOutputEOL(Constants.BYTEBUFLINEBREAK);

      boolean eof = false;
      boolean inheader = true;

      int content_length = 0;
      long content_length_header_pos = -1;

      while (!eof) {
        buf.setLength(0);
        eof = (buf.read(in, 1024) < 0);
        if (eof) {
          linebuf.pushEOF();
        } else {
          linebuf.pushBytes(buf);
        }

        while (linebuf.pullLine(line)) {
          if (inheader) {
            byte first = line.byteAt(0);
            if (first == '\r' || first == '\n') {
              inheader = false;

              // Once we're at the end of the headers, write out the
              // updated X-Mozilla-Status header and a dummy
              // Content-Length header.
              //
              long xms = BerkeleyMessage.makeMozillaFlags(m);

              line.setLength(0);
              line.append("X-Mozilla-Status: ");
              String s = Long.toHexString(xms);
              int i = s.length();
              if (i == 1) line.append("000");
              else if (i == 2) line.append("00");
              else if (i == 3) line.append("0");
              line.append(s);
              line.append(Constants.LINEBREAK);

              // Put this line (the X-Mozilla-Status header) in the headers
              // object and in the file.
              headers.addHeaderLine(line.toString());
              out.write(line.toBytes(), 0, line.length());


              // To generate the Content-Length header, we write out a
              // dummy header (one containing only spaces) and then we
              // overwrite it later, once we've written all the data.
              // jwz thinks this is incredibly cheesy, but terry thinks
              // it's simple and elegant.
              //
              line.setLength(0);
              line.append("Content-Length: ");
              out.write(line.toBytes(), 0, line.length());

              // remember the position at which to drop down the number.
              content_length_header_pos = out.getFilePointer();

              // write out the blank header field, and terminal newline.
              // leave room for 10 characters (2^32 in decimal.)
              line.setLength(0);
              line.append("          ");
              line.append(Constants.LINEBREAK);
              out.write(line.toBytes(), 0, line.length());

              // Now write newline.
              line.setLength(0);
              line.append(Constants.LINEBREAK);
              out.write(line.toBytes(), 0, line.length());

            } else {
              // we're still in the headers.

              // Don't write out the X-Mozilla-Status or Content-Length
              // headers yet.
              if ((first == 'x' || first == 'X') &&
                  line.regionMatches(true, 0, "X-Mozilla-Status:", 0, 17)) {
                // ignore existing value of this header
              } else if ((first == 'c' || first == 'C') &&
                         line.regionMatches(true, 0, "Content-Length:",
                                            0, 15)) {
                // ignore existing value of this header
              } else {
                // Not header we want to ignore and regenerate -- write it.
                headers.addHeaderLine(line.toString());
                out.write(line.toBytes(), 0, line.length());
              }
            }

          } else {
            // not in headers -- pass it through.
            // also compute correct value for Content-Length.

            byte b[] = line.toBytes();
            int L = line.length();

            // Mangle "From " to ">From ".
            if (L > 4 &&
                b[0] == 'F' && b[1] == 'r' && b[2] == 'o' && b[3] == 'm' &&
                b[4] == ' ') {
              out.write((int) '>');
              content_length++;
            }

            out.write(b, 0, L);
            content_length += L;
          }
        }
      }

      // We have written out the whole message, and now know the correct
      // value for Content-Length.  Go back and finish the job, overwriting
      // the dummy header we left earlier.
      //
      if (content_length_header_pos > 0 &&
          content_length >= 0) {

        // write to the file if CL will fit in ten digits (pretty likely...)
        //
        if (content_length <= 9999999999L) {
          out.seek(content_length_header_pos);
          line.setLength(0);
          line.append(Long.toString(content_length));
          out.write(line.toBytes(), 0, line.length());
        }

        // Add this new Content-Length value into the Header object.
        line.setLength(0);
        line.append("Content-Length: ");
        line.append(Long.toString(content_length));
        line.append(Constants.LINEBREAK);
        headers.addHeaderLine(line.toString());
      }

    } finally {
      if (buf != null)  ByteBuf.Recycle(buf);
      if (line != null) ByteBuf.Recycle(line);
    }
    return headers;
  }


  private Object disk_lock_object = null;

  /** On those systems which support locking of disk files, this causes
      a lock to be acquired on the file associated with this folder.
      It is imperative that a balanced unlockFolderFile() call be made:
      be sure to only use this within try/finally.
   */
  void lockFolderFile() {
//    System.err.println("locking file " + fFile);

    if (disk_lock_object != null) {
      // Nested locks aren't allowed...
      Assert.Assertion(false);

    } else if (false) {    // #### change this test to `Constants.ISUNIX' once
                           // #### the UnixDotLock class actually works.
      try {

        disk_lock_object = new UnixDotLock(fFile);

      } catch (IOException e) {
        // Ignore it, I guess -- it's ok to try and visit a folder in a
        // read-only directory, or in someone else's directory, and in that
        // case we won't be able to acquire the lock.
        // (Or would that be a SecurityException?)
        System.err.println("warning: I/O error when trying to lock folder:"
                           + e);

      } catch (InterruptedException e) {
        // #### GAG, what do I do here??
        // I don't really want the fact that class UnixDotLock calls wait()
        // to escape up to the higher levels.  What to do...
        System.err.println("exception ignored: " + e);

      }

    } else {
      // Non-Unix -- put some random non-null object in the lock slot.
      disk_lock_object = Boolean.TRUE;
    }
  }

  /** Releases the lock obtained by lockFolderFile(). */
  void unlockFolderFile() {
//    System.err.println("unlocking file " + fFile);

    if (disk_lock_object == null) {
      // It should be locked, if we're unlocking it...
      Assert.Assertion(false);
    } else {
      Object o = disk_lock_object;
      disk_lock_object = null;
      if (o != Boolean.TRUE)  // see above
        ((UnixDotLock) o).unlock();
    }
  }



  static private final String MONTHS[] = {"Jan", "Feb", "Mar", "Apr",
                                          "May", "Jun", "Jul", "Aug",
                                          "Sep", "Oct", "Nov", "Dec"};
  static private final String DAYS[] = {"Mon", "Tue", "Wed", "Thu",
                                        "Fri", "Sat", "Sun"};

  static DecimalFormat TwoDigitNumFormat = new DecimalFormat("00");
  protected String TwoDigitNum(int num) {
    return TwoDigitNumFormat.format(num);
  }

  protected void makeDummyEnvelope(ByteBuf buf) {
    Calendar c = new GregorianCalendar();
    c.setTime(new Date());

    int zone = (c.get(Calendar.ZONE_OFFSET) +
                c.get(Calendar.DST_OFFSET)) / (1000 * 60);

    boolean zonenegative = (zone < 0);
    zone = Math.abs(zone);
    int zonehours = zone / 60;
    int zoneminutes = zone % 60;

    buf.append("From - ");
    buf.append(DAYS[c.get(Calendar.DAY_OF_WEEK) - 1]);
    buf.append(", ");
    buf.append(c.get(Calendar.DATE));
    buf.append(" ");
    buf.append(MONTHS[c.get(Calendar.MONTH)]);
    buf.append(" ");
    buf.append(c.get(Calendar.YEAR));
    buf.append(" ");
    buf.append(TwoDigitNum(c.get(Calendar.HOUR_OF_DAY)));
    buf.append(":");
    buf.append(TwoDigitNum(c.get(Calendar.MINUTE)));
    buf.append(":");
    buf.append(TwoDigitNum(c.get(Calendar.SECOND)));
    buf.append(" ");
    buf.append(zonenegative ? "-" : "+");
    buf.append(TwoDigitNum(zonehours));
    buf.append(TwoDigitNum(zoneminutes));
    buf.append(Constants.LINEBREAK);
  }

// ### I'm too lazy to port this test right now to the new world, so I'm
// just commenting it out.  - Terry
//
//  public static final void main(String av[]) throws Exception {
//
//    BerkeleyFolder f3 = new BerkeleyFolder(new File("/u/jwz/tmp/f3"), "f3");
////    f3.ensureLoaded();
//    f3.expunge();
//
///*
//    BerkeleyFolder f1 = new BerkeleyFolder(new File("/u/jwz/tmp/f1"), "f1");
//    BerkeleyFolder f2 = new BerkeleyFolder(new File("/u/jwz/tmp/f2"), "f2");
//
//    Enumeration e1 = f1.getMessages();
////    Enumeration e2 = f2.getMessages();
//    BerkeleyMessage m1 = (BerkeleyMessage) e1.nextElement();
//
//    System.err.println("flagged = " + m1.isFlagged() + " (" + m1.flags + ")");
//    m1.setFlagged(!m1.isFlagged());
//    System.err.println("flagged = " + m1.isFlagged() + " (" + m1.flags + ")");
//
//    f2.addMessage(m1);
//    System.err.println("sleeping");
//    Thread.sleep(50 * 1000);
//    System.err.println("exiting");
//*/
//  }

  public boolean delete(boolean value) {
    Assert.NotYetImplemented("BerkeleyFolder.delete");
    return false;
  }

  public boolean renameTo(Folder f) {
    Assert.NotYetImplemented("BerkeleyFolder.renameTo");
    return false;
  }

  public void open(int mode) {
    ensureLoaded();
  }

  public void close(boolean doExpunge) throws MessagingException {
    if (doExpunge) expunge();
    // ### Throw away message array and stuff?
  }

  public boolean isOpen() {
    return true;                // ### Wrong!
  }



}


/** This class implements a thread which runs in the background and writes
    out all modified mail summary files.  This thread dies once the files
    have been written; it will be re-spawned once some folder or folders
    become dirty.

    <P> There is only ever one of these threads: it writes the summaries for
    all modified folders, serially, then exits.
  */
class SummaryUpdateThread extends Thread {

  static int delay = 30; // seconds
  static SummaryUpdateThread the_thread = null;        // singleton

  private Vector dirty_folders = new Vector();

  static synchronized SummaryUpdateThread Get() {
    if (the_thread == null || !the_thread.isAlive()) {
      the_thread = new SummaryUpdateThread();
      the_thread.setDaemon(true);
      the_thread.setName(the_thread.getClass().getName());
      the_thread.start();
    }
    return the_thread;
  }

  synchronized void addDirtyFolder(BerkeleyFolder f) {
    if (!dirty_folders.contains(f))
      dirty_folders.addElement(f);
  }

  public void run() {

    try {
      sleep(delay * 1000);
    } catch (InterruptedException e) {
      return;   // is this the right way to do this?
    }

    synchronized (this.getClass()) {
      // Do this first, so that if someone calls add() just as we're coming
      // out of the loop, below, the thing they've added doesn't get lost.
      the_thread = null;
    }

    for (int i = 0; i < dirty_folders.size(); i++) {
      BerkeleyFolder f = (BerkeleyFolder) dirty_folders.elementAt(i);
      try {

System.err.println("background thread writing summary file for " + f.fFile);

        f.writeSummaryFile();
      } catch (Exception e) {
        f.setSummaryDirty(true);
      }
    }
  }
}


/** This class implements a thread which runs in the background and overwrites
    the changed X-Mozilla-Status headers in a particular folder.  This thread
    dies once the file has been updated; it will be re-spawned once some
    folder's messages become dirty again.

    <P> There can be one of these threads per folder.
  */
class StatusFlagsUpdateThread extends Thread {

  static int delay = 5; // seconds
  BerkeleyFolder folder = null;

  StatusFlagsUpdateThread(BerkeleyFolder folder) {
    this.folder = folder;
    setDaemon(true);
    setName(getClass().getName() + " " + folder.fFile);
  }

  public void run() {

    try {
      sleep(delay * 1000);
    } catch (InterruptedException e) {
      return;   // is this the right way to do this?
    }

    System.err.println("background thread updating X-Mozilla-Status headers " +
                       "in " + folder.fFile);
    try {
      synchronized (folder) {
        folder.statusFlagsUpdateThread = null;
        folder.flushMozillaStatus();
      }
    } catch (IOException e) {
      // nothing to be done?
      System.err.println("IOException while writing X-Mozilla-Status: " + e);
    }
  }
}
