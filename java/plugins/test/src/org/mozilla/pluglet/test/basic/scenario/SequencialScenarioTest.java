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



package org.mozilla.pluglet.test.basic.scenario;

import java.io.*;
import java.util.*;
import java.net.*;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.test.basic.*;

public class SequencialScenarioTest implements Test {

    
    private static String STAGE_PROP_NAME = "STAGE_";
    private static String STAGES_NUMBER_PROP_NAME = "STAGES_NUMBER";
    private static String STAGE_ACTION_PROP_SUFFIX = "_ACTION";
    private static String STAGE_ACTION_ARGUMENT_PROP_SUFFIX = "_ACTION_ARGUMENT";
    private static String STAGE_REPEATABLE_PROP_SUFFIX = "_REPEATABLE";
    private static String LOAD_STREAM_ACTION_NAME = "LOAD_STREAM";
    private static String LOAD_DEFAULT_BACK_URL_ACTION_NAME = "LOAD_DEFAULT_BACK_URL";
    private static String WINDOW_CLOSE_ACTION_NAME = "WINDOW_CLOSE";
    private static String WINDOW_RELOAD_ACTION_NAME = "WINDOW_RELOAD";
    private static String BACK_ACTION_NAME = "BACK";
    private static String WAITING_FOR_WRONG_ACTION_NAME = "WAITING_FOR_WRONG";
    private static String FORWARD_ACTION_NAME = "FORWARD";
    private static int stagesCounter = 0;
    private static int stagesNumber = 0;

    private static String action = null;

    private TestStage prevStage = null;
    private TestStage reqStage = null; 
    private boolean repeatable = false;
    private String actionName = null;
    private String actionArgument = null;

    public void execute(TestContext context){
        if (context.failedIsRegistered()) {
            return;
        }
        //one time getting the number of stages
        if (stagesCounter == 0) {
            try {
                stagesNumber = (new Integer(context.getProperty(STAGES_NUMBER_PROP_NAME))).intValue();
            } catch (Exception e) {
                context.registerFAILED(STAGES_NUMBER_PROP_NAME
                                       +" value isn't correctly set");
                return;
            }
        }
  
        TestStage curStage = context.getTestStage();
        if (!(curStage.equals(prevStage) && repeatable)){ 
            stagesCounter++;
            repeatable = false;
            if (stagesCounter > stagesNumber) {
                context.registerFAILED("Extra stage "+curStage.getName());
                return;
            }
            reqStage = 
                context.getStageByPropertyName(STAGE_PROP_NAME+stagesCounter);
            if (reqStage == null) {
                context.registerFAILED(STAGE_PROP_NAME
                                       +stagesCounter
                                       +" value isn't correctly set");
                return;
            }
            //one time for one stage getting the action and repeatable if any
            actionName = context.getProperty(STAGE_PROP_NAME
                                             +stagesCounter
                                             +STAGE_ACTION_PROP_SUFFIX);

            actionArgument = context.getProperty(STAGE_PROP_NAME
                                             +stagesCounter
                                             +STAGE_ACTION_ARGUMENT_PROP_SUFFIX);

            String rp = context.getProperty(STAGE_PROP_NAME
                                            +stagesCounter
                                            +STAGE_REPEATABLE_PROP_SUFFIX); 
            repeatable =(new Boolean(rp)).booleanValue(); 

        } 
            
        if (!reqStage.equals(curStage)) {
            context.registerFAILED("Current "+curStage.getName()
                                   +" isn't equal to required "+reqStage.getName());
            return;
        } else if (stagesCounter == stagesNumber) {
            context.registerPASSED("");
        }

        if (actionName != null && !runAction(actionName, actionArgument, context)) {
            context.registerFAILED(STAGE_PROP_NAME+stagesCounter
                                   +STAGE_ACTION_PROP_SUFFIX
                                   +" value "+actionName+" isn't correct");
            return;
        }
    
        
    }

    public static String getActionClear(){
        String res = action;
        action = null;
        return res;
    }
       
   private boolean runAction(String actionName, String actionArgument, TestContext context) {
       if (actionName == null){
           return true;
       } else if (actionName.equalsIgnoreCase(LOAD_STREAM_ACTION_NAME)){
           readStream(context);
           return true;
       } else if (actionName.equalsIgnoreCase(WINDOW_CLOSE_ACTION_NAME)) {
           action = WINDOW_CLOSE_ACTION_NAME;
           return true;
       } else if (actionName.equalsIgnoreCase(WINDOW_RELOAD_ACTION_NAME)) {
           action = WINDOW_RELOAD_ACTION_NAME;
           return true;
       } else if (actionName.equalsIgnoreCase(LOAD_DEFAULT_BACK_URL_ACTION_NAME)) {
           action = LOAD_DEFAULT_BACK_URL_ACTION_NAME;
           return true;
       } else if (actionName.equalsIgnoreCase(BACK_ACTION_NAME)) {
           action = BACK_ACTION_NAME;
           return true;            
       }
       return false;
       
   }


    private boolean readStream(TestContext context) {
        int lengthToRead = context.getLength();
        InputStream is = context.getInputStream();
        byte buf[] = new byte[1024];
        int readLength = 0;
        try {
            int l;
            while (readLength < lengthToRead) { 
                l = is.read(buf, 0, 1024);
                if (l > 0) {
                    readLength += l;
                } else {
                    break;
                }
            } 
        } catch (IOException e) {
            return false;
        }
        return true;
        
    }

}
