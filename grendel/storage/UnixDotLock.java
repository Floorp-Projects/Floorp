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
 * Created: Jamie Zawinski <jwz@netscape.com>, 20 Sep 1997.
 */

package grendel.storage;

import calypso.util.Assert;
import java.lang.Thread;
import java.lang.SecurityException;
import java.lang.InterruptedException;
import java.util.Vector;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

/** Implements Unix-style dot-locking (locking file <TT>"FOO"</TT> by using an
    atomically-created file in the same directory named <TT>"FOO.lock"</TT>.)

    <P> Use it like this:

    <P><UL>
 <TT>UnixDotLock lock = new UnixDotLock(file);&nbsp; // </TT><I>this blocks</I>
 <BR><TT>// </TT><I>... long-running manipulations of file</I>
 <BR><TT>lock.unlock();</TT>
    </UL>

    <P> All the lock-retrying and lock-date-maintenance happens under
    the covers.

    <P> See also the
    <A HREF="http://home.netscape.com/eng/mozilla/2.0/relnotes/demo/movemail.html#movemail">
    description of movemail</A>.

   <P><B>Implementation details:</B>
   <BLOCKQUOTE>
    <P>The protocol for getting a lock on some file <TT>"FOO"</TT> is
    as follows:

    <P><OL>
     <LI> Create a file <TT>"FOO.1234"</TT> (random unused name.)<P>

     <LI> Hard-link <TT>"FOO.1234"</TT> to <TT>"FOO.lock"</TT>.<BR>
          (this is the trick, because <TT>link()</TT> happens to be
          one of the few atomic, synchronized operations over NFS.)<P>

     <LI> Unlink <TT>"FOO.1234"</TT><BR>
          (regardless of whether step 2 succeeded; now either we have
          a <TT>"FOO.lock"</TT> file or we don't.)<P>

     <LI> If we obtained the lock (the <TT>link()</TT> call in
          step #2 succeeded), then we're done.<P>

     <LI> Else if the creation-time of <TT>"FOO.lock"</TT> is more
          than 60 seconds in the past, then smash the lock (unlink
          <TT>"FOO.lock"</TT>) and goto step #1.)<P>

     <LI> Else, the lock is held and current.  Wait a second, then
          goto step #1 and try again.
    </OL>
    <P>
    One thing implied by this is that if one wants to hold a lock for longer
    than 60 seconds (which we do in some cases) then one must maintain the
    lock file: its modification-date must be updated every less-than-60
    seconds.  We do this by having a ``heartbeat'' thread that wakes up
    periodically and updates all held locks.
   </BLOCKQUOTE>
 */

public class UnixDotLock {

  /** Turn this on to cause activity to be logged to System.err. */
  static private final boolean debug = true;

  /** Holds the (one) thread that updates the lock date every <60 seconds. */
  static private Thread lock_heartbeat_thread = null;

  /** List of locked locks.  The vector holds UnixDotLock objects. */
  /* #### We really want this vector to hold weak pointers, so that the locks
     get deleted if the lock object is GCed.  But, failing to explicitly
     unlock the lock doesn't have <I>that</I> bad a failure mode, since the
     protocol is to smash the lock after 60 seconds anyway.
   */
  static private Vector active_locks = null;

  /** After someone else's lock is 60 seconds old, we assume it has been left
      dangling, and smash it.  This is a part of the de-facto dot-locking
      protocol, folks: I wouldn't make this stuff up.  The way you hold a
      lock for longer than 60 seconds is by periodically updating the
      modification date of the lock file to prove that you're still alive.
   */
  static private final int maximum_lock_age = 60;

  /** How often to update the write-date on a lock file.  If we want to
      hold a lock for longer than 60 seconds, we need to update its
      write date, and this is how often we do that.  This should be
      less than `maximum_lock_age' by enough to be comfortable that
      system load and thread starvation won't cause the heartbeat
      thread to fail to update the lock file date in time.
   */
  static final int heart_rate = 30;

  /** <B>System dependency:</B> multiply the result of File.lastModified() by
      this to convert it to seconds.  Java doesn't specify the units in which
      File.lastModified() measures time, but we need to be able to add N
      seconds to it, to tell when a file is more than N seconds old.  It
      happens that, with JDK 1.1.3 on Irix, this scale is 1000.  We must
      assume that all other Unixen behave the same.  If this changes in some
      future Java implementation, we're fucked.
   */
  static private final int File_lastModified_scale = 1000;

  /** The name of the file for which this lock is being held. */
  private File locked_file = null;

  /** Lock the named file.  Unlock it by calling the unlock() method.
      If you do not call unlock() before discarding the UnixDotLock object,
      the file will remain locked!

      @exception SecurityException      the lock file could not be created
                                        (file permission problems?)
      @exception IOException            a disk I/O error occurred.
      @exception InterruptedException   this thread was killed while waiting
                                        for the lock.
   */
  public UnixDotLock(File file)
    throws SecurityException, IOException, InterruptedException {
    if (!file.isAbsolute())
      file = new File(file.getAbsolutePath());

    // We want to work in the real directory of the file, not in the
    // directory of a symlink to the file.
    file = new File(file.getCanonicalPath());

    createDiskLock(file);
    setLocked(file);
  }

