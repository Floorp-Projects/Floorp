/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.LinkedList;

import android.os.SystemClock;

public class FennecMochitestAssert implements Assert {
    private LinkedList<testInfo> mTestList = new LinkedList<testInfo>();

    // Internal state variables to make logging match up with existing mochitests
    private int mPassed = 0;
    private int mFailed = 0;
    private int mTodo = 0;

    // Used to write the first line of the test file
    private boolean mLogStarted = false;

    // Used to write the test-start/test-end log lines
    private String mLogTestName = "";

    // Measure the time it takes to run test case
    private long mStartTime = 0;

    // Structured logger
    private StructuredLogger mLogger;

    /** Write information to a logfile and logcat */
    public void dumpLog(String message) {
        mLogger.info(message);
    }

    public void dumpLog(String message, Throwable t) {
        mLogger.error(message + " - " + t.toString());
    }

    /** Write information to a logfile and logcat */
    static class DumpLogCallback implements StructuredLogger.LoggerCallback {
        public void call(String output) {
            FennecNativeDriver.log(FennecNativeDriver.LogLevel.INFO, output);
        }
    }


    public FennecMochitestAssert() {
        mLogger = new StructuredLogger("robocop", new DumpLogCallback());
    }

    /** Set the filename used for dumpLog. */
    public void setLogFile(String filename) {
        FennecNativeDriver.setLogFile(filename);

        String message;
        if (!mLogStarted) {
            mLogger.info("SimpleTest START");
            mLogStarted = true;
        }

        if (mLogTestName != "") {
            long diff = SystemClock.uptimeMillis() - mStartTime;
            mLogger.testEnd(mLogTestName, "OK", "finished in " + diff + "ms");
            mLogTestName = "";
        }
    }

    public void setTestName(String testName) {
        String[] nameParts = testName.split("\\.");
        mLogTestName = nameParts[nameParts.length - 1];
        mStartTime = SystemClock.uptimeMillis();

        mLogger.testStart(mLogTestName);
    }

    class testInfo {
        public boolean mResult;
        public String mName;
        public String mDiag;
        public boolean mTodo;
        public boolean mInfo;
        public testInfo(boolean r, String n, String d, boolean t, boolean i) {
            mResult = r;
            mName = n;
            mDiag = d;
            mTodo = t;
            mInfo = i;
        }

    }

    /** Used to log a subtest's result.
     * test represents the subtest (an assertion).
     * passStatus and passExpected are the actual status and the expected status if the assertion is true.
     * failStatus and failExpected are the actual status and the expected status otherwise.
     */
    private void _logMochitestResult(testInfo test, String passStatus, String passExpected, String failStatus, String failExpected) {
        boolean isError = true;
        if (test.mResult || test.mTodo) {
            isError = false;
        }
        if (test.mResult)
        {
            mLogger.testStatus(mLogTestName, test.mName, passStatus, passExpected, test.mDiag);
        } else {
            mLogger.testStatus(mLogTestName, test.mName, failStatus, failExpected, test.mDiag);
        }

        if (test.mInfo) {
            // do not count TEST-INFO messages
        } else if (test.mTodo) {
            mTodo++;
        } else if (isError) {
            mFailed++;
        } else {
            mPassed++;
        }
        if (isError) {
            String message = "TEST-UNEXPECTED-" + failStatus + " | " + mLogTestName + " | "
                    + test.mName + " - " + test.mDiag;
            junit.framework.Assert.fail(message);
        }
    }

    public void endTest() {
        String message;

        if (mLogTestName != "") {
            long diff = SystemClock.uptimeMillis() - mStartTime;
            mLogger.testEnd(mLogTestName, "OK", "finished in " + diff + "ms");
            mLogTestName = "";
        }

        mLogger.info("TEST-START | Shutdown");
        mLogger.info("Passed: " + Integer.toString(mPassed));
        mLogger.info("Failed: " + Integer.toString(mFailed));
        mLogger.info("Todo: " + Integer.toString(mTodo));
        mLogger.info("SimpleTest FINISHED");
    }

    public void ok(boolean condition, String name, String diag) {
        testInfo test = new testInfo(condition, name, diag, false, false);
        _logMochitestResult(test, "PASS", "PASS", "FAIL", "PASS");
        mTestList.add(test);
    }

