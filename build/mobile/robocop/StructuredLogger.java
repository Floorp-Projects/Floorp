/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.HashSet;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.json.JSONObject;

// This implements the structured logging API described here: http://mozbase.readthedocs.org/en/latest/mozlog_structured.html
public class StructuredLogger {
    private final static HashSet<String> validTestStatus = new HashSet<String>(Arrays.asList("PASS", "FAIL", "TIMEOUT", "NOTRUN", "ASSERT"));
    private final static HashSet<String> validTestEnd = new HashSet<String>(Arrays.asList("PASS", "FAIL", "OK", "ERROR", "TIMEOUT",
                                                                               "CRASH", "ASSERT", "SKIP"));

    private String mName;
    private String mComponent;
    private LoggerCallback mCallback;

    static public interface LoggerCallback {
        public void call(String output);
    }

    /* A default logger callback that prints the JSON output to stdout.
     * This is not to be used in robocop as we write to a log file. */
    static class StandardLoggerCallback implements LoggerCallback {
        public void call(String output) {
            System.out.println(output);
        }
    }

    public StructuredLogger(String name, String component, LoggerCallback callback) {
        mName = name;
        mComponent = component;
        mCallback = callback;
    }

    public StructuredLogger(String name, String component) {
        this(name, component, new StandardLoggerCallback());
    }

    public StructuredLogger(String name) {
        this(name, null, new StandardLoggerCallback());
    }

    public void suiteStart(List<String> tests, Map<String, Object> runInfo) {
        HashMap<String, Object> data = new HashMap<String, Object>();
        data.put("tests", tests);
        if (runInfo != null) {
            data.put("run_info", runInfo);
        }
        this.logData("suite_start", data);
    }

    public void suiteStart(List<String> tests) {
        this.suiteStart(tests, null);
    }

    public void suiteEnd() {
        this.logData("suite_end");
    }

    public void testStart(String test) {
        HashMap<String, Object> data = new HashMap<String, Object>();
        data.put("test", test);
        this.logData("test_start", data);
    }

    public void testStatus(String test, String subtest, String status, String expected, String message) {
        status = status.toUpperCase();
        if (!StructuredLogger.validTestStatus.contains(status)) {
            throw new IllegalArgumentException("Unrecognized status: " + status);
        }

        HashMap<String, Object> data = new HashMap<String, Object>();
        data.put("test", test);
        data.put("subtest", subtest);
        data.put("status", status);

        if (message != null) {
            data.put("message", message);
        }
        if (!expected.equals(status)) {
            data.put("expected", expected);
        }

        this.logData("test_status", data);
    }

    public void testStatus(String test, String subtest, String status, String message) {
        this.testStatus(test, subtest, status, "PASS", message);
    }

    public void testEnd(String test, String status, String expected, String message, Map<String, Object> extra) {
        status = status.toUpperCase();
        if (!StructuredLogger.validTestEnd.contains(status)) {
            throw new IllegalArgumentException("Unrecognized status: " + status);
        }

        HashMap<String, Object> data = new HashMap<String, Object>();
        data.put("test", test);
        data.put("status", status);

        if (message != null) {
            data.put("message", message);
        }
        if (extra != null) {
            data.put("extra", extra);
        }
        if (!expected.equals(status) && !status.equals("SKIP")) {
            data.put("expected", expected);
        }

        this.logData("test_end", data);
    }

    public void testEnd(String test, String status, String expected, String message) {
        this.testEnd(test, status, expected, message, null);
    }

    public void testEnd(String test, String status, String message) {
        this.testEnd(test, status, "OK", message, null);
    }


    public void debug(String message) {
        this.log("debug", message);
    }

    public void info(String message) {
        this.log("info", message);
    }

    public void warning(String message) {
        this.log("warning", message);
    }

    public void error(String message) {
        this.log("error", message);
    }

    public void critical(String message) {
        this.log("critical", message);
    }

    private void log(String level, String message) {
        HashMap<String, Object> data = new HashMap<String, Object>();
        data.put("message", message);
        data.put("level", level);
        this.logData("log", data);
    }

    private HashMap<String, Object> makeLogData(String action, Map<String, Object> data) {
        HashMap<String, Object> allData = new HashMap<String, Object>();
        allData.put("action", action);
        allData.put("time", System.currentTimeMillis());
        allData.put("thread", JSONObject.NULL);
        allData.put("pid", JSONObject.NULL);
        allData.put("source", mName);
        if (mComponent != null) {
            allData.put("component", mComponent);
        }

        allData.putAll(data);

        return allData;
    }

    private void logData(String action, Map<String, Object> data) {
        HashMap<String, Object> logData = this.makeLogData(action, data);
        JSONObject jsonObject = new JSONObject(logData);
        mCallback.call(jsonObject.toString());
    }

    private void logData(String action) {
        this.logData(action, new HashMap<String, Object>());
    }

}
