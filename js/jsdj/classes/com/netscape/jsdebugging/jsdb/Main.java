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

/* JavaScript command line Debugger
 * @author Alex Rakhlin
 */

package com.netscape.jsdebugging.jsdb;

import java.io.*;
import java.util.*;

import com.netscape.jsdebugging.engine.*;
import com.netscape.jsdebugging.api.*;

public class Main {

    public static void main(String args[]) {
        new Main(args);
    }

    private void showCreation(Object o, String name) {
//        System.out.println(name+" "+(o==null?"NOT ":"")+"created");
    }

    public static final String LOCAL_CREATOR_NAME = 
        "com.netscape.jsdebugging.engine.local.RuntimeCreatorLocal";

    public static final String RHINO_CREATOR_NAME = 
        "com.netscape.jsdebugging.engine.rhino.RuntimeCreatorRhino";

    private void usage() {
        System.out.println("jsdb");
        System.out.println("");
        System.out.println("usage java jsdb [local|rhino]");
        System.out.println("");
    }

    private Main(String[] args) {
        String creatorName = null;

        if(args.length == 0) {
            usage();
            return;
        }
        if(args[0].toLowerCase().equals("local"))
            creatorName = LOCAL_CREATOR_NAME;
        else if(args[0].toLowerCase().equals("rhino"))
            creatorName = RHINO_CREATOR_NAME;
        else {
            usage();
            return;
        }

        try {
            Class clazz = Class.forName(creatorName);

            IRuntimeCreator creator =
                (IRuntimeCreator) clazz.newInstance();

            _runtime = creator.newRuntime(IRuntimeCreator.ENABLE_DEBUGGING, null);
            showCreation(_runtime, "runtime");

            _context = _runtime.newContext(null);
            showCreation(_context, "context");

            _controller = _runtime.getDebugController();
            showCreation(_controller, "controller");

            _sourceTextProvider = _runtime.getSourceTextProvider();
            showCreation(_sourceTextProvider, "sourceTextProvider");

            LocalSink sink = new LocalSink();
            _context.setPrintSink(sink);
            _context.setErrorSink(sink);
            

            _controller.setScriptHook(new JSDBScriptHook(_controller));
            _interruptHook = new JSDBInterruptHook(_controller, _sourceTextProvider);
            _controller.setInterruptHook(_interruptHook);
            _controller.setErrorReporter(new JSDBErrorReporter());
        }
        catch(Throwable t)
        {
            System.out.println("exception thrown " + t );
        }

        //-------------------- LOOP -----------------
        DataInputStream in = new DataInputStream (System.in);
        String inputLine="";
        while (!_quitting){
            System.out.print (">");
            try {
                inputLine = in.readLine();
            }
            catch (IOException e){
                //eat it...
            }

            processLine (inputLine);
        }
        //--------------------- END -----------------
    }

    /**
     * Print a help message.
     *
     */
    public void help() {
        System.out.println ("********************************** HELP **********************************");
        System.out.println ("load <filename>             Load and run a javascript file.");
        System.out.println ("suspend                     Stop before first instruction is executed");
        System.out.println ("stop at <filename> <line>   Set a breakpoint. File doesn't have to be loaded");
        System.out.println ("clear at <filename> <line>  Clear a breakpoint.");
        System.out.println ("bp                          List breakpoints.");
        System.out.println ("list <filename> <line>      Print source code.");
        System.out.println ("scripts                     List loaded scripts.");
        System.out.println ("quit, exit                  Quit jsdb.");
        System.out.println ("**************************************************************************");
    }

    // Quit the shell
    public void quit() {
        _quitting = true;
    }

    public void processLine (String line){
        StringTokenizer st = new StringTokenizer(line);
        if (st.countTokens() == 0) return;
        if (st.countTokens() == 1){
            String first = st.nextToken();
            if (first.equals ("quit") || first.equals ("exit")) quit();
            else
            if (first.equals ("help") || first.equals ("?")) help();
            else
            if (first.equals ("suspend")) {
                System.out.println ("Execution will be stopped at the first instruction");
                _controller.sendInterrupt();
            }
            else
            if (first.equals ("scripts")) {
                _interruptHook.listScripts();
            }
            else
            if (first.equals ("bp")) {
                _interruptHook.listBreakpoints();
            }
            else System.out.println ("Invalid command");
        }
        else
        if (st.countTokens() >= 2){
            String first=st.nextToken();
            String second = st.nextToken();
            if (first.equals ("load")) _context.loadFile(second);
            else
            if (first.equals ("stop") && second.equals ("at")) _interruptHook.stopAt (st);
            else
            if (first.equals ("clear") && second.equals ("at")) _interruptHook.clearAt (st);
            else
            if (first.equals ("list")) _interruptHook.list(st, second);
            else System.out.println ("Invalid command");
        }

    }

    public DebugController getController()    {return _controller;}
    public SourceTextProvider getSourceTextProvider() {return _sourceTextProvider;}

    private IRuntime _runtime;
    private IContext _context;
    private DebugController _controller;
    private SourceTextProvider _sourceTextProvider;

    private boolean _quitting;
    private JSDBInterruptHook _interruptHook;
}

class LocalSink implements IPrintSink, IErrorSink
{
    public void print(String s) {
        System.out.print(s);
    }
    public void error(String msg, String filename, int lineno,
                      String lineBuf, int offset)
    {
//        System.out.println("");
//        System.out.println("-------------------------------------");
//        System.out.println(msg);
//        System.out.println(filename);
//        System.out.println(lineno);
//        System.out.println(lineBuf);
//        System.out.println(offset);
//        System.out.println("-------------------------------------");
//        System.out.println("");
    }
}    
