/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
* Standard initialization of the debugger
*/

/* Top level code is run when this file is first loaded... (duh) */

var noisy = false
//var noisy = true

jsd.SetScriptHook(scriptHook);
jsd.SetExecutionHook(execHook);
jsd.SetErrorReporterHook(errorReporterHook);

function scriptHook(handle, creating)
{
    if(creating) {
        JSDScript.add(new JSDScript(handle,
                                    jsd.GetScriptFilename(handle),
                                    jsd.GetScriptFunctionName(handle),
                                    jsd.GetScriptBaseLineNumber(handle),
                                    jsd.GetScriptLineExtent(handle)));
        setAllBreakpointsForScript(handle);

        if(jsd.GetScriptLineExtent(handle) < 1) {
            debugger;
            jsd.GetScriptLineExtent(handle);
        }

    }
    else {
        JSDScript.remove(JSDScript.find(handle));
    }
    return true;
}

function errorReporterHook(msg, filename, lineno, lineBuf, tokenOffset)
{
    if(jsd.Evaluating)
        return jsd.JSD_ERROR_REPORTER_PASS_ALONG;

    print("............................");
    print("msg = "+msg);
    print("filename = "+filename);
    print("lineno = "+lineno);
    print("lineBuf = "+lineBuf);
    print("tokenOffset = "+tokenOffset);
    print("............................");

    var answer = -1
    while(-1 == answer) {
        switch(prompt("[E]at [i]gnore [p]ass along [d]ebug ?").substr(0,1)) {
            case "I":
            case "i":
                answer = jsd.JSD_ERROR_REPORTER_RETURN;
                break;
            case "E":
            case "e":
                answer = jsd.JSD_ERROR_REPORTER_CLEAR_RETURN;
                break;
            case "P":
            case "p":
                answer = jsd.JSD_ERROR_REPORTER_PASS_ALONG;
                break;
            case "D":
            case "d":
                answer = jsd.JSD_ERROR_REPORTER_DEBUG;
                break;
            // let's simplify the user's life
            default:
                answer = jsd.JSD_ERROR_REPORTER_CLEAR_RETURN;
                break;
        }
    }
    return answer;
}

function execHook(type)
{
    // reset these globals
    stack = null;
    currentFrame = 0;

    buildStack();

    // save the reason for stopping
    hookType = type;

    if(jsd.InterruptSet)
        jsd.ClearInterrupt();
    switch(type) {
        case jsd.JSD_HOOK_INTERRUPTED:
            break;
        case jsd.JSD_HOOK_BREAKPOINT:
            break;
        case jsd.JSD_HOOK_DEBUG_REQUESTED:
            break;
        case jsd.JSD_HOOK_DEBUGGER_KEYWORD:
            break;
        case jsd.JSD_HOOK_THROW:
            break;
        default:
            print("hit unknown hook type!")
            return jsd.JSD_HOOK_RETURN_CONTINUE;
    }
    writeln("");
    why();
    writeln("");
    where();
    writeln("");
    listc();
    writeln("");
    return doEvalLoop();
}

/***************************************************************************/
/* Debug Objects */

function JSDScript(handle, url, funname, base, extent)
{
    this.handle = handle;
    this.url = url;
    this.funname = funname;
    this.base = base;
    this.extent = extent;
    this.toString = function()
            {return "JSDScript: "+url+":"+
                    (funname!=null&&funname.length?funname:"_TOP_LEVEL_")+
                    ":"+base+":"+extent;}
}

JSDScript.scripts = new Object;

JSDScript.add = function(script)
{
    JSDScript.scripts[script.handle] = script;
    if(noisy)print("Created "+script);
    ScriptCreatedHook(script);
    return "";
}

JSDScript.remove = function(script)
{
    if(!script)
        return;
    if(noisy)print("Deleting "+script);
    ScriptAboutToBeDeletedHook(script);
    delete JSDScript.scripts[script.handle];
    return "";
}

