/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package com.netscape.jsdebugging.ifcui.palomar.util;

import java.util.Hashtable;

/**
* ASSERT support for Java
* <PRE>
* usage:
*     // set flag in your class
*     private static final boolean ASS = true;    // false to disable ASSERT
*
*     // use in your class
*     if(ASS)ER.T( expr, message, this);
*
*     // use in your class
*     if(ASS)ER.T( i==5, "i is screwed up in foo()", this);
* </PRE>
*
* There are various versions of the T() method.
*
* Setting  "ASS = false" in this class will globally disable
* ASSERT, but the local expressions will still be evaluated.
*
* Debuggers can be set to catch "com.netscape.jsdebugging.ifcui.palomar.util.DebuggerCaughtException"
* exceptions. The effect of a DebugBreak() is thus available.
*
* Handlers can be set on a per thread basis. If present, the handler for
* the given thread will be called on assert failure. This allows the possiblity
* of showing an assert dialog and leting the user decide how to proceed.
* e.g. "Continue", "Abort", "Debug".
*
*/

public class ER
{

    /**
    * This class is never instantiated
    */
    private ER(){}

    /**
    * call with only an expression
    */
    public static void T( boolean expr )
    {
        if( ASS )
            if( ! expr )
                assert_fail( null, null, true );
    }

    /**
    * call with an expression and an object that is either a msg or 'this'
    */
    public static void T( boolean expr, Object ob )
    {
        if( ASS )
            if( ! expr )
                if( ob instanceof String )
                    assert_fail( (String)ob, null, true );
                else
                    assert_fail( null, ob, true );
    }

    /**
    * Runs like the normal T method, except that it is intended to
    * work when not on the IFC thread, which would otherwise freeze
    * the program.  This just throws the DebuggerCaughtException.
    *
    * Should modify this to promt the user on the command line
    * but this is not implemented yet.
    */
    public static void T_NO_GUI( boolean expr, String message )
    {
        if( ASS )
            if( ! expr )
                assert_fail(message, null, false);
    }

    /**
    * call with an expression, a msg, and a classname (for use in static methods)
    */
    public static void T( boolean expr, String msg, String classname )
    {
        if( ASS )
            if( ! expr )
                try
                {
                    assert_fail( msg, Class.forName(classname), true );
                }
                catch( ClassNotFoundException e )
                {
                    assert_fail( msg, null, true );
                }
    }

    /**
    * call with an expression, a msg, and 'this' (the 'normal' case)
    */
    public static void T( boolean expr, String msg, Object ob )
    {
        if( ASS )
            if( ! expr )
                assert_fail( msg, ob, true );
    }

    /**
    * Set a handler to be used for failures on given thread
    */
    public static void setFailureHandler(Thread t, AssertFailureHandler h)
    {
        if(ASS)
        {
            if( null == _FailureHandlers )
                _FailureHandlers = new Hashtable();
            if( null != h )
                _FailureHandlers.put(t,h);
            else
                _FailureHandlers.remove(t);
        }
    }

    /**
    * Get handler for given thread
    */
    public static AssertFailureHandler getFailureHandler(Thread t)
    {
        if(ASS)
        {
            if( null != _FailureHandlers )
                return (AssertFailureHandler) _FailureHandlers.get(t);
        }
        return null;
    }

    private static void assert_fail(String msg, Object ob, boolean useHandler)
    {
        if( ASS )
        {
            String errMsg = buildString( msg, ob );
            System.out.println( "===============!!!==============" );
            System.out.println( errMsg );
            System.out.println( "===============!!!==============" );

            int choice = AssertFailureHandler.DEBUG;

            if( useHandler && null != _FailureHandlers )
            {
                AssertFailureHandler handler = (AssertFailureHandler) 
                        _FailureHandlers.get(Thread.currentThread());
                if( null != handler )
                    choice = handler.assertFailed( msg, errMsg, ob );
            }

            switch(choice)
            {
                case AssertFailureHandler.CONTINUE:
                    // do nothing...
                    break;
                case AssertFailureHandler.ABORT:
                    System.exit(0);
                    break;
                case AssertFailureHandler.DEBUG:
                default:
                    try
                    {
                        throw new DebuggerCaughtException(errMsg);
                    }
                    catch( DebuggerCaughtException e )
                    {
                        // eat exception (but catch in debugger)
                    }
            }
        }
    }

    private static String buildString( String msg, Object ob )
    {
        String str = null;
        if( ASS )
        {
            str = "!!!Assertion failed!!!";
            if( null != msg )
                str += "\n  " + msg;
            if( null != ob )
            {
                str += "\n  Classname =  " + ob.getClass();
                str += "\n  Object dump: " + ob;
            }
        }
        return str;
    }

    private static Hashtable _FailureHandlers = null;

    private static final boolean ASS = true; // false to diasable
}

/**
* Set debugger to catch this class of exceptions (use full name)
*/
class DebuggerCaughtException extends RuntimeException {
    DebuggerCaughtException(String msg) {
        super(msg);
    }
}

