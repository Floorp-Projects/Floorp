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
import java.lang.reflect.*;
import sun.tools.javac.*;
import java.io.*;

/**
 * A test shell.
 */

public final class Shell {
  private static boolean debug = true;
  static int failureCount = 0;

  public static void main( String[] args ) {

    try {

    int count = args.length;
      String encoding = "UTF-8"; // Alternatives include: "UTF-16", "US-ASCII" (7-bit)

      if( args[0].equals("-l") ) {
        /* run the input text through the scanner */
        
        try {
          Debugger.trace("start "+args[count-1]);
          FileInputStream in = new FileInputStream( args[count-1] );
          InputStreamReader reader = new InputStreamReader( in, "UTF-8" );
          FileOutputStream tokfile = new FileOutputStream( args[count-1] + ".inp" );
          PrintWriter tokwriter = new PrintWriter( tokfile);
          FileOutputStream errfile = new FileOutputStream( args[count-1] + ".err" );
          PrintStream errwriter = new PrintStream( errfile );
          //char[] temp = new char[0xffff];
          //int len = reader.read( temp, 0, temp.length );
          //char[] src = new char[len+1];
          //System.arraycopy(temp, 0, src, 0, len);
          
          InputBuffer inp = new InputBuffer( reader, errwriter, args[count-1] );
          int result;
          String follows;
          for( int i = 0; (result  = inp.nextchar()) != 0; i++ ) { 
                  tokwriter.println( "" + Integer.toHexString(result) + " " + (char)result );
          }
          tokwriter.close();
          Debugger.trace("finish "+args[count-1]);
        }
        catch( Exception e ) {
          e.printStackTrace();
        }
      }
/*
      if( args[0].equals("-g") ) {
        
        try {
          FileInputStream srcfile = new FileInputStream( args[count-1] );
          InputStreamReader reader = new InputStreamReader( srcfile );
          FileOutputStream errfile = new FileOutputStream( args[count-1] + ".err" );
          PrintStream errwriter = new PrintStream( errfile );
          char[] temp = new char[0x7ffff];
          int len = reader.read( temp, 0, temp.length );
          char[] src = new char[len+1];
          System.arraycopy(temp, 0, src, 0, len);
          
          UnicodeNormalizationFormCReader ucreader = new UnicodeNormalizationFormCReader( src, errwriter, args[count-1] );
          UnicodeNormalizationFormCReader.generateCharacterClassesArray(ucreader);
        }
        catch( Exception e ) {
          e.printStackTrace();
        }
      }
*/
    }

    catch( Exception e ) {
      e.printStackTrace();
    }
  }


  /**
   *
  **/

}

/*
 * The end.
 */