JSDScript.find = function(handle)
{
    return JSDScript.scripts[handle];
}

JSDScript.dump = function(handle)
{
    for(var p in JSDScript.scripts)
        print("JSDScript.scripts['"+p+"'] = "+JSDScript.scripts[p]);
    return "";
}

// in case this file is reloaded, rebuild the script array
jsd.IterateScripts(scriptHook, true);

/* For convienence - overload if you want... */
function ScriptCreatedHook(script)          {}
function ScriptAboutToBeDeletedHook(script) {}

/************************************************/

function JSDFrame(handle)
{
    this.handle = handle;
    this.script = JSDScript.find(jsd.GetScriptForStackFrame(handle));
    this.pc = jsd.GetPCForStackFrame(handle);
    this.lineno = jsd.GetClosestLine(this.script.handle, this.pc);
}

function buildStack()
{
    var count = jsd.GetCountOfStackFrames();
    stack = new Array(count);

    var v = jsd.GetStackFrame();
    var vv = new JSDFrame(v);
    stack[0] = vv;

//    stack[0] = new JSDFrame(jsd.GetStackFrame());
    for(var i = 0; i < count-1; i++)
        stack[i+1] = new JSDFrame(jsd.GetCallingStackFrame(stack[i].handle));
}

function where()
{
    if(! stack)
        print("no stack!");

    for(var i = 0; i < stack.length; i++) {
        print((i == currentFrame ? " >":"  ")+
               "line "+lpad(""+stack[i].lineno, 2)+" of "+stack[i].script);
    }
    return "";
}

function up()
{
    if(currentFrame < stack.length-1)
        currentFrame++ ;
    where();
    writeln("");
    listc();
    writeln("");
    return "";
}

function down()
{
    if(currentFrame > 0)
        currentFrame-- ;
    where();
    writeln("");
    listc();
    writeln("");
    return "";
}


/***************************************************************************/
/* Help System */

var helpLines = new Array;
helpLines.toString = function() {return "[object HelpStrings]";}

function help()
{
    var _
    var _ = "------------type help() to get help--------------";
    var _
    show_help(arguments);
    return "";
}
function show_help(args)
{
    if(args.length == 0) {
        for(var i = 0; i < helpLines.length; i++)
            print(helpLines[i]);
    }
    else {
        // by convention these are going to be the heading lines...
        print(helpLines[0]);
        print(helpLines[1]);
        for(var j = 0; j < args.length; j++) {
            for(var i = 0; i < helpLines.length; i++) {
                if( 0 == String(helpLines[i]).indexOf(args[j]) )
                    print(helpLines[i]);
            }
        }
    }
}

function addHelp(str)
{
    helpLines[helpLines.length] = str;
}

addHelp("Command   Usage                  Description");
addHelp("=======   =====                  ===========");
addHelp("help      help([name ...])       Display usage and help messages");
addHelp("resume    resume()               Leave JSDB and resume program");
addHelp("quit      quit()                 Leave JSDB and terminate script");
addHelp("version   version([number])      Get or set JavaScript version number");
addHelp("load      load(['foo.js' ...])   Load files named by string arguments");
addHelp("print     print([expr,...])      Evaluate and print expressions");
addHelp("write     write(expr)            Evaluate and print one expr. w/o \\n");
addHelp("writeln   writeln(expr)          Evaluate and print one expr. w/ \\n");
addHelp("props     props(object)          Display properties of debugger object");
addHelp("rprops    rprops('object')       Display properties of target object");
addHelp("suspend   suspend()              Send Interrupt");
addHelp("unsuspend unsuspend()            Clear Interrupt");
addHelp("step      step()                 Simple instruction step");
addHelp("why       why()                  Display reason for stopping");
addHelp("where     where()                Show current Stack");
addHelp("up        up()                   Set caller as current frame");
addHelp("down      down()                 Set callee as current frame");
addHelp("show      show('expression')     Evaluate expression in current frame");
addHelp("sources   sources()              Show list of source files");
addHelp("list      list([url,line,count]) Show the source text");
addHelp("listc     listc()                Show current source line with context");
addHelp("listcw    listcw()               Show current source line with wide context");
addHelp("trap      trap([url],lineno)     Set a breakpoint");
addHelp("untrap    untrap([url],lineno)   Clear a breakpoint");
addHelp("safeEval  safeEval('expr'[,url,lineno]) Eval w/o terminate on failure");
addHelp("gets      gets()                 Get string from console");
addHelp("prompt    prompt(str)            Show string and return user input");
addHelp("returnVal returnVal(val)         Resume and force script to return this val");
addHelp("throwVal  throwVal(val)          Resume and force script to throw this val");
addHelp("locals    locals()               Show function local variables");
addHelp("args      args()                 Show function arguments");

