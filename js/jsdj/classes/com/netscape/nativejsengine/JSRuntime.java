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

package com.netscape.nativejsengine;

public class JSRuntime
{
    public static final String NATIVE_LIBRARY_NAME     = "nativejsengine";
    public static final String NATIVE_JSD_LIBRARY_NAME = "jsd";

    public static final boolean ENABLE_DEBUGGING = true;
    public static final boolean NO_DEBUGGING     = false;

    private JSRuntime() {
        // empty
    }
    public static JSRuntime newRuntime(boolean enableDebugging) {
        JSRuntime runtime = new JSRuntime();
        if(enableDebugging && ! _jsdNativesLoaded) {
            System.loadLibrary(NATIVE_JSD_LIBRARY_NAME);
            _jsdNativesLoaded = true;
        }
        if(! runtime._init(enableDebugging))
            return null;
        return runtime;
    }

    static {
        System.loadLibrary(NATIVE_LIBRARY_NAME);
    }

    public boolean isValid() {
        return _nativeRuntime != 0;
    }

    // finalization not guaranteed to happen in right order - trust programmer
    public void destroy() {
        _exit();
        _nativeRuntime = 0;
    }

    long    getNativeRuntime() {return _nativeRuntime;}
    long    getNativeDebugSupport() {return _nativeDebugSupport;}

    private native boolean _init(boolean enableDebugging);
    private native void _exit();

    private long    _nativeRuntime;
    private long    _nativeDebugSupport;

    private static boolean _jsdNativesLoaded = false;
}    