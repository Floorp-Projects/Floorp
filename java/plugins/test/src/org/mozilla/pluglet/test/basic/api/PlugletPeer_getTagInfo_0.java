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


package org.mozilla.pluglet.test.basic.api;

import java.util.*;
import java.io.*;
import java.net.*;
import org.mozilla.pluglet.test.basic.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.*;

public class PlugletPeer_getTagInfo_0  implements Test
{
   public static boolean actionPerformed = false;
   private static String TEST_ATTRIBUTE_NAME = "type";
   private static PlugletTagInfo info = null;
   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletPeer_getTagInfo_0()
   {
   }


   /**
    *
    ***********************************************************
    *  Starting point of application
    *
    *  @param   args    Array of command line arguments
    *
    ***********************************************************
    */
   public static void main(String[] args)
   {
   }

   /**
    ***********************************************************
    *
    *  Execute Method 
    *
    *  @param   contex  TestContext reference with properties and parameters    
    *  @return          void, the result set to true or false via context
    *
    ***********************************************************
    */
   public void execute(TestContext context)
   {	
     PlugletPeer PlugletPeer_obj = null;
	
     if( context == null ) {
       TestContext.registerFAILED("ERROR:null context passed in execute(TestContext)" +
				  " of PlugletPeer_getTagInfo_0");
       return;
     }
     PlugletPeer_obj = context.getPlugletPeer();
     info =	PlugletPeer_obj.getTagInfo();
     actionPerformed = true; 
     System.err.println("DEBUG: PlugletPeer_getTagInfo returns \"" + info + "\"value");
   }
   public static void verifyAttr(String allAttrs)
   {
     if (info == null) {
       TestContext.registerFAILED("PlugletPeer_getTagInfo returns null value");
       return;
     }
     Properties attr = new Properties();
     try {
       attr.load((InputStream)new ByteArrayInputStream(allAttrs.getBytes()));
     } catch (Exception e) {
       TestContext.registerFAILED("Exception " + e + " ocurred");
       return;
     }
     String attrValue = info.getAttribute(TEST_ATTRIBUTE_NAME);
     if (attrValue == null) {
       TestContext.registerFAILED("null value returned for attribute \"" + TEST_ATTRIBUTE_NAME + 
				  "\" from PlugletTagInfo object");
       return;
     }
     String goodAttrValue = attr.getProperty(TEST_ATTRIBUTE_NAME,null);
     if (goodAttrValue == null) {
       TestContext.registerFAILED("Attribute with name \"" + TEST_ATTRIBUTE_NAME + "\" isn't passed from JS");
       return;
     }
     if (goodAttrValue.equals(attrValue)) {
       TestContext.registerPASSED("Good TagInfo object returned");
       return;
     } else {
       TestContext.registerFAILED("Strange TagInfo object returned, " + TEST_ATTRIBUTE_NAME + "=\"" +
				  attrValue + "\" instead of \"" + goodAttrValue + "\"");
       return;
     }
   }
   /**
    *
    ***********************************************************
    *  Routine where OS specific checks are made. 
    *
    *  @param   os      OS Name (SunOS/Linus/MacOS/...)
    ***********************************************************
    *
    */
   private void osRoutine(String os)
   {
     if(os == null) return;

     os = os.trim();
     if(os.compareTo("SunOS") == 0) {}
     else if(os.compareTo("Linux") == 0) {}
     else if(os.compareTo("Windows") == 0) {}
     else if(os.compareTo("MacOS") == 0) {}
     else {}
   }
}
