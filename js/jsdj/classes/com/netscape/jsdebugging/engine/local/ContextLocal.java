/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