/* More can be added anywhere */

/***************************************************************************/
/* Misc */

function pad(str, len)
{
    if(str.length >= len)
        return str;
    /* hideous recursion */
    return pad(str+" ", len);
}

function lpad(str, len, c)
{
    if(str.length >= len)
        return str;
    /* hideous recursion */
    return lpad(" "+str, len);
}

function lzpad(str, len)
{
    if(str.length >= len)
        return str;
    /* hideous recursion */
    return lzpad("0"+str, len);
}

function props(ob)
{
    for(var p in ob)
    {
        var text;
        if(typeof ob[p] == 'function')
            text = "[function "+String(ob[p]).match(/function (\w*)/)[1]+"]";
        else
            text = ob[p];
        print("['"+p+"'] = "+text);
    }
    return "";
}

function unsuspend()
{
    jsd.ClearInterrupt();
    return "";
}

function suspend()
{
    jsd.SendInterrupt();
    return "";
}

function step()
{
    jsd.SendInterrupt();
    resume();
    return "";
}

/***************************************************************************/

function rprops(text)
{
    var name = "___UNIQUE_NAME__";
    var fun = ""+
"for(var p in ob)"+
"{"+
"    var text;"+
"    if(typeof ob[p] == 'function')"+
"        text = \\\"[function \\\"+String(ob[p]).match(/function (\\\\w*)/)[1]+\\\"]\\\";"+
"    else"+
"        text = ob[p];"+
"    if(p!=\\\"___UNIQUE_NAME__\\\")"+
"        print(\\\"['\\\"+p+\\\"'] = \\\"+text);"+
"}"+
"return \\\"\\\";";

    reval(name+" = new Function(\"ob\",\""+fun+"\")");
//    show(name);
    reval(name+"("+text+")");
    reval("delete "+name);

    return "";
}


function show(text,filename,lineno)
{
    print(_reval(arguments));
    return "";
}

function reval(text,filename,lineno)
{
    _reval(arguments);
    return "";
}

function _reval(args)
{
    if(!args.length)
        return print("_reval requires at least one arg");

    var a = new Array(args.length+1);
    a[0] = stack[currentFrame].handle;
    for(var i = 0; i < args.length; i++)
        a[i+1] = args[i];

    return jsd.EvaluateScriptInStackFrame.apply(this,a);
}

function revalToValue(text,filename,lineno)
{
    return _revalToValue(arguments);
}

function _revalToValue(args)
{
    if(!args.length)
        return print("_revalToValue requires at least one arg");

    var a = new Array(args.length+1);
    a[0] = stack[currentFrame].handle;
    for(var i = 0; i < args.length; i++)
        a[i+1] = args[i];

    return new JSDValue(jsd.EvaluateScriptInStackFrameToValue.apply(this,a));
}

function why()
{
    var s;
    var e;

    switch(hookType) {
        case jsd.JSD_HOOK_INTERRUPTED:
            print("hit interrupt hook");
            break;
        case jsd.JSD_HOOK_BREAKPOINT:
            print("hit breakpoint hook");
            break;
        case jsd.JSD_HOOK_DEBUG_REQUESTED:
            print("hit debug break hook");
            break;
        case jsd.JSD_HOOK_DEBUGGER_KEYWORD:
            print("hit debugger keyword hook");
            break;
        case jsd.JSD_HOOK_THROW:
            e = new JSDValue(jsd.GetException());
            if(e)
               s = " => " + e.GetValueString();
            else
                s = "";
            print("hit throw hook"+s);
            break;
        default:
            print("hit unknown hook type!")
    }
    return "";
}

