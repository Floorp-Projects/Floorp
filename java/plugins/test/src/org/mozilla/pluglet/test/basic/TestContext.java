/* 
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
 */



package org.mozilla.pluglet.test.basic;

import java.util.*;
import java.io.*;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.util.*;

public class TestContext {

    public static String TEST_PARAMETERS_FILE_NAME = "TestProperties";
    public static String COMMON_PROPERTIES_FILE_NAME = "CommonProperties";
    public static String START_PROPERTIES_FILE_NAME = "StartProperties";
    
    public static String LOG_FILE_NAME_PROP_NAME = "LOG_FILE";
    public static String RESULT_FILE_NAME_PROP_NAME = "RESULT_FILE";
    public static String TEST_TOP_DIR_PROP_NAME = "TEST_TOP_DIR";
    public static String TEST_DIR_PROP_NAME = "TEST_DIR";

    public static int FAILED = 0;
    public static int NOT_DEF = 1;
    public static int PASSED = 3;

    
    private static String logFile;
    private static String resFile;
    private static int result = NOT_DEF;

    private static String tmpString;
    private static String tmpFile;
    private static boolean tmpFromStart;

    TestLoaderPluglet plugletFactory = null;
    String mimeType = null;
    Pluglet plugletInstance = null;
    PlugletStreamListener plugletStreamListener = null;
    PlugletManager pm = null;
    PlugletPeer peer =  null;
    PlugletStreamInfo plugletStreamInfo = null;  
    InputStream is = null;
    int length;
    String fileName = null;
    int status;
    int streamType;
    TestStage stage = null;
    TestDescr descr = null;
    Vector parameters = null;
    
    Properties props = new Properties();

    public TestContext() {

    }

    public void initialize() {
        try {
            File propsFile = new File(START_PROPERTIES_FILE_NAME);
            props.load(new FileInputStream(propsFile));
            propsFile = new File(getTopDir() + "/config/" + COMMON_PROPERTIES_FILE_NAME);
            props.load(new FileInputStream(propsFile));
            logFile = props.getProperty(LOG_FILE_NAME_PROP_NAME);
            resFile = props.getProperty(RESULT_FILE_NAME_PROP_NAME);            

            propsFile = new File(getTestDir() + "/" + TEST_PARAMETERS_FILE_NAME);
            props.load(new FileInputStream(propsFile));
        } catch (Exception e) {
            printLog("Cann't load properties file - exception:");
            printLog(e.getMessage());
            e.printStackTrace();
        }
        descr = new TestDescr();
        descr.initialize(props, this);
    }

    public TestContext getCopy() {
        TestContext tc = new TestContext();

        tc.plugletFactory = this.plugletFactory;
        tc.plugletInstance = this.plugletInstance;
        tc.plugletStreamListener = this.plugletStreamListener;
        tc.pm = this.pm;
        tc.peer = this.peer;
        tc.plugletStreamInfo = this.plugletStreamInfo;
        tc.is = this.is;
        tc.length = this.length;
        tc.status = this.status;
        tc.stage = this.stage;
        tc.descr = this.descr;
        tc.parameters = this.parameters;
        tc.props = props;
        return tc;
    }
    

    public void setMimeType(String mimeType) {
        this.mimeType = mimeType;
    }

    public String getMimeType() {
        return mimeType;
    }

    public void setPlugletFactory(TestLoaderPluglet plugletFactory) {
        this.plugletFactory = plugletFactory;
    }

    public PlugletFactory getPlugletFactory() {
        return plugletFactory;
    }

    public void setPlugletManager(PlugletManager pm) {
        this.pm = pm;
    }

    public PlugletManager getPlugletManager() {
        return pm;
    }

    public void setPluglet(Pluglet plugletInstance) {
        this.plugletInstance = plugletInstance;
    }

    public Pluglet getPluglet() {
        return plugletInstance;
    }

