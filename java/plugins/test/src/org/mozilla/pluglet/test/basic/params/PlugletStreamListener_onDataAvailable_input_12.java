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
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.test.basic.*;

/*  
 *  Description:
 *  Test for passed in onDataAvailable method 2d parameter (input)   
 *  The goal is to check the correctnesses of its method read(byte[],int,int) 
 *  The test try to pass the 0 value as third parameter. 
 *  The 0 should be returned.
 *
 */

public class PlugletStreamListener_onDataAvailable_input_12 implements Test {
    private int state = 0;
    private int readAmount = 0;
 
    public void execute(TestContext context){
        if (context.failedIsRegistered()) {
            return;
        }
        if (state == 2) {
            // onDataAvailable call after reading the whole stream 
            context.registerFAILED("onDataAvailable is still being called");
            return;
        }
 
        InputStream is = context.getInputStream();

        if (state == 0) {
            state = 1;
            byte[] buf = new byte[1024];
            
            // fill buf with 0
            initBuf(buf, 0, 1024, (byte)0);
            int l = 0;
            try {
                l = is.read(buf, 0, 0);
            } catch (IOException e) {
                context.registerFAILED("Calling read method caused IOException");
                return;
            }
            if (l != 0) {
                context.registerFAILED("Reading with 0 3d argument didn't return 0"); 
                return;
            }
            if (!checkBuf(buf, 0, 1024, (byte)0)) {
                context.registerFAILED("The buffer was affected");
                return;
            }
            
            // repeat the same with initializing buffer with 1
            initBuf(buf, 0, 1024, (byte)1);
            try {
                l = is.read(buf, 0, 0);
            } catch (IOException e) {
                context.registerFAILED("Calling read method caused IOException");
                return;
            }
            if (l != 0) {
                context.registerFAILED("Reading with 0 3d argument didn't return 0"); 
                return;
            }
            if (!checkBuf(buf, 0, 1024, (byte)1)) {
                context.registerFAILED("The buffer was affected");
                return;
            }
            
        } 
        // now check the absence of the side effect on the input stream
        int length = context.getLength();
        if (length < 1) {
            context.registerFAILED("Just pushed length is less than 1");
            return;
        }
        long fileLength;
        RandomAccessFile testFile;
        try {
            testFile = new RandomAccessFile(context.getTestDir()+"/test.tdt", "r");
            fileLength = testFile.length();
        } catch (IOException e) {
            context.registerFAILED("The IOException on the test file");
            return;
        }
        if (!readAndCheckStreamIdentity(is, testFile, readAmount, length)) {
            context.registerFAILED("The stream had been affected by reading 0 bytes");
            return;            
        }  
        readAmount += length;
        if (readAmount == fileLength) {
            // after that and only after that the onStopBinding call should be
            state = 2;
            context.registerPASSED("");
        } else if (readAmount > fileLength) {
            context.registerFAILED("Read amount of data is great than should be");
            return;
        }  
        
    } 
    
    private void initBuf(byte[] buf, int off, int len, byte fillingValue) {
        for (int i = 0; i < len; i++) {
            buf[off + i] = fillingValue;            
        }
    } 

    private boolean checkBuf(byte[] buf, int off, int len, byte goldValue) {
        for (int i = 0; i < len; i++) {
            if (buf[off + i] != goldValue) {
                return false;
            }            
        }
        return true;
    }

    private boolean readAndCheckStreamIdentity(InputStream stream, 
                                               RandomAccessFile file, 
                                               int off, 
                                               int len) 
    {
        byte[] buf = new byte[len];
        int readLength = 0;
        try {
            file.seek(off);
            do {
                int l = stream.read(buf, readLength, len - readLength);  
                if (l <= 0) {
                    return false;                    
                }
                byte b;
                for (int i = 0; i < l; i++) {
                    b = file.readByte();
                    if (b != buf[readLength + i]) {
                        return false;
                    }                    
                }
                readLength += l;
            } while ( readLength < len );
        } catch (IOException e) {
            return false;
        }
        return true;
        
    }


}