/***************************************************************************/
/* Source functions */
function sourceIterator(handle)
{
    print("  "+jsd.GetSourceURL(handle));
    return true;
}

function sources()
{
    return print(jsd.IterateSources(sourceIterator)+" source files(s) found");
}

function listc()
{
    return list(-2,5);
}

function listcw()
{
    return list(-20,41);
}

function list()
{
    var argsLength = arguments.length;
    var baseArg = 0;
    var filename;
    var lineno = 0;
    var count = 1;
    var usingStack = false;
    if(arguments.length && typeof(arguments[0]) == 'string') {
        filename = arguments[0];
    }
    else {
        filename = stack[currentFrame].script.url;
        usingStack = true;
        baseArg = -1;
    }

    if(arguments.length > baseArg+1) {
        lineno = arguments[baseArg+1];
        if(lineno < 0) {
            if(usingStack) {
                lineno = Math.max(0, stack[currentFrame].lineno) + lineno;
            }
            else {
                return print("can't use a negative lineno on explicit url")
            }
        }
        else if(lineno == 0){
            lineno = 1;
            count = Number.MAX_VALUE/2; /* i.e. a really big number */
        }
        if(arguments.length > baseArg+2) {
            count = arguments[baseArg+2];
            if(count < 1)
                count = Number.MAX_VALUE/2; /* i.e. a really big number */
        }
    }
    else if(usingStack) {
        lineno = stack[currentFrame].lineno;
        count = 1;
    }
    else
        count = Number.MAX_VALUE/2; /* i.e. a really big number */

    listWithFilename(filename, lineno, count);
    return "";
}

function listWithFilename(filename, lineno, count)
{
    var handle = jsd.FindSourceForURL(filename);
    if(handle == null)
        return print("unable to find source for "+filename);

    var fulltext = jsd.GetSourceText(handle);
    var re = new RegExp(".*\n", "g");
    var result;
    var i;
    for(var i = 1; i < lineno+count; i++) {
        if(null == (result = re.exec(fulltext)))
            break;
        if(i >= lineno)
            printLineFormatted(result[0].substr(0,result[0].length-1),
                               filename, i);
        if(0 == re.lastIndex)
            break;
    }
    return "";
}

function printLineFormatted(text, filename, lineno)
{
    var lead0;
    var lead1;

    if(hasBreakpoint(filename, lineno))
        lead0 = "*";
    else
        lead0 = " ";

    if(filename == stack[currentFrame].script.url &&
       lineno == stack[currentFrame].lineno)
        lead1 = ">";
    else
        lead1 = " ";

    print(lead0+lead1+lzpad(""+lineno,4)+": "+text);
    return "";
}

function scripts()
{
    return props(JSDScript.scripts);
}

/***************************************************************************/
/* breakpoints */

function Breakpoint(url, line)
{
    this.url = url;
    this.line = line;
    this.toString = function()
                {return "Breakpoint at line "+this.line+" of "+this.url;}
}

// Only create the breakpoint array the first time we are loaded
if(!this.breakpoints) {
    breakpoints = new Array();
    breakpoints[breakpoints.length] = null;
}

function hasBreakpoint(url, line)
{
    for(var i = 0; i < breakpoints.length; i++)
        if(breakpoints[i] &&
           breakpoints[i].line == line &&
           breakpoints[i].url == url)
            return i;
    return 0;
}

function addBreakpoint(url, line)
{
    if(hasBreakpoint(url, line))
        return print("breakpoint already set at line "+line+" of "+url);
    var bp = new Breakpoint(url, line);
    breakpoints[breakpoints.length] = bp;
    jsd.IterateScripts(breakpointManipulatingCallback, url, line, true);
    return "";
}