  /** Unlock the file.  This must be called before discarding the UnixDotLock
      object, and may be called only once.  The UnixDotLock object must not
      be used again after calling this.

      @exception InterruptedException   this thread was killed while waiting
                                        for the lock.
   */
  public void unlock() {
    if (locked_file == null)
      throw new Error("not locked");
    removeDiskLock(locked_file);
    try {
      setLocked(null);
    } catch (InterruptedException e) {
      Assert.Assertion(false);   // shouldn't be thrown when only unlocking.
    }
  }

  /** Create a lock file name for the given file (append ".lock" to it.) */
  private File makeLockName(File file) {
    return new File(file.toString() + ".lock");
  }

  /** Create the name of a new file in the same directory as the given file.
      This picks a random name and then checks to make sure it doesn't
      already exist.  It doesn't actually create the file (so there's a
      very slight race here.) */
  private File gentemp(File prefix) {
    File f;
    do {
      int r = (int) Double.doubleToLongBits(Math.random());
      f = new File(prefix.toString() + "." + Integer.toHexString(r));
    } while (f.exists());
    return f;
  }

  /** Attempt to lock the file, using the hairy dot-locking protocol.
      Does not return until the lock has been obtained.
      @exception SecurityException      the lock file could not be created
                                        (file permission problems?).
      @exception IOException            a disk I/O error occurred.
      @exception InterruptedException   this thread was killed while waiting
                                        for the lock.
    */
  private void createDiskLock(File file)
    throws SecurityException, IOException, InterruptedException {

    File dir = new File(file.getParent());
    File lock = makeLockName(file);

    if (debug)
      System.err.println("LOCK " + file + " (with " + lock + ")");

    while (true) {

      if (!dir.canWrite())
        throw new SecurityException("unwritable directory " + dir);

      // 1:  create file "FOO.1234" (random name)
      //
      File tmp = gentemp(lock);
      if (debug) System.err.println("\n  1: create " + tmp);
      FileOutputStream stream = new FileOutputStream(tmp);
      stream.close();
      stream = null;

      // Get the current time as the *disk* sees it, not as the *system*
      // sees it -- this avoids lossage when this machine and the file
      // server do not have synchronized clocks.
      long current_time = tmp.lastModified();

      // 2: hard-link "FOO.1234" to "FOO.lock"
      // (this is the trick, because link() is an atomic, synchronized
      // operation over NFS)
      //
      if (debug) System.err.println("  2: link " + tmp + " " + lock);
      // #### this part is wrong -- I don't know how make link() syscall
      boolean link_succeeded = false;
      if (!lock.exists()) {
        if (debug) System.err.println("  2: (#### but not really)");
        stream = new FileOutputStream(lock);
        stream.close();
        stream = null;
        link_succeeded = true;
      }

      // 3: unlink "FOO.1234"
      // (regardless of whether #2 succeeded; now either we have an
      // "FOO.lock" file or we don't)
      //
      if (debug) System.err.println("  3: delete " + tmp);
      if (!tmp.delete())
        throw new IOException("unable to delete " + tmp);

      // 4: if we obtained the lock (the link() call succeeded), we're done
      //
      if (link_succeeded) {
        if (debug) System.err.println("  4: locked " + lock);
        break;
      }

      // 5: else if creation-time of "FOO.lock" is > 60 seconds old,
      //    smash the lock (unlink "FOO.lock") and goto 1.
      //
      else if ((lock.lastModified() +
                (maximum_lock_age * File_lastModified_scale))
               <= current_time) {
        if (debug)
          System.err.println("  5: smash lock " + lock + " (" +
                             ((current_time - lock.lastModified()) /
                              File_lastModified_scale) +
                             " seconds old)");
        lock.delete();
      }

      // 6: else, the lock is current; wait a second, then goto 1
      //    and try again.
      //
      else {
        if (debug)
          System.err.println("  6: wait for " + lock + " (" +
                             ((current_time - lock.lastModified()) /
                              File_lastModified_scale) +
                             " seconds old)");
        Thread.sleep(1000);
      }
    }
  }

  /** Assumes that this process had at some point obtained a lock on the
      given file, and removes that lock.  Calling this without having
      obtained the lock will smash someone else's lock, and you don't
      want to do that.
   */
  private void removeDiskLock(File file) {
    File lock = makeLockName(file);
    if (debug) System.err.println("UNLOCK " + lock);
    lock.delete();      // this had better do the Unix unlink() syscall.
  }

