/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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
