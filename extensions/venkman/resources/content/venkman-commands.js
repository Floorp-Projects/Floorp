/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is The JavaScript Debugger
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Robert Ginda, <rginda@netscape.com>, original author
 *
 */

const CMD_CONSOLE    = 0x01; // command is available via the console
const CMD_NEED_STACK = 0x02; // command only works if we're stopped
const CMD_NO_STACK   = 0x04; // command only works if we're *not* stopped
const CMD_NO_HELP    = 0x08; // don't whine if there is no help for this command

function initCommands(commandObject)
{
    console.commandManager = new CommandManager();
    
    CommandManager.contextFunction = getCommandContext;
    
    var cmdary =
        [/* "real" commands */
         ["break",          cmdBreak,              CMD_CONSOLE],
         ["bp-props",       cmdBPProps,            0],
         ["chrome-filter",  cmdChromeFilter,       CMD_CONSOLE],
         ["clear",          cmdClear,              CMD_CONSOLE],
         ["clear-all",      cmdClearAll,           CMD_CONSOLE],
         ["clear-profile",  cmdClearProfile,       CMD_CONSOLE],
         ["clear-script",   cmdClearScript,        0],
         ["close",          cmdClose,              CMD_CONSOLE],
         ["commands",       cmdCommands,           CMD_CONSOLE],
         ["cont",           cmdCont,               CMD_CONSOLE | CMD_NEED_STACK],
         ["emode",          cmdEMode,              CMD_CONSOLE],
         ["eval",           cmdEval,               CMD_CONSOLE | CMD_NEED_STACK],
         ["evald",          cmdEvald,              CMD_CONSOLE],
         ["fbreak",         cmdFBreak,             CMD_CONSOLE],
         ["find-bp",        cmdFindBp,             0],
         ["find-creator",   cmdFindCreatorOrCtor,  0],
         ["find-ctor",      cmdFindCreatorOrCtor,  0],
         ["find-file",      cmdFindFile,           CMD_CONSOLE],
         ["find-frame",     cmdFindFrame,          CMD_NEED_STACK],
         ["find-url",       cmdFindURL,            CMD_CONSOLE],
         ["find-url-soft",  cmdFindURL,            0],
         ["find-script",    cmdFindScript,         0],
         ["finish",         cmdFinish,             CMD_CONSOLE | CMD_NEED_STACK],
         ["focus-input",    cmdFocusInput,         0],
         ["frame",          cmdFrame,              CMD_CONSOLE | CMD_NEED_STACK],
         ["help",           cmdHelp,               CMD_CONSOLE],
         ["loadd",          cmdLoadd,              CMD_CONSOLE],
         ["next",           cmdNext,               CMD_CONSOLE | CMD_NEED_STACK],
         ["open-dialog",    cmdOpenDialog,         CMD_CONSOLE],
         ["open-url",       cmdOpenURL,            0],
         ["pprint",         cmdPPrint,             CMD_CONSOLE],
         ["pref",           cmdPref,               CMD_CONSOLE],
         ["profile",        cmdProfile,            CMD_CONSOLE],
         ["props",          cmdProps,              CMD_CONSOLE | CMD_NEED_STACK],
         ["propsd",         cmdPropsd,             CMD_CONSOLE],
         ["quit",           cmdQuit,               CMD_CONSOLE],
         ["reload",         cmdReload,             CMD_CONSOLE],
         ["save-source",    cmdSaveSource,         CMD_CONSOLE],
         ["save-profile",   cmdSaveProfile,        CMD_CONSOLE],
         ["scope",          cmdScope,              CMD_CONSOLE | CMD_NEED_STACK],
         ["startup-init",   cmdStartupInit,        CMD_CONSOLE],
         ["step",           cmdStep,               CMD_CONSOLE | CMD_NEED_STACK],
         ["stop",           cmdStop,               CMD_CONSOLE | CMD_NO_STACK],
         ["tmode",          cmdTMode,              CMD_CONSOLE],
         ["version",        cmdVersion,            CMD_CONSOLE],
         ["where",          cmdWhere,              CMD_CONSOLE | CMD_NEED_STACK],
         
         /* aliases */
         ["profile-tb",     "profile toggle",       CMD_CONSOLE],
         ["this",           "props this",           CMD_CONSOLE],
         ["toggle-chrome",  "chrome-filter toggle", 0],
         ["toggle-ias",     "startup-init toggle",  0],
         ["toggle-profile", "profile toggle",       0],
         ["em-cycle",       "emode cycle",          0],
         ["em-ignore",      "emode ignore",         0],
         ["em-trace",       "emode trace",          0],
         ["em-break",       "emode break",          0],
         ["tm-cycle",       "tmode cycle",          0],
         ["tm-ignore",      "tmode ignore",         0],
         ["tm-trace",       "tmode trace",          0],
         ["tm-break",       "tmode break",          0]
        ];

    defineVenkmanCommands (cmdary);

    console.commandManager.argTypes.__aliasTypes__ (["index", "breakpointIndex",
                                                     "lineNumber"], "int");
    console.commandManager.argTypes.__aliasTypes__ (["scriptText", "windowFlags",
                                                     "expression", "prefValue"],
                                                    "rest");
}