  /** Marks the object as locked or unlocked.  Manages the heartbeat
      thread (creating or killing it, as appropriate.)

      @exception InterruptedException   this thread was killed while waiting
                                        for the lock.  This can only be thrown
                                        when locking, not when unlocking.
   */
  private synchronized void setLocked(File file) throws InterruptedException {

    // This method is synchronized to protect access to the `locked_file'
    // instance variable.  This method will only be called from the locking
    // thread, but the heartbeat() method may be called from the heartbeat
    // thread, and we must avoid contention between those two threads.

    if (debug) System.err.println("LOCK = " + file);
    this.locked_file = file;
    updateLockList(this);
  }

  /** If we currently own a lock file, update its modification time.
      This is a way of informing other processes that this process is
      still alive, and still desires to hold the lock.

      @exception IOException            a disk I/O error occurred.
   */
  private synchronized void heartbeat() throws IOException {

    // This method is synchronized to protect access to the `locked_file'
    // instance variable.  This method will only be called from the heartbeat
    // thread, but the setLocked() method will be called from the locking
    // thread, and we must avoid contention between those two threads.

    if (locked_file != null) {
      File lock = makeLockName(locked_file);
      if (lock.exists()) {
        if (debug) System.err.println("  heartbeat touch " + lock);
        FileOutputStream stream = new FileOutputStream(lock);
        stream.close();
      }
    }
  }

  /** Call the heartbeat() method on every UnixDotLock in `active_locks'.
   */
  synchronized static void globalHeartbeat() {
    // This class-method is synchronized because all manipulations of the
    // active_locks class-variable must be protected by a lock on the class.
    // (The updateLockList() class-method also uses active_locks.)

    if (debug) System.err.println("heartbeat awake");
    if (active_locks != null) {
      for (int i = 0; i < active_locks.size(); i++) {
        UnixDotLock d = (UnixDotLock) active_locks.elementAt(i);
        if (d != null) {
          try {
            d.heartbeat();
          } catch (IOException e) {
            // ignore errors.
            if (debug) System.err.println("ignoring " + e);
          }
        }
      }
    }
  }

  private synchronized static void updateLockList(UnixDotLock lock) {
    // This class-method is synchronized because all manipulations of the
    // class-variables active_locks and lock_heartbeat_thread must be
    // protected by a lock on the class.  (It happens that this is the
    // only method to touch lock_heartbeat_thread, but active_locks is
    // also used by globalHeartbeat().)

    if (lock.locked_file != null) {             // locking

      if (active_locks == null)
        active_locks = new Vector();
      active_locks.addElement(lock);

      if (lock_heartbeat_thread == null ||
          !lock_heartbeat_thread.isAlive()) {
        Thread t = new UnixDotLockHeartbeatThread();
        lock_heartbeat_thread = t;
        t.setDaemon(true);
        t.setName(t.getClass().getName());
        if (debug) System.err.println("LAUNCHING " + t);
        t.start();
      }

    } else {                                    // unlocking
      active_locks.removeElement(lock);
      if (active_locks.isEmpty() &&
          lock_heartbeat_thread != null) {
        if (debug) System.err.println("KILLING " + lock_heartbeat_thread);
        Thread h = lock_heartbeat_thread;
        lock_heartbeat_thread = null;
        h.stop();
        try {
          h.join();
        } catch (InterruptedException e) {
          // ignore it -- not important.
        }
      }
    }
  }

  /** If this object becomes reclaimed without unlock() having been called,
      then call it (thus unlocking the underlying disk file.)
      @exception Throwable
    */
  protected synchronized void finalize() throws Throwable {
    if (locked_file != null) {
      if (debug)
        System.err.println("unlock " + locked_file + " due to finalization!");
      unlock();
    }
    super.finalize();
  }

  public static final void main(String arg[])
    throws SecurityException, IOException, InterruptedException {

    System.runFinalizersOnExit(true);

    File file1 = new File("/tmp/a");
    File file2 = new File("/tmp/b");
    UnixDotLock lock1 = new UnixDotLock(file1);
    Thread.sleep(3 * 1000);
    UnixDotLock lock2 = new UnixDotLock(file2);
    Thread.sleep(100 * 1000);
    lock1.unlock();
    Thread.sleep(40 * 1000);
    lock2.unlock();
    Thread.sleep(3 * 1000);
  }
}


/** This is the thread that runs as a daemon and periodically updates
    the write-dates on long-lived file locks which any user of UnixDotLock
    process holds.  This thread is launched as soon as there are any
    outstanding locks, and is killed when there are none; but if there
    is more than one lock, there is still only one heartbeat thread which
    manages them all.

    @see UnixDotLock
 */
class UnixDotLockHeartbeatThread extends Thread {
  public void run() {
    while (true) {
      try {
        Thread.sleep(UnixDotLock.heart_rate * 1000);
      } catch (InterruptedException e) {
        return;   // is this the right way to do this?
      }
      UnixDotLock.globalHeartbeat();
    }
  }
}
