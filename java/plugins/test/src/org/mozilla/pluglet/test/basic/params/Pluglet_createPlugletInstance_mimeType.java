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
 *  The test class compares the passed in mime type with 
 *  gold value  pointed in test properties. 
 *  
 *  
 *
 */



public class Pluglet_createPlugletInstance_mimeType implements Test {
    public static String TEST_MIME_TYPE_PROP_NAME = "TEST_MIME_TYPE";


    public void execute(TestContext context){
        String passedMimeType = context.getMimeType();
        String origMimeType = context.getProperty(TEST_MIME_TYPE_PROP_NAME);
        if (origMimeType != null) {
            if (origMimeType.equals(passedMimeType)) {
                context.registerPASSED("");
            } else {
                context.registerFAILED("MimeTypes aren't equal: received  "+passedMimeType+" and required "+origMimeType);
            }
	    } else {
            context.registerFAILED(TEST_MIME_TYPE_PROP_NAME+" isn't correctly set"); 
        }
    }
}