/**
 * Defines commands stored in |cmdary| on the venkman command manager.  Expects
 * to find "properly named" strings for the commands via getMsg().
 */
function defineVenkmanCommands (cmdary)
{
    var cm = console.commandManager;
    var len = cmdary.length;
    for (var i = 0; i < len; ++i)
    {
        var name  = cmdary[i][0];
        var func  = cmdary[i][1];
        var flags = cmdary[i][2];
        var usage = getMsg("cmd." + name + ".params", null, "");

        var helpDefault;
        var labelDefault = name;
        var aliasFor;
        if (flags & CMD_NO_HELP)
            helpDefault = MSG_NO_HELP;

        if (typeof func == "string")
        {
            var ary = func.match(/(\S+)/);
            if (ary)
                aliasFor = ary[1];
            helpDefault = getMsg (MSN_DEFAULT_ALIAS_HELP, func); 
            labelDefault = getMsg ("cmd." + aliasFor + ".label", null, name);
        }

        var label = getMsg("cmd." + name + ".label", null, labelDefault);
        var help  = getMsg("cmd." + name + ".help", null, helpDefault);
        var command = new CommandRecord (name, func, usage, help, label, flags);
        cm.addCommand(command);

        var key = getMsg ("cmd." + name + ".key", null, "");
        if (key)
            cm.setKey("dynamicKeys", command, key);
    }
}

/**
 * Used as CommandManager.contextFunction.
 */
function getCommandContext (id, cx)
{
    switch (id)
    {
        case "popup:project":
            cx = console.projectView.getContext(cx);
            break;

        case "popup:source":
            cx = console.sourceView.getContext(cx);
            break;
            
        case "popup:script":
            cx = console.scriptsView.getContext(cx);
            break;
            
        case "popup:stack":
            cx = console.stackView.getContext(cx);
            break;

        default:
            dd ("getCommandContext: unknown id '" + id + "'");

        case "mainmenu:file-popup":
        case "mainmenu:view-popup":
        case "mainmenu:debug-popup":            
        case "mainmenu:profile-popup":
        case "popup:console":
            cx = {
                commandManager: console.commandManager,
                contextSource: "default"
            };
            break;
    }

    if (typeof cx == "object")
    {
        cx.commandManager = console.commandManager;
        if (!("contextSource" in cx))
            cx.contextSource = id;
        if ("dbgContexts" in console && console.dbgContexts)
            dd ("context '" + id + "'\n" + dumpObjectTree(cx));
    }

    return cx;
}

/**
 * Used as a callback for CommandRecord.getDocumentation() to format the command
 * flags for the "Notes:" field.
 */
function formatCommandFlags (f)
{
    var ary = new Array();
    if (f & CMD_CONSOLE)
        ary.push(MSG_NOTE_CONSOLE);
    if (f & CMD_NEED_STACK)
        ary.push(MSG_NOTE_NEEDSTACK);
    if (f & CMD_NO_STACK)
        ary.push(MSG_NOTE_NOSTACK);
    
    return ary.length ? ary.join ("\n") : MSG_VAL_NA;
}

