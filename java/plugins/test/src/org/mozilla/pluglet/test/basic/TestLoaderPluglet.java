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
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import java.awt.*;
import java.awt.print.*;
import java.io.*;
import java.net.URL;


public class TestLoaderPluglet implements PlugletFactory {

    public static String DELAY_INTERNAL_PROP_NAME = "DELAY_INTERNAL";

    public static int defaultResult = TestContext.FAILED;

    public TestContext context = new TestContext();

    public TestLoaderPluglet(){
        System.out.println("--TestLoaderPluglet()");
        context.initialize();
        context.setPlugletFactory(this);
        try {
            PlugletCheckingThread checkingThread = 
                new PlugletCheckingThread((new Integer(context.getProperty(DELAY_INTERNAL_PROP_NAME))).intValue()*1000);
            checkingThread.start();	
        } catch (Exception e) {
            TestContext.printLog(DELAY_INTERNAL_PROP_NAME+" props isn't correctly set");
        }
    }


    public void initialize(PlugletManager manager) {	
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_INITIALIZE));
        context.setPlugletManager(manager);
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public Pluglet createPluglet(String mimeType) {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_CREATE_PLUGLET_INSTANCE));
        context.setMimeType(mimeType);
        context.setPluglet(new TestInstance(context.getCopy()));
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        }
        return context.getPluglet();    
    }

    public void shutdown() {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_SHUTDOWN));
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }
}


class PlugletCheckingThread extends Thread {
    int timeout = Integer.MAX_VALUE;

    PlugletCheckingThread(int timeout){
        this.timeout = timeout;
    }

    public void run(){ 
        System.out.println("started thread to sleep "+timeout);
        try {
            sleep(timeout);
        } catch (Exception e) {
            TestContext.printLog("Checking thread had been interrupted");
        }
        TestContext.registerIfNotRegistered(TestLoaderPluglet.defaultResult);	    
    }

}



