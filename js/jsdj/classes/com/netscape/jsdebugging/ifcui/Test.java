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

/*
* Test code (while developing) - currently for new Value/Props stuff
*/

package com.netscape.jsdebugging.ifcui;

import com.netscape.jsdebugging.api.*;
import java.io.DataInputStream;
import netscape.util.Hashtable;
import netscape.util.Enumeration;

class ErrorEater implements JSErrorReporter
{
    // implement JSErrorReporter interface
    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
        // eat it
        System.out.print(msg);
        return JSErrorReporter.RETURN;
    }
}

class HintMgr
{
    // To statically add hits:
    // 1) add a "String[] XXX_hints" Object
    // 2) add a {"XXX", XXX_hintsString} pair to "_hintSets"

    static final String[]  Arguments_hints =
    {"length", "callee", "caller"};

    static final String[]  Call_hints =
    {"arguments", "__callee__", "__caller__", "__call__"};

    static final String[]  Function_hints =
    {"arguments", "__arity__", "length", "__caller__", "__name__", "__call__"};

    static final String[]  Object_hints =
    {"__proto__", "__parent__", "__count__"};

    static final String[]  RegExp_hints =
    {"source", "global", "ignoreCase", "lastIndex"};

    static final String[]  String_hints =
    {"length"};

    static final Object[] _hintSets =  {
        "Arguments",    Arguments_hints,
        "Call",         Call_hints,
        "Function",     Function_hints,
        "Object",       Object_hints,
        "RegExp",       RegExp_hints,
        "String",       String_hints
    };

    /**************************************************************/

    public HintMgr() {
        int count = _hintSets.length;
        _table = new Hashtable(count);
        for(int i = 0; i < count; i += 2) {
            _setHints((String)_hintSets[i], (String[])_hintSets[i+1]);
        }
    }

    public String[] getHints(String classname) {
        return (String[])_table.get(classname);
    }

    public void setHints(String classname, String[] hints) {
        if(null == classname || 0 == classname.length())
            return;
        _table.remove(classname);
        if(hints != null)
            addHints(classname, hints);
    }

    private void _setHints(String classname, String[] hints) {
        _table.put(classname, hints);
    }

    public void addHints(String classname, String[] hints) {
        int newLength = 0;
        if(null == classname || 0 == classname.length() ||
           null == hints || 0 == (newLength = hints.length))
            return;

        String[] oldHints = getHints(classname);
        if(null != oldHints) {
            int oldLength = oldHints.length;
            String[] addHints = new String[newLength];
            int added = 0;
     outer: for(int k = 0; k < newLength; k++) {
                String hint = hints[k];
                if(null == hint || 0 == hint.length())
                    continue;
                for(int i = 0; i < oldLength; i++) {
                    if(hint.equals(oldHints[i]))
                        continue outer;
                }
                addHints[added++] = hint;
            }
            if(0 != added) {
                String[] combined = new String[oldLength + added];
                System.arraycopy(oldHints, 0, combined, 0, oldLength);
                System.arraycopy(addHints, 0, combined, oldLength, added);
                _setHints(classname, combined);
            }
        }
        else {
            _setHints(classname, hints);
        }
    }

    public void addHint(String classname, String hint) {
        String[] hints = {hint};
        addHints(classname, hints);
    }

    public void dumpHints() {
        System.out.println("HintMgr dump:");
        System.out.println(_table);
    }

    private Hashtable _table;
}


class Test
{
    static HintMgr _hintMgr = new HintMgr();

    static void doTest(Emperor emperor)
    {
        dumpPromptedObject(emperor, 1);
//        dumpCallScopeThis(emperor);
//        getPropTest(emperor);
//        hintMgrTest(emperor);
    }

    static final String[] helpText = {
        "valid commands: #help | #call | #scope | #this | #{0-9}",
        "",
        "Property flags legend:",
        "e...... canEnumerate()",
        ".r..... isReadOnly()",
        "..p.... isPermanent()",
        "...a... hasAlias()",
        "....A.. isArgument()",
        ".....V. isVariable()",
        "......H isHinted()",
        "",
        "Value flags legend:",
        "o.......... isObject()",
        ".n......... isNumber()",
        "..i........ isInt()",
        "...d....... isDouble()",
        "....s...... isString()",
        ".....b..... isBoolean()",
        "......N.... isNull()",
        ".......V... isVoid()",
        "........p.. isPrimitive()",
        ".........f. isFunction()",
        "..........n isNative()"
    };

