/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 */

package calypso.util;

/**
 * A "read-write" lock. This lock allows for an arbitrary number of
 * simultaneous readers. The lock can be upgraded to a write lock in two
 * ways. First, the lock can be upgraded without guaranteeing invariance
 * across the transition (in other words, the read lock may need to be
 * released before the write lock can be acquired).  The other form of
 * upgrade guarantees invariance; however, the upgrade can only be
 * performed by initially locking the lock using the invariant read lock
 * enter method. Upgrading the lock in either case involves waiting until
 * there are no more readers. This implementation gives priority to
 * upgrades and invariant locks which may lead to reader starvation. <p>
 *
 * Each thread using the lock may re-enter the lock as many times as
 * needed. However, attempting to re-enter the lock with the invariant
 * read lock will fail unless the lock was originally entered that way by
 * the invoking thread. <p>
 *
 * Only one thread may enter the invariant read lock at a time; other
 * threads attempting this will block until the owning thread exits the
 * lock completely. <p>
 *
 * Note that the implementation assumes that the user of instances of
 * this class properly pairs the enters/exits. <p>
 */
public final class RWLock {
  private int fNumReaders;
  private Thread fWriteLockOwner;
  private int fWriteLockCount;
  private Thread fInvariantLockOwner;
  private int fInvariantLockCount;

  public RWLock() {
    fStateList = new LockState();
    fStateList.fNext = fStateList;
    fStateList.fPrev = fStateList;
    fFreeList = new LockState();
    fFreeList.fNext = fFreeList;
    fFreeList.fPrev = fFreeList;
  }

  public synchronized void enterReadLock()
    throws InterruptedException
  {
    Thread me = Thread.currentThread();

    // Wait for writer, if any, to release the lock
    if ((fWriteLockOwner != null) && (me != fWriteLockOwner)) {
      while (fWriteLockOwner != null) {
        wait();
      }
    }

    // XXX write me:
    // If my lock count is zero and there is a writer waiting, block
    // until the writer is done

    // Add another state record to the list
    appendLockState(me, kRead);
    fNumReaders++;
  }

  public synchronized void exitReadLock() {
    Thread me = Thread.currentThread();
    removeLockState(me, kRead);
    fNumReaders--;
    notify();
  }

  /**
   * Enter the invariant read lock. Only one thread at a time can hold
   * the invariant read lock. This lock guarantees to upgrade to a write
   * lock without needing to release the read lock.
   */
  public synchronized void enterInvariantReadLock()
    throws InterruptedException
  {
    Thread me = Thread.currentThread();
    if (me != fInvariantLockOwner) {
      // If we don't have the invariant read lock already then we cannot
      // try to get it if we are holding either a read or a write lock
      if (isLocked(me, kRead) || isLocked(me, kWrite)) {
        throw new Error("enterInvariantReadLock while holding r/w lock");
      }

      /**
       * If we get here it means we don't currently hold the invariant read
       * lock and we aren't holding any read locks or write locks which
       * means we can't be the fWriteLockOwner or the fInvariantOwner.
       * Wait until either of those threads, if any, is done with the lock.
       */
      while ((fWriteLockOwner != null) || (fInvariantLockOwner != null)) {
        wait();
      }

      // Claim the lock
      fInvariantLockOwner = me;
    }
    fInvariantLockCount++;
    fNumReaders++;
    appendLockState(me, kInvariantRead);
  }

  public synchronized void exitInvariantReadLock() {
    Thread me = Thread.currentThread();
    removeLockState(me, kInvariantRead);
    --fNumReaders;
    if (--fInvariantLockCount == 0) {
      fInvariantLockOwner = null;
    }
    notify();
  }