/********************************************************************************
 * Command implementations from here on down...
 *******************************************************************************/
    
function cmdBreak (e)
{    
    var i;
    var bpr;
    
    if (!e.fileName)
    {  /* if no input data, just list the breakpoints */
        var bplist = console.breakpoints.childData;

        if (bplist.length == 0)
        {
            display (MSG_NO_BREAKPOINTS_SET);
            return true;
        }
        
        display (getMsg(MSN_BP_HEADER, bplist.length));
        for (i = 0; i < bplist.length; ++i)
        {
            bpr = bplist[i];
            feedback (e, getMsg(MSN_BP_LINE, [i, bpr.fileName, bpr.line,
                                              bpr.scriptMatches]));
        }
        return true;
    }

    var matchingFiles = matchFileName (e.fileName);
    if (matchingFiles.length == 0)
    {
        feedback (e, getMsg(MSN_ERR_BP_NOSCRIPT, e.fileName), MT_ERROR);
        return false;
    }
    
    for (i in matchingFiles)
    {
        bpr = setBreakpoint (matchingFiles[i], e.lineNumber);
        if (bpr)
            feedback (e, getMsg(MSN_BP_CREATED, [bpr.fileName, bpr.line,
                                                 bpr.scriptMatches]));
    }
    
    return true;
}

function cmdBPProps (e)
{
    dd ("command bp-props");
}

function cmdChromeFilter (e)
{
    var currentState = console.prefs["enableChromeFilter"];
    
    if (e.toggle != null)
    {
        if (e.toggle == "toggle")
            e.toggle = !currentState;

        if (e.toggle != currentState)
        {
            if (e.toggle)
                console.jsds.insertFilter (console.chromeFilter, null);
            else
                console.jsds.removeFilter (console.chromeFilter);
        }
        
        console.scriptsView.freeze();
        for (var container in console.scripts)
        {
            if (console.scripts[container].fileName.indexOf("chrome:") == 0)
            {
                var rec = console.scripts[container];
                var scriptList = console.scriptsView.childData;
                if (e.toggle)
                {
                    /* filter is on, remove chrome file from scripts view */
                    if ("parentRecord" in rec)
                    {
                        //dd ("remove index " + rec.childIndex + ", " +
                        //    rec.fileName);
                        scriptList.removeChildAtIndex(rec.childIndex);
                    }
                    else
                        dd ("record already seems to out of the tree");
                }
                else
                {
                    /* filter is off, add chrome file to scripts view */
                    if (!("parentRecord" in rec))
                    {
                        //dd ("cmdChromeFilter: append " +
                        //    tov_formatRecord(rec, ""));
                        scriptList.appendChild(rec);
                    }
                    else
                        dd ("record already seems to be in the tree");
                }
            }
        }
        console.scriptsView.thaw();

        currentState = 
            console.enableChromeFilter = 
            console.prefs["enableChromeFilter"] = e.toggle;
    }

    feedback (e, getMsg(MSN_CHROME_FILTER,
                        currentState ? MSG_VAL_ON : MSG_VAL_OFF));
}

function cmdClear (e)
{
    if ("breakpointIndexList" in e && e.breakpointIndexList.length > 0)
    {
        var ev = new Object();
        if ("isInteractive" in e)
            ev.isInteractive = true;
        e.breakpointIndexList.sort();
        for (var i = e.breakpointIndexList.length - 1; i >= 0 ; --i)
        {
            ev.breakpointIndex = e.breakpointIndexList[i];
            cmdClear(ev);
        }
        return;
    }
            
    var bpr = clearBreakpointByNumber (e.breakpointIndex);
    if (bpr)
        feedback (e, getMsg(MSN_BP_CLEARED, [bpr.fileName, bpr.line,
                                             bpr.scriptMatches]));
}

function cmdClearAll(e)
{
    for (var i = console.breakpoints.childData.length - 1; i >= 0; --i)
        clearBreakpointByNumber (i);
}

