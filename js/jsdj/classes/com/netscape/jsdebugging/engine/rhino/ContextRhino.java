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

package com.netscape.jsdebugging.engine.rhino;

import java.io.*;
import java.util.*;
import com.netscape.jsdebugging.engine.*;
import com.netscape.javascript.*;
import com.netscape.javascript.debug.*;


public class ContextRhino 
    extends ScriptableObject 
    implements IContext, com.netscape.javascript.ErrorReporter
{
    private ContextRhino(){}

    ContextRhino(RuntimeRhino runtime, String[] args) {
        _runtime = runtime;
        _cx = new Context();

        try {
            _cx.enter();
        } catch (InterruptedException e) {
            throw new Error(e.getMessage());
        } catch (ThreadAssociationException e) {
            throw new Error(e.getMessage());
        }

        // Initialize the standard objects (Object, Function, etc.)
        // This must be done before scripts can be executed.
        _cx.initStandardObjects(this);

        // Define some global functions particular to the shell. Note
        // that these functions are not part of ECMA.
        String[] names = {"print", "version", "load"};

        try {
            this.defineFunctionProperties(names, ContextRhino.class,
                                          ScriptableObject.DONTENUM);
        } catch (PropertyDefinitionException e) {
            throw new Error(e.getMessage());
        }

        _cx.setErrorReporter(this);

        DebugManager dbm = _runtime.getDebugManager();

        if(null != dbm) {
            _cx.setDebugLevel(6);
            _cx.setSourceTextManager(_runtime.getSourceTextManager());
            dbm.createdContext(_cx);
        }


        // XXX more stuff...
    }

    public boolean isValid() {return ! _destroyed;}

    // finalization not guaranteed to happen in right order - trust programmer
    public void destroy() {
        _destroyed = true;
        try {
            _cx.exit();
        } catch (ThreadAssociationException e) {
            throw new Error(e.getMessage());
        }
        // XXX more stuff...
    }

    public String evalString(String s, String filename, int lineno) {
        
        try {
            return Context.toString(
                    _cx.evaluateString(this, s, filename, lineno, null));
        }
        catch(Exception e) {
            // eat it silently
//            System.out.println(e);
            return "";
        }
    }

    public String loadFile(String filename) {
        return processSource(_cx, this, filename);
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

    public IRuntime getRuntime() {return _runtime;}


    // required for ScriptableObject.
    public String getClassName() {
        return "global";
    }

    /**
     * Load and execute a set of JavaScript source files.
     *
     * This method is defined as a JavaScript function.
     *
     */
    public static void load(Context cx, Scriptable thisObj,
                            Object[] args, Function funObj)
    {
        for (int i=0; i < args.length; i++) {
            processSource(cx, (ContextRhino)thisObj,cx.toString(args[i]));
        }
    }

    /**
     * Evaluate JavaScript source.
     * Code is taken from Shell.java
     *
     * @param cx the current context
     * @param filename the name of the file to compile
     */
    public static String processSource(Context cx, ContextRhino self, String filename) {
            Reader in = null;
            String result = "";
            try {
                in = new PushbackReader(new FileReader(filename));
                int c = in.read();
                // Support the executable script #! syntax:  If
                // the first line begins with a '#', treat the whole
                // line as a comment.
                if (c == '#') {
                    while ((c = in.read()) != -1) {
                        if (c == '\n' || c == '\r')
                            break;
                    }
                    ((PushbackReader) in).unread(c);
                } else {
                    // No '#' line, just reopen the file and forget it
                    // ever happened.  OPT closing and reopening
                    // undoubtedly carries some cost.  Is this faster
                    // or slower than leaving the PushbackReader
                    // around?
                    in.close();
                    in = new FileReader(filename);
                }

            }
            catch (FileNotFoundException ex) {
                System.out.println ("Couldn't open");
                return result;
            } catch (IOException ioe) {
                System.err.println(ioe.toString());
            }

            try {
                // Here we evalute the entire contents of the file as
                // a script. Text is printed only if the print() function
                // is called.
                result = Context.toString(
                            cx.evaluateReader(self, in, filename, 1, null));
            }
            catch (WrappedException we) {
                System.err.println(we.getWrappedException().toString());
                we.printStackTrace();
            }
            catch (EvaluatorException ee) {
                // Already printed message, so just fall through.
            }
            catch (JavaScriptException jse) {
            }
            catch (IOException ioe) {
                System.err.println(ioe.toString());
            }
            finally {
                try {
                    in.close();
                }
                catch (IOException ioe) {
                    System.err.println(ioe.toString());
                }
            }
        return result;
    }

    /**
     * Get and set the language version.
     *
     * This method is defined as a JavaScript function.
     */
    public static double version(Context cx, Scriptable thisObj,
                                 Object[] args, Function funObj)
    {
        double result = (double) cx.getLanguageVersion();
        if (args.length > 0) {
            double d = cx.toNumber(args[0]);
            cx.setLanguageVersion((int) d);
        }
        return result;
    }

    /**
     * Print the string values of its arguments.
     *
     * This method is defined as a JavaScript function.
     * Note that its arguments are of the "varargs" form, which
     * allows it to handle an arbitrary number of arguments
     * supplied to the JavaScript function.
     *
     */
    public static Object print(Context cx, Scriptable thisObj,
                               Object[] args, Function funObj)
    {
        IPrintSink sink = ((ContextRhino)thisObj)._printSink;

        if(null != sink) {
            for (int i=0; i < args.length; i++) {
                if (i > 0)
                    sink.print(" ");
    
                // Convert the
                // arbitrary JavaScript value into a string form.
    
                String s = Context.toString(args[i]);
    
                sink.print(s);
            }
            sink.print("\n");
        }
        return Context.getUndefinedValue();
    }

    // implement com.netscape.javascript.ErrorReporter
    public void warning(String message, String sourceName, int line,
                        String lineSource, int lineOffset)
    {
        if(null != _errorSink)
            _errorSink.error(message, sourceName, line, lineSource, lineOffset);
    }

    public void error(String message, String sourceName, int line,
                      String lineSource, int lineOffset)
    {
        if(null != _errorSink)
            _errorSink.error(message, sourceName, line, lineSource, lineOffset);
        throw new EvaluatorException(message);
    }

    public EvaluatorException runtimeError(String message, String sourceName,
                                           int line, String lineSource, 
                                           int lineOffset)
    {
        if(null != _errorSink)
            _errorSink.error(message, sourceName, line, lineSource, lineOffset);
        return new EvaluatorException(message);
    }

    private boolean _destroyed = false;
    private RuntimeRhino _runtime;
    private Context _cx;
    private IErrorSink  _errorSink;
    private IPrintSink  _printSink;
}    
