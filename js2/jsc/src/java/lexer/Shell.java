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
 * class Shell
 */

public final class Shell implements Tokens {
  private static boolean debug = false;
  static int failureCount = 0;

  public static void main( String[] args ) {

    try {

    int count = args.length;

      if( args[0].equals("-l") ) {
        /* run the input text through the scanner */
        
        try {
          FileInputStream srcfile = new FileInputStream( args[count-1] );
          InputStreamReader reader = new InputStreamReader( srcfile );
          FileOutputStream tokfile = new FileOutputStream( args[count-1] + ".tok" );
          PrintWriter tokwriter = new PrintWriter( tokfile);
          FileOutputStream errfile = new FileOutputStream( args[count-1] + ".err" );
          PrintStream errwriter = new PrintStream( errfile );
          Scanner scanner = new Scanner( reader, errwriter, args[count-1] );
          int result;
          String follows;
          for( int i = 0; (result  = scanner.nexttoken()) != eos_token; i++ ) { 
                  follows = scanner.followsLineTerminator() ? "*" : "";
                  tokwriter.print( scanner.getPersistentTokenText(result) + follows + " " );
          }
          tokwriter.close();

/*
          for( int i = 0; (result  = scanner.nexttoken()) != eos_token; i++ ) { 
                  System.out.println( scanner.lexeme() + "\t\t\t" 
                           + Token.getTokenClassName(scanner.getTokenClass(result)) 
                           + " (" + result + ")" 
                           + ": " + scanner.lexeme() );
          }
*/            
        }
        catch( Exception e ) {
          e.printStackTrace();
        }
      }
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