function cmdClearProfile(e)
{
    if ("scriptRecList" in e)
    {
        for (var i = 0; i < e.scriptRecList.length; ++i)
            e.scriptRecList[i].script.clearProfileData();
    }
    else if ("scriptRec" in e)
    {
        e.scriptRec.script.clearProfileData();
    }
    else
    {
        console.jsds.clearProfileData();
    }
    
    feedback (e, MSG_PROFILE_CLEARED);
}

function cmdClearScript (e)
{
    var i;
    
    if ("scriptRecList" in e)
    {
        for (i = 0; i < e.scriptRecList.length; ++i)
            cmdClearScript ({scriptRec: e.scriptRecList[i]});
        return true;
    }

    /* walk backwards so as not to disturb the indicies */
    for (i = console.breakpoints.childData.length - 1; i >= 0; --i)
    {
        var bpr = console.breakpoints.childData[i];
        if (bpr.hasScriptRecord(e.scriptRec))
            clearBreakpointByNumber (i);
    }

    return true;
}

function cmdClose()
{
    window.close();
}

function cmdCommands (e)
{
    display (MSG_TIP_HELP);
    var names = console.commandManager.listNames(e.pattern, CMD_CONSOLE);
    names = names.join(MSG_COMMASP);
    
    if (e.pattern)
        display (getMsg(MSN_CMDMATCH, [e.pattern, "["  + names + "]"]));
    else
        display (getMsg(MSN_CMDMATCH_ALL, "[" + names + "]"));
    return true;
}

function cmdCont (e)
{
    disableDebugCommands();
    console.stackView.saveState();
    console.jsds.exitNestedEventLoop();
}

function cmdEMode (e)
{    
    if (e.mode != null)
    {
        e.mode = e.mode.toLowerCase();

        if (e.mode == "cycle")
        {
            switch (console.errorMode)
            {
                case EMODE_IGNORE:
                    e.mode = "trace";
                    break;
                case EMODE_TRACE:
                    e.mode = "break";
                    break;
                case EMODE_BREAK:
                    e.mode = "ignore";
                    break;
            }
        }

        switch (e.mode)
        {
            case "ignore":
                console.errorMode = EMODE_IGNORE;
                break;
            case "trace": 
                console.errorMode = EMODE_TRACE;
                break;
            case "break":
                console.errorMode = EMODE_BREAK;
                break;
            default:
                display (getMsg(MSN_ERR_INVALID_PARAM, ["mode", e.mode]),
                         MT_ERROR);
                return false;
        }
    }
    
    switch (console.errorMode)
    {
        case EMODE_IGNORE:
            display (MSG_EMODE_IGNORE);
            break;
        case EMODE_TRACE:
            display (MSG_EMODE_TRACE);
            break;
        case EMODE_BREAK:
            display (MSG_EMODE_BREAK);
            break;
    }

    return true;
}

function cmdEval (e)
{
    display (e.scriptText, MT_FEVAL_IN);
    var rv = evalInTargetScope (e.scriptText);
    if (typeof rv != "undefined")
    {
        if (rv != null)
        {
            refreshValues();
            var l = $.length;
            $[l] = rv;
            
            display (getMsg(MSN_FMT_TMP_ASSIGN, [l, formatValue (rv)]),
                     MT_FEVAL_OUT);
        }
        else
            dd ("evalInTargetScope returned null");
    }
    return true;
}

function cmdEvald (e)
{
    display (e.scriptText, MT_EVAL_IN);
    var rv = evalInDebuggerScope (e.scriptText);
    if (typeof rv != "undefined")
        display (String(rv), MT_EVAL_OUT);
    return true;
}

function cmdFBreak(e)
{
    if (!e.fileName)
    {  /* if no input data, just list the breakpoints */
        var bplist = console.breakpoints.childData;

        if (bplist.length == 0)
        {
            display (MSG_NO_FBREAKS_SET);
            return true;
        }
        
        display (getMsg(MSN_FBP_HEADER, bplist.length));
        for (var i = 0; i < bplist.length; ++i)
        {
            var bpr = bplist[i];
            if (bpr.scriptMatches == 0)
                display (getMsg(MSN_FBP_LINE, [i, bpr.fileName, bpr.line]));
        }
        return true;
    }

    if (setFutureBreakpoint (e.fileName, e.lineNumber))
        feedback (e, getMsg(MSN_FBP_CREATED, [e.fileName, e.lineNumber]));
    return true;
}

