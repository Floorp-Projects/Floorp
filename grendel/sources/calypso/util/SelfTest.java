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

/* ####
import calypso.core.SystemLog;
*/

/**
 * SelfTest for calypso.util package
 *
 * @author  sclark   04-22-97 3:52pm
 * @notes   runAllTests must be successfully run after every check-in
 */
public class SelfTest {
  public static void main(String args[]) {
    SelfTest selfTest = new SelfTest();
    try {
      selfTest.runAllTests();
    } catch (SelfTestException e) {
      System.err.println("SelfTestException: " + e);
      e.printStackTrace();
    }
  }

  /**
   * Default constructor
   *
   * @author     sclark   04-22-97 3:53pm
   **/
  public SelfTest()
  {
    reset();
  }

  private void reset()
  {
  }

  public void runAllTests()
    throws SelfTestException
  {
    runUnitTest();
    runThreadTest();
    runStressTest();
    runIDMapTest();
    runAtomTest();
  }

  public void runUnitTest()
    throws SelfTestException
  {

/* ####
    SystemLog.Instance().log("============================");
    SystemLog.Instance().log("Util unit tests:");

    SystemLog.Instance().log("Begin RWLock test.");
    runRWLockTest();
    SystemLog.Instance().log("End RWLock test.");

    SystemLog.Instance().log("============================");
*/
  }


  public void runThreadTest()
    throws SelfTestException
  {

/* ####
    SystemLog.Instance().log("============================");
    SystemLog.Instance().log("Util thread test.");


    SystemLog.Instance().log("============================");
*/
  }

  public void runStressTest()
    throws SelfTestException
  {

/* ####
    SystemLog.Instance().log("============================");
    SystemLog.Instance().log("Util stress test.");

    SystemLog.Instance().log("============================");
*/
  }

  public void runIDMapTest()
    throws SelfTestException
  {
    SelfTestIDMap.run();
  }

  public void runAtomTest()
    throws SelfTestException
  {
    SelfTestAtom.run();
  }

  public void runRWLockTest()
    throws SelfTestException
  {
    SelfTestRWLock.run();
  }
}
