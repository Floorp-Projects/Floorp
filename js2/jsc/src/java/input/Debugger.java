/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

package com.compilercompany.ecmascript;
import java.io.*;

/*
 * Debugging tool.
 */

public final class Debugger {

  public static PrintStream dbg = null;
  private static boolean useSystemOut = false;

  public static void setOutFile(String filename) {
      try {
      PrintStream out = new PrintStream( new FileOutputStream( filename ) );
      System.setOut( out );
	  //System.setErr( outfile );
      } catch ( Exception e ) {
          e.printStackTrace();
      }
  }

  public static void setErrFile(String filename) {
      try {
      PrintStream err = new PrintStream( new FileOutputStream( filename ) );
      //System.setOut( out );
	  System.setErr( err );
      } catch ( Exception e ) {
          e.printStackTrace();
      }
  }

  public static void setDbgFile(String filename) {
      try {
      if( dbg!=null) {
          dbg.close();
      }
      dbg = new PrintStream( new FileOutputStream( filename ) );
      //System.setOut( outfile );
	  //System.setErr( outfile );
      } catch ( Exception e ) {
          e.printStackTrace();
      }
  }

  public static void trace( String str ) {
    try {
      if( useSystemOut )
        System.out.println( str );
      else {
        if( dbg == null ) {
          dbg = new PrintStream( new FileOutputStream( "debug.out" ) );
		  //System.setOut( outfile );
		  //System.setErr( outfile );
		}
        dbg.println( str );
      }
    }
    catch( SecurityException e ) {
      useSystemOut = true;
      System.out.println( str );
    }
    catch( Exception e ) {
      e.printStackTrace();
    }
  }
}

/*
 * The end.
 */