function clearBreakpoint(url, line)
{
    var which = hasBreakpoint(url, line);
    if(!which)
        return print("no breakpoint set at line "+line+" of "+url);
    jsd.IterateScripts(breakpointManipulatingCallback, url, line, false);
    delete breakpoints[which];
    return "";
}


function breakpointManipulatingCallback(handle, url, lineno, set)
{
    var script = JSDScript.find(handle);
    if(script && script.url == url &&
       script.base <= lineno && script.base+script.extent > lineno)
    {
        var pc = jsd.GetClosestPC(handle,lineno);
        if(jsd.GetClosestLine(handle, pc) == lineno)
        {
//            print(""+(set?"setting":"clearing")+" trap at line "+lineno+" of "+url);
            if(set)
                jsd.SetTrap(handle, pc);
            else
                jsd.ClearTrap(handle, pc);
        }
    }
    return true;
}

function setAllBreakpointsForScript(handle)
{
    for(var i = 0; i < breakpoints.length; i++)
        if(breakpoints[i])
            breakpointManipulatingCallback(handle, breakpoints[i].url,
                                           breakpoints[i].line, true);
    return "";
}

function bp()
{
    for(var i = 0; i < breakpoints.length; i++)
        if(breakpoints[i])
            print(breakpoints[i]);
    return "";
}

function getUrlAndLineFor(args, o)
{
    var filename;
    var lineno = 0;
    var baseArg = 0;
    var usingStack = false;

    if(args.length && typeof(args[0]) == 'string') {
        o.filename = args[0];
    }
    else {
        o.filename = stack[currentFrame].script.url;
        usingStack = true;
        baseArg = -1;
    }
    if(args.length > baseArg+1) {
        o.lineno = args[baseArg+1];
        return true;
    }
    print("need a line number");
    return false;
}

function trap()
{
    o = new Object();
    if(getUrlAndLineFor(arguments, o))
        addBreakpoint(o.filename, o.lineno);
    return "";
}
function untrap()
{
    o = new Object();
    if(getUrlAndLineFor(arguments, o))
        clearBreakpoint(o.filename, o.lineno);
    return "";
}

/***************************************************************************/

function writeln(str)
{
    write(str+"\n");
    return "";
}

function print()
{
    var i;
    var str = "";
    for(var i = 0; i < arguments.length; i++) {
        str += (i!=0? " ":"")+arguments[i];
    }
    if(i && !(arguments.length == 1 &&
              typeof(arguments[0]) == 'string' &&
              arguments[0] == ""))
        str += "\n";
    write(str);
    return "";
}

var evalLoopLineno = 1;
var evalLoopPrompt = "jsdb"+jsd.DebuggerDepth+">";
var evalLoopFilename = "jsdb_evalLoop"+jsd.DebuggerDepth;

function doEvalLoop()
{
    resumeCode = -1;

    while(-1 == resumeCode) {
        write(evalLoopPrompt);
        var str = gets();
        if(str.length)
            print(safeEval(str+"\n", evalLoopFilename, evalLoopLineno++));
    }
    return resumeCode;
}

function prompt(str)
{
    write(str);
    return gets();
}

function abort()
{
    return resume(jsd.JSD_HOOK_RETURN_ABORT);
}

function quit()
{
    return resume(jsd.JSD_HOOK_RETURN_HOOK_ERROR);
}

function returnVal(val)
{
    jsd.ReturnExpression = val;
    resume(jsd.JSD_HOOK_RETURN_RET_WITH_VAL);
    return "";
}

function throwVal(val)
{
    jsd.ReturnExpression = val;
    resume(jsd.JSD_HOOK_RETURN_THROW_WITH_VAL);
    return "";
}