    static void showHelp() {
        for(int i = 0; i < helpText.length; i++)
            System.out.println(helpText[i]);
        System.out.println();
    }

    static void hintMgrTest(Emperor emperor)
    {
        String[] hints0 = {"foo", "length"};
        String[] hints1 = {"bar", "baz"};
        String[] hints2 = {"length"};

        // start state:
        _hintMgr.dumpHints();
        System.out.println("adding String : foo");
        _hintMgr.addHint("String", "foo");
        _hintMgr.dumpHints();
        System.out.println("adding String : foo, length");
        _hintMgr.addHints("String", hints0);
        _hintMgr.dumpHints();
        System.out.println("adding String : bar, baz");
        _hintMgr.addHints("String", hints1);
        _hintMgr.dumpHints();
        System.out.println("clearing String");
        _hintMgr.setHints("String", null);
        _hintMgr.dumpHints();
        System.out.println("adding String : length");
        _hintMgr.addHints("String", hints2);
        _hintMgr.dumpHints();
        System.out.println("clearing String");
        _hintMgr.setHints("String", null);
        _hintMgr.dumpHints();
        System.out.println("setting String : length");
        _hintMgr.addHints("String", hints2);
        _hintMgr.dumpHints();
    }

    static void getPropTest(Emperor emperor)
    {
        ControlTyrant controlTyrant = emperor.getControlTyrant();
        if(controlTyrant.getState() != ControlTyrant.STOPPED)
            return;

        try {
            DebugController dc = emperor.getDebugController();
            JSStackFrameInfo frame =
                (JSStackFrameInfo) emperor.getStackTyrant().getCurrentFrame();

            if( null == dc || null == frame )
            {
                System.out.println("dc or frame bad");
                return;
            }

            JSErrorReporter oldER =
                    controlTyrant.setErrorReporter(new ErrorEater());

            System.out.print("\n");

            String strToEval = "fun";
            System.out.println(strToEval);
            ExecResult r =
                dc.executeScriptInStackFrameValue(frame,strToEval,"test",1);
            Value the_obj = r.getResultValue();
//            the_obj = frame.getCallObject();

            printVal(the_obj, 0);
//            if(null != the_obj)
//                the_obj = the_obj.getPrototype();

            Property[] props = the_obj.getProperties();

            if(null == props)
                printLine(0, "props is null");
            else if(props.length == 0)
                printLine(0, "props.length = 0");
            else
            {
                printLine(0, "props.length = "+ props.length);

                for(int i = 0; i < props.length; i++)
                {
                    System.out.println("");
                    printLine(0, "-------------prop---------------");
                    printProp(props[i], 0);
                    printLine(0, "-------------value---------------");
                    printVal(props[i].getValue(), 0);
                }
            }

            if(null != the_obj)
            {
                printLine(0, "-------------setPropertyHints ---------------");

                if(null != the_obj) {
                    String[] hints = _hintMgr.getHints(the_obj.getClassName());
                    if(null != hints) {
                        the_obj.setPropertyHints(hints);
                        the_obj.refresh();
                    }
                }
                printVal(the_obj, 0);

                props = the_obj.getProperties();

                if(null == props)
                    printLine(0, "props is null");
                else if(props.length == 0)
                    printLine(0, "props.length = 0");
                else
                {
                    printLine(0, "props.length = "+ props.length);

                    for(int i = 0; i < props.length; i++)
                    {
                        System.out.println("");
                        printLine(0, "-------------prop---------------");
                        printProp(props[i], 0);
                        printLine(0, "-------------value---------------");
                        printVal(props[i].getValue(), 0);
                    }
                }
            }

            System.out.println();

            controlTyrant.setErrorReporter(oldER);
        }
        catch(Exception e) {
            System.out.println(e);
            e.printStackTrace();
            // eat it...
        }


    }


