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

package org.mozilla.javascript.debug.util;

import java.util.Hashtable;

/**
* ASSERT support for Java
* <PRE>
* usage:
*     // use in your class
*     if(AS.S)ER.T(expr, message, this);
*
*     // use in your class
*     if(AS.S)ER.T(i==5, "i is screwed up in foo()", this);
* </PRE>
*
* There are various versions of the T() method.
*
* Debuggers can be set to catch "org.mozilla.javascript.debug.util.DebuggerCaughtException"
* exceptions. The effect of a DebugBreak() is thus available.
*
* Handlers can be set on a per thread basis. If present, the handler for
* the given thread will be called on assert failure. This allows the possiblity
* of showing an assert dialog and leting the user decide how to proceed.
* e.g. "Continue", "Abort", "Debug".
*
*/

public final class ER
{
    /**
    * This class is never instantiated
    */
    private ER(){}

    /**
    * call with only an expression
    */
    public static void T(boolean expr)
    {
        if(AS.S)
            if(! expr)
                assert_fail(null, null, true);
    }

    /**
    * call with an expression and an object that is either a msg or 'this'
    */
    public static void T(boolean expr, Object ob)
    {
        if(AS.S)
            if(! expr)
                if(ob instanceof String)
                    assert_fail((String)ob, null, true);
                else
                    assert_fail(null, ob, true);
    }

    /**
    * call with an expression, a msg, and a classname (for use in static methods)
    */
    public static void T(boolean expr, String msg, String classname)
    {
        if(AS.S)
            if(! expr)
                try
                {
                    assert_fail(msg, Class.forName(classname), true);
                }
                catch(ClassNotFoundException e)
                {
                    assert_fail(msg, null, true);
                }
    }

    /**
    * call with an expression, a msg, and 'this' (the 'normal' case)
    */
    public static void T(boolean expr, String msg, Object ob)
    {
        if(AS.S)
            if(! expr)
                assert_fail(msg, ob, true);
    }

    /**
    * Set a handler to be used for failures on given thread
    */
    public static void setFailureHandler(Thread t, AssertFailureHandler h)
    {
        if(AS.S)
        {
            if(null == _FailureHandlers)
                _FailureHandlers = new Hashtable();
            if(null != h)
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
        if(AS.S)
        {
            if(null != _FailureHandlers)
                return (AssertFailureHandler) _FailureHandlers.get(t);
        }
        return null;
    }

    public static boolean getDumpStackOnFailure()
    {
        return _dumpStackOnFailure;
    }
    public static void    setDumpStackOnFailure(boolean dump)
    {
        _dumpStackOnFailure = dump;
    }

    public static void dumpThreadsAndStack()
    {
        Thread t = Thread.currentThread();
        System.out.println("----------------------------------------------");
        t.getThreadGroup().list();
        System.out.println("----------------------------------------------");
        System.out.println(t);
        System.out.println("----------------------------------------------");
        t.dumpStack();
    }

    private static void assert_fail(String msg, Object ob, boolean useHandler)
    {
        if(AS.S)
        {
            String errMsg = buildString(msg, ob);
            System.out.println("==============================!!!=============================");
            System.out.println(errMsg);
            if(_dumpStackOnFailure)
                dumpThreadsAndStack();
            System.out.println("==============================!!!=============================");

            int choice = AssertFailureHandler.DEBUG;

            if(useHandler && null != _FailureHandlers)
            {
                AssertFailureHandler handler = (AssertFailureHandler) 
                        _FailureHandlers.get(Thread.currentThread());
                if(null != handler)
                    choice = handler.assertFailed(msg, errMsg, ob);
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
                    catch(DebuggerCaughtException e)
                    {
                        // eat exception (but catch in debugger)
                    }
            }
        }
    }

    private static String buildString(String msg, Object ob)
    {
        String str = null;
        if(AS.S)
        {
            str = "!!!Assertion failed!!!";
            if(null != msg)
                str += "\n  " + msg;
            if(null != ob)
            {
                str += "\n  Classname =  " + ob.getClass();
                str += "\n  Object dump: " + ob;
            }
        }
        return str;
    }

    private static Hashtable _FailureHandlers = null;
    private static boolean _dumpStackOnFailure = true;
}

/**
* Set debugger to catch this class of exceptions (use full name)
*/
class DebuggerCaughtException extends RuntimeException {
    DebuggerCaughtException(String msg) {
        super(msg);
    }
}