function resume(code)
{
    if(arguments.length &&
       code >= jsd.JSD_HOOK_RETURN_HOOK_ERROR &&
       code <= jsd.JSD_HOOK_RETURN_CONTINUE_THROW) {
        resumeCode = code;
    }
    else {
        if(hookType == jsd.JSD_HOOK_THROW)
            resumeCode = jsd.JSD_HOOK_RETURN_CONTINUE_THROW;
        else
            resumeCode = jsd.JSD_HOOK_RETURN_CONTINUE;
    }
    return "";
}

/***************************************************************************/
/* property and value objects */

function JSDValueProto()
{
    this.RefreshValue          = function() {this.flags = null; jsd.RefreshValue(this.handle);}
    this.IsValueObject         = function() {return jsd.IsValueObject(this.handle);}
    this.IsValueNumber         = function() {return jsd.IsValueNumber(this.handle);}
    this.IsValueInt            = function() {return jsd.IsValueInt(this.handle);}
    this.IsValueDouble         = function() {return jsd.IsValueDouble(this.handle);}
    this.IsValueString         = function() {return jsd.IsValueString(this.handle);}
    this.IsValueBoolean        = function() {return jsd.IsValueBoolean(this.handle);}
    this.IsValueNull           = function() {return jsd.IsValueNull(this.handle);}
    this.IsValueVoid           = function() {return jsd.IsValueVoid(this.handle);}
    this.IsValuePrimitive      = function() {return jsd.IsValuePrimitive(this.handle);}
    this.IsValueFunction       = function() {return jsd.IsValueFunction(this.handle);}
    this.IsValueNative         = function() {return jsd.IsValueNative(this.handle);}
    this.GetValueBoolean       = function() {return jsd.GetValueBoolean(this.handle);}
    this.GetValueInt           = function() {return jsd.GetValueInt(this.handle);}
    this.GetValueDouble        = function() {return jsd.GetValueDouble(this.handle);}
    this.GetValueString        = function() {return jsd.GetValueString(this.handle);}
    this.GetValueFunctionName  = function() {return jsd.GetValueFunctionName(this.handle);}
    this.GetCountOfProperties  = function() {return jsd.GetCountOfProperties(this.handle);}
    this.GetValuePrototype     = function() {return new JSDValue(jsd.GetValuePrototype(this.handle));}
    this.GetValueParent        = function() {return new JSDValue(jsd.GetValueParent(this.handle));}
    this.GetValueConstructor   = function() {return new JSDValue(jsd.GetValueConstructor(this.handle));}
    this.GetValueClassName     = function() {return jsd.GetValueClassName(this.handle);}
    this.GetValueProperty      = function(name) {return new JSDProperty(jsd.GetValueProperty(this.handle, name));}

    this.GetProperties = function() {
        var a = new Array();
        function cb(ob, prop, a)
        {
//            print(new JSDProperty(prop).GetPropertyName().GetValueString());
            a.push(new JSDProperty(prop));
            return true;
        }
        jsd.IterateProperties(this.handle, cb, a);
        return a;
    }

    this.toString = function()
    {
        return this.flagString() + 
               " : " + 
               this.GetValueString();
    }

    this.flags = null;
    this.flagString = function()
    {
        if(!this.flags)
        {
            this.flags = (this.IsValueObject()    ? "o" : ".") +
                         (this.IsValueNumber()    ? "n" : ".") +
                         (this.IsValueInt()       ? "i" : ".") +
                         (this.IsValueDouble()    ? "d" : ".") +
                         (this.IsValueString()    ? "s" : ".") +
                         (this.IsValueBoolean()   ? "b" : ".") +
                         (this.IsValueNull()      ? "N" : ".") +
                         (this.IsValueVoid()      ? "V" : ".") +
                         (this.IsValuePrimitive() ? "p" : ".") +
                         (this.IsValueFunction()  ? "f" : ".") +
                         (this.IsValueNative()    ? "n" : ".");
        }
        return this.flags;
    }
}

function JSDValue(handle)
{
    if(!handle)
        return null;
    this.handle = handle;
}
JSDValue.prototype = new JSDValueProto;

/*********************************************/

