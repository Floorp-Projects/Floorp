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

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;

//
// NativeFunctionDebug and NativeScriptDebug should be identical except:
//    1) the classname
//    2) the superclassname
//    3) the ctor name
//

public class NativeScriptDebug extends NativeScript
    implements INativeFunScript
{
    public NativeScriptDebug()
    {
        NativeDelegate.init(this);
    }

    public void finalize()
        throws Throwable
    {
        NativeDelegate.finalize(this);
        super.finalize();
    }

    public int pc2lineno(int pc)
    {
        return NativeDelegate.pc2lineno(this, pc);
    }

    public int lineno2pc(int lineno)
    {
        return NativeDelegate.lineno2pc(this, lineno);
    }

    public void setTrap(int pc)
    {
        NativeDelegate.setTrap(this, pc);
    }

    public void clearTrap(int pc) 
    {
        NativeDelegate.clearTrap(this, pc);
    }

    public void clearAllTraps()
    {
        NativeDelegate.clearAllTraps(this);
    }

    public void setInterrupt()
    {
        NativeDelegate.setInterrupt(this);
    }
    public void clearInterrupt()
    {
        NativeDelegate.clearInterrupt(this);
    }

    /*********************************************************/

    public static int getDebugLevel(NativeFunction fun)
    {
        return NativeDelegate.getDebugLevel(fun);
    }
    public static String getSourceName(NativeFunction fun)
    {
        return NativeDelegate.getSourceName(fun);
    }
    public static boolean isFunction(NativeFunction fun)
    {
        return NativeDelegate.isFunction(fun);
    }
    public static String getFunctionName(NativeFunction fun)
    {
        return NativeDelegate.getFunctionName(fun);
    }

    /*********************************************************/

    public int getDebugLevel()
    {
        return NativeDelegate.getDebugLevel(this);
    }
    public String getSourceName()
    {
        return NativeDelegate.getSourceName(this);
    }
    public boolean isFunction()
    {
        return NativeDelegate.isFunction(this);
    }
    public String getFunctionName()
    {
        return NativeDelegate.getFunctionName(this);
    }

    /*********************************************************/
    // helpers to be called by our generated subclass

    protected int callInterruptHook(Context cx, Object[] retVal, 
                                    int pc, Scriptable varObj)
    {
        return NativeDelegate.callInterruptHook(this, cx, retVal, pc, varObj);
    }

    protected int callTrappedHook(Context cx, Object[] retVal, 
                                  int pc, Scriptable varObj)
    {
        return NativeDelegate.callTrappedHook(this, cx, retVal, pc, varObj);
    }

    protected void callLoadingScriptHook()
    {
        NativeDelegate.callLoadingScriptHook(this);
    }

    /*************************************************************************/
    // implements INativeFunScript
    // these must all be identical in NativeFunctionDebug and NativeScriptDebug
    public final Context get_debug_creatingContext() {return debug_creatingContext;}
    public final int     get_debug_stopFlags()       {return debug_stopFlags;}
    public final int[]   get_debug_trapMap()         {return debug_trapMap;}
    public final String  get_debug_linenoMap()       {return debug_linenoMap;}
    public final int     get_debug_trapCount()       {return debug_trapCount;}
    public final int     get_debug_baseLineno()      {return debug_baseLineno;}
    public final int     get_debug_endLineno()       {return debug_endLineno;}
    public final IScript get_debug_script()          {return debug_script;}

    public final void    set_debug_creatingContext(Context cx){debug_creatingContext = cx;}
    public final void    set_debug_stopFlags(int flags)   {debug_stopFlags = flags; }
    public final void    set_debug_trapCount(int count)   {debug_trapCount = count; }
    public final void    set_debug_script(IScript script) {debug_script    = script;}

    // these must all be identical in NativeFunctionDebug and NativeScriptDebug
    protected Context debug_creatingContext;
    protected int     debug_stopFlags;
    protected int[]   debug_trapMap;
    protected String  debug_linenoMap;
    protected int     debug_trapCount;
    protected int     debug_baseLineno;
    protected int     debug_endLineno;
    protected IScript debug_script;
    /*************************************************************************/
}    
