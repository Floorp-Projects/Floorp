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
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
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

// API class

package org.mozilla.javascript.tools.debugger;

import org.mozilla.javascript.*;
import org.mozilla.javascript.debug.*;
import org.mozilla.javascript.tools.shell.*;

import java.io.*;
import java.util.*;

public class DebugShell implements Debugger {

    public DebugShell(Context cx) {
        cx.setDebugger(this);
    }

    public void handleBreakpointHit(Context cx) {
        Frame frame = cx.getFrame(0);
        Main.getOut().println("Hit breakpoint at \"" + frame.getSourceName() +
                              "\", line " + frame.getLineNumber());
        enterShell(cx, frame.getVariableObject());
    }
    
    public void enterShell(Context cx, Scriptable scope) {
        cx.setBreakNextLine(false);
        BufferedReader in = new BufferedReader
            (new InputStreamReader(Main.getIn()));
        while (true) {
            Main.getOut().print("# ");
            String line;
            try {
                line = in.readLine();
            }
            catch (IOException ioe) {
                Main.getErr().println(ioe.toString());
                break;
            }
            if (line == null || line.length() == 0 || line.equals("#"))
                return;
            String command = line;
            String args = null;
            int space = line.indexOf(" ");
            if (space != -1) {
                command = line.substring(0, space);
                args = line.substring(space+1);
            }
            if (command.equals("b")) {
                Object o = scope.get(args, scope);  // XXX: b obj.f
                if (o == Scriptable.NOT_FOUND) {
                    Main.getErr().println("function " + args + " was not found");
                    continue;
                }
                if (!(o instanceof DebuggableScript)) {
                    Main.getErr().println("function " + args + " is not debuggable");
                    continue;
                }
                Enumeration e = ((DebuggableScript)o).getLineNumbers();
                Object n;
                int min = Integer.MAX_VALUE;
                while (e.hasMoreElements()) {
                    n = e.nextElement();
                    int i = ((Integer) n).intValue();
                    if (i < min)
                        min = i;
                }
                if (((DebuggableScript)o).placeBreakpoint(min))
                    Main.getErr().println("Breakpoint placed at line " + min); /// xx source
                else
                    Main.getErr().println("Cannot place breakpoint for " + args);
            }
            if (command.equals("p")) {
                Reader reader = new StringReader(args);
                Object result = Main.evaluateReader(cx, scope, reader, 
                                                    "<p command>", 1);
                Main.getErr().println(cx.toString(result));
            }
            if (command.equals("s")) {
                cx.setBreakNextLine(true);
                break;
            }
        }
    }
}