  public synchronized void enterWriteLock()
    throws InterruptedException
  {
    Thread me = Thread.currentThread();
    if (me != fWriteLockOwner) {
      // Count up how many read-locks we have currently. The only possible
      // type of lock state is kRead or kInvariantRead.
      int readCount = 0;
      LockState list = fStateList.fPrev;
      while (list != fStateList) {
        LockState prev = list.fPrev;
        if (list.fThread == me) {
          Assert.Assertion(list.fState != kWrite);
          readCount++;
        }
        list = prev;
      }

      /**
       * If we have the invariant read lock then we don't have to release
       * our read locks, otherwise we do. When we do we must notify any
       * other threads that are waiting for this to occur. When we don't
       * we change the wait loop below to wait until all the <b>other</b>
       * read locks are released isntead of waiting until <b>all</b> the
       * read locks are released.
       */
      int baseNumReaders = readCount;
      if (me != fInvariantLockOwner) {
        // Release our read locks now
        baseNumReaders = 0;
        fNumReaders -= readCount;
        notify();
      }

      // Wait for all other readers or a writer to exit the lock
      while ((fWriteLockOwner != null) || (fNumReaders > baseNumReaders)) {
        wait();
      }

      // At this point fWriteLockOwner == null and fNumReaders ==
      // baseNumReaders which means that we can claim the
      // write-lock. Restore fNumReaders to account for any of our read
      // locks on the list.
      if (me != fInvariantLockOwner) {
        // Recover our read locks. Note that we left them on the state
        // list this entire time, we just reduced the counts to that
        // the various wait loops would think they were gone.
        fNumReaders += readCount;
      }
      fWriteLockOwner = me;
    }
    fWriteLockCount++;
    appendLockState(me, kWrite);
  }

  public synchronized void exitWriteLock() {
    Thread me = Thread.currentThread();
    removeLockState(me, kWrite);
    if (--fWriteLockCount == 0) {
      fWriteLockOwner = null;
    }
    notify();
  }

  //----------------------------------------------------------------------

  static class LockState {
    // Linkage for either fStateStack or for fFreeList
    LockState fPrev, fNext;
    Thread fThread;
    int fState;
  }

  // Legal state values for a LockState. These indicate what kind
  // of enter was done on the lock.
  static final int kRead = 0;
  static final int kWrite = 1;
  static final int kInvariantRead = 2;
  static String[] kStateToString = {
    "read-lock", "write-lock", "invariant-read-lock"
  };

  /**
   * Circular list of LockState objects. When the lock changes state (via
   * one of the three enter methods) we append a LockState object to the
   * end of the stack. When a thread exits the lock we find it's last
   * LockState object and remove it from the list.
   */
  private LockState fStateList;

  /**
   * A list of free LockState objects used to reduce the execution
   * time for allocation.
   */
  private LockState fFreeList;
  private int fFreeListLength;
  private static final int kMaxFreeListLength = 10;

  /**
   * See if aThread has the given type of lock
   */
  private boolean isLocked(Thread aThread, int aHow) {
    LockState list = fStateList.fPrev;
    while (list != fStateList) {
      if (list.fThread == aThread) {
        if (list.fState == aHow) {
          return true;
        }
      }
      list = list.fPrev;
    }
    return false;
  }

  private void appendLockState(Thread aThread, int aState) {
    LockState ls = newLockState();
    ls.fThread = aThread;
    ls.fState = aState;

    // Append ls to the state list
    LockState list = fStateList;
    ls.fNext = list;
    ls.fPrev = list.fPrev;
    list.fPrev.fNext = ls;
    list.fPrev = ls;
  }

  /**
   * Walk up the thread state list and look for aThread. Verify that the
   * LockState is in the right state and if it is then we remove that
   * LockState from the list.
   */
  private void removeLockState(Thread aThread, int aState) {
    LockState tos = fStateList.fPrev;
    while (tos != fStateList) {
      if (tos.fThread == aThread) {
        if (tos.fState != aState) {
          throw new Error("mismatched lock exit: entry=" +
                          kStateToString[tos.fState] + " exit=" +
                          kStateToString[aState]);
        }

        // Remove tos from the list
        tos.fPrev.fNext = tos.fNext;
        tos.fNext.fPrev = tos.fPrev;
        freeLockState(tos);
        return;
      }
      tos = tos.fPrev;
    }
    throw new Error("unmatched lock exit");
  }

  private LockState newLockState() {
    if (fFreeListLength == 0) {
      return new LockState();
    }

    // Get a LockState from the free list
    LockState tos = fFreeList.fPrev;
    tos.fPrev.fNext = tos.fNext;
    tos.fNext.fPrev = tos.fPrev;
    fFreeListLength--;
    return tos;
  }

  private void freeLockState(LockState aState) {
    if (fFreeListLength < kMaxFreeListLength) {
      LockState list = fFreeList;

      // Append aState to the free list
      aState.fNext = list;
      aState.fPrev = list.fPrev;
      list.fPrev.fNext = aState;
      list.fPrev = aState;
      fFreeListLength++;
    }
  }
}
