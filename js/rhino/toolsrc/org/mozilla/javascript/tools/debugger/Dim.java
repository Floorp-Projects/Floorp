/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Rhino JavaScript Debugger code, released
 * November 21, 2000.
 *
 * The Initial Developer of the Original Code is SeeBeyond Corporation.

 * Portions created by SeeBeyond are
 * Copyright (C) 2000 SeeBeyond Technology Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 * Matt Gould
 * Christopher Oliver
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

package org.mozilla.javascript.tools.debugger;

import org.mozilla.javascript.*;
import org.mozilla.javascript.debug.*;
import java.util.*;
import java.io.*;
import java.net.URL;

/**
 * Dim or Debugger Implementation for Rhino.
*/
class Dim {

    static final int STEP_OVER = 0;
    static final int STEP_INTO = 1;
    static final int STEP_OUT = 2;
    static final int GO = 3;
    static final int BREAK = 4;
    static final int EXIT = 5;

    static class ContextData
    {
        static ContextData get(Context cx) {
            return (ContextData)cx.getDebuggerContextData();
        }

        int frameCount() {
            return frameStack.size();
        }

        StackFrame getFrame(int frameNumber) {
            return (StackFrame) frameStack.get(frameStack.size() - frameNumber - 1);
        }

        void pushFrame(StackFrame frame) {
            frameStack.push(frame);
        }

        void popFrame() {
            frameStack.pop();
        }

        ObjArray frameStack = new ObjArray();
        boolean breakNextLine;
        int stopAtFrameDepth = -1;
        boolean eventThreadFlag;
        Throwable lastProcessedException;
    }

    static class StackFrame implements DebugFrame {

        StackFrame(Context cx, Dim dim, FunctionSource fsource)
        {
            this.dim = dim;
            this.contextData = ContextData.get(cx);
            this.fsource = fsource;
            this.breakpoints = fsource.sourceInfo().breakpoints;
            this.lineNumber = fsource.firstLine();
            contextData.pushFrame(this);
        }

        public void onEnter(Context cx, Scriptable scope,
                            Scriptable thisObj, Object[] args)
        {
            this.scope = scope;
            this.thisObj = thisObj;
            if (dim.breakOnEnter) {
                dim.handleBreakpointHit(this, cx);
            }
        }

        public void onLineChange(Context cx, int lineno)
        {
            this.lineNumber = lineno;

            if (!breakpoints[lineno] && !dim.breakFlag) {
                boolean lineBreak = contextData.breakNextLine;
                if (lineBreak && contextData.stopAtFrameDepth >= 0) {
                    lineBreak = (contextData.frameCount()
                                 <= contextData.stopAtFrameDepth);
                }
                if (!lineBreak) {
                    return;
                }
                contextData.stopAtFrameDepth = -1;
                contextData.breakNextLine = false;
            }

            dim.handleBreakpointHit(this, cx);
        }

        public void onExceptionThrown(Context cx, Throwable exception)
        {
            dim.handleExceptionThrown(cx, exception, this);
        }

        public void onExit(Context cx, boolean byThrow, Object resultOrException)
        {
            if (dim.breakOnReturn && !byThrow) {
                dim.handleBreakpointHit(this, cx);
            }
            contextData.popFrame();
        }

        SourceInfo sourceInfo() {
            return fsource.sourceInfo();
        }

        ContextData contextData()
        {
            return contextData;
        }

        Object scope()
        {
            return scope;
        }

        Object thisObj()
        {
            return thisObj;
        }

        String getUrl()
        {
            return fsource.sourceInfo().url();
        }

        int getLineNumber() {
            return lineNumber;
        }

        private Dim dim;
        private ContextData contextData;
        private Scriptable scope;
        private Scriptable thisObj;
        private FunctionSource fsource;
        private boolean[] breakpoints;
        private int lineNumber;
    }

    static class FunctionSource
    {
        private SourceInfo sourceInfo;
        private int firstLine;
        private String name;

        FunctionSource(SourceInfo sourceInfo, int firstLine, String name)
        {
            if (name == null) throw new IllegalArgumentException();
            this.sourceInfo = sourceInfo;
            this.firstLine = firstLine;
            this.name = name;
        }

        SourceInfo sourceInfo()
        {
            return sourceInfo;
        }

        int firstLine()
        {
            return firstLine;
        }

        String name()
        {
            return name;
        }
    }

    static class SourceInfo
    {
        private String source;
        private String url;

        private int minLine;
        private boolean[] breakableLines;
        boolean[] breakpoints;