function cmdFinish (e)
{
    if (console.frames.length == 1)
        return cmdCont();
    
    console._stepOverLevel = 1;
    setStopState(false);
    console.jsds.functionHook = console._callHook;
    disableDebugCommands()
    console.stackView.saveState();
    console.jsds.exitNestedEventLoop();
    return true;
}

function cmdFindBp (e)
{    
    return dispatch ("find-url", {url: e.breakpointRec.fileName,
                                  rangeStart: e.breakpointRec.line,
                                  rangeEnd: e.breakpointRec.line});
}

function cmdFindCreatorOrCtor (e)
{
    var objVal = e.jsdValue.objectValue;
    if (!objVal)
    {
        display (MSG_NOT_AN_OBJECT, MT_ERROR);
        return false;
    }

    var name = e.command.name;
    var url;
    var line;
    if (name == "find-creator")
    {
        url  = objVal.creatorURL;
        line = objVal.creatorLine;
    }
    else
    {
        url  = objVal.constructorURL;
        line = objVal.constructorLine;
    }

    return dispatch ("find-url",
                     {url: url, rangeStart: line - 1, rangeEnd: line - 1});
}

function cmdFindURL (e)
{
    if (!e.url)
    {
        console.sourceView.displaySourceText(null);
        return null;
    }
    
    var sourceText;
    
    if (e.url.indexOf("x-jsd:") == 0)
    {
        if (e.url == "x-jsd:help")
        {
            dispatch ("help");
            return e.url;
        }

        display (getMsg(MSN_ERR_INVALID_PARAM, ["url", e.url]), MT_ERROR);
        return null;
    }
    
    if (e.url in console.scripts)
        sourceText = console.scripts[e.url].sourceText;
    else if (e.url in console.files)
        sourceText = console.files[e.url];
    else
        sourceText = console.files[e.url] = new SourceText (null, e.url);

    console.sourceView.details = null;
    console.highlightFile = e.url;
    var line = 1;
    delete console.highlightStart;
    delete console.highlightEnd;
    if ("rangeStart" in e && e.rangeStart != null)
    {
        line = e.rangeStart;
        if (e.rangeEnd != null)
        {
            console.highlightStart = e.rangeStart - 1;
            console.highlightEnd = e.rangeEnd - 1;
        }
    }
    
    if ("lineNumber" in e && e.lineNumber != null)
        line = e.lineNumber;

    console.sourceView.displaySourceText(sourceText);
    console.sourceView.tree.invalidate();
    if ("command" in e && e.command.name == "find-url-soft")
        console.sourceView.softScrollTo (line);
    else
        console.sourceView.scrollTo (line - 2, -1);
    return e.url;
}

function cmdFindFile (e)
{
    if (!e.fileName || e.fileName == "?")
    {
        var rv = pickOpen(MSG_OPEN_FILE, "$all");
        if (rv.reason == PICK_CANCEL)
            return null;
        e.fileName = rv.file;
    }
    
    return dispatch ("find-url", {url: getURLSpecFromFile (e.fileName)});
}

function cmdFindFrame (e)
{
    var frame = e.frameRec.frame;

    displayFrame (frame, e.frameRec.childIndex, true);

    if (frame.isNative)
        return true;
    
    var scriptContainer = console.scripts[frame.script.fileName];
    if (!scriptContainer)
    {
        dd ("frame from unknown source");
        return false;
    }
    var scriptRecord =
        scriptContainer.locateChildByScript (frame.script);
    if (!scriptRecord)
    {
        dd ("frame with unknown script");
        return false;
    }

    return cmdFindScript ({scriptRec: scriptRecord});
}