    static void dumpCallScopeThis(Emperor emperor)
    {
        ControlTyrant controlTyrant = emperor.getControlTyrant();
        if(controlTyrant.getState() != ControlTyrant.STOPPED)
            return;

        try {
            DebugController dc = emperor.getDebugController();
            JSStackFrameInfo frame =
                (JSStackFrameInfo) emperor.getStackTyrant().getCurrentFrame();

            if( null == dc || null == frame )
            {
                System.out.println("dc or frame bad");
                return;
            }

            JSErrorReporter oldER =
                    controlTyrant.setErrorReporter(new ErrorEater());

            System.out.print("\n");

            System.out.print("Call Object\n");
            Value callObject = frame.getCallObject();
            printVal(callObject, 0);

            Property[] props = callObject.getProperties();

            if(null == props)
                printLine(0, "props is null");
            else if(props.length == 0)
                printLine(0, "props.length = 0");
            else
            {
                printLine(0, "props.length = "+ props.length);

                for(int i = 0; i < props.length; i++)
                {
                    System.out.println("");
                    printLine(0, "-------------prop---------------");
                    printProp(props[i], 0);
                    printLine(0, "-------------value---------------");
                    printVal(props[i].getValue(), 0);
                }
            }


            System.out.print("Scope Chain\n");
            Value scopeChain = frame.getScopeChain();
            printVal(scopeChain, 0);

            System.out.print("this\n");
            Value thisp = frame.getThis();
            printVal(thisp, 0);

            System.out.println();

            controlTyrant.setErrorReporter(oldER);
        }
        catch(Exception e) {
            System.out.println(e);
            e.printStackTrace();
            // eat it...
        }


    }

    static void dumpPromptedObject(Emperor emperor, int maxDepth)
    {
        ControlTyrant controlTyrant = emperor.getControlTyrant();
        if(controlTyrant.getState() != ControlTyrant.STOPPED)
            return;

        while(true)
        {
            System.out.print("\n");
            System.out.print("eval> ");
            try {
                String str = new DataInputStream(System.in).readLine();
                if(null == str || 0 == str.length())
                {
                    System.out.println();
                    return;
                }

                DebugController dc = emperor.getDebugController();
                JSStackFrameInfo frame =
                    (JSStackFrameInfo) emperor.getStackTyrant().getCurrentFrame();

                if( null == dc || null == frame )
                {
                    System.out.println("dc or frame bad");
                    return;
                }

                JSErrorReporter oldER =
                        controlTyrant.setErrorReporter(new ErrorEater());

                Value val = null;

                if(str.charAt(0) == '#') {
                    if(str.equals("#help")) {
                        showHelp();
                        continue;
                    }
                    else if(str.equals("#call"))
                        val = frame.getCallObject();
                    else if(str.equals("#scope"))
                        val = frame.getScopeChain();
                    else if(str.equals("#this"))
                        val = frame.getThis();
                    else {
                        try {
                            maxDepth = Integer.parseInt(str.substring(1).trim());
                            System.out.println("maxDepth set to "+maxDepth+"\n");

                        } catch(NumberFormatException e) {
                            System.out.println("bad input...");
                            showHelp();
                        }
                        continue;
                    }
                }
                else {
                    ExecResult r =
                        dc.executeScriptInStackFrameValue(frame,str,"test",1);
                    if(r.getErrorOccured())
                    {
                        System.out.println("Error...");

                        System.out.println(r.getErrorMessage()     +"\t"+"getErrorMessage()");
                        System.out.println(r.getErrorFilename()    +"\t"+"getErrorFilename()");
                        System.out.println(r.getErrorLineNumber()  +"\t"+"getErrorLineNumber()");
                        System.out.println(r.getErrorLineBuffer()  +"\t"+"getErrorLineBuffer()");
                        System.out.println(r.getErrorTokenOffset() +"\t"+"getErrorTokenOffset()");
                        System.out.println();
                        continue;
                    }
                    val = r.getResultValue();
                }
                if(null != val && val.isObject()) {
                    String classname = val.getClassName();
                    if(null != classname) {
                        String[] hints = _hintMgr.getHints(classname);
                        if(null != hints) {
                            val.setPropertyHints(hints);
                            val.refresh();
                        }
                    }
                }
                printValRecursive(val, 0, maxDepth);

                System.out.println();

                controlTyrant.setErrorReporter(oldER);
            }
            catch(Exception e) {
                System.out.println(e);
                e.printStackTrace();
                // eat it...
            }
        }
    }