        private static final boolean[] EMPTY_BOOLEAN_ARRAY = new boolean[0];

        private FunctionSource[] functionSources;

        SourceInfo(String source, DebuggableScript[] functions,
                   String normilizedUrl)
        {
            this.source = source;
            this.url = normilizedUrl;

            int N = functions.length;
            int[][] lineArrays = new int[N][];
            for (int i = 0; i != N; ++i) {
                lineArrays[i] = functions[i].getLineNumbers();
            }

            int minAll = 0, maxAll = -1;
            int[] firstLines = new int[N];
            for (int i = 0; i != N; ++i) {
                int[] lines = lineArrays[i];
                if (lines == null || lines.length == 0) {
                    firstLines[i] = -1;
                } else {
                    int min, max;
                    min = max = lines[0];
                    for (int j = 1; j != lines.length; ++j) {
                        int line = lines[j];
                        if (line < min) {
                            min = line;
                        } else if (line > max) {
                            max = line;
                        }
                    }
                    firstLines[i] = min;
                    if (minAll > maxAll) {
                        minAll = min;
                        maxAll = max;
                    } else {
                        if (min < minAll) {
                            minAll = min;
                        }
                        if (max > maxAll) {
                            maxAll = max;
                        }
                    }
                }
            }

            if (minAll > maxAll) {
                // No line information
                this.minLine = -1;
                this.breakableLines = EMPTY_BOOLEAN_ARRAY;
                this.breakpoints = EMPTY_BOOLEAN_ARRAY;
            } else {
                if (minAll < 0) {
                    // Line numbers can not be negative
                    throw new IllegalStateException(String.valueOf(minAll));
                }
                this.minLine = minAll;
                int linesTop = maxAll + 1;
                this.breakableLines = new boolean[linesTop];
                this.breakpoints = new boolean[linesTop];
                for (int i = 0; i != N; ++i) {
                    int[] lines = lineArrays[i];
                    if (lines != null && lines.length != 0) {
                        for (int j = 0; j != lines.length; ++j) {
                            int line = lines[j];
                            this.breakableLines[line] = true;
                        }
                    }
                }
            }
            this.functionSources = new FunctionSource[N];
            for (int i = 0; i != N; ++i) {
                String name = functions[i].getFunctionName();
                if (name == null) {
                    name = "";
                }
                this.functionSources[i]
                    = new FunctionSource(this, firstLines[i], name);
            }
        }

        String source()
        {
            return this.source;
        }

        String url()
        {
            return this.url;
        }

        int functionSourcesTop()
        {
            return functionSources.length;
        }

        FunctionSource functionSource(int i)
        {
            return functionSources[i];
        }

        void copyBreakpointsFrom(SourceInfo old)
        {
            int end = old.breakpoints.length;
            if (end > this.breakpoints.length) {
                end = this.breakpoints.length;
            }
            for (int line = 0; line != end; ++line) {
                if (old.breakpoints[line]) {
                    this.breakpoints[line] = true;
                }
            }
        }

        boolean breakableLine(int line)
        {
            return (line < this.breakableLines.length)
                   && this.breakableLines[line];
        }

        boolean breakpoint(int line)
        {
            if (!breakableLine(line)) {
                throw new IllegalArgumentException(String.valueOf(line));
            }
            return line < this.breakpoints.length && this.breakpoints[line];
        }

        boolean breakpoint(int line, boolean value)
        {
            if (!breakableLine(line)) {
                throw new IllegalArgumentException(String.valueOf(line));
            }
            boolean changed;
            synchronized (breakpoints) {
                if (breakpoints[line] != value) {
                    breakpoints[line] = value;
                    changed = true;
                } else {
                    changed = false;
                }
            }
            return changed;
        }

        void removeAllBreakpoints()
        {
            synchronized (breakpoints) {
                for (int line = 0; line != breakpoints.length; ++line) {
                    breakpoints[line] = false;
                }
            }
        }
    }

    private final Debugger debuggerImpl = new Debugger()
    {
        public DebugFrame getFrame(Context cx, DebuggableScript fnOrScript)
        {
            return mirrorStackFrame(cx, fnOrScript);
        }

        public void handleCompilationDone(Context cx,
                                          DebuggableScript fnOrScript,
                                          String source)
        {
            onCompilationDone(cx, fnOrScript, source);
        }
    };

    GuiCallback callback;

    boolean breakFlag = false;

    ScopeProvider scopeProvider;

    int frameIndex = -1;

    boolean isInterrupted = false;
    boolean nonDispatcherWaiting = false;
    int dispatcherIsWaiting = 0;
    volatile ContextData currentContextData = null;

    private Object monitor = new Object();
    private Object swingMonitor = new Object();
    private int returnValue = -1;
    private boolean insideInterruptLoop;
    private String evalRequest;
    private StackFrame evalFrame;
    private String evalResult;

    boolean breakOnExceptions;
    boolean breakOnEnter;
    boolean breakOnReturn;

    private final Hashtable urlToSourceInfo = new Hashtable();
    private final Hashtable functionNames = new Hashtable();
    private final Hashtable functionToSource = new Hashtable();

    void enableForAllNewContexts()
    {
        Context.addContextListener(new ContextListener() {

            public void contextCreated(Context cx)
            {
                initNewContext(cx);
            }

            public void contextEntered(Context cx) { }

            public void contextExited(Context cx) { }

            public void contextReleased(Context cx) { }
        });
    }

    void initNewContext(Context cx)
    {
        ContextData contextData = new ContextData();
        cx.setDebugger(debuggerImpl, contextData);
        cx.setGeneratingDebug(true);
        cx.setOptimizationLevel(-1);
    }

    DebugFrame mirrorStackFrame(Context cx, DebuggableScript fnOrScript)
    {
        FunctionSource item = getFunctionSource(fnOrScript);
        if (item == null) {
            // Can not debug if source is not available
            return null;
        }
        return new StackFrame(cx, this, item);
    }

    void onCompilationDone(Context cx, DebuggableScript fnOrScript,
                           String source)
    {
        if (!fnOrScript.isTopLevel()) {
            return;
        }
        registerTopScript(fnOrScript, source);
    }

    FunctionSource getFunctionSource(DebuggableScript fnOrScript)
    {
        FunctionSource fsource = functionSource(fnOrScript);
        if (fsource == null) {
            String url = getNormilizedUrl(fnOrScript);
            SourceInfo si = sourceInfo(url);
            if (si == null) {
                if (!fnOrScript.isGeneratedScript()) {
                    // Not eval or Function, try to load it from URL
                    String source = loadSource(url);
                    if (source != null) {
                        DebuggableScript top = fnOrScript;
                        for (;;) {
                            DebuggableScript parent = top.getParent();
                            if (parent == null) {
                                break;
                            }
                            top = parent;
                        }
                        registerTopScript(top, source);
                        fsource = functionSource(fnOrScript);
                    }
                }
            }
        }
        return fsource;
    }

    private String loadSource(String sourceUrl)
    {
        String source = null;
        int hash = sourceUrl.indexOf('#');
        if (hash >= 0) {
            sourceUrl = sourceUrl.substring(0, hash);
        }
        try {
            InputStream is;
          openStream:
            {
                if (sourceUrl.indexOf(':') < 0) {
                    // Can be a file name
                    try {
                        if (sourceUrl.startsWith("~/")) {
                            String home = System.getProperty("user.home");
                            if (home != null) {
                                String pathFromHome = sourceUrl.substring(2);
                                File f = new File(new File(home), pathFromHome);
                                if (f.exists()) {
                                    is = new FileInputStream(f);
                                    break openStream;
                                }
                            }
                        }
                        File f = new File(sourceUrl);
                        if (f.exists()) {
                            is = new FileInputStream(f);
                            break openStream;
                        }
                    } catch (SecurityException ex) { }
                    // No existing file, assume missed http://
                    if (sourceUrl.startsWith("//")) {
                        sourceUrl = "http:" + sourceUrl;
                    } else if (sourceUrl.startsWith("/")) {
                        sourceUrl = "http://127.0.0.1" + sourceUrl;
                    } else {
                        sourceUrl = "http://" + sourceUrl;
                    }
                }

                is = (new URL(sourceUrl)).openStream();
            }

            try {
                source = Kit.readReader(new InputStreamReader(is));
            } finally {
                is.close();
            }
        } catch (IOException ex) {
            System.err.println
                ("Failed to load source from "+sourceUrl+": "+ ex);
        }
        return source;
    }

    private void registerTopScript(DebuggableScript topScript, String source)
    {
        if (!topScript.isTopLevel()) {
            throw new IllegalArgumentException();
        }
        String url = getNormilizedUrl(topScript);
        DebuggableScript[] functions = getAllFunctions(topScript);
        final SourceInfo sourceInfo = new SourceInfo(source, functions, url);

        synchronized (urlToSourceInfo) {
            SourceInfo old = (SourceInfo)urlToSourceInfo.get(url);
            if (old != null) {
                sourceInfo.copyBreakpointsFrom(old);
            }
            urlToSourceInfo.put(url, sourceInfo);
            for (int i = 0; i != sourceInfo.functionSourcesTop(); ++i) {
                FunctionSource fsource = sourceInfo.functionSource(i);
                String name = fsource.name();
                if (name.length() != 0) {
                    functionNames.put(name, fsource);
                }
            }
        }

        synchronized (functionToSource) {
            for (int i = 0; i != functions.length; ++i) {
                FunctionSource fsource = sourceInfo.functionSource(i);
                functionToSource.put(functions[i], fsource);
            }
        }

        callback.updateSourceText(sourceInfo);
    }

    FunctionSource functionSource(DebuggableScript fnOrScript)
    {
        return (FunctionSource)functionToSource.get(fnOrScript);
    }

    String[] functionNames()
    {
        String[] a;
        synchronized (urlToSourceInfo) {
            Enumeration e = functionNames.keys();
            a = new String[functionNames.size()];
            int i = 0;
            while (e.hasMoreElements()) {
                a[i++] = (String)e.nextElement();
            }
        }
        return a;
    }

    FunctionSource functionSourceByName(String functionName)
    {
        return (FunctionSource)functionNames.get(functionName);
    }

    SourceInfo sourceInfo(String url)
    {
        return (SourceInfo)urlToSourceInfo.get(url);
    }

    String getNormilizedUrl(DebuggableScript fnOrScript)
    {
        String url = fnOrScript.getSourceName();
        if (url == null) { url = "<stdin>"; }
        else {
            // Not to produce window for eval from different lines,
            // strip line numbers, i.e. replace all #[0-9]+\(eval\) by
            // (eval)
            // Option: similar teatment for Function?
            char evalSeparator = '#';
            StringBuffer sb = null;
            int urlLength = url.length();
            int cursor = 0;
            for (;;) {
                int searchStart = url.indexOf(evalSeparator, cursor);
                if (searchStart < 0) {
                    break;
                }
                String replace = null;
                int i = searchStart + 1;
                boolean hasDigits = false;
                while (i != urlLength) {
                    int c = url.charAt(i);
                    if (!('0' <= c && c <= '9')) {
                        break;
                    }
                    ++i;
                }
                if (i != searchStart + 1) {
                    // i points after #[0-9]+
                    if ("(eval)".regionMatches(0, url, i, 6)) {
                        cursor = i + 6;
                        replace = "(eval)";
                    }
                }
                if (replace == null) {
                    break;
                }
                if (sb == null) {
                    sb = new StringBuffer();
                    sb.append(url.substring(0, searchStart));
                }
                sb.append(replace);
            }
            if (sb != null) {
                if (cursor != urlLength) {
                    sb.append(url.substring(cursor));
                }
                url = sb.toString();
            }
        }
        return url;
    }

    private static DebuggableScript[] getAllFunctions(DebuggableScript function)
    {
        ObjArray functions = new ObjArray();
        collectFunctions_r(function, functions);
        DebuggableScript[] result = new DebuggableScript[functions.size()];
        functions.toArray(result);
        return result;
    }

    private static void collectFunctions_r(DebuggableScript function,
                                           ObjArray array)
    {
        array.add(function);
        for (int i = 0; i != function.getFunctionCount(); ++i) {
            collectFunctions_r(function.getFunction(i), array);
        }
    }

    void clearAllBreakpoints()
    {
        Enumeration e = urlToSourceInfo.elements();
        while (e.hasMoreElements()) {
            SourceInfo si = (SourceInfo)e.nextElement();
            si.removeAllBreakpoints();
        }
    }

    void handleBreakpointHit(StackFrame frame, Context cx) {
        breakFlag = false;
        interrupted(cx, frame, null);
    }

    void handleExceptionThrown(Context cx, Throwable ex, StackFrame frame) {
        if (breakOnExceptions) {
            ContextData cd = frame.contextData();
            if (cd.lastProcessedException != ex) {
                interrupted(cx, frame, ex);
                cd.lastProcessedException = ex;
            }
        }
    }

    /* end Debugger interface */

    private Object withContext(ContextAction action)
    {
        return Context.call(action);
    }

    void compileScript(final String url, final String text)
    {
        withContext(new ContextAction() {
            public Object run(Context cx)
            {
                cx.compileString(text, url, 1, null);
                return null;
            }
        });
    }

    void evalScript(final String url, final String text)
    {
        withContext(new ContextAction() {
            public Object run(Context cx)
            {
                Scriptable scope = null;
                if (scopeProvider != null) {
                    scope = scopeProvider.getScope();
                }
                if (scope == null) {
                    scope = new ImporterTopLevel(cx);
                }
                cx.evaluateString(scope, text, url, 1, null);
                return null;
            }
        });
    }

    void contextSwitch (int frameIndex) {
        this.frameIndex = frameIndex;
    }

    ContextData currentContextData() {
        return currentContextData;
    }

    private void interrupted(Context cx, final StackFrame frame,
                             Throwable scriptException)
    {
        boolean eventThreadFlag = callback.isGuiEventThread();
        ContextData contextData = frame.contextData();
        contextData.eventThreadFlag = eventThreadFlag;
        int line = frame.getLineNumber();
        String url = frame.getUrl();

        synchronized (swingMonitor) {
            if (eventThreadFlag) {
                dispatcherIsWaiting++;
                if (nonDispatcherWaiting) {
                    // Another thread is stopped in the debugger
                    // process events until it resumes and we
                    // can enter
                    while (nonDispatcherWaiting) {
                        try {
                            callback.dispatchNextGuiEvent();
                            if (this.returnValue == EXIT) {
                                return;
                            }
                            swingMonitor.wait(1);
                        } catch (InterruptedException exc) {
                            return;
                        }
                    }
                }
            } else {
                while (isInterrupted || dispatcherIsWaiting > 0) {
                    try {
                        swingMonitor.wait();
                    } catch (InterruptedException exc) {
                        return;
                    }
                }
                nonDispatcherWaiting = true;
            }
            isInterrupted = true;
            currentContextData = contextData;
        }
        do {
            int frameCount = contextData.frameCount();
            this.frameIndex = frameCount -1;

            final String threadTitle = Thread.currentThread().toString();
            final String alertMessage;
            if (scriptException == null) {
                alertMessage = null;
            } else {
                alertMessage = scriptException.toString();
            }

            int returnValue = -1;
            if (!eventThreadFlag) {
                synchronized (monitor) {
                    if (insideInterruptLoop) Kit.codeBug();
                    this.insideInterruptLoop = true;
                    this.evalRequest = null;
                    this.returnValue = -1;
                    callback.enterInterrupt(frame, threadTitle, alertMessage);
                    try {
                        for (;;) {
                            try {
                                monitor.wait();
                            } catch (InterruptedException exc) {
                                Thread.currentThread().interrupt();
                                break;
                            }
                            if (evalRequest != null) {
                                this.evalResult = null;
                                try {
                                    evalResult = do_eval(cx, evalFrame, evalRequest);
                                } finally {
                                    evalRequest = null;
                                    evalFrame = null;
                                    monitor.notify();
                                }
                                continue;
                            }
                            if (this.returnValue != -1) {
                                returnValue = this.returnValue;
                                break;
                            }
                        }
                    } finally {
                        insideInterruptLoop = false;
                    }
                }
            } else {
                this.returnValue = -1;
                callback.enterInterrupt(frame, threadTitle, alertMessage);
                while (this.returnValue == -1) {
                    try {
                        callback.dispatchNextGuiEvent();
                    } catch (InterruptedException exc) {
                    }
                }
                returnValue = this.returnValue;
            }
            switch (returnValue) {
            case STEP_OVER:
                contextData.breakNextLine = true;
                contextData.stopAtFrameDepth = contextData.frameCount();
                break;
            case STEP_INTO:
                contextData.breakNextLine = true;
                contextData.stopAtFrameDepth = -1;
                break;
            case STEP_OUT:
                if (contextData.frameCount() > 1) {
                    contextData.breakNextLine = true;
                    contextData.stopAtFrameDepth = contextData.frameCount() -1;
                }
                break;
            }
        } while (false);

        synchronized (swingMonitor) {
            currentContextData = null;
            isInterrupted = false;
            if (eventThreadFlag) {
                dispatcherIsWaiting--;
            } else {
                nonDispatcherWaiting = false;
            }
            swingMonitor.notifyAll();
        }
    }

    void setReturnValue(int returnValue)
    {
        synchronized (monitor) {
            this.returnValue = returnValue;
            monitor.notify();
        }
    }

    void go()
    {
        synchronized (monitor) {
            this.returnValue = GO;
            monitor.notifyAll();
        }
    }

