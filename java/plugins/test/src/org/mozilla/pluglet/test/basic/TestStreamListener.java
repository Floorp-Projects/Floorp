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
import org.mozilla.util.*;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import java.awt.*;
import java.awt.print.*;
import java.io.*;
import java.net.URL;

public class TestStreamListener implements PlugletStreamListener {

    TestContext context = null;

    public TestStreamListener(TestContext context){
        this.context = context;
        context.setPlugletStreamListener(this);
    }

    public void onStartBinding(PlugletStreamInfo streamInfo) {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_STREAM_LISTENER_ON_START_BINDING));
        context.setPlugletStreamInfo(streamInfo);
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public void onDataAvailable(PlugletStreamInfo streamInfo, InputStream input,int  length) {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_STREAM_LISTENER_ON_DATA_AVAILABLE));
        context.setPlugletStreamInfo(streamInfo);
        context.setInputStream(input);
        context.setLength(length);
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public void onFileAvailable(PlugletStreamInfo streamInfo, String fileName) {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_STREAM_LISTENER_ON_FILE_AVAILABLE));
        context.setPlugletStreamInfo(streamInfo);
        context.setFileName(fileName);
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public void onStopBinding(PlugletStreamInfo streamInfo,int status) {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_STREAM_LISTENER_ON_STOP_BINDING));
        context.setPlugletStreamInfo(streamInfo);
        context.setStatus(status);
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
    }

    public int  getStreamType() {
        context.setTestStage(TestStage.getStageById(TestStage.PLUGLET_STREAM_LISTENER_GET_STREAM_TYPE));
        context.setStreamType(STREAM_TYPE_NORMAL);
        Test test = null;
        test = context.getTestDescr().getTestInstance(context.getTestStage());
        if (test !=null) {
            test.execute(context);
        } 
        return context.getStreamType();
    }
}