    static void printValRecursive(Value val, int indent, int maxDepth)
    {
        if(null == val)
        {
            System.out.println("Value is null");
        }
        else
        {
            printVal(val, indent);
            Property[] props = val.getProperties();

            if(null == props)
                printLine(indent, "props is null");
            else if(props.length == 0)
                printLine(indent, "props.length = 0");
            else
            {
                printLine(indent, "props.length = "+ props.length);

                for(int i = 0; i < props.length; i++)
                {
                    System.out.println("");
                    printLine(indent, "-------------prop---------------");
                    printProp(props[i], indent);
                    printLine(indent, "-------------value---------------");
                    printVal(props[i].getValue(), indent);
                }
            }
            if(null != val.getPrototype() && maxDepth-1 > 0)
            {
                printLine(indent, "=====================================");
                printLine(indent, "=============prototype===============");
                printValRecursive(val.getPrototype(), indent+4, maxDepth-1);
            }
        }
    }


    static void printProp(Property prop, int indent)
    {
        printLine(indent, ""+prop.toString());
/**
*         printLine(indent, ""+prop.isNameString()  +"\t"+"isNameString()");
*         printLine(indent, ""+prop.isAliasString() +"\t"+"isAliasString()");
*         printLine(indent, ""+prop.getNameString() +"\t"+"getNameString()");
*         printLine(indent, ""+prop.getNameInt()    +"\t"+"getNameInt()");
*         printLine(indent, ""+prop.getAliasString()+"\t"+"getAliasString()");
*         printLine(indent, ""+prop.getAliasInt()   +"\t"+"getAliasInt()");
* //        printLine(indent, ""+prop.getValue()      +"\t"+"getValue()");
*         printLine(indent, ""+prop.getValArgSlot() +"\t"+"getValArgSlot()");
*         printLine(indent, ""+prop.getFlags()      +"\t"+"getFlags()");
*         printLine(indent, ""+prop.canEnumerate()  +"\t"+"canEnumerate()");
*         printLine(indent, ""+prop.isReadOnly()    +"\t"+"isReadOnly()");
*         printLine(indent, ""+prop.isPermanent()   +"\t"+"isPermanent()");
*         printLine(indent, ""+prop.hasAlias()      +"\t"+"hasAlias()");
*         printLine(indent, ""+prop.isArgument()    +"\t"+"isArgument()");
*         printLine(indent, ""+prop.isVariable()    +"\t"+"isVariable()");
*/
    }

    static void printVal(Value val, int indent)
    {

        printLine(indent, ""+val.toString());

/**
*         printLine(indent, ""+val.isObject()       +"\t"+"isObject()");
*         printLine(indent, ""+val.isNumber()       +"\t"+"isNumber()");
*         printLine(indent, ""+val.isInt()          +"\t"+"isInt()");
*         printLine(indent, ""+val.isDouble()       +"\t"+"isDouble()");
*         printLine(indent, ""+val.isString()       +"\t"+"isString()");
*         printLine(indent, ""+val.isBoolean()      +"\t"+"isBoolean()");
*         printLine(indent, ""+val.isNull()         +"\t"+"isNull()");
*         printLine(indent, ""+val.isVoid()         +"\t"+"isVoid()");
*         printLine(indent, ""+val.isPrimitive()    +"\t"+"isPrimitive()");
*         printLine(indent, ""+val.isFunction()     +"\t"+"isFunction()");
*         printLine(indent, ""+val.isNative()       +"\t"+"isNative()");
*         printLine(indent, ""+val.getBoolean()     +"\t"+"getBoolean()");
*         printLine(indent, ""+val.getInt()         +"\t"+"getInt()");
*         printLine(indent, ""+val.getDouble()      +"\t"+"getDouble()");
*         printLine(indent, ""+val.toString()       +"\t"+"toString()");
*         printLine(indent, ""+val.getFunctionName()+"\t"+"getFunctionName()");
*         printLine(indent, ""+val.getPrototype()   +"\t"+"getPrototype()");
*         printLine(indent, ""+val.getParent()      +"\t"+"getParent()");
*         printLine(indent, ""+val.getConstructor() +"\t"+"getConstructor()");
*/
    }
    static void printLine(int indent, String str) {
        for(int i=0; i < indent; i++)
            System.out.print(" ");
        System.out.println(str);
        System.out.flush();
    }
}