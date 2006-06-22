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
 *
 * Portions created by SeeBeyond are
 * Copyright (C) 2000 SeeBeyond Technology Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 * Matt Gould
 * Christopher Oliver
 * Cameron McCormack
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

import java.io.InputStream;
import java.io.PrintStream;

import javax.swing.JFrame;

import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.shell.Global;

/**
 * Rhino script debugger main class.  This class links together a
 * debugger object ({@link Dim}) and a debugger GUI object ({@link SwingGui}).
 * {@link ContextListener} is only implemented for compatibility.
 */
public class Main implements ContextListener {

    /**
     * The debugger.
     */
    private Dim dim;

    /**
     * The debugger frame.
     */
    private SwingGui debugGui;

    /**
     * Creates a new Main.
     */
    public Main(String title) {
        dim = new Dim();
        debugGui = new SwingGui(dim, title);
    }

    /**
     * Returns the debugger window {@link JFrame}.
     */
    public JFrame getDebugFrame() {
        return debugGui;
    }

    /**
     * Breaks execution of the script.
     */
    public void doBreak() {
        dim.setBreak();
    }

    /**
     * Sets whether execution should break when a script exception is thrown.
     */
    public void setBreakOnExceptions(boolean value) {
        dim.setBreakOnExceptions(value);
        debugGui.getMenubar().getBreakOnExceptions().setSelected(value);
    }

    /**
     * Sets whether execution should break when a function is entered.
     */
    public void setBreakOnEnter(boolean value) {
        dim.setBreakOnEnter(value);
        debugGui.getMenubar().getBreakOnEnter().setSelected(value);
    }

    /**
     * Sets whether execution should break when a function is left.
     */
    public void setBreakOnReturn(boolean value) {
        dim.setBreakOnReturn(value);
        debugGui.getMenubar().getBreakOnReturn().setSelected(value);
    }

    /**
     * Removes all breakpoints.
     */
    public void clearAllBreakpoints() {
        dim.clearAllBreakpoints();
    }

    /**
     * Resumes execution of the script.
     */
    public void go() {
        dim.go();
    }

    /**
     * Sets the scope to be used for script evaluation.
     */
    public void setScope(Scriptable scope) {
        setScopeProvider(IProxy.newScopeProvider(scope));
    }

    /**
     * Sets the {@link ScopeProvider} that provides a scope to be used
     * for script evaluation.
     */
    public void setScopeProvider(ScopeProvider p) {
        dim.setScopeProvider(p);
    }

    /**
     * Assign a Runnable object that will be invoked when the user
     * selects "Exit..." or closes the Debugger main window.
     */
    public void setExitAction(Runnable r) {
        debugGui.setExitAction(r);
    }

    /**
     * Returns an {@link InputStream} for stdin from the debugger's internal
     * Console window.
     */
    public InputStream getIn() {
        return debugGui.getConsole().getIn();
    }

    /**
     * Returns a {@link PrintStream} for stdout to the debugger's internal
     * Console window.
     */
    public PrintStream getOut() {
        return debugGui.getConsole().getOut();
    }

    /**
     * Returns a {@link PrintStream} for stderr in the Debugger's internal
     * Console window.
     */
    public PrintStream getErr() {
        return debugGui.getConsole().getErr();
    }

    /**
     * Packs the debugger GUI frame.
     */
    public void pack() {
        debugGui.pack();
    }

    /**
     * Sets the debugger GUI frame dimensions.
     */
    public void setSize(int w, int h) {
        debugGui.setSize(w, h);
    }

    /**
     * Sets the visibility of the debugger GUI frame.
     */
    public void setVisible(boolean flag) {
        debugGui.setVisible(flag);
    }

    /**
     * Returns whether the debugger GUI frame is visible.
     */
    public boolean isVisible() {
        return debugGui.isVisible();
    }

    /**
     * Frees any resources held by the debugger.
     */
    public void dispose() {
        clearAllBreakpoints();
        dim.go();
        debugGui.dispose();
        dim = null;
    }

    /**
     * Attaches the debugger to the given {@link ContextFactory}.
     */
    public void attachTo(ContextFactory factory) {
        dim.attachTo(factory);
    }

    /**
     * Detaches from the current {@link ContextFactory}.
     */
    public void detach() {
        dim.detach();
    }

    /**
     * Main entry point.  Creates a debugger attached to a Rhino
     * {@link org.mozilla.tools.shell.Main} shell session.
     */
    public static void main(String[] args) {
        Main main = new Main("Rhino JavaScript Debugger");
        main.doBreak();
        main.setExitAction(new IProxy(IProxy.EXIT_ACTION));

        System.setIn(main.getIn());
        System.setOut(main.getOut());
        System.setErr(main.getErr());

        Global global = org.mozilla.javascript.tools.shell.Main.getGlobal();
        global.setIn(main.getIn());
        global.setOut(main.getOut());
        global.setErr(main.getErr());

        main.attachTo(
            org.mozilla.javascript.tools.shell.Main.shellContextFactory);

        main.setScope(global);

        main.pack();
        main.setSize(600, 460);
        main.setVisible(true);

        org.mozilla.javascript.tools.shell.Main.exec(args);
    }

