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
 * Christopher Oliver
 * Matt Gould
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

import javax.swing.*;
import javax.swing.text.*;
import javax.swing.event.*;
import javax.swing.table.*;
import java.awt.*;
import java.awt.event.*;
import java.util.StringTokenizer;

import org.mozilla.javascript.*;
import org.mozilla.javascript.debug.*;
import org.mozilla.javascript.tools.shell.ConsoleTextArea;
import java.util.*;
import java.io.*;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.DefaultTreeCellRenderer;
import java.lang.reflect.Method;
import java.net.URL;

public class Main implements Debugger, ContextListener {

    DebugGui debugGui;

    static final int STEP_OVER = 0;
    static final int STEP_INTO = 1;
    static final int STEP_OUT = 2;
    static final int GO = 3;
    static final int BREAK = 4;
    static final int RUN_TO_CURSOR = 5;
    static final int EXIT = 6;

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

        StackFrame(Context cx, Main db, DebuggableScript fnOrScript)
        {
            this.db = db;
            this.contextData = ContextData.get(cx);
            this.fnOrScript = fnOrScript;
            ScriptItem item = db.getScriptItem(fnOrScript);
            if (item != null) {
                this.sourceInfo = item.getSourceInfo();
                this.lineNumber = item.getFirstLine();
            }
            contextData.pushFrame(this);
        }

        public void onEnter(Context cx, Scriptable scope,
                            Scriptable thisObj, Object[] args)
        {
            this.scope = scope;
            this.thisObj = thisObj;
            if (db.breakOnEnter) {
                db.handleBreakpointHit(this, cx);
            }
        }

        public void onLineChange(Context cx, int lineno) {
            this.lineNumber = lineno;

          checks:
            if (sourceInfo == null || !sourceInfo.hasBreakpoint(lineno)) {
                if (db.breakFlag) {
                    break checks;
                }
                if (!contextData.breakNextLine) {
                    return;
                }
                if (contextData.stopAtFrameDepth > 0) {
                    if (contextData.frameCount() > contextData.stopAtFrameDepth) {
                        return;
                    }
                    contextData.stopAtFrameDepth = -1;
                }
                contextData.breakNextLine = false;
            }
            db.handleBreakpointHit(this, cx);
        }

        public void onExceptionThrown(Context cx, Throwable exception) {
            db.handleExceptionThrown(cx, exception, this);
        }

        public void onExit(Context cx, boolean byThrow, Object resultOrException) {
            if (db.breakOnReturn && !byThrow) {
                db.handleBreakpointHit(this, cx);
            }
            contextData.popFrame();
        }

        SourceInfo sourceInfo() {
            return sourceInfo;
        }

        ContextData contextData()
        {
            return contextData;
        }

        Scriptable scope()
        {
            return scope;
        }

        Scriptable thisObj()
        {
            return thisObj;
        }

        String getUrl() {
            if (sourceInfo != null) {
                return sourceInfo.getUrl();
            }
            return db.getNormilizedUrl(fnOrScript);
        }

        int getLineNumber() {
            return lineNumber;
        }

        DebuggableScript getScript() {
            return fnOrScript;
        }

