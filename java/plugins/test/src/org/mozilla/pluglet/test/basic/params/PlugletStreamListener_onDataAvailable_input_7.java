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



package org.mozilla.pluglet.test.basic.params;

import java.io.*;
import java.util.*;
import java.net.*;
import org.mozilla.util.*;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.test.basic.*;

/*  
 *  Description:
 *  Test for passed in onDataAvailable method 2d parameter (input)   
 *  The goal is to check the correctnesses of its method close() 
 *  The test reads the first whole portion of the data, closes the stream and returns.
 *  Passed result will be registered only if after that the     
 *  onStopBinding method will be called.
 *
 */

public class PlugletStreamListener_onDataAvailable_input_7 implements Test {
    private int readLength = 0;
    private int state = 0;

    public void execute(TestContext context){
        if (context.failedIsRegistered()) {
            return;
        }
        if (context.getTestStage()
            .equals(TestStage.getStageById(TestStage.PLUGLET_STREAM_LISTENER_ON_DATA_AVAILABLE))) {

            if (state == 1) {
                context.registerFAILED("The onDataAvailable called after closing stream");
                return;
            }
            state = 1;
            
            InputStream is = context.getInputStream();
            int length = context.getLength();
            byte[] buf = new byte[1024];
            int l;
            int readAmount = 0;
            while (readAmount < length) {                
                try {
                    l = is.read(buf);
                } catch (IOException e) {
                    context.registerFAILED("The pushed portion can't be read");
                    return;
                }
                if (l > 0) {
                    readAmount += l;
                } else if (l == 0) {
                    context.registerFAILED("The read length is 0");
                    return;
                } else {
                    context.registerFAILED("The pushed portion can't be read");
                    return;
                }            
            }
            
            try {
                is.close();
            } catch (IOException e) {
                context.registerFAILED("Exception while closing stream");
                return;            
            }
        } else { 
            // it is on the TestStage.PLUGLET_STREAM_LISTENER_ON_STOP_BINDING
            context.registerPASSED("");
        }
    }

}
