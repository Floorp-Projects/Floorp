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

class SelfTestRWLock {
  static void simpleReadTest(RWLock aLock)
    throws InterruptedException
  {
    for (int i = 0; i < 5; i++) {
      aLock.enterReadLock();
    }
    for (int i = 0; i < 5; i++) {
      aLock.exitReadLock();
    }
  }

  static void simpleReadWriteTest(RWLock aLock)
    throws InterruptedException
  {
    for (int i = 0; i < 5; i++) {
      aLock.enterReadLock();
    }
    for (int i = 0; i < 5; i++) {
      aLock.enterWriteLock();
    }
    for (int i = 0; i < 5; i++) {
      aLock.exitWriteLock();
    }
    for (int i = 0; i < 5; i++) {
      aLock.exitReadLock();
    }
  }

  static void simpleInvariantReadTest(RWLock aLock)
    throws InterruptedException
  {
    for (int i = 0; i < 5; i++) {
      aLock.enterInvariantReadLock();
    }
    for (int i = 0; i < 5; i++) {
      aLock.exitInvariantReadLock();
    }
  }

  static void simpleInvariantReadWriteTest(RWLock aLock)
    throws InterruptedException
  {
    for (int i = 0; i < 5; i++) {
      aLock.enterInvariantReadLock();
    }
    for (int i = 0; i < 5; i++) {
      aLock.enterWriteLock();
    }
    for (int i = 0; i < 5; i++) {
      aLock.exitWriteLock();
    }
    for (int i = 0; i < 5; i++) {
      aLock.exitInvariantReadLock();
    }
  }

  // Verify that an out of order exit throws an error
  static void outOfOrderExit0(RWLock aLock)
    throws InterruptedException, SelfTestException
  {
    aLock.enterReadLock();
    try {
      aLock.exitWriteLock();
    } catch (Error e) {
      return;
    }
    throw new SelfTestException("whoops");
  }

  // Verify that an out of order exit throws an error
  static void outOfOrderExit1(RWLock aLock)
    throws InterruptedException, SelfTestException
  {
    aLock.enterReadLock();
    try {
      aLock.exitInvariantReadLock();
    } catch (Error e) {
      return;
    }
    throw new SelfTestException("whoops");
  }

  static void outOfOrderExit2(RWLock aLock)
    throws InterruptedException, SelfTestException
  {
    aLock.enterInvariantReadLock();
    try {
      aLock.exitReadLock();
    } catch (Error e) {
      return;
    }
    throw new SelfTestException("whoops");
  }

  static void outOfOrderExit3(RWLock aLock)
    throws InterruptedException, SelfTestException
  {
    aLock.enterInvariantReadLock();
    try {
      aLock.exitWriteLock();
    } catch (Error e) {
      return;
    }
    throw new SelfTestException("whoops");
  }

  static void outOfOrderExit4(RWLock aLock)
    throws InterruptedException, SelfTestException
  {
    aLock.enterWriteLock();
    try {
      aLock.exitReadLock();
    } catch (Error e) {
      return;
    }
    throw new SelfTestException("whoops");
  }

  static void outOfOrderExit5(RWLock aLock)
    throws InterruptedException, SelfTestException
  {
    aLock.enterWriteLock();
    try {
      aLock.exitInvariantReadLock();
    } catch (Error e) {
      return;
    }
    throw new SelfTestException("whoops");
  }

  public static void run()
    throws SelfTestException
  {
    // Perform simple tests that have no contention with another thread
    try {
      simpleReadTest(new RWLock());
      simpleReadWriteTest(new RWLock());
      simpleInvariantReadTest(new RWLock());
      simpleInvariantReadWriteTest(new RWLock());

      outOfOrderExit0(new RWLock());
      outOfOrderExit1(new RWLock());
      outOfOrderExit2(new RWLock());
      outOfOrderExit3(new RWLock());
      outOfOrderExit4(new RWLock());
      outOfOrderExit5(new RWLock());

      verifyBlocking(new RWLock(), kRead, kWrite);
      verifyBlocking(new RWLock(), kInvariantRead, kWrite);
      verifyBlocking(new RWLock(), kWrite, kRead);
      verifyBlocking(new RWLock(), kWrite, kInvariantRead);

      stressTestContention(new RWLock());
    } catch (InterruptedException e) {
      // Test was interrupted
      e.printStackTrace();
    }
  }

