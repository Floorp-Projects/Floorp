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
 *     
 *      
 *  
 *
 */

public class PlugletStreamListener_onDataAvailable_input_6 implements Test {
    private int readLength = 0;
    private int state = 0;

    public void execute(TestContext context){
        if (state == 1) {
            return;
        }
        state = 1;

        InputStream is = context.getInputStream();
        byte[] buf = new byte[1];
        int l;
        //reading something from the stream
        try {
            l = is.read(buf);
        } catch (IOException e) {
            context.registerFAILED("The input cann't be read");
            return;
        }
        if (l < 0) {
            context.registerFAILED("The input reported end of stream");
            return;
        }
        //closing the stream
        try {
            is.close();
        } catch (IOException e) {
            context.registerFAILED("Exception while closing stream");
            return;            
        }
        //trying to read something from the stream
        try {
            l = is.read(buf);
        } catch (IOException e) {
            context.registerPASSED("");
            return;
        }
        context.registerFAILED("The input haven't closed "+l);
        
    }
}
