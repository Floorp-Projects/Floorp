/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Client QA Team, St. Petersburg, Russia
 */


 
package org.mozilla.webclient.test.basic;

/*
 * TestContext.java
 */

import java.util.Properties;
import java.util.StringTokenizer;
import java.awt.Frame;
import java.awt.TextArea;
import java.awt.Panel;
import java.awt.Button;
import java.awt.BorderLayout;
import java.io.FileInputStream;
import java.io.File;
import java.io.RandomAccessFile;
import java.io.IOException;
import org.mozilla.webclient.*;

public class TestContext {
    public static String TEST_PROPERTIES_FILE_NAME = "TestProperties";
    public static String COMMON_PROPERTIES_FILE_NAME = "CommonProperties";
    
    //Names for properties from common properties file
    public static String DELAY_INTERNAL_PROP_NAME = "DELAY_INTERNAL";
    public static String DELAY_OF_CYCLE_PROP_NAME = "DELAY_OF_CYCLE";
    public static String TEST_LOG_FILE_PROP_NAME = "TEST_LOG_FILE";
    public static String TEST_RESULT_FILE_PROP_NAME = "TEST_RESULT_FILE";
  
    //Names for properties from init file    
    public static String BROWSER_BIN_DIR_PROP_NAME = "BROWSER_BIN_DIR";
    public static String TEST_TOP_DIR_PROP_NAME = "TEST_TOP_DIR";
    public static String TEST_ID_PROP_NAME = "TEST_ID";
    public static String TEST_ARGUMENTS_PROP_NAME = "TEST_ARGUMENTS";

    //Names for properties from individual TestProperties files
    public static String TEST_URL_PROP_NAME = "TEST_HTML";
    public static String PAGES_TO_LOAD_PROP_NAME="PAGES_TO_LOAD";
    public static String TEST_CLASS_PROP_NAME = "TEST_CLASS";

    public static int FAILED = 0;
    public static int NOT_DEF = 1;
    public static int PASSED = 3;
    public static int UNIMPLEMENTED = 4;

    private static String defaultComment = "default";
    public static int defaultResult = FAILED;
    public static int result = NOT_DEF;
    private static String logFile = null;
    private static String resFile = null;

    //Internal private variables
    private Properties testProps = null;
    private BrowserControl browserControl = null;
    private TestRunner runner = null;
    private TestDocLoadListener tdl = null;
    private String fileSeparator = null;
    private Frame testWindow = null;
    private TextArea descrArea = null;
    private Panel buttonPanel = null;
    private int executionCount = 0;