function cmdFindScript (e)
{
    var rv;

    if (console.sourceView.prettyPrint)
    {
        delete console.highlightFile;
        delete console.highlightStart;
        delete console.highlightEnd;
        console.sourceView.displaySourceText(e.scriptRec.sourceText);
        rv = e.scriptRec.script.fileName;
    }
    else
    {
        rv = dispatch("find-url", {url: e.scriptRec.parentRecord.fileName,
                                   rangeStart: e.scriptRec.baseLineNumber,
                                   rangeEnd: e.scriptRec.baseLineNumber + 
                                             e.scriptRec.lineExtent - 1});
    }

    console.sourceView.details = e.scriptRec;
    
    return rv;
}

function cmdFocusInput (e)
{
    console.ui["sl-input"].focus();
}

function cmdFrame (e)
{
    if (e.frameIndex != null)
        setCurrentFrameByIndex(e.frameIndex);
    else    
        e.frameIndex = getCurrentFrameIndex();

    dispatch ("find-frame",
              {frameRec: console.stackView.stack.childData[e.frameIndex]});
    return true;
}
            
function cmdHelp (e)
{
    var ary;
    if (!e.pattern)
    {
        console.sourceView.displaySourceText(console.sourceText);
    }
    else
    {
        ary = console.commandManager.list (e.pattern, CMD_CONSOLE);
 
        if (ary.length == 0)
        {
            display (getMsg(MSN_ERR_NO_COMMAND, e.pattern), MT_ERROR);
            return false;
        }

        for (var i in ary)
        {        
            display (getMsg(MSN_FMT_USAGE, [ary[i].name, ary[i].usage]), 
                     MT_USAGE);
            display (ary[i].help, MT_HELP);
        }
    }

    return true;    
}

function cmdLoadd (e)
{
    var ex;
    
    if (!("_loader" in console))
    {
        const LOADER_CTRID = "@mozilla.org/moz/jssubscript-loader;1";
        const mozIJSSubScriptLoader = 
            Components.interfaces.mozIJSSubScriptLoader;

        var cls;
        if ((cls = Components.classes[LOADER_CTRID]))
            console._loader = cls.createInstance(mozIJSSubScriptLoader);
    }
    
    var obj = ("scope" in e) ? e.scope : null;
    try
    {
        var rvStr;
        var rv = rvStr = console._loader.loadSubScript(e.url, obj);
        if (typeof rv == "function")
            rvStr = MSG_TYPE_FUNCTION;
        display(getMsg(MSN_SUBSCRIPT_LOADED, [e.url, rvStr]), MT_INFO);
        return rv;
    }
    catch (ex)
    {
        display (getMsg(MSN_ERR_SCRIPTLOAD, e.url));
        display (formatException(ex), MT_ERROR);
    }
    
    return null;
}

function cmdNext ()
{
    console._stepOverLevel = 0;
    dispatch ("step");
    console.jsds.functionHook = console._callHook;
    return true;
}

function cmdOpenDialog (e)
{
    return openDialog (e.url, e.windowName, e.windowFlags);
}

function cmdOpenURL (e)
{
    var url = prompt (MSG_OPEN_URL, "http://");
    if (url)
        return dispatch ("find-url",{url: url});

    return null;
}    

function cmdPPrint (e)
{
    setPrettyPrintState(!console.sourceView.prettyPrint);
    return true;
}

function cmdPref (e)
{
    if (e.prefName)
    {
        if (e.prefName[0] == "-")
        {
            console.prefs.prefBranch.clearUserPref(e.prefName.substr(1));
            return true;
        }
        
        if (!(e.prefName in console.prefs))
        {
            display (getMsg(MSN_ERR_INVALID_PARAM, ["prefName", e.prefName]),
                     MT_ERROR);
            return false;
        }
        
        if (e.prefValue)
            console.prefs[e.prefName] = e.prefValue;
        else
            e.prefValue = console.prefs[e.prefName];

        display (getMsg(MSN_FMT_PREFVALUE, [e.prefName, e.prefValue]));
    }
    else
    {
        for (var i in console.prefs.prefNames)
        {
            var name = console.prefs.prefNames[i];
            display (getMsg(MSN_FMT_PREFVALUE, [name, console.prefs[name]]));
        }
    }

    return true;
}

