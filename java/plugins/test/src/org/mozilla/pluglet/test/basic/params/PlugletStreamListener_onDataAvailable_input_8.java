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
 *  The mark-reset functionality is tested.
 *  The supporting of the mark-reset isn't specified so test is passed by default 
 *  in case when stream doesn't supported this functionality.
 *  The testing is done with mark value less than just pushed length.
 *  
 *  
 *
 */

public class PlugletStreamListener_onDataAvailable_input_8 implements Test {
    private int readLength = 0;

    public void execute(TestContext context){
        if (context.failedIsRegistered()) {
            return;
        }
        InputStream is = context.getInputStream();
        int length = context.getLength();
        if (length < 1) {
            context.registerFAILED("The pushed length is less than 1");
        }
        if (is.markSupported()) {
            is.mark(length-1);
            byte[] buf = new byte[length-1];
            if (!fill(buf, is)) {
                context.registerFAILED("The pushed portion can't be read");
                return;
            }
            // reset and reading again
            try {
                is.reset();
            } catch (IOException e) {
                context.registerFAILED("The stream can't be reset");
                return;
            }
            byte[] repeatBuf = new byte[length];
            if (!fill(repeatBuf, is)) {
                context.registerFAILED("The pushed portion can't be read again after reset");
                return;
            }  
            // check the corectnesses of the read again data and the next byte
            RandomAccessFile testFile;
            try {
                testFile = new RandomAccessFile(context.getTestDir()+"/test.tdt", "r");    
                testFile.seek(readLength);
            } catch (IOException e) {
                context.registerFAILED("The file can't be seek");
                return;
            }                
            byte b;
            for (int i = 0; i < length; i++) {
                try {
                    b = testFile.readByte();
                } catch (IOException e) {
                    context.registerFAILED("The file can't be read");
                    return;
                }
                if (b != repeatBuf[i]) {
                    context.registerFAILED("The bytes aren't equal");
                    return;
                }                    
            }
            readLength += length;
            try {
                if (readLength == testFile.length()) {
                    context.registerPASSED("");
                } 
            } catch (IOException e) {
                context.registerFAILED("The file length can't be got");
                return;
            }


        } else { 
            context.registerPASSED("Mark functionality isn't supported - passed by default");
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
