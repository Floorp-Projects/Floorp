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

import org.mozilla.util.*;

/*  
 *  Description:
 *  Test for passed in onDataAvailable method 2d parameter (input)   
 *  The test reads at first the amount of data less than just
 *  pushed length. On the next onDataAvailable calls it is  
 *  supposed to read all other data. Currently  
 *  the incorectnesses of such behaviour isn't specified.
 *
 */

public class PlugletStreamListener_onDataAvailable_input_2 implements Test {
    private int readLength = 0;
    private int state = 0;
    private long fileLength = 0;

    public void execute(TestContext context){
        if (context.getTestStage()
                .equals(TestStage.getStageById(TestStage.PLUGLET_STREAM_LISTENER_ON_DATA_AVAILABLE))) {
            if (state == 2) {
                context.registerFAILED("onStopBinding before onDataAvailble");
                return;
            } else {
                InputStream is = context.getInputStream();
                int length = context.getLength();
                if (length < 0) {
                    context.registerFAILED("The pushed data is less than 0");
                    return;
                } else if (length == 0) {
                    return;                    
                }  
                byte[] buf = new byte[1024];
                int l;
                if (state == 0) {
                    // first time the amount of read data is less than 
                    // pointed by 3d parameter
                    state = 1;                    
                    int lengthToRead = Math.min(1024, length-1);
                    try {
                        l = is.read(buf, 0, lengthToRead);
                    } catch (IOException e) {
                        context.registerFAILED("The pushed data can't be read due to IOException");
                        return;                                                
                    }
                    if ( l != -1 ) {
                        if (l > 0) {
                            readLength += l;
                        } else if ( (l != 0) || (length != 0) ) {
                            context.registerFAILED("The read length is incorrect");
                            return;
                        }
                    } else {
                        context.registerFAILED("The pushed data can't be read");
                        return;                        
                    }
                } else {
                    // if not first time then read all what can be read, and compare.
                    try {
                        RandomAccessFile testFile = new RandomAccessFile(context.getTestDir()+"/test.tdt", "r");
                        fileLength = testFile.length();
                        testFile.seek(readLength);
                        l = is.read(buf, 0, 1024);
                        while (l != -1) {
                            if ( l <= 0) {
                                context.registerFAILED("The read length is less than 1");
                                return;
                            }
                            byte b;
                            for (int i = 0; i < l; i++) {
                                b = testFile.readByte();
                                if (b != buf[i]) {
                                    context.registerFAILED("The bytes aren't equal: readLength "+readLength+" i "+i+" b "+b+" and b[i] "+buf[i] );
                                    return;
                                }                    
                            }                            
                            readLength += l;
                            l = is.read(buf, 0, 1024);
                        }
                    } catch (IOException e) {
                        context.registerFAILED("Exception occured "+e.toString());
                    }
                }
            }
        } else {
            if (state != 1) {
                context.registerFAILED("onStopBinding w/o onDataAvailble");
            }
            state = 2;
            if (readLength == fileLength) {
                context.registerPASSED("");
            } else {
                context.registerFAILED("onStopBinding was called, but all data wasn't read"); 
            }
        }
        
    }
}