function cmdProfile(e)
{
    if (e.toggle && e.toggle == "toggle")
        e.toggle = !(console.jsds.flags & COLLECT_PROFILE_DATA);
    
    if (e.toggle != null)
        setProfileState(e.toggle);
    else
        e.toggle = console.jsds.flags & COLLECT_PROFILE_DATA;
    
    feedback (e, getMsg(MSN_PROFILE_STATE,
                        [e.toggle ? MSG_VAL_ON : MSG_VAL_OFF]));
}

function cmdProps (e, forceDebuggerScope)
{
    var v;
    
    if (forceDebuggerScope)
        v = evalInDebuggerScope (e.scriptText);
    else
        v = evalInTargetScope (e.scriptText);
    
    if (!(v instanceof jsdIValue) || v.jsType != jsdIValue.TYPE_OBJECT)
    {
        var str = (v instanceof jsdIValue) ? formatValue(v) : String(v)
        display (getMsg(MSN_ERR_INVALID_PARAM, [MSG_VAL_EXPR, str]),
                 MT_ERROR);
        return false;
    }
    
    display (getMsg(forceDebuggerScope ? MSN_PROPSD_HEADER : MSN_PROPS_HEADER,
                    e.scriptText));
    displayProperties(v);
    return true;
}

function cmdPropsd (e)
{
    return cmdProps (e, true);
}

function cmdQuit()
{
    goQuitApplication();
}

function cmdReload ()
{
    if (!("childData" in console.sourceView))
        return false;
    
    console.sourceView.childData.reloadSource();
    return true;
}

function cmdSaveSource (e)
{
    if (!e.targetFile || e.targetFile == "?")
    {
        var fileName = console.sourceView.childData.fileName;
        if (fileName.search(/^\w+:/) < 0)
        {
            var shortName = getFileFromPath(fileName);
            if (fileName != shortName)
                fileName = shortName;
            else
                fileName = "";
        }
        else
            fileName = "";

        var rv = pickSaveAs(MSG_SAVE_SOURCE, "$all $text *.js", fileName);
        if (rv.reason == PICK_CANCEL)
            return null;
        e.targetFile = rv.file;
    }

    var file = fopen (e.targetFile, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE);
    var lines = console.sourceView.childData.lines;
    for (var i = 0; i < lines.length; ++i)
        file.write(lines[i] + "\n");
    file.close();

    return file.localFile.path;
}

function cmdSaveProfile (e)
{
    function writeHeaderCSV(file)
    {
        file.write ("# path, file, function, start-line, end-line, " +
                    "call-count, recurse-depth, total-time, min-time, " +
                    "max-time, avg-time\n");
    };
    
    function writeSummaryCSV(file, summary)
    {
        for (var i = 0; i < summary.length; ++i)
        {
            var r = summary[i];
            file.write (r.path + ", " + r.file + ", " + r.fun + ", " + r.base +
                        ", " + r.end + ", " + r.ccount + ", " + r.recurse +
                        ", " + r.total + ", " + r.min + ", " + r.max +
                        ", " + r.avg +"\n");
        }
    };
        
    function writeSummaryText(file, summary, fileName)
    {
        file.write ((fileName ? fileName : "** all files **") + "\n");
        for (var i = 0; i < summary.length; ++i)
            file.write ("\t" + summary[i].str + "\n");
    };
    
    if (!e.targetFile || e.targetFile == "?")
    {
        var rv = pickSaveAs(MSG_SAVE_PROFILE, "$html *.csv $text $all");
        if (rv.reason == PICK_CANCEL)
            return null;
        e.targetFile = rv.file;
    }
    
    var file = fopen (e.targetFile, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE);
    var ary = file.localFile.path.match(/(csv|html?)$/i);
    var writeHeader = null;
    var writeSummary = writeSummaryText;
    var writeFooter = null;
    if (ary)
    {
        var ext = ary[1].toLowerCase();
        if (ext == "csv")
        {
            writeHeader = writeHeaderCSV;
            writeSummary = writeSummaryCSV;
        }
        else
        {
            writeHeader = writeHeaderHTML;
            writeSummary = writeSummaryHTML;
            writeFooter = writeFooterHTML;
        }
    }
    
    if (writeHeader)
        writeHeader(file);

    var i;
    
    if ("urlList" in e && e.urlList.length > 0 ||
        "url" in e && e.url && (e.urlList = [e.url]))
    {
        for (i = 0; i < e.urlList.length; ++i)
            writeSummary(file, console.getProfileSummary(e.urlList[i]),
                         e.urlList[i]);
    }
    else
    {
        var scriptURLs = keys(console.scripts).sort();
        for (i = 0; i < scriptURLs.length; ++i)
        {
            var fileName = console.scripts[scriptURLs[i]].fileName;
            writeSummary(file, console.getProfileSummary(fileName), fileName);
        }
    }

    if (writeFooter)
        writeFooter(file);

    display (getMsg(MSN_PROFILE_SAVED, getURLSpecFromFile(file.localFile)));
    file.close();
    return file.localFile;
}