function JSDPropertyProto()
{
    this.JSDPD_ENUMERATE = jsd.JSDPD_ENUMERATE;
    this.JSDPD_READONLY  = jsd.JSDPD_READONLY ;
    this.JSDPD_PERMANENT = jsd.JSDPD_PERMANENT;
    this.JSDPD_ALIAS     = jsd.JSDPD_ALIAS    ;
    this.JSDPD_ARGUMENT  = jsd.JSDPD_ARGUMENT ;
    this.JSDPD_VARIABLE  = jsd.JSDPD_VARIABLE ;
    this.JSDPD_HINTED    = jsd.JSDPD_HINTED   ;

    this.GetPropertyName       = function() {return new JSDValue(jsd.GetPropertyName(this.handle));}
    this.GetPropertyValue      = function() {return new JSDValue(jsd.GetPropertyValue(this.handle));}
    this.GetPropertyAlias      = function() {return new JSDValue(jsd.GetPropertyAlias(this.handle));}
    this.GetPropertyFlags      = function() {return jsd.GetPropertyFlags(this.handle);}
    this.GetPropertyVarArgSlot = function() {return jsd.GetPropertyVarArgSlot(this.handle);}

    this.toString = function()
    {
        return this.flagString() + 
               " : " + 
               this.GetPropertyName().toString() +
               " : " + 
               this.GetPropertyValue().toString();
    }
    this.flags = null;
    this.flagString = function()
    {
        var f;
        if(!this.flags)
        {
            f = this.GetPropertyFlags();
            this.flags = (f & jsd.JSDPD_ENUMERATE ? "e" : ".") +
                         (f & jsd.JSDPD_READONLY  ? "r" : ".") +
                         (f & jsd.JSDPD_PERMANENT ? "p" : ".") +
                         (f & jsd.JSDPD_ALIAS     ? "a" : ".") +
                         (f & jsd.JSDPD_ARGUMENT  ? "A" : ".") +
                         (f & jsd.JSDPD_VARIABLE  ? "V" : ".") +
                         (f & jsd.JSDPD_HINTED    ? "H" : ".");
        }
        return this.flags;
    }
}    

function JSDProperty(handle)
{
    if(!handle)
        return null;
    this.handle = handle;
}

JSDProperty.prototype = new JSDPropertyProto;

/***************************************************************************/

function locals()
{
    _localsOrArgs(true);
    return "";
}        

function args()
{
    _localsOrArgs(false);
    return "";
}        

function _localsOrArgs(loc)
{
    var found = 0;
    var frameHandle = stack[currentFrame].handle;
    var callObjHandle = jsd.GetCallObjectForStackFrame(frameHandle);

    if(callObjHandle)
    {
        var callObj = new JSDValue(callObjHandle);
        if(callObj)
        {
            var props = callObj.GetProperties();
            if(props)
            {
                for(var i = 0; i < props.length; i++)
                {
                    var prop = props[i];
                    var doit = loc ? 
                            prop.GetPropertyFlags() & jsd.JSDPD_VARIABLE :
                            prop.GetPropertyFlags() & jsd.JSDPD_ARGUMENT ;
                    if(doit)
                    {
                        var name = prop.GetPropertyName();
                        var val  = prop.GetPropertyValue();
                        print(name.GetValueString()+" : "+val.GetValueString());
                        found++ ;
                    }
                }
            }
        }
    }

    if(!found)
        print(loc ? "no locals" : "no args");
}



/***************************************************************************/
/* User defined... can use load() ! */

function f() {return load('f.js');}

/**
* function f(){
* //    for(var i = 0; i< 1000; i++) {
*     while(1) {
*         why();
*         show('arguments.length');
*         up()
*         show('arguments.length');
*         up()
*         show('arguments.length');
*         up()
*         show('arguments.length');
*         step();
*     }
* }
*/
/***************************************************************************/
/* Signal Success */

print("successfully loaded debugger.js for depth "+jsd.DebuggerDepth);