        private Main db;
        private ContextData contextData;
        private Scriptable scope;
        private Scriptable thisObj;
        private DebuggableScript fnOrScript;
        private SourceInfo sourceInfo;
        private int lineNumber;
    }

    static class ScriptItem {

        ScriptItem(DebuggableScript fnOrScript, SourceInfo sourceInfo) {
            this.fnOrScript = fnOrScript;
            this.sourceInfo = sourceInfo;
        }

        DebuggableScript getScript() { return fnOrScript; }

        SourceInfo getSourceInfo() { return sourceInfo; }

        int getFirstLine() {
            return (firstLine == 0) ? 1 : firstLine;
        }

        void setFirstLine(int firstLine) {
            if (firstLine <= 0) { throw new IllegalArgumentException(); }
            if (this.firstLine != 0) { throw new IllegalStateException(); }
            this.firstLine = firstLine;
        }

        private DebuggableScript fnOrScript;
        private SourceInfo sourceInfo;
        private int firstLine;
    }

    static class SourceInfo {

        static String getShortName(String url) {
            int lastSlash = url.lastIndexOf('/');
            if (lastSlash < 0) {
                lastSlash = url.lastIndexOf('\\');
            }
            String shortName = url;
            if (lastSlash >= 0 && lastSlash + 1 < url.length()) {
                shortName = url.substring(lastSlash + 1);
            }
            return shortName;
        }

        SourceInfo(String sourceUrl, String source) {
            this.sourceUrl = sourceUrl;
            this.source = source;
        }

        String getUrl() {
            return sourceUrl;
        }

        String getSource() {
            return source;
        }

        synchronized void setSource(String source) {
            if (!this.source.equals(source)) {
                this.source = source;
                endLine = 0;
                breakableLines = null;

                if (breakpoints != null) {
                    for (int i = breakpoints.length - 1; i >= 0; --i) {
                        if (breakpoints[i] == BREAK_FLAG) {
                            breakpoints[i] = OLD_BREAK_FLAG;
                        }
                    }
                }
            }
        }

        synchronized void updateLineInfo(ScriptItem item) {

            int[] lines = item.getScript().getLineNumbers();
            if (lines.length == 0) {
                return;
            }

            int fnFirstLine = lines[0];
            int fnEndLine = fnFirstLine + 1;
            for (int i = 1; i != lines.length; ++i) {
                int line = lines[i];
                if (line < fnFirstLine) {
                    fnFirstLine = line;
                }else if (line >= fnEndLine) {
                    fnEndLine = line + 1;
                }
            }
            item.setFirstLine(fnFirstLine);

            if (endLine < fnEndLine) {
                endLine = fnEndLine;
            }
            if (breakableLines == null) {
                int newLength = 20;
                if (newLength < endLine) { newLength = endLine; }
                breakableLines = new boolean[newLength];
            }else if (breakableLines.length < endLine) {
                int newLength = breakableLines.length * 2;
                if (newLength < endLine) { newLength = endLine; }
                boolean[] tmp = new boolean[newLength];
                System.arraycopy(breakableLines, 0, tmp, 0, breakableLines.length);
                breakableLines = tmp;
            }
            int breakpointsEnd = (breakpoints == null) ? 0 : breakpoints.length;
            for (int i = 0; i != lines.length; ++i) {
                int line = lines[i];
                breakableLines[line] = true;
                if (line < breakpointsEnd) {
                    if (breakpoints[line] == OLD_BREAK_FLAG) {
                        breakpoints[line] = BREAK_FLAG;
                    }
                }
            }
        }

        boolean breakableLine(int line) {
            boolean[] breakableLines = this.breakableLines;
            if (breakableLines != null && line < breakableLines.length) {
                return breakableLines[line];
            }
            return false;
        }

        boolean hasBreakpoint(int line) {
            byte[] breakpoints = this.breakpoints;
            if (breakpoints != null && line < breakpoints.length) {
                return breakpoints[line] == BREAK_FLAG;
            }
            return false;
        }

        synchronized boolean placeBreakpoint(int line) {
            if (breakableLine(line)) {
                if (breakpoints == null) {
                    breakpoints = new byte[endLine];
                }else if (line >= breakpoints.length) {
                    byte[] tmp = new byte[endLine];
                    System.arraycopy(breakpoints, 0, tmp, 0, breakpoints.length);
                    breakpoints = tmp;
                }
                breakpoints[line] = BREAK_FLAG;
                return true;
            }
            return false;
        }

        synchronized boolean removeBreakpoint(int line) {
            boolean wasBreakpoint = false;
            if (breakpoints != null && line < breakpoints.length) {
                wasBreakpoint = (breakpoints[line] == BREAK_FLAG);
                breakpoints[line] = 0;
            }
            return wasBreakpoint;
        }

        synchronized void removeAllBreakpoints() {
            breakpoints = null;
        }

        private String sourceUrl;
        private String source;

        private int endLine;
        private boolean[] breakableLines;

        private static final byte BREAK_FLAG = 1;
        private static final byte OLD_BREAK_FLAG = 2;
        private byte[] breakpoints;

    }



    int runToCursorLine;
    String runToCursorFile;
    private Hashtable scriptItems = new Hashtable();
    Hashtable sourceNames = new Hashtable();

    Hashtable functionNames = new Hashtable();

    boolean breakFlag = false;

    ScopeProvider scopeProvider;
    Runnable exitAction;

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


    /* ContextListener interface */

    public void contextCreated(Context cx) {

        ContextData contextData = new ContextData();
        cx.setDebugger(this, contextData);
        cx.setGeneratingDebug(true);
        cx.setOptimizationLevel(-1);
    }

    public void contextEntered(Context cx) {
    }

    public void contextExited(Context cx) {
    }

    public void contextReleased(Context cx) {
    }

    /* end ContextListener interface */

    public void doBreak() {
        breakFlag = true;
    }

    ScriptItem getScriptItem(DebuggableScript fnOrScript) {
        ScriptItem item = (ScriptItem)scriptItems.get(fnOrScript);
        if (item == null) {
            String url = getNormilizedUrl(fnOrScript);
            SourceInfo si = (SourceInfo)sourceNames.get(url);
            if (si == null) {
                if (!fnOrScript.isGeneratedScript()) {
                    // Not eval or Function, try to load it from URL
                    String source = null;
                    try {
                        InputStream is = openSource(url);
                        try {
                            source = Kit.readReader(new InputStreamReader(is));
                        } finally {
                            is.close();
                        }
                    } catch (IOException ex) {
                        System.err.println
                            ("Failed to load source from "+url+": "+ ex);
                    }
                    if (source != null) {
                        si = registerSource(url, source);
                    }
                }
            }
            if (si != null) {
                item = registerScript(si, fnOrScript);
            }
        }
        return item;
    }

    /* Debugger Interface */

    public void handleCompilationDone(Context cx, DebuggableScript fnOrScript,
                                      String source)
    {
         String sourceUrl = getNormilizedUrl(fnOrScript);
         SourceInfo si = registerSource(sourceUrl, source);
         registerScript(si, fnOrScript);
    }

    String getNormilizedUrl(DebuggableScript fnOrScript) {
        String url = fnOrScript.getSourceName();
        if (url == null) { url = "<stdin>"; }
        else {
            // Not to produce window for eval from different lines,
            // strip line numbers, i.e. replace all #[0-9]+\(eval\) by (eval)
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

    private static InputStream openSource(String sourceUrl)
        throws IOException
    {
        int hash = sourceUrl.indexOf('#');
        if (hash >= 0) {
            sourceUrl = sourceUrl.substring(0, hash);
        }
        if (sourceUrl.indexOf(':') < 0) {
            // Can be a file name
            try {
                if (sourceUrl.startsWith("~/")) {
                    String home = System.getProperty("user.home");
                    if (home != null) {
                        String pathFromHome = sourceUrl.substring(2);
                        File f = new File(new File(home), pathFromHome);
                        if (f.exists()) {
                            return new FileInputStream(f);
                        }
                    }
                }
                File f = new File(sourceUrl);
                if (f.exists()) {
                    return new FileInputStream(f);
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

        return (new java.net.URL(sourceUrl)).openStream();
    }

    private SourceInfo registerSource(String sourceUrl, String source) {
        SourceInfo si;
        synchronized (sourceNames) {
            si = (SourceInfo)sourceNames.get(sourceUrl);
            if (si == null) {
                si = new SourceInfo(sourceUrl, source);
                sourceNames.put(sourceUrl, si);
            } else {
                si.setSource(source);
            }
        }
        return si;
    }

    private ScriptItem registerScript(final SourceInfo si,
                                      DebuggableScript fnOrScript)
    {
        ScriptItem item = new ScriptItem(fnOrScript, si);
        si.updateLineInfo(item);
        scriptItems.put(fnOrScript, item);

        String name = fnOrScript.getFunctionName();
        if (name != null && name.length() > 0 && !name.equals("anonymous")) {
            functionNames.put(name, item);
        }

        swingInvokeLater(new Runnable() {
            public void run()
            {
                debugGui.updateFileText(si);
            }
        });

        return item;
    }

    void handleBreakpointHit(StackFrame frame, Context cx) {
        breakFlag = false;
        interrupted(cx, frame, null);
    }

    private static String exceptionString(StackFrame frame, Throwable ex)
    {
        String details;
        if (ex instanceof JavaScriptException) {
            JavaScriptException jse = (JavaScriptException)ex;
            details = ScriptRuntime.toString(jse.getValue());
        } else if (ex instanceof EcmaError) {
            details = ex.toString();
        } else {
            if (ex instanceof WrappedException) {
                Throwable wrapped
                    = ((WrappedException)ex).getWrappedException();
                if (wrapped != null) {
                    ex = wrapped;
                }
            }
            details = ex.toString();
            if (details == null || details.length() == 0) {
                details = ex.getClass().toString();
            }
        }
        String url = frame.getUrl();
        int lineNumber = frame.getLineNumber();
        return details+" (" + url + ", line " + lineNumber + ")";
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

    public DebugFrame getFrame(Context cx, DebuggableScript fnOrScript) {
        return new StackFrame(cx, this, fnOrScript);
    }

    /* end Debugger interface */


    Scriptable getScope() {
        return (scopeProvider != null) ? scopeProvider.getScope() : null;
    }

    static void swingInvokeLater(Runnable f)
    {
        SwingUtilities.invokeLater(f);
    }

    void contextSwitch (int frameIndex) {
        this.frameIndex = frameIndex;
    }

    ContextData currentContextData() {
        return currentContextData;
    }

    void interrupted(Context cx, final StackFrame frame, Throwable scriptException)
    {
        boolean eventThreadFlag = SwingUtilities.isEventDispatchThread();
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
                            dispatchNextAwtEvent();
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
            if (runToCursorFile != null) {
                if (url != null && url.equals(runToCursorFile)) {
                    if (line == runToCursorLine) {
                        runToCursorFile = null;
                    } else {
                        SourceInfo si = frame.sourceInfo();
                        if (si == null || !si.hasBreakpoint(line))
                        {
                            break;
                        } else {
                            runToCursorFile = null;
                        }
                    }
                }
            }
            int frameCount = contextData.frameCount();
            this.frameIndex = frameCount -1;

            final String threadTitle = Thread.currentThread().toString();
            final String alertMessage;
            if (scriptException == null) {
                alertMessage = null;
            } else {
                alertMessage = exceptionString(frame, scriptException);
            }

            Runnable enterAction = new Runnable() {
                public void run()
                {
                    debugGui.enterInterrupt(frame, threadTitle, alertMessage);
                }
            };

            int returnValue = -1;
            if (!eventThreadFlag) {
                synchronized (monitor) {
                    if (insideInterruptLoop) Kit.codeBug();
                    this.insideInterruptLoop = true;
                    this.evalRequest = null;
                    this.returnValue = -1;
                    swingInvokeLater(enterAction);
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
                enterAction.run();
                while (this.returnValue == -1) {
                    try {
                        dispatchNextAwtEvent();
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
            case RUN_TO_CURSOR:
                contextData.breakNextLine = true;
                contextData.stopAtFrameDepth = -1;
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

    boolean stringIsCompilableUnit(String expr) {
        Context cx = Context.enter();
        boolean result = cx.stringIsCompilableUnit(expr);
        cx.exit();
        return result;
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
            Object result = script.call(cx, frame.scope(), frame.thisObj(),
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

    void Exit() {
        // stop handling events
        this.returnValue = EXIT;
        // call the exit handler if any
        if (exitAction != null) {
            swingInvokeLater(exitAction);
        }
    }

    private static java.awt.EventQueue awtEventQueue = null;

    private static void dispatchNextAwtEvent()
        throws InterruptedException
    {
        java.awt.EventQueue queue = awtEventQueue;
        if (queue == null) {
            queue = java.awt.Toolkit.getDefaultToolkit().getSystemEventQueue();
            awtEventQueue = queue;
        }
        AWTEvent event = queue.getNextEvent();
        if (event instanceof ActiveEvent) {
            ((ActiveEvent)event).dispatch();
        } else {
            Object source = event.getSource();
            if (source instanceof Component) {
                Component comp = (Component)source;
                comp.dispatchEvent(event);
            } else if (source instanceof MenuComponent) {
                ((MenuComponent)source).dispatchEvent(event);
            }
        }
    }

    //
    // public interface
    //

    public Main(String title)
    {
        debugGui = new DebugGui(this, title);
    }

   /**
    *  Toggle Break-on-Exception behavior
    */
    public void setBreakOnExceptions(boolean value) {
        this.breakOnExceptions = value;
    }

   /**
    *  Toggle Break-on-Enter behavior
    */
    public void setBreakOnEnter(boolean value) {
        this.breakOnEnter = value;
    }

   /**
    *  Toggle Break-on-Return behavior
    */
    public void setBreakOnReturn(boolean value) {
        this.breakOnReturn = value;
    }

    /**
     *
     * Remove all breakpoints
     */
    public void clearAllBreakpoints() {
        Enumeration e = sourceNames.elements();
        while (e.hasMoreElements()) {
            SourceInfo si = (SourceInfo)e.nextElement();
            si.removeAllBreakpoints();
        }
    }

   /**
    *  Resume Execution
    */
    public void go()
    {
        synchronized (monitor) {
            this.returnValue = GO;
            monitor.notifyAll();
        }
    }

    /**
     * Assign an object that can provide a Scriptable that will
     * be used as the scope for loading scripts from files
     * when the user selects "Open..." or "Run..."
     */
    public void setScopeProvider(ScopeProvider p) {
        scopeProvider = p;
    }

    /**
     * Assign a Runnable object that will be invoked when the user
     * selects "Exit..." or closes the Debugger main window
     */
    public void setExitAction(Runnable r) {
        exitAction = r;
    }

    /**
     * Get an input stream to the Debugger's internal Console window
     */

    public InputStream getIn() {
        return debugGui.console.getIn();
    }

    /**
     * Get an output stream to the Debugger's internal Console window
     */

    public PrintStream getOut() {
        return debugGui.console.getOut();
    }

    /**
     * Get an error stream to the Debugger's internal Console window
     */

    public PrintStream getErr() {
        return debugGui.console.getErr();
    }

    public static void main(String[] args)
        throws Exception
    {
        Main sdb = new Main("Rhino JavaScript Debugger");
        sdb.breakFlag = true;
        sdb.setExitAction(new Runnable() {
                public void run() {
                    System.exit(0);
                }
            });
        System.setIn(sdb.getIn());
        System.setOut(sdb.getOut());
        System.setErr(sdb.getErr());
        Context.addContextListener(sdb);
        sdb.setScopeProvider(new ScopeProvider() {
                public Scriptable getScope() {
                    return org.mozilla.javascript.tools.shell.Main.getScope();
                }
            });

        sdb.debugGui.pack();
        sdb.debugGui.setSize(600, 460);
        sdb.debugGui.setVisible(true);

        org.mozilla.javascript.tools.shell.Main.exec(args);
    }

}

