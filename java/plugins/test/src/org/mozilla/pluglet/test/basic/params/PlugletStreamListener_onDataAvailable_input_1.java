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
 *  The data is read and compared with data from original   
 *  file.    
 *  
 *
 */

public class PlugletStreamListener_onDataAvailable_input_1 implements Test {
    private int readLength = 0;

    public void execute(TestContext context){
        InputStream is = context.getInputStream();
        byte[] buf = new byte[1024];
        try {
            RandomAccessFile testFile = new RandomAccessFile(context.getTestDir()+"/test.tdt", "r");            
            int l = is.read(buf, 0, 1024);
            while ( l != -1 ) {
                if (l <= 0) {
                    context.registerFAILED("Read length isn't great than zero ");
                    return;                    
                }
                testFile.seek(readLength);
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
            if (readLength == testFile.length()) {
                context.registerPASSED("");
            } 

        } catch (IOException e) {
            context.registerFAILED("Exception occured "+e.toString());
        }
    }
}
