/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          script-001.js
   Section:
   Description:        new NativeScript object


   js> parseInt(123,"hi")
   123
   js> parseInt(123, "blah")
   123
   js> s
   js: s is not defined
   js> s = new Script

   undefined;


   js> s = new Script()

   undefined;


   js> s.getJSClass
   js> s.getJSClass = Object.prototype.toString
   function toString() {
   [native code]
   }

   js> s.getJSClass()
   [object Script]
   js> s.compile( "return 3+4" )
   js: JavaScript exception: javax.javascript.EvaluatorException: "<Scr
   js> s.compile( "3+4" )

   3 + 4;


   js> typeof s
   function
   js> s()
   Jit failure!
   invalid opcode: 1
   Jit Pass1 Failure!
   javax/javascript/gen/c13 initScript (Ljavax/javascript/Scriptable;)V
   An internal JIT error has occurred.  Please report this with .class
   jit-bugs@itools.symantec.com

   7
   js> s.compile("3+4")

   3 + 4;


   js> s()
   Jit failure!
   invalid opcode: 1
   Jit Pass1 Failure!
   javax/javascript/gen/c17 initScript (Ljavax/javascript/Scriptable;)V
   An internal JIT error has occurred.  Please report this with .class
   jit-bugs@itools.symantec.com

   7
   js> quit()

   C:\src\ns_priv\js\tests\ecma>shell

   C:\src\ns_priv\js\tests\ecma>java -classpath c:\cafe\java\JavaScope;
   :\src\ns_priv\js\tests javax.javascript.examples.Shell
   Symantec Java! JustInTime Compiler Version 210.054 for JDK 1.1.2
   Copyright (C) 1996-97 Symantec Corporation

   js> s = new Script("3+4")

   3 + 4;


   js> s()
   7
   js> s2 = new Script();

   undefined;


   js> s.compile( "3+4")

   3 + 4;


   js> s()
   Jit failure!
   invalid opcode: 1
   Jit Pass1 Failure!
   javax/javascript/gen/c7 initScript (Ljavax/javascript/Scriptable;)V
   An internal JIT error has occurred.  Please report this with .class
   jit-bugs@itools.symantec.com

   7
   js> quit()
   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "script-001";
var TITLE   = "NativeScript";

writeHeaderToLog( SECTION + " "+ TITLE);

if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
  new TestCase( "var s = new Script(); typeof s",
                "Script not supported, test skipped.",
                "Script not supported, test skipped." );
}
else
{
  var s = new Script();
  s.getJSClass = Object.prototype.toString;

  new TestCase( "var s = new Script(); typeof s",
                "function",
                typeof s );

  new TestCase( "s.getJSClass()",
                "[object Script]",
                s.getJSClass() );
}

test();