  static final int kRead = 0;
  static final int kWrite = 1;
  static final int kInvariantRead = 2;
  static String[] kStateToString = {
    "read-lock", "write-lock", "invariant-read-lock"
  };

  // Verify that blocking is occuring by using the RWLock to
  // protect the helper.fValue and guarantee a correct ordering
  // of evaluation.
  static void verifyBlocking(RWLock aLock, int aMe, int aHelper)
    throws SelfTestException, InterruptedException
  {
    Enter(aLock, aMe);

    RWTestHelper helper = new RWTestHelper(aLock, aHelper, 1, 1, 0);
    Thread t1 = new Thread(helper);
    t1.start();

    // Make sure RWTestHelper is waiting to get the write-lock
    Thread.sleep(500);
    if (helper.fValue != 1) {
      throw new SelfTestException("write-lock didn't block");
    }

    // Give RWTestHelper the lock; set value to 2 before it gets
    // the lock
    helper.fValue = 2;
    Exit(aLock, aMe);

    // make sure RWTestHelper is done
    Thread.sleep(500);
    if (helper.fValue != 0) {
      throw new SelfTestException("helper didn't finish");
    }
  }

  static void stressTestContention(RWLock aLock)
    throws SelfTestException, InterruptedException
  {
    int count = 500000;
    int delay = 123;

    long start = System.currentTimeMillis();

    // Fire up a bunch of threads
    for (int i = 0; i < 3*100; i++) {
      RWTestHelper helper = new RWTestHelper(aLock, i % 3, 1, count, delay);
      Thread t = new Thread(helper);
      t.start();
      ThreadEnter();
    }

    ThreadWait();
    long end = System.currentTimeMillis();
    System.out.println("Stress test: duration=" + (end - start) + "ms");
  }

  static Object gThreadCounterLock = new Object();
  static int gThreadCounter;

  static void ThreadEnter()
    throws InterruptedException
  {
    synchronized (gThreadCounterLock) {
      gThreadCounter++;
    }
  }

  static void ThreadExit()
    throws InterruptedException
  {
    synchronized (gThreadCounterLock) {
      if (--gThreadCounter == 0) {
        gThreadCounterLock.notify();
      }
    }
  }

  static void ThreadWait()
    throws InterruptedException
  {
    synchronized (gThreadCounterLock) {
      while (gThreadCounter > 0) {
        gThreadCounterLock.wait();
      }
    }
  }

  static final boolean noisy = false;

  static void Enter(RWLock aLock, int aHow)
    throws InterruptedException
  {
    if (noisy) {
      System.out.println(Thread.currentThread() + ": enter " +
                         kStateToString[aHow]);
    }
    switch (aHow) {
    case kRead:
      aLock.enterReadLock();
      break;
    case kWrite:
      aLock.enterWriteLock();
      break;
    case kInvariantRead:
      aLock.enterInvariantReadLock();
      break;
    }
  }

  static void Exit(RWLock aLock, int aHow)
    throws InterruptedException
  {
    if (noisy) {
      System.out.println(Thread.currentThread() + ": exit " +
                         kStateToString[aHow]);
    }
    switch (aHow) {
    case kRead:
      aLock.exitReadLock();
      break;
    case kWrite:
      aLock.exitWriteLock();
      break;
    case kInvariantRead:
      aLock.exitInvariantReadLock();
      break;
    }
  }

  static class RWTestHelper implements Runnable {
    RWLock fLock;
    int fHow;
    int fValue;
    int fCount;
    int fDelay;

    RWTestHelper(RWLock aLock, int aHow, int aValue, int aCount, int aDelay) {
      fLock = aLock;
      fHow = aHow;
      fValue = aValue;
      fCount = aCount;
      fDelay = aDelay;
    }

    public void run() {
      try {
        for (int i = 0; i < fCount; i++) {
          SelfTestRWLock.Enter(fLock, fHow);
          fValue /= 4;
          SelfTestRWLock.Exit(fLock, fHow);
          if ((i % 1000) == 0) {
            Thread.sleep(fDelay);
          }
        }
        SelfTestRWLock.ThreadExit();
      } catch (InterruptedException e) {
      }
    }
  }
}
