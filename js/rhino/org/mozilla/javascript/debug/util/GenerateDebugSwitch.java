/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
* This class is meant to be run from the command line to create
* a customized Debug state file. The generated file can then be 
* used by other classes to turn debug code on or off (at compile time)
*/

public class GenerateDebugSwitch
{
    public static void main(String[] args)
    {
        if(args.length == 1 && 
           ("true".equals(args[0]) || "false".equals(args[0])))
        {
            p("");
            p("// Automatically generated file");
            p("");
            p("package org.mozilla.javascript.debug.util;");
            p("");
            p("public interface AS");
            p("{");
            p("    public static final boolean S     = "+args[0]+";");
            p("    public static final boolean DEBUG = "+args[0]+";");
            p("}");
        }
        else
        {
            p("Generates java source for debug on or off switch");
            p("usage:");
            p("   java GenerateDebugSwitchSourceFile {true|false} > AS.java");
        }
    }
    private static void p(String s) {System.out.println(s);}
}
