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

public class JSContext
{
    private JSContext() {
        // empty
    }
    public static JSContext newContext(JSRuntime runtime) {
        JSContext context = new JSContext();
        context._runtime = runtime;
        if(! context._init())
            return null;
        return context;
    }

    public boolean isValid() {
        return _nativeContext != 0 && _runtime != null;
    }

    // finalization not guaranteed to happen in right order - trust programmer
    public void destroy() {
        _exit();
        _nativeContext = 0;
        _runtime = null;
    }

    public void eval(String s, String filename, int lineno) {
        if(isValid())
            _eval(s, filename, lineno);
    }

    public void load(String filename) {
        if(isValid())
            _load(filename);
    }

    public IErrorSink getErrorSink() {return _errorSink;}
    public IErrorSink setErrorSink(IErrorSink sink) {
        IErrorSink result = _errorSink;
        _errorSink = sink;
        return result;
    }

    public IPrintSink getPrintSink() {return _printSink;}
    public IPrintSink setPrintSink(IPrintSink sink) {
        IPrintSink result = _printSink;
        _printSink = sink;
        return result;
    }

    /*******************************************************************/

    private void _print(String s) {
        if(null != _printSink)
            _printSink.print(s);
    }

    private void _reportError(String msg, String filename, int lineno,
                              String lineBuf, int offset)
    {
        if(null != _errorSink)
            _errorSink.error(msg, filename, lineno, lineBuf, offset);
    }

    private synchronized native boolean _init();
    private synchronized native void _exit();
    private synchronized native void _eval(String s, String filename, int lineno);
    private synchronized native void _load(String filename);

    private long        _nativeContext;
    private JSRuntime   _runtime;
    private IErrorSink  _errorSink;
    private IPrintSink  _printSink;
}    
