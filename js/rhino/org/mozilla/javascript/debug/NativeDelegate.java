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

public final class NativeDelegate
{
    public static final int DEBUG_FLAG_INTERRUPT = 0x1;
    public static final int DEBUG_FLAG_TRAP      = 0x2;

    private NativeDelegate(){}  // prohibit instanciation

    public static void init(INativeFunScript self)
    {
        // used in ctor of overriding generated code to call hook in the
        // ctor and in the finalizer (which would not be able to get the
        // 'right' context because it is called on a system thread)
        self.set_debug_creatingContext(Context.getCurrentContext());
    }

    public static void finalize(INativeFunScript self)
    {
        Context cx = self.get_debug_creatingContext();
        if(null != cx)
        {
            DeepScriptHook hook = cx.getScriptHook();
            if(null != hook)
                hook.unloadingScript(cx, (NativeFunction)self);
        }
    }

    /*************************************************************************/

    public static int pc2lineno(INativeFunScript self, int pc)
    {
        String map = self.get_debug_linenoMap();

        if(null == map)
            return pc;

        if(pc <= 0 || pc >= map.length())
            return 0;
        else
            return (int) map.charAt(pc);
    }

    public static int lineno2pc(INativeFunScript self, int lineno)
    {
        String map = self.get_debug_linenoMap();

        if(null == map)
            return lineno;

        int count = map.length();
        for(int i = 0; i < count; i++)
            if(((int)map.charAt(i)) >= lineno)
                return i;
        return 0;
    }

    /*************************************************************************/

    public static void setTrap(INativeFunScript self, int pc)
    {
        int[] map = self.get_debug_trapMap();
        if(null == map)
            return;
        synchronized(map) 
        {
            int index = pc >> 5;
            int mask = 1 << (pc & 0x1f);

            map[index] |= mask;
            if(1 == _incrementTrapCount(self))
                _setStopFlag(self, DEBUG_FLAG_TRAP);
//            _dumpTrapBitmap(self, "setTrap", pc);
        }
    }

    public static void clearTrap(INativeFunScript self, int pc)
    {
        int[] map = self.get_debug_trapMap();
        if(null == map)
            return;
        synchronized(map)
        {
            int index = pc >> 5;
            int mask = ~(1 << (pc & 0x1f));

            map[index] &= mask;
            if(0 == _decrementTrapCount(self))
                _clearStopFlag(self, DEBUG_FLAG_TRAP);
//            _dumpTrapBitmap(self, "clearTrap", pc);
        }
    }

    public static void clearAllTraps(INativeFunScript self)
    {
        int[] map = self.get_debug_trapMap();
        if(null == map)
            return;
        synchronized(map) {
            _clearStopFlag(self, DEBUG_FLAG_TRAP);
            self.set_debug_trapCount(0);
            for(int i = 0; i < map.length; i++)
                map[i] = 0;
//            _dumpTrapBitmap(self, "clearAllTraps", 0);
        }
    }

    public static void setInterrupt(INativeFunScript self)
    {
        _setStopFlag(self, DEBUG_FLAG_INTERRUPT);
    }

    public static void clearInterrupt(INativeFunScript self)
    {
        _clearStopFlag(self, DEBUG_FLAG_INTERRUPT);
    }

    // private helpers...
    private static int _incrementTrapCount(INativeFunScript self)
    {
        int count = self.get_debug_trapCount() + 1;
        self.set_debug_trapCount(count);
        return count;
    }

    private static int _decrementTrapCount(INativeFunScript self)
    {
        int count = self.get_debug_trapCount() - 1;
        self.set_debug_trapCount(count);
        return count;
    }

    private static void _setStopFlag(INativeFunScript self, int mask)
    {
        self.set_debug_stopFlags(self.get_debug_stopFlags() | mask);
    }


    private static void _clearStopFlag(INativeFunScript self, int mask)
    {
        self.set_debug_stopFlags(self.get_debug_stopFlags() & ~mask);
    }

//    // test...
//    public static void dumpLinenoMap(INativeFunScript self, IScript script)
//    {
//        System.out.println("linenoMap for "+script);
//        if(null == debug_linenoMap)
//            System.out.println("no linenoMap");
//        else
//        {
//            for(int i = 0; i < debug_linenoMap.length(); i++)
//                System.out.println("  ["+i+"] = "+(int) debug_linenoMap.charAt(i));
//        }
//        if(null == debug_trapMap)
//            System.out.println("debug_trapMap is null");
//        else
//            System.out.println("debug_trapMap has "+debug_trapMap.length+" ints");
//    }

//    // test...
//    private static void _dumpTrapBitmap(INativeFunScript self, String action, int pc)
//    {
//        System.out.println("trapMap dump after "+action+ " at "+pc+
//                           " with trap count "+debug_trapCount+
//                           " and stop flags "+debug_stopFlags+ "...");
//        StringBuffer map = new StringBuffer();
//        for(int k = 0; k < debug_trapMap.length; k++)
//        {
//            map.setLength(0);
//            int row = debug_trapMap[k];
//            for(int i = 31; i >= 0; i--)
//                map.append(((row & (1<<i)) == 0) ? "0":"1");
//            System.out.println("trapMap["+k+"] = "+map.toString());
//        }
//    }


    /*************************************************************************/

    public static int getDebugLevel(NativeFunction fun)
    {
        return fun.debug_level;
    }
    public static String getSourceName(NativeFunction fun)
    {
        return fun.debug_srcName;
    }
    public static boolean isFunction(NativeFunction fun)
    {
        if(fun instanceof NativeScript)
            return false;
        String fname = fun.jsGet_name();
        return fname != null && fname.length() != 0;
    }
    public static String getFunctionName(NativeFunction fun)
    {
        return fun.jsGet_name();
    }

