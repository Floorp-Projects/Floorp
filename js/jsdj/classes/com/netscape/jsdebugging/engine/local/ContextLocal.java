/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package com.netscape.jsdebugging.engine.local;

import java.io.*;
import java.util.*;
import com.netscape.jsdebugging.engine.*;


public class ContextLocal 
    implements IContext,
               com.netscape.nativejsengine.IPrintSink, 
               com.netscape.nativejsengine.IErrorSink
{
    private ContextLocal(){}

    ContextLocal(RuntimeLocal runtime, String[] args) {
        _runtime = runtime;

        _context = com.netscape.nativejsengine.JSContext.newContext(
                                                _runtime.getNativeRuntime());
        if(null == _context) {
            _destroyed = true;
            return;
        }

        _context.setPrintSink(this);
        _context.setErrorSink(this);
    }

    public boolean isValid() {return ! _destroyed;}

    // finalization not guaranteed to happen in right order - trust programmer
    public void destroy() {
        _destroyed = true;
        _context.destroy();
    }

    public String evalString(String s, String filename, int lineno) {
        _context.eval(s, filename, lineno);
        return null;
    }

    public String loadFile(String filename) {
        _context.load(filename);
        return null;
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

    // implement com.netscape.nativejsengine.IPrintSink
    public void print(String s) {
        if(null != _printSink)
            _printSink.print(s);
    }

    // implement com.netscape.nativejsengine.IErrorSink
    public void error(String msg, String filename, int lineno,
                      String lineBuf, int offset)
    {
        if(null != _errorSink)
            _errorSink.error(msg, filename, lineno, lineBuf, offset);
    }

    public IRuntime getRuntime() {return _runtime;}

    private boolean _destroyed = false;
    private RuntimeLocal _runtime;
    private com.netscape.nativejsengine.JSContext _context;
    private IErrorSink  _errorSink;
    private IPrintSink  _printSink;
}    
