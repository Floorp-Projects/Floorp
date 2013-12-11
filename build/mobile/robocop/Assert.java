/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

public interface Assert {
    void dumpLog(String message);
    void dumpLog(String message, Throwable t);
    void setLogFile(String filename);
    void setTestName(String testName);
    void endTest();

    void ok(boolean condition, String name, String diag);
    void is(Object actual, Object expected, String name);
    void isnot(Object actual, Object notExpected, String name);
    void todo(boolean condition, String name, String diag);
    void todo_is(Object actual, Object expected, String name);
    void todo_isnot(Object actual, Object notExpected, String name);
    void info(String name, String message);

    // robocop-specific asserts
    void ispixel(int actual, int r, int g, int b, String name);
    void isnotpixel(int actual, int r, int g, int b, String name);
}
