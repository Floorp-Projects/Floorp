/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.LinkedList;
import android.os.SystemClock;

public class FennecMochitestAssert implements Assert {
    private LinkedList<testInfo> mTestList = new LinkedList<testInfo>();

    // Internal state variables to make logging match up with existing mochitests
    private int mLineNumber = 0;
    private int mPassed = 0;
    private int mFailed = 0;
    private int mTodo = 0;
    
    // Used to write the first line of the test file
    private boolean mLogStarted = false;

    // Used to write the test-start/test-end log lines
    private String mLogTestName = "";

    // Measure the time it takes to run test case
    private long mStartTime = 0;

    public FennecMochitestAssert() {
    }

    /** Write information to a logfile and logcat */
    public void dumpLog(String message) {
        FennecNativeDriver.log(FennecNativeDriver.LogLevel.INFO, message);
    }

    /** Write information to a logfile and logcat */
    public void dumpLog(String message, Throwable t) {
        FennecNativeDriver.log(FennecNativeDriver.LogLevel.INFO, message, t);
    }

    /** Set the filename used for dumpLog. */
    public void setLogFile(String filename) {
        FennecNativeDriver.setLogFile(filename);

        String message;
        if (!mLogStarted) {
            dumpLog(Integer.toString(mLineNumber++) + " INFO SimpleTest START");
            mLogStarted = true;
        }

        if (mLogTestName != "") {
            long diff = SystemClock.uptimeMillis() - mStartTime;
            message = Integer.toString(mLineNumber++) + " INFO TEST-END | " + mLogTestName;
            message += " | finished in " + diff + "ms";
            dumpLog(message);
            mLogTestName = "";
        }
    }

    public void setTestName(String testName) {
        String[] nameParts = testName.split("\\.");
        mLogTestName = nameParts[nameParts.length - 1];
        mStartTime = SystemClock.uptimeMillis();

        dumpLog(Integer.toString(mLineNumber++) + " INFO TEST-START | " + mLogTestName);
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

    private void _logMochitestResult(testInfo test, String passString, String failString) {
        boolean isError = true;
        String resultString = failString;
        if (test.mResult || test.mTodo) {
            isError = false;
        }
        if (test.mResult)
        {
            resultString = passString;
        }
        String diag = test.mName;
        if (test.mDiag != null) diag += " - " + test.mDiag;

        String message = Integer.toString(mLineNumber++) + " INFO " + resultString + " | " + mLogTestName + " | " + diag;
        dumpLog(message);

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
            junit.framework.Assert.fail(message);
        }
    }

    public void endTest() {
        String message;

        if (mLogTestName != "") {
            long diff = SystemClock.uptimeMillis() - mStartTime;
            message = Integer.toString(mLineNumber++) + " INFO TEST-END | " + mLogTestName;
            message += " | finished in " + diff + "ms";
            dumpLog(message);
            mLogTestName = "";
        }

        message = Integer.toString(mLineNumber++) + " INFO TEST-START | Shutdown";
        dumpLog(message);
        message = Integer.toString(mLineNumber++) + " INFO Passed: " + Integer.toString(mPassed);
        dumpLog(message);
        message = Integer.toString(mLineNumber++) + " INFO Failed: " + Integer.toString(mFailed);
        dumpLog(message);
        message = Integer.toString(mLineNumber++) + " INFO Todo: " + Integer.toString(mTodo);
        dumpLog(message);
        message = Integer.toString(mLineNumber++) + " INFO SimpleTest FINISHED";
        dumpLog(message);
    }

    public void ok(boolean condition, String name, String diag) {
        testInfo test = new testInfo(condition, name, diag, false, false);
        _logMochitestResult(test, "TEST-PASS", "TEST-UNEXPECTED-FAIL");
        mTestList.add(test);
    }

    public void is(Object a, Object b, String name) {
        boolean pass = checkObjectsEqual(a,b);
        ok(pass, name, getEqualString(a,b, pass));
    }
    
    public void isnot(Object a, Object b, String name) {
        boolean pass = checkObjectsNotEqual(a,b);
        ok(pass, name, getNotEqualString(a,b,pass));
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
        _logMochitestResult(test, "TEST-UNEXPECTED-PASS", "TEST-KNOWN-FAIL");
        mTestList.add(test);
    }

    public void todo_is(Object a, Object b, String name) {
        boolean pass = checkObjectsEqual(a,b);
        todo(pass, name, getEqualString(a,b,pass));
    }

    public void todo_isnot(Object a, Object b, String name) {
        boolean pass = checkObjectsNotEqual(a,b);
        todo(pass, name, getNotEqualString(a,b,pass));
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
        testInfo test = new testInfo(true, name, message, false, true);
        _logMochitestResult(test, "TEST-INFO", "INFO FAILED?");
    }
}