function cmdScope ()
{
    if (getCurrentFrame().scope.propertyCount == 0)
        display (getMsg (MSN_NO_PROPERTIES, MSG_WORD_SCOPE));
    else
        displayProperties (getCurrentFrame().scope);
    
    return true;
}

function cmdStartupInit (e)
{
    if (e.toggle != null)
    {
        if (e.toggle == "toggle")
            console.jsds.initAtStartup = !console.jsds.initAtStartup;
        else
            console.jsds.initAtStartup = e.toggle;
    }

    display (getMsg(MSN_IASMODE,
                    console.jsds.initAtStartup ? MSG_VAL_ON : MSG_VAL_OFF));
    return true;
}

function cmdStep()
{
    setStopState(true);
    var topFrame = console.frames[0];
    console._stepPast = topFrame.script.fileName;
    if (console.sourceView.prettyPrint)
    {
        console._stepPast +=
            topFrame.script.pcToLine(topFrame.pc, PCMAP_PRETTYPRINT);
    }
    else
    {
        console._stepPast += topFrame.line;
    }
    disableDebugCommands()
    console.stackView.saveState();
    console.jsds.exitNestedEventLoop();
    return true;
}

function cmdStop (e)
{
    if (console.jsds.interruptHook)
        setStopState(false);
    else
        setStopState(true);
}

function cmdTMode (e)
{
    if (e.mode != null)
    {
        e.mode = e.mode.toLowerCase();

        if (e.mode == "cycle")
        {
            switch (console.throwMode)
            {
                case TMODE_IGNORE:
                    e.mode = "trace";
                    break;
                case TMODE_TRACE:
                    e.mode = "break";
                    break;
                case TMODE_BREAK:
                    e.mode = "ignore";
                    break;
            }
        }

        switch (e.mode.toLowerCase())
        {
            case "ignore":
                console.jsds.throwHook = null;
                console.throwMode = TMODE_IGNORE;
                break;
            case "trace": 
                console.jsds.throwHook = console._executionHook;
                console.throwMode = TMODE_TRACE;
                break;
            case "break":
                console.jsds.throwHook = console._executionHook;
                console.throwMode = TMODE_BREAK;
                break;
            default:
                display (getMsg(MSN_ERR_INVALID_PARAM, ["mode", e.mode]),
                         MT_ERROR);
                return false;
        }
    }
    
    switch (console.throwMode)
    {
        case EMODE_IGNORE:
            display (MSG_TMODE_IGNORE);
            break;
        case EMODE_TRACE:
            display (MSG_TMODE_TRACE);
            break;
        case EMODE_BREAK:
            display (MSG_TMODE_BREAK);
            break;
    }

    return true;
}

function cmdVersion ()
{
    display(MSG_HELLO, MT_HELLO);
    display(getMsg(MSN_VERSION, console.version), MT_HELLO);
}

function cmdWhere ()
{
    displayCallStack();
    return true;
}