    boolean stringIsCompilableUnit(final String expr)
    {
        Boolean result = (Boolean)withContext(new ContextAction() {
            public Object run(Context cx)
            {
                return cx.stringIsCompilableUnit(expr)
                    ? Boolean.TRUE : Boolean.FALSE;
            }
        });
        return result.booleanValue();
    }

    String objectToString(final Object object)
    {
        if (object == Undefined.instance) {
            return "undefined";
        }
        if (object == null) {
            return "null";
        }
        if (object instanceof NativeCall) {
            return "[object Call]";
        }
        String result = (String)withContext(new ContextAction() {
            public Object run(Context cx)
            {
                return Context.toString(object);
            }
        });
        return result;
    }

    Object getObjectProperty(Object object, Object id)
    {
        Scriptable scriptable = (Scriptable)object;
        Object result;
        if (id instanceof String) {
            String name = (String)id;
            if (name.equals("this")) {
                result = scriptable;
            } else if (name.equals("__proto__")) {
                result = scriptable.getPrototype();
            } else if (name.equals("__parent__")) {
                result = scriptable.getParentScope();
            } else {
                result = ScriptableObject.getProperty(scriptable, name);
                if (result == ScriptableObject.NOT_FOUND) {
                    result = Undefined.instance;
                }
            }
        } else {
            int index = ((Integer)id).intValue();
            result = ScriptableObject.getProperty(scriptable, index);
            if (result == ScriptableObject.NOT_FOUND) {
                result = Undefined.instance;
            }
        }
        return result;
    }

    Object[] getObjectIds(Object object)
    {
        if (!(object instanceof Scriptable) || object == Undefined.instance) {
            return Context.emptyArgs;
        }
        final Scriptable scriptable = (Scriptable)object;
        Object[] ids = (Object[])withContext(new ContextAction() {
            public Object run(Context cx)
            {
                if (scriptable instanceof DebuggableObject) {
                    return ((DebuggableObject)scriptable).getAllIds();
                } else {
                    return scriptable.getIds();
                }
            }
        });
        Scriptable proto = scriptable.getPrototype();
        Scriptable parent = scriptable.getParentScope();
        int extra = 0;
        if (proto != null) {
            ++extra;
        }
        if (parent != null) {
            ++extra;
        }
        if (extra != 0) {
            Object[] tmp = new Object[extra + ids.length];
            System.arraycopy(ids, 0, tmp, extra, ids.length);
            ids = tmp;
            extra = 0;
            if (proto != null) {
                ids[extra++] = "__proto__";
            }
            if (parent != null) {
                ids[extra++] = "__parent__";
            }
        }
        return ids;
    }

    String eval(String expr)
    {
        String result = "undefined";
        if (expr == null) {
            return result;
        }
        ContextData contextData = currentContextData();
        if (contextData == null || frameIndex >= contextData.frameCount()) {
            return result;
        }
        StackFrame frame = contextData.getFrame(frameIndex);
        if (contextData.eventThreadFlag) {
            Context cx = Context.getCurrentContext();
            result = do_eval(cx, frame, expr);
        } else {
            synchronized (monitor) {
                if (insideInterruptLoop) {
                    evalRequest = expr;
                    evalFrame = frame;
                    monitor.notify();
                    do {
                        try {
                            monitor.wait();
                        } catch (InterruptedException exc) {
                            Thread.currentThread().interrupt();
                            break;
                        }
                    } while (evalRequest != null);
                    result = evalResult;
                }
            }
        }
        return result;
    }

    private static String do_eval(Context cx, StackFrame frame, String expr)
    {
        String resultString;
        Debugger saved_debugger = cx.getDebugger();
        Object saved_data = cx.getDebuggerContextData();
        int saved_level = cx.getOptimizationLevel();

        cx.setDebugger(null, null);
        cx.setOptimizationLevel(-1);
        cx.setGeneratingDebug(false);
        try {
            Callable script = (Callable)cx.compileString(expr, "", 0, null);
            Object result = script.call(cx, frame.scope, frame.thisObj,
                                        ScriptRuntime.emptyArgs);
            if (result == Undefined.instance) {
                resultString = "";
            } else {
                resultString = ScriptRuntime.toString(result);
            }
        } catch (Exception exc) {
            resultString = exc.getMessage();
        } finally {
            cx.setGeneratingDebug(true);
            cx.setOptimizationLevel(saved_level);
            cx.setDebugger(saved_debugger, saved_data);
        }
        if (resultString == null) {
            resultString = "null";
        }
        return resultString;
    }
}