    /**
     * Entry point for embedded applications.  This method attaches
     * to the global {@link ContextFactory} with a scope of a newly
     * created {@link Global} object.  No I/O redirection is performed
     * as with {@link #main(String[])}.
     */
    public static void mainEmbedded(String title) {
        ContextFactory factory = ContextFactory.getGlobal();
        Global global = new Global();
        global.init(factory);
        mainEmbedded(factory, global, title);
    }

    /**
     * Entry point for embedded applications.  This method attaches
     * to the given {@link ContextFactory} with the given scope.  No
     * I/O redirection is performed as with {@link #main(String[])}.
     */
    public static void mainEmbedded(ContextFactory factory,
                                    Scriptable scope,
                                    String title) {
        mainEmbeddedImpl(factory, scope, title);
    }

    /**
     * Entry point for embedded applications.  This method attaches
     * to the given {@link ContextFactory} with the given scope.  No
     * I/O redirection is performed as with {@link #main(String[])}.
     */
    public static void mainEmbedded(ContextFactory factory,
                                    ScopeProvider scopeProvider,
                                    String title) {
        mainEmbeddedImpl(factory, scopeProvider, title);
    }

    /**
     * Helper method for {@link #mainEmbedded}.
     */
    private static void mainEmbeddedImpl(ContextFactory factory,
                                         Object scopeProvider,
                                         String title) {
        if (title == null) {
            title = "Rhino JavaScript Debugger (embedded usage)";
        }
        Main main = new Main(title);
        main.doBreak();
        main.setExitAction(new IProxy(IProxy.EXIT_ACTION));

        main.attachTo(factory);
        if (scopeProvider instanceof ScopeProvider) {
            main.setScopeProvider((ScopeProvider)scopeProvider);
        } else {
            Scriptable scope = (Scriptable)scopeProvider;
            if (scope instanceof Global) {
                Global global = (Global)scope;
                global.setIn(main.getIn());
                global.setOut(main.getOut());
                global.setErr(main.getErr());
            }
            main.setScope(scope);
        }

        main.pack();
        main.setSize(600, 460);
        main.setVisible(true);
    }

    // Deprecated methods

    /**
     * @deprecated Use {@link #setSize(int, int)} instead.
     */
    public void setSize(java.awt.Dimension dimension) {
        debugGui.setSize(dimension.width, dimension.height);
    }

    /**
     * @deprecated
     * The method does nothing and is only present for compatibility.
     */
    public void setOptimizationLevel(int level) {
    }

    /**
     * @deprecated
     * The method is only present for compatibility and should not be called.
     */
    public void contextEntered(Context cx) {
        throw new IllegalStateException();
    }

    /**
     * @deprecated
     * The method is only present for compatibility and should not be called.
     */
    public void contextExited(Context cx) {
        throw new IllegalStateException();
    }

    /**
     * @deprecated
     * The method is only present for compatibility and should not be called.
     */
    public void contextCreated(Context cx) {
        throw new IllegalStateException();
    }

    /**
     * @deprecated
     * The method is only present for compatibility and should not be called.
     */
    public void contextReleased(Context cx)
    {
        throw new IllegalStateException();
    }

    /**
     * Class to consolidate all internal implementations of interfaces
     * to avoid class generation bloat.
     */
    private static class IProxy implements Runnable, ScopeProvider {

        // Constants for 'type'.
        public static final int EXIT_ACTION = 1;
        public static final int SCOPE_PROVIDER = 2;

        /**
         * The type of interface.
         */
        private final int type;

        /**
         * The scope object to expose when {@link #type} =
         * {@link #SCOPE_PROVIDER}.
         */
        private Scriptable scope;

        /**
         * Creates a new IProxy.
         */
        public IProxy(int type) {
            this.type = type;
        }

        /**
         * Creates a new IProxy that acts as a {@link ScopeProvider}.
         */
        public static ScopeProvider newScopeProvider(Scriptable scope) {
            IProxy scopeProvider = new IProxy(SCOPE_PROVIDER);
            scopeProvider.scope = scope;
            return scopeProvider;
        }

        // ContextAction

        /**
         * Exit action.
         */
        public void run() {
            if (type != EXIT_ACTION) Kit.codeBug();
            System.exit(0);
        }

        // ScopeProvider

        /**
         * Returns the scope for script evaluations.
         */
        public Scriptable getScope() {
            if (type != SCOPE_PROVIDER) Kit.codeBug();
            if (scope == null) Kit.codeBug();
            return scope;
        }
    }
}