    /*************************************************************************/
    // wrapper to be called by our generated subclass

    public static Object callDebug(Context cx, Object fun, Object thisArg,
                                   Object[] args)
        throws JavaScriptException
    {
        Object o = null;
        DeepCallHook hook = cx.getCallHook();
        if (null != hook)
        {
            Object hookData = hook.preCall(cx, fun, thisArg, args);
            try {
                o = ScriptRuntime.call(cx, fun, thisArg, args);
            }
            catch(JavaScriptException e) {
                hook.postCall(cx, hookData, null, e);
                throw(e);
            }
            hook.postCall(cx, hookData, o, null);
        }
        else
            o = ScriptRuntime.call(cx, fun, thisArg, args);
        return o;
    }

    public static Object callSpecialDebug(Context cx, Object fun, 
                                          Object thisArg, Object[] args,
                                          Scriptable enclosingThisArg,
                                          Scriptable scope,
                                          String filename, int lineNumber)
        throws JavaScriptException
    {
        Object o = null;
        DeepCallHook hook = cx.getCallHook();
        if (null != hook)
        {
            Object hookData = hook.preCall(cx, fun, thisArg, args);
            try {
                o = ScriptRuntime.callSpecial(cx, fun, thisArg, args,
                                              enclosingThisArg, scope,
                                              filename, lineNumber);
            }
            catch(JavaScriptException e) {
                hook.postCall(cx, hookData, null, e);
                throw(e);
            }
            hook.postCall(cx, hookData, o, null);
        }
        else
            o = ScriptRuntime.callSpecial(cx, fun, thisArg, args,
                                          enclosingThisArg, scope,   
                                          filename, lineNumber);
        return o;
    }

    public static Object executeDebug(Context cx, NativeFunction fun, 
                                      Scriptable scope)
        throws JavaScriptException
    {
        Object o = null;
        DeepExecuteHook hook = cx.getExecuteHook();
        if (null != hook)
        {
            Object hookData = hook.preExecute(cx, fun, scope);
            try {
                o = fun.call(cx, scope, scope, null);
            }
            catch(JavaScriptException e) {
                hook.postExecute(cx, hookData, null, e);
                throw(e);
            }
            hook.postExecute(cx, hookData, o, null);
        }
        else
            o = fun.call(cx, scope, scope, null);
        return o;
    }

    public static Scriptable newObjectDebug(Context cx, Object fun, Object[] args)
        throws JavaScriptException
    {
        Scriptable o = null;
        DeepNewObjectHook hook = cx.getNewObjectHook();
        if (null != hook)
        {
            Object hookData = hook.preNewObject(cx, fun, args);
            try {
                o = ScriptRuntime.newObject(cx, fun, args, null);
            }
            catch(JavaScriptException e) {
                hook.postNewObject(cx, hookData, null, e);
                throw(e);
            }
            hook.postNewObject(cx, hookData, o, null);
        }
        else
            o = ScriptRuntime.newObject(cx, fun, args, null);
        return o;
    }

    public static Scriptable newObjectSpecialDebug(Context cx, Object fun, 
                                                   Object[] args, 
                                                   Scriptable scope)
        throws JavaScriptException
    {
        Scriptable o = null;
        DeepNewObjectHook hook = cx.getNewObjectHook();
        if (null != hook)
        {
            Object hookData = hook.preNewObject(cx, fun, args);
            try {
                o = ScriptRuntime.newObjectSpecial(cx, fun, args, scope);
            }
            catch(JavaScriptException e) {
                hook.postNewObject(cx, hookData, null, e);
                throw(e);
            }
            hook.postNewObject(cx, hookData, o, null);
        }
        else
            o = ScriptRuntime.newObjectSpecial(cx, fun, args, scope);
        return o;
    }


    /*************************************************************************/
    // helpers to be called by our generated subclass

    public static int callInterruptHook(INativeFunScript self,
                                        Context cx, Object[] retVal, 
                                        int pc, Scriptable varObj)
    {
        DeepBytecodeHook hook = cx.getBytecodeHook();
        if(null == hook)
            return DeepBytecodeHook.CONTINUE;
        
        // set current pc into the frame
        setPC(cx, varObj, pc);

        // call the hook
        return hook.interrupted(cx, pc, retVal);
    }

    public static int callTrappedHook(INativeFunScript self,
                                      Context cx, Object[] retVal, 
                                      int pc, Scriptable varObj)
    {
        DeepBytecodeHook hook = cx.getBytecodeHook();
        if(null == hook)
            return DeepBytecodeHook.CONTINUE;
        
        // set current pc into the frame
        setPC(cx, varObj, pc);

        // call the hook
        return hook.trapped(cx, pc, retVal);
    }

    public static void callLoadingScriptHook(INativeFunScript self)
    {
        Context cx = self.get_debug_creatingContext();
        if(null != cx)
        {
            DeepScriptHook hook = cx.getScriptHook();
            if(null != hook)
                hook.loadingScript(cx, (NativeFunction)self);
        }
    }
    
    public static void setPC(Context cx, Scriptable varObj, int debugPC) {
        NativeCall frame = getFrame(cx, varObj);
        if (frame != null)
            frame.debugPC = debugPC;
    }
    
    private static NativeCall getFrame(Context cx, Scriptable varObj) {
        while (varObj != null) {
            if (varObj instanceof NativeCall) 
                return (NativeCall) varObj;
            varObj = varObj.getParentScope();
        }
        NativeCall activation = ScriptRuntime.getCurrentActivation(cx);
        return activation;
    }
}    