    public void setPlugletStreamListener(PlugletStreamListener plugletStreamListener) {
        this.plugletStreamListener = plugletStreamListener;
    }

    public PlugletStreamListener getPlugletStreamListener() {
        return plugletStreamListener;
    }

    public void setPlugletPeer(PlugletPeer peer) {
        this.peer = peer;
    }

    public PlugletPeer getPlugletPeer() {
        return peer;
    }

    public void setTestStage(TestStage stage) {
        this.stage = stage;
    }

    public TestStage getTestStage() {
        return stage;
    }

    public TestDescr getTestDescr() {
        return descr;
    }

    public void setPlugletStreamInfo(PlugletStreamInfo plugletStreamInfo){
        this.plugletStreamInfo = plugletStreamInfo;
    }

    public PlugletStreamInfo getPlugletStreamInfo(){
        return plugletStreamInfo;
    }

    public void setStatus(int status){
        this.status = status;
    }

    public int getStatus(){
        return status;
    }

    public void setInputStream(InputStream is) {
        this.is = is;
    }

    public InputStream getInputStream() {
        return is;
    }

    public void setLength(int length) {
        this.length = length;
    }

    public int getLength() {
        return length;
    }

    public void setFileName(String fileName){
        this.fileName = fileName;
    }

    public String getFileName(){
        return fileName;
    }

    public void setStreamType(int streamType) {
        this.streamType = streamType;
    }

    public int getStreamType() {
        return streamType;
    }

    // nb: move to TestDescr
    public void setParameters(Vector parameters){
        this.parameters = parameters;
    }

    public Vector getParameters(){
        return parameters;
    }

    public static Pluglet getOtherPluglet(){
        return null;
    }

    public String getTopDir() {
        return props.getProperty(TEST_TOP_DIR_PROP_NAME);
    }

    public String getTestDir() {
        return props.getProperty(TEST_DIR_PROP_NAME);
    }    

    public String getProperty(String propName){
        return props.getProperty(propName);    
    }

    public TestStage getStageByPropertyName(String propName) {

        String stageName = props.getProperty(propName);
        if (stageName == null) {
            printLog(propName + " isn't set");
            return null;
        } else {
            TestStage stage = TestStage.getStageByName(stageName);
            if (stage == null) {
                printLog(propName +" is set into non-existent value");
                return null;                
            }
            return stage;
        }
        
    }


    public static void registerIfNotRegistered(int res) {
        if (result == NOT_DEF){
            if (res == FAILED) {
                registerFAILED("By checking thread"); 
            } else {
                registerPASSED("By checking thread");
            }
        }
    }

    public static void registerFAILED(String comment) {
        result = FAILED;
        appendToFile(logFile, "FAILED: "+comment+"\n", false);
        appendToFile(resFile, "FAILED\n", true);
    }

    public static void registerPASSED(String comment) {
        if (result == NOT_DEF) {
            result = PASSED;
            appendToFile(logFile, "PASSED: "+comment+"\n", false);
            appendToFile(resFile, "PASSED\n", true);
        }
    }

    public static boolean failedIsRegistered(){
        return (result == FAILED);
    }

    public static void printLog(String logStr) {
        appendToFile(logFile, logStr+"\n", false);    
    }

    private static void appendToFile(String file, String str, boolean fromStart) {
        tmpString = str;
        tmpFile = file;
        tmpFromStart = fromStart;
        java.security.AccessController
            .doPrivileged(new java.security.PrivilegedAction(){
                public Object run(){
                    try {
                        RandomAccessFile f = new RandomAccessFile(tmpFile, "rw");
                        if (tmpFromStart) {
                            f.seek(0);
                        } else {
                            long length = f.length();
                            f.seek(length);
                        }
                        f.writeBytes(tmpString);
                        f.close();
                    } catch (IOException e) {
                        DebugPluglet.print("IOException appending to file "+tmpFile);
                        DebugPluglet.print(e.toString());
                    }     
                    return null;
                }
            });
    }
                

}