    public TestContext(String initFileName) {
        File propFile = new File(initFileName);
        fileSeparator = System.getProperty("file.separator");
        testProps = new Properties();
        //Load properties from init file
        try {
            testProps.load(new FileInputStream(propFile));
        } catch (Exception e) {
            System.err.println("BAD initFile : " + e);
            System.exit(-99);
        }
        //Load properties from file with common properties
        propFile = new File(getTestTopDir() + fileSeparator + "config" + fileSeparator + COMMON_PROPERTIES_FILE_NAME);
        try {
            testProps.load(new FileInputStream(propFile));
        } catch (Exception e) {
            System.err.println("Couldn't read CommonProperties : " + e);
            System.exit(-99);
        }
        //Load properties from file with test properties
        propFile = new File(getTestDir() + fileSeparator + TEST_PROPERTIES_FILE_NAME);
        try {
            testProps.load(new FileInputStream(propFile));
        } catch (Exception e) {
            System.err.println("Couldn't read TestProperties : " + e);
            System.exit(-99);
        }
        logFile = testProps.getProperty(TEST_LOG_FILE_PROP_NAME);
        resFile = testProps.getProperty(TEST_RESULT_FILE_PROP_NAME);
    }
    public String getProperty(String propName) {
        return testProps.getProperty(propName,null);
    }
    public Properties getProperties() {
        return testProps;
    }
    public void setRunner(TestRunner runner) {
        this.runner = runner;
    }
    public TestRunner getRunner() {
        return runner;
    }
    public String getTestTopDir() {
        return getProperty(TEST_TOP_DIR_PROP_NAME);
    }
    public String getBrowserBinDir() {
        System.out.println("Browser bin is: "  + getProperty(BROWSER_BIN_DIR_PROP_NAME));
	return getProperty(BROWSER_BIN_DIR_PROP_NAME);
    }
    public String getTestID() {
        return getProperty(TEST_ID_PROP_NAME);
    }
    public String getTestClass() {
        return getProperty(TEST_CLASS_PROP_NAME);
    }
    public String getCurrentTestArguments() {
        return getProperty(TEST_ARGUMENTS_PROP_NAME);
    } 
    public String getTestURL() {
        return getProperty(TEST_URL_PROP_NAME);
    }
    public String[] getPagesToLoad() {
        StringTokenizer st = new StringTokenizer(getProperty(PAGES_TO_LOAD_PROP_NAME),",");
        String[] pageArr = new String[st.countTokens()];
        for(int i=0;st.hasMoreTokens();i++) {
            pageArr[i] = st.nextToken();
        }
        String[] tmp = new String[pageArr.length];
        for(int i=0;i<pageArr.length;i++) {
            tmp[i]=pageArr[i];
        }
        return tmp;
    }
    public String getTestDir() {
        String ID = getTestID();
        if (ID.indexOf(":") > 0) {
            ID = ID.substring(0,ID.indexOf(":"));
        }
        return getTestTopDir() + fileSeparator + "build" + fileSeparator + "test" + fileSeparator + ID;
    }
    public void setBrowserControl(BrowserControl browserControl) {
        this.browserControl = browserControl;
    }
    public BrowserControl getBrowserControl() {
        return browserControl;
    }
    public TestDocLoadListener getDocLoadListener() {
        return tdl;
    }
    public void setDocLoadListener(TestDocLoadListener tdl) {
        this.tdl = tdl; 
    }
    public void removeDocLoadListener() {
        this.tdl = null;
    }
    public void setTestWindow(Frame testWindow) {
        this.testWindow = testWindow;
    }
    public Frame getTestWindow() {
        return testWindow;
    }
    public void setButtonPanel(Panel bp) {
        buttonPanel = bp;
    }
    public void addButton(Button b) {
        buttonPanel.add(b);
	buttonPanel.doLayout();
    }
    public void setDescrArea(TextArea ta) {
        descrArea = ta;
    }
    public void addDescription(String msg) {
	System.out.println("Appending to DA: "+msg);
        descrArea.append(msg);
	descrArea.repaint();
    }
    public void setDescription(String msg) {
        descrArea.setText(msg);
    }
    public void increaseExecutionCount() {
	executionCount++;
    }
    public int getExecutionCount() {
	return executionCount;
    }
    public void setExecutionCount(int count) {
	executionCount = count;
    }
    public void setDefaultResult(int res) { 
        defaultResult = res;
    }
    public void setDefaultComment(String comm) { 
        defaultComment = comm;
    }

    static public void registerFAILED(String comment) {
        if ((result == NOT_DEF)||(result == PASSED)) {
            result = FAILED;
            appendToFile(logFile, "FAILED: "+comment+"\n", false);
            appendToFile(resFile, "FAILED="+comment+"\n", true);
            System.err.println("Register FAILED " + comment);
        }
    }
    static public void registerUNIMPLEMENTED(String comment) {
        //We must set UNIMPLEMENTED over ANI previous result
            result = UNIMPLEMENTED;
            appendToFile(logFile, "UNIMPLEMENTED: "+comment+"\n", false);
            appendToFile(resFile, "UNIMPLEMENTED="+comment+"\n", true);
            System.err.println("Register UNIMPLEMENTED: " + comment);
    }
    static public void registerPASSED(String comment) {
        if (result == NOT_DEF) {
            result = PASSED;
            appendToFile(logFile, "PASSED: "+comment+"\n", false);
            appendToFile(resFile, "PASSED="+comment+"\n", true);
            System.err.println("Register PASSED: " + comment); 
        }
    }
    public static void registerIfNotRegistered(int res) {
        if (result == NOT_DEF){
            if (res == FAILED) {
                registerFAILED("By checking thread:" + defaultComment); 
            } else {
                registerPASSED("By checking thread:" + defaultComment);
            }
        }
    }
    public static boolean failedIsRegistered() {
        return (result == FAILED)||(result == UNIMPLEMENTED);
    }
    public static boolean resultIsSet() {
        return (result != NOT_DEF);
    }
    private static void appendToFile(String file, String str, boolean fromStart) {
        try {
            RandomAccessFile f = new RandomAccessFile(file, "rw");
            if (fromStart) {
                f.seek(0);
            } else {
                long length = f.length();
                f.seek(length);
            }
            f.writeBytes(str);
            f.close();
        } catch (IOException e) {
            System.out.println("IOException appending to file " + file);
            System.out.println(e.toString());
        }     
    }
    public void delete() {
        registerIfNotRegistered(defaultResult);
        testProps = null;
        runner = null;
        browserControl = null;
        fileSeparator = null;
        testWindow = null;
    }
}











