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

public class TestInstance implements Pluglet{
  
    TestContext context = null;
  
    public TestInstance(TestContext context){
        this.context = context;
        context.setPluglet(this);
    }

    public void initialize(PlugletPeer peer) {

        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_INSTANCE_INITIALIZE));
        context.setPlugletPeer(peer);
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public void start() {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_INSTANCE_START));
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public void stop() {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_INSTANCE_STOP));
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public void destroy() {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_INSTANCE_DESTROY));
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public PlugletStreamListener newStream() {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_INSTANCE_NEW_STREAM));
        context.setPlugletStreamListener(new TestStreamListener(context.getCopy()));
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
        return context.getPlugletStreamListener();    
    }

    public void setWindow(Frame frame) {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_INSTANCE_SET_WINDOW));
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public void print(PrinterJob printerJob) {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_INSTANCE_PRINT));
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }
}

