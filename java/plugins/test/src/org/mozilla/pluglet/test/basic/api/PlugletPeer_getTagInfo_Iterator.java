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

public class  PlugletPeer_getTagInfo_Iterator implements TestListIterator
{
   TestContext context = null;
   public static int flag = 0;

   /**
    *
    ***********************************************************
    *  Constructor
    ***********************************************************
    *
    */
   public PlugletPeer_getTagInfo_Iterator()
   {
     //Method has 0 parameters
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
    *  Initialize Method 
    *
    *  This method initialize parameters array and prepare for
    *  access parameters via nextParams method
    *  
    *  @param   contex  TestContext reference with properties and parameters    
    *  @return          nothing
    *
    ***********************************************************
    */
   public boolean initialize(TestContext context)
   {
     this.context = context;
     return true;
   }
   public boolean next()
   {
     if (context == null) 
       {
	 TestContext.registerFAILED("context not initialized");
	 return false;
       }
     if (flag == 0) //Only one time we should work(In future may be stress test for many calls :)
       { 
	 context.setParameters(new Vector());
	 flag = 1;
	 return true;
       } else 
	 { 
	   return false;
      }
   } 


}
