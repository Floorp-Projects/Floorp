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
 *  The non-supporting of mark-reset functionality is tested.
 *  The supporting of the mark-reset isn't specified so test is passed by default 
 *  in case when stream  supports this functionality.
 *  
 *  
 *  
 *
 */

public class PlugletStreamListener_onDataAvailable_input_9 implements Test {
    private int readLength = 0;
    private int state = 0; 

    public void execute(TestContext context){
        if (context.failedIsRegistered()) {
            return;
        }
        if (state == 1) {
            return;
        }
        state = 1;
        InputStream is = context.getInputStream();
        int length = context.getLength();
        if (length < 1) {
            context.registerFAILED("The pushed length is less than 1");
        }
        if (!is.markSupported()) {
            try {
                is.reset();
            } catch (IOException e) {
                context.registerPASSED("");
                return;
            }

            // if there isn't IOException then check about fixed state
            int avail;
            try {
                avail = is.available();
            } catch (IOException e) {
                context.registerFAILED("Can't get available value - IOException");
                return;
            }
            byte[] buf = new byte[avail];
            if (!fill(buf, is)){
                context.registerFAILED("Can't read available amount");
                return;
            }
            try {
                is.reset();
            } catch (IOException e) {
                context.registerFAILED("Mark isn't supported, second (but not first) call throws IOException ");
                return;
            }
            byte[] testBuf = new byte[avail];
            if (!fill(testBuf, is)){
                context.registerFAILED("Can't read again available amount");
                return;
            }
            for (int i = 0; i < avail; i++) {
                if (buf[i] != testBuf[i]) {
                    context.registerFAILED("The data isn't the same after reset");
                    return;
                }
            }
            context.registerPASSED("");
            
        } else { 
            context.registerPASSED("Mark functionality is supported - passed by default");
        }
            
    }

    private boolean fill(byte[] buf, InputStream is) { 
        int readAmount = 0;
        int l = 0;
        while (readAmount < buf.length) {                
            try {
                l = is.read(buf, readAmount, buf.length - readAmount);
            } catch (IOException e) {
                return false;
            }
            if (l > 0) {
                readAmount += l;
            } else if (l == 0) {
                return false;
            } else {
                return false;
            }            
        }
        return true;        
    }

}