    public void is(Object actual, Object expected, String name) {
        boolean pass = checkObjectsEqual(actual, expected);
        ok(pass, name, getEqualString(actual, expected, pass));
    }

    public void isnot(Object actual, Object notExpected, String name) {
        boolean pass = checkObjectsNotEqual(actual, notExpected);
        ok(pass, name, getNotEqualString(actual, notExpected, pass));
    }

    public void ispixel(int actual, int r, int g, int b, String name) {
        int aAlpha = ((actual >> 24) & 0xFF);
        int aR = ((actual >> 16) & 0xFF);
        int aG = ((actual >> 8) & 0xFF);
        int aB = (actual & 0xFF);
        boolean pass = checkPixel(actual, r, g, b);
        ok(pass, name, "Color rgba(" + aR + "," + aG + "," + aB + "," + aAlpha + ")" + (pass ? " " : " not") + " close enough to expected rgb(" + r + "," + g + "," + b + ")");
    }

    public void isnotpixel(int actual, int r, int g, int b, String name) {
        int aAlpha = ((actual >> 24) & 0xFF);
        int aR = ((actual >> 16) & 0xFF);
        int aG = ((actual >> 8) & 0xFF);
        int aB = (actual & 0xFF);
        boolean pass = checkPixel(actual, r, g, b);
        ok(!pass, name, "Color rgba(" + aR + "," + aG + "," + aB + "," + aAlpha + ")" + (!pass ? " is" : " is not") + " different enough from rgb(" + r + "," + g + "," + b + ")");
    }

    private boolean checkPixel(int actual, int r, int g, int b) {
        // When we read GL pixels the GPU has already processed them and they
        // are usually off by a little bit. For example a CSS-color pixel of color #64FFF5
        // was turned into #63FFF7 when it came out of glReadPixels. So in order to compare
        // against the expected value, we use a little fuzz factor. For the alpha we just
        // make sure it is always 0xFF. There is also bug 691354 which crops up every so
        // often just to make our lives difficult. However the individual color components
        // should never be off by more than 8.
        int aAlpha = ((actual >> 24) & 0xFF);
        int aR = ((actual >> 16) & 0xFF);
        int aG = ((actual >> 8) & 0xFF);
        int aB = (actual & 0xFF);
        boolean pass = (aAlpha == 0xFF) /* alpha */
                           && (Math.abs(aR - r) <= 8) /* red */
                           && (Math.abs(aG - g) <= 8) /* green */
                           && (Math.abs(aB - b) <= 8); /* blue */
        if (pass) {
            return true;
        } else {
            return false;
        }
    }

    public void todo(boolean condition, String name, String diag) {
        testInfo test = new testInfo(condition, name, diag, true, false);
        _logMochitestResult(test, "PASS", "FAIL", "FAIL", "FAIL");
        mTestList.add(test);
    }

    public void todo_is(Object actual, Object expected, String name) {
        boolean pass = checkObjectsEqual(actual, expected);
        todo(pass, name, getEqualString(actual, expected, pass));
    }

    public void todo_isnot(Object actual, Object notExpected, String name) {
        boolean pass = checkObjectsNotEqual(actual, notExpected);
        todo(pass, name, getNotEqualString(actual, notExpected, pass));
    }

    private boolean checkObjectsEqual(Object a, Object b) {
        if (a == null || b == null) {
            if (a == null && b == null) {
                return true;
            }
            return false;
        } else {
            return a.equals(b);
        }
    }

    private String getEqualString(Object a, Object b, boolean pass) {
        if (pass) {
            return a + " should equal " + b;
        }
        return "got " + a + ", expected " + b;
    }

    private boolean checkObjectsNotEqual(Object a, Object b) {
        if (a == null || b == null) {
            if ((a == null && b != null) || (a != null && b == null)) {
                return true;
            } else {
                return false;
            }
        } else {
            return !a.equals(b);
        }
    }

    private String getNotEqualString(Object a, Object b, boolean pass) {
        if(pass) {
            return a + " should not equal " + b;
        }
        return "didn't expect " + a + ", but got it";
    }

    public void info(String name, String message) {
        mLogger.info(name + " | " + message);
    }
}
