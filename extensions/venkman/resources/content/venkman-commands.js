/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is The JavaScript Debugger.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
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

const CMD_CONSOLE    = 0x01; // command is available via the console
const CMD_NEED_STACK = 0x02; // command only works if we're stopped
const CMD_NO_STACK   = 0x04; // command only works if we're *not* stopped
const CMD_NO_HELP    = 0x08; // don't whine if there is no help for this command

function initCommands()
{
    console.commandManager = new CommandManager(console.defaultBundle);
    
    var cmdary =
        [/* "real" commands */
         ["about-mozilla",  cmdAboutMozilla,                                 0],
         ["break",          cmdBreak,                              CMD_CONSOLE],
         ["break-props",    cmdBreakProps,                         CMD_CONSOLE],
         ["change-container", cmdChangeContainer,                  CMD_CONSOLE],
         ["change-value",   cmdChangeValue,                                  0],
         ["chrome-filter",  cmdChromeFilter,                       CMD_CONSOLE],
         ["clear",          cmdClear,                              CMD_CONSOLE],
         ["clear-all",      cmdClearAll,                           CMD_CONSOLE],
         ["clear-break",    cmdClearBreak,                                   0],
         ["clear-fbreak",   cmdClearFBreak,                                  0],
         ["clear-profile",  cmdClearProfile,                       CMD_CONSOLE],
         ["clear-session",  cmdHook,                               CMD_CONSOLE],
         ["clear-script",   cmdClearScript,                                  0],
         ["clear-instance", cmdClearInstance,                                0],
         ["close",          cmdClose,                              CMD_CONSOLE],
         ["commands",       cmdCommands,                           CMD_CONSOLE],
         ["cont",           cmdCont,              CMD_CONSOLE | CMD_NEED_STACK],
         ["debug-script",   cmdSetScriptFlag,                                0],
         ["debug-instance-on",  cmdToggleSomething,                          0],
         ["debug-instance-off", cmdToggleSomething,                          0],
         ["debug-instance",     cmdSetScriptFlag,                            0],
         ["debug-transient",    cmdSetTransientFlag,                         0],
         ["emode",          cmdEMode,                              CMD_CONSOLE],
         ["eval",           cmdEval,                               CMD_CONSOLE],
         ["evald",          cmdEvald,                              CMD_CONSOLE],
         ["fbreak",         cmdFBreak,                             CMD_CONSOLE],
         ["set-eval-obj",   cmdSetEvalObj,                                   0],
         ["set-break",      cmdBreak,                                        0],
         ["set-fbreak",     cmdFBreak,                                       0],
         ["fclear",         cmdFClear,                             CMD_CONSOLE],
         ["fclear-all",     cmdFClearAll,                          CMD_CONSOLE],
         ["find-bp",        cmdFindBp,                                       0],
         ["find-creator",   cmdFindCreatorOrCtor,                            0],
         ["find-ctor",      cmdFindCreatorOrCtor,                            0],
         ["find-file",      cmdFindFile,                           CMD_CONSOLE],
         ["find-frame",     cmdFindFrame,                       CMD_NEED_STACK],
         ["find-sourcetext",      cmdFindSourceText,                         0],
         ["find-sourcetext-soft", cmdFindSourceText,                         0],
         ["find-script",          cmdFindScript,                             0],
         ["find-scriptinstance",  cmdFindScriptInstance,                     0],
         ["find-url",       cmdFindURL,                            CMD_CONSOLE],
         ["find-url-soft",  cmdFindURL,                                      0],
         ["finish",         cmdFinish,            CMD_CONSOLE | CMD_NEED_STACK],
         ["focus-input",    cmdHook,                                         0],
         ["frame",          cmdFrame,             CMD_CONSOLE | CMD_NEED_STACK],
         ["gc",             cmdGC,                                 CMD_CONSOLE],
         ["help",           cmdHelp,                               CMD_CONSOLE],
         ["loadd",          cmdLoadd,                              CMD_CONSOLE],
         ["move-view",      cmdMoveView,                           CMD_CONSOLE],
         ["mozilla-help",   cmdMozillaHelp,                                  0],
         ["next",           cmdNext,              CMD_CONSOLE | CMD_NEED_STACK],
         ["open-dialog",    cmdOpenDialog,                         CMD_CONSOLE],
         ["open-url",       cmdOpenURL,                                      0],
         ["pprint",         cmdPPrint,                             CMD_CONSOLE],
         ["pref",           cmdPref,                               CMD_CONSOLE],
         ["profile",        cmdProfile,                            CMD_CONSOLE],
         ["profile-script",       cmdSetScriptFlag,                          0],
         ["profile-instance",     cmdSetScriptFlag,                          0],
         ["profile-instance-on",  cmdSetScriptFlag,                          0],
         ["profile-instance-off", cmdSetScriptFlag,                          0],
         ["props",          cmdProps,                              CMD_CONSOLE],
         ["propsd",         cmdProps,                              CMD_CONSOLE],
         ["quit",           cmdQuit,                               CMD_CONSOLE],
         ["restore-layout", cmdRestoreLayout,                      CMD_CONSOLE],
         ["restore-settings", cmdRestoreSettings,                  CMD_CONSOLE],
         ["release-notes",  cmdReleaseNotes,                                 0],
         ["run-to",         cmdRunTo,                           CMD_NEED_STACK],
         ["save-breakpoints", cmdSaveBreakpoints,                  CMD_CONSOLE],
         ["save-layout",    cmdSaveLayout,                         CMD_CONSOLE],
         ["save-profile",   cmdSaveProfile,                        CMD_CONSOLE],
         ["save-settings",  cmdSaveSettings,                       CMD_CONSOLE],
         ["scan-source",    cmdScanSource,                                   0],
         ["scope",          cmdScope,             CMD_CONSOLE | CMD_NEED_STACK],
         ["this-expr",      cmdThisExpr,                           CMD_CONSOLE],
         ["toggle-float",   cmdToggleFloat,                        CMD_CONSOLE],
         ["toggle-view",    cmdToggleView,                         CMD_CONSOLE],
         ["toggle-pref",    cmdTogglePref,                         CMD_CONSOLE],
         ["startup-init",   cmdStartupInit,                        CMD_CONSOLE],
         ["step",           cmdStep,              CMD_CONSOLE | CMD_NEED_STACK],
         ["stop",           cmdStop,                CMD_CONSOLE | CMD_NO_STACK],
         ["tmode",          cmdTMode,                              CMD_CONSOLE],
         ["version",        cmdVersion,                            CMD_CONSOLE],
         ["where",          cmdWhere,             CMD_CONSOLE | CMD_NEED_STACK],
         
         /* aliases */
         ["exit",                     "quit",                                0],
         ["save-default-layout",      "save-layout default",                 0],
         ["profile-tb",               "profile toggle",                      0],
         ["this",                     "props this",                CMD_CONSOLE],
         ["toggle-chrome",            "chrome-filter toggle",                0],
         ["toggle-ias",               "startup-init toggle",                 0],
         ["toggle-pprint",            "pprint toggle",                       0],
         ["toggle-profile",           "profile toggle",                      0],
         ["toggle-save-layout",       "toggle-pref saveLayoutOnExit",        0],
         ["toggle-save-settings",     "toggle-pref saveSettingsOnExit",      0],
         ["em-cycle",                 "emode cycle",                         0],
         ["em-ignore",                "emode ignore",                        0],
         ["em-trace",                 "emode trace",                         0],
         ["em-break",                 "emode break",                         0],
         ["tm-cycle",                 "tmode cycle",                         0],
         ["tm-ignore",                "tmode ignore",                        0],
         ["tm-trace",                 "tmode trace",                         0],
         ["tm-break",                 "tmode break",                         0],

         /* hooks */
         ["hook-break-set",                                         cmdHook, 0],
         ["hook-break-clear",                                       cmdHook, 0],
         ["hook-debug-stop",                                        cmdHook, 0],
         ["hook-debug-continue",                                    cmdHook, 0],
         ["hook-display-sourcetext",                                cmdHook, 0],
         ["hook-display-sourcetext-soft",                           cmdHook, 0],
         ["hook-eval-done",                                         cmdHook, 0],
         ["hook-fbreak-clear",                                      cmdHook, 0],
         ["hook-fbreak-set",                                        cmdHook, 0],
         ["hook-guess-complete",                                    cmdHook, 0],
         ["hook-transient-script",                                  cmdHook, 0],
         ["hook-script-manager-created",                            cmdHook, 0],
         ["hook-script-manager-destroyed",                          cmdHook, 0],
         ["hook-script-instance-created",                           cmdHook, 0],
         ["hook-script-instance-sealed",                            cmdHook, 0],
         ["hook-script-instance-destroyed",                         cmdHook, 0],
         ["hook-session-display",                                   cmdHook, 0],
         ["hook-source-load-complete",                              cmdHook, 0],
         ["hook-window-closed",                                     cmdHook, 0],
         ["hook-window-loaded",                                     cmdHook, 0],
         ["hook-window-opened",                                     cmdHook, 0],
         ["hook-window-resized",                                    cmdHook, 0],
         ["hook-window-unloaded",                                   cmdHook, 0],
         ["hook-venkman-exit",                                      cmdHook, 0],
         ["hook-venkman-query-exit",                                cmdHook, 0],
         ["hook-venkman-started",                                   cmdHook, 0]
        ];

    cmdary.stringBundle = console.defaultBundle;
    console.commandManager.defineCommands (cmdary);

    console.commandManager.argTypes.__aliasTypes__ (["index", "breakpointIndex",
                                                     "lineNumber"], "int");
    console.commandManager.argTypes.__aliasTypes__ (["windowFlags",
                                                     "expression",
                                                     "prefValue"],
                                                     "rest");
    console.commandManager.argTypes.__aliasTypes__ (["deletePref"],
                                                     "state");

    console.commandManager.installKeys(console.mainWindow.document,
                                       console.commandManager.commands);
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

function getToggle (toggle, currentState)
{
    if (toggle == "toggle")
        toggle = !currentState;

    return toggle;
}

/*******************************************************************************
 * Command implementations from here on down...
 ******************************************************************************/

function cmdAboutMozilla ()
{
    openTopWin ("about:mozilla");
}

function cmdBreak (e)
{
    if (!("isInteractive" in e))
        e.isInteractive = false;
    
    if (!e.urlPattern)
    {
        /* if no input data, just list the breakpoints */
        var i = 0;
        
        for (var b in console.breaks)
        {
            var brk = console.breaks[b];
            display (getMsg(MSN_BP_LINE, [++i, brk.url, brk.lineNumber]));
        }

        if (i == 0)
            display (MSG_NO_BREAKS_SET);

        return;
    }

    var found;
    
    for (var url in console.scriptManagers)
    {
        var manager = console.scriptManagers[url];

        if (url.search(e.urlPattern) != -1 &&
            manager.isLineExecutable(e.lineNumber))
        {
            found = true;
            if (manager.hasBreakpoint (e.lineNumber))
            {
                feedback (e, getMsg(MSN_BP_EXISTS, [url, e.lineNumber]));
            }
            else
            {
                var props = e.properties;
                var fbreak;
                if (e.parent)
                    fbreak = e.parent;
                else
                    fbreak = getFutureBreakpoint(url, e.lineNumber);
                if (!fbreak)
                {
                    dispatch ("fbreak", { isInteractive: e.isInteractive,
                                          urlPattern: url,
                                          lineNumber: e.lineNumber,
                                          props: props});
                    fbreak = getFutureBreakpoint(url, e.lineNumber);
                    // hard breakpoint properties will be inherited
                    props = null;
                }

                console.scriptManagers[url].setBreakpoint (e.lineNumber, fbreak,
                                                           props);
                feedback (e, getMsg(MSN_BP_CREATED, [url, e.lineNumber]));
            }
        }
    }

    if (!found)
    {
        feedback (e, getMsg(MSN_ERR_BP_NOLINE, [e.urlPattern, e.lineNumber]),
                  MT_ERROR);
    }
}

function cmdBreakProps (e)
{
    if ("propsWindow" in e.breakWrapper)
    {
        e.breakWrapper.propsWindow.focus();
        return;
    }
        
    e.breakWrapper.propsWindow = 
        openDialog ("chrome://venkman/content/venkman-bpprops.xul", "",
                    "chrome,extrachrome,menubar,resizable", e.breakWrapper);
}

function cmdChangeContainer(e)
{
    if (!(e.viewId in console.views))
    {
        display(getMsg(MSN_ERR_NO_SUCH_VIEW, e.viewId), MT_ERROR);
        return;
    }

    var view = console.views[e.viewId];
    
    if (!view.currentContent)
    {
        display(getMsg(MSN_ERR_INVALID_PARAM, ["<view-id>", e.viewId]),
                MT_ERROR);
        return;
    }
    
    e.newType = e.newType.toLowerCase();
    
    if (e.newType.search(/^(horizontal|vertical|tab)$/) != 0)
    {
        display(getMsg(MSN_ERR_INVALID_PARAM, ["<new-type>", e.newType]),
                MT_ERROR);
        return;
    }

    console.viewManager.changeContainer(view.currentContent.parentNode,
                                        e.newType);
}

function cmdChangeValue(e)
{
    var obj = e.parentValue.getWrappedValue();
    
    if (!e.newValue)
    {
        var ok;
        
        var current = obj[e.propertyName];
        if (typeof current == "string")
            current = current.quote();
        else if (typeof current == "object")
            current = "";
        else
            current = String(current);

        while (!ok)
        {
            
            var expr = prompt(getMsg(MSN_ENTER_EXPRESSION, e.propertyName),
                              current);
            if (expr == null || expr == "")
                return;
            
            try
            {
                if ("frames" in console)
                {
                    e.newValue = evalInTargetScope(expr, true);
                    e.newValue = e.newValue.getWrappedValue();
                }
                else
                {
                    var parent = e.parentValue.jsParent.getWrappedValue();
                    e.newValue = eval(expr, parent);
                }
                ok = true;
            }
            catch(ex)
            {
                if (ex instanceof jsdIValue)
                    alert (String(ex.getWrappedValue));
                else
                    alert (String(ex));

                current = expr;
            }
        }
    }

    obj[e.propertyName] = e.newValue;
    dispatch ("hook-eval-done");
}

function cmdChromeFilter(e)
{
    const FLAGS = SCRIPT_NODEBUG | SCRIPT_NOPROFILE;
    
    function setFlag (scriptWrapper)
    {
        if (!scriptWrapper.jsdScript.isValid)
            return;
        
        if (e.toggle)
        {
            scriptWrapper.lastFlags = scriptWrapper.jsdScript.flags;
            scriptWrapper.jsdScript.flags |= FLAGS;
        }
        else
        {
            if ("lastFlags" in scriptWrapper)
            {
                scriptWrapper.jsdScript.flags = scriptWrapper.lastFlags;
                delete scriptWrapper.lastFlags;
            }
            else if (isURLVenkman(scriptWrapper.jsdScript.fileName))
            {
                scriptWrapper.jsdScript.flags |= FLAGS;
            }
            else
            {
                scriptWrapper.jsdScript.flags &= ~(FLAGS);
            }
        }

        if (scriptWrapper.jsdScript.flags & SCRIPT_NODEBUG)
            ++scriptWrapper.scriptInstance.disabledScripts;
    };
    
    var currentState = console.prefs["enableChromeFilter"];

    if (e.toggle != null)
    {
        currentState = console.prefs["enableChromeFilter"];
        e.toggle = getToggle (e.toggle, currentState);
        console.prefs["enableChromeFilter"] = e.toggle;
        

        if (e.toggle != currentState)
        {
            for (var url in console.scriptManagers)
            {
                if (url.search (/^chrome:/) == -1 &&
                    (!("componentPath" in console) ||
                     url.indexOf(console.componentPath) == -1))
                {
                    continue;
                }

                //dd ("setting chrome filter " + e.toggle + " for " + url);
                
                var mgr = console.scriptManagers[url];
                if (e.toggle)
                {
                    mgr.lastDisableTransients = mgr.disableTransients;
                    mgr.disableTransients = true;
                }
                else
                {
                    if ("lastDisableTransients" in mgr)
                    {
                        mgr.disableTransients = mgr.lastDisableTransients;
                        delete mgr.lastDisableTransients;
                    }
                    else
                    {
                        mgr.disableTransients = false;
                    }
                }
                    
                for (var i in mgr.instances)
                {
                    var instance = mgr.instances[i];
                    instance.disabledScripts = 0;
                    if (instance.topLevel)
                        setFlag (instance.topLevel);
                    
                    for (var f in instance.functions)
                        setFlag(instance.functions[f]);
                }
            }
        }
    }

    feedback (e, getMsg(MSN_CHROME_FILTER,
                        currentState ? MSG_VAL_ON : MSG_VAL_OFF));
}

function cmdClear (e)
{
    var found = false;
    
    for (var b in console.breaks)
    {
        var brk = console.breaks[b];
        if ((!e.lineNumber || 
             "lineNumber" in brk && e.lineNumber == brk.lineNumber) &&
            brk.scriptWrapper.jsdScript.fileName.search(e.urlPattern) != -1)
        {
            found = true;
            brk.scriptWrapper.clearBreakpoint(brk.pc);
            feedback (e, getMsg(MSN_BP_CLEARED,
                                [brk.scriptWrapper.jsdScript.fileName,
                                 e.lineNumber]));
        }
    }

    if (!found)
    {
        feedback (e, getMsg(MSN_ERR_BP_NODICE, [e.urlPattern, e.lineNumber]),
                  MT_ERROR);
    }
}

function cmdClearAll (e)
{
    if (!("isInteractive" in e))
        e.isInteractive = false;
    
    var breakWrapperList = new Array()
    for (var i in console.breaks)
        breakWrapperList.push (console.breaks[i]);

    if (breakWrapperList.length)
    {
        dispatch ("clear-break", { isInteractive: e.isInteractive,
                                   breakWrapper: breakWrapperList[0],
                                   breakWrapperList: breakWrapperList });
    }
}

function cmdClearBreak (e)
{
    if (!("isInteractive" in e))
        e.isInteractive = false;
    
    function clear (wrapper)
    {
        if (wrapper instanceof BreakInstance)
        {
            wrapper.scriptWrapper.clearBreakpoint(wrapper.pc);
        }
        else if (wrapper instanceof FutureBreakpoint)
        {
            for (var b in wrapper.childrenBP)
                clear (wrapper.childrenBP[b]);
        }
    };

    if ("breakWrapperList" in e)
    {
        for (var i = 0; i < e.breakWrapperList.length; ++i)
            clear(e.breakWrapperList[i]);
    }
    else
    {
        clear (e.breakWrapper);
    }
}

function cmdClearFBreak (e)
{
    if (!("isInteractive" in e))
        e.isInteractive = false;
    
    function clear (wrapper)
    {
        if (wrapper instanceof FutureBreakpoint)
        {
            var params = {
                isInteractive: e.isInteractive,
                urlPattern: wrapper.url,
                lineNumber: wrapper.lineNumber
            };
        
            dispatch ("fclear", params);
        }
        else if (wrapper instanceof BreakInstance && wrapper.parentBP)
        {
            clear (wrapper.parentBP);
        }
    };
            
    if ("breakWrapperList" in e)
    {
        for (var i = 0; i < e.breakWrapperList.length; ++i)
            clear(e.breakWrapperList[i]);
    }
    else
    {
        clear (e.breakWrapper);
    }
}
    
function cmdClearProfile (e)
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

function cmdClearInstance (e)
{
    if ("scriptInstanceList" in e)
    {
        for (var i = 0; i < e.scriptInstanceList.length; ++i)
            cmdClearInstance ({ scriptInstance: e.scriptInstanceList[i] });
        return true;
    }


    if (e.scriptInstance.topLevel)
        e.scriptInstance.topLevel.clearBreakpoints();
    
    for (var w in e.scriptInstance.functions)
        e.scriptInstance.functions[w].clearBreakpoints();

    return true;
}
    
function cmdClearScript (e)
{
    var i;
    
    if ("scriptWrapperList" in e)
    {
        for (i = 0; i < e.scriptWrapperList.length; ++i)
            cmdClearScript ({scriptWrapper: e.scriptWrapperList[i]});
        return true;
    }

    e.scriptWrapper.clearBreakpoints();
    
    return true;
}

function cmdClose(e)
{
    if ("sourceWindow" in e)
        e.sourceWindow.close();
    else
        window.close();
}

function cmdCommands (e)
{
    display (getMsg(MSN_TIP1_HELP, 
                    console.prefs["sessionView.requireSlash"] ? "/" : ""));
    display (MSG_TIP2_HELP);

    var names = console.commandManager.listNames(e.pattern, CMD_CONSOLE);
    if (!names.length)
    {
        display (getMsg(MSN_NO_CMDMATCH, e.pattern), MT_ERROR);
        return true;
    }

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

        console.prefs["lastErrorMode"] = e.mode;
    }
    
    switch (console.errorMode)
    {
        case EMODE_IGNORE:
            feedback (e, MSG_EMODE_IGNORE);
            break;
        case EMODE_TRACE:
            feedback (e, MSG_EMODE_TRACE);
            break;
        case EMODE_BREAK:
            feedback (e, MSG_EMODE_BREAK);
            break;
    }

    return true;
}

function cmdEval (e)
{
    var urlFile;
    var functionName;
    var rv;
    
    if (!("currentEvalObject" in console))
    {
        display (MSG_ERR_NO_EVAL_OBJECT, MT_ERROR);
        return false;
    }

    display(getMsg(MSN_EVAL_IN, [leftPadString(console.evalCount, 3, "0"),
                                 e.expression]),
            MT_FEVAL_IN);

    if (console.currentEvalObject instanceof jsdIStackFrame)
    {
        rv = evalInTargetScope (e.expression);
        if (typeof rv != "undefined")
        {
            if (rv != null)
            {
                var l = $.length;
                $[l] = rv;
                
                display (getMsg(MSN_FMT_TMP_ASSIGN, [l, formatValue (rv)]),
                         MT_FEVAL_OUT);
            }
            else
                dd ("evalInTargetScope returned null");
        }
    }
    else
    {
        var parent;
        var jsdValue = console.jsds.wrapValue (console.currentEvalObject);
        if (jsdValue.jsParent)
            parent = jsdValue.jsParent.getWrappedValue();
        if (!parent)
            parent = console.currentEvalObject;
        if ("location" in parent)
            urlFile = getFileFromPath(parent.location.href);
        else
            urlFile = MSG_VAL_UNKNOWN;

        try
        {
            rv = console.doEval.apply(console.currentEvalObject,
                                      [e.expression, parent]);
            display (String(rv), MT_FEVAL_OUT);
        }
        catch (ex)
        {
            display (formatException(ex), MT_ERROR);
        }
    }    
    
    dispatch ("hook-eval-done");
    return true;
}

function cmdEvald (e)
{
    display (e.expression, MT_EVAL_IN);
    var rv = evalInDebuggerScope (e.expression);
    if (typeof rv != "undefined")
        display (String(rv), MT_EVAL_OUT);

    dispatch ("hook-eval-done");
    return true;
}

function cmdFBreak(e)
{       
    if (!e.urlPattern)
    {
        /* if no input data, just list the breakpoints */
        var i = 0;
        
        for (var f in console.fbreaks)
        {
            var brk = console.fbreaks[f];
            display (getMsg(MSN_FBP_LINE, [++i, brk.url, brk.lineNumber]));
        }

        if (i == 0)
            display (MSG_NO_FBREAKS_SET);

        return null;
    }
    else
    {
        var fbreak = setFutureBreakpoint (e.urlPattern, e.lineNumber, 
                                          e.properties);
        if (fbreak)
        {
            feedback (e, getMsg(MSN_FBP_CREATED, [e.urlPattern, e.lineNumber]));
        }
        else
        {
            feedback (e, getMsg(MSN_FBP_EXISTS, [e.urlPattern, e.lineNumber]),
                      MT_ERROR);
        }
        return fbreak;
    }
}

function cmdFinish (e)
{
    if (console.frames.length == 1)
        return cmdCont();
    
    console._stepOverDepth = console.frames.length - 1;
    setStopState(false);
    console.jsds.functionHook = console.callHook;
    disableDebugCommands()
    console.jsds.exitNestedEventLoop();
    return true;
}

function cmdFindBp (e)
{
    if ("scriptWrapper" in e.breakpoint)
    {
        dispatch ("find-script",
                  { scriptWrapper: e.breakpoint.scriptWrapper, 
                    targetPc: e.breakpoint.pc });
    }
    else
    {
        dispatch ("find-url", {url: e.breakpoint.url,
                               rangeStart: e.breakpoint.lineNumber,
                               rangeEnd: e.breakpoint.lineNumber});
    }
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
                     {url: url, rangeStart: line, rangeEnd: line});
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
    var jsdFrame = console.frames[e.frameIndex];

    if ("isInteractive" in e && e.isInteractive)
        displayFrame (jsdFrame, e.frameIndex, true);

    if (jsdFrame.isNative)
        return true;
    
    var scriptWrapper = console.scriptWrappers[jsdFrame.script.tag]
    return dispatch ("find-script",
                     { scriptWrapper: scriptWrapper, targetPc: jsdFrame.pc });
}

function cmdFindScript (e)
{
    var jsdScript = e.scriptWrapper.jsdScript;
    var targetLine = 1;
    var rv, params;

    if (console.prefs["prettyprint"] && jsdScript.isValid)
    {
        if (e.targetPc != null)
            targetLine = jsdScript.pcToLine(e.targetPc, PCMAP_PRETTYPRINT);
        
        console.currentDetails = e.scriptWrapper;

        params = {
            sourceText: e.scriptWrapper.sourceText,
            rangeStart: null,
            rangeEnd:   null,
            targetLine: targetLine,
            details: e.scriptWrapper
        };
        dispatch ("hook-display-sourcetext-soft", params);
        rv = jsdScript.fileName;
    }
    else
    {
        if (e.targetPc != null && jsdScript.isValid)
            targetLine = jsdScript.pcToLine(e.targetPc, PCMAP_SOURCETEXT);
        else
            targetLine = jsdScript.baseLineNumber;

        params = {
            sourceText: e.scriptWrapper.scriptInstance.sourceText,
            rangeStart: jsdScript.baseLineNumber,
            rangeEnd: jsdScript.baseLineNumber + 
                      jsdScript.lineExtent - 1,
            targetLine: targetLine,
            details: e.scriptWrapper
        };
        rv = dispatch("find-sourcetext-soft", params);
    }

    return rv;
}

function cmdFindScriptInstance (e)
{
    var params = Clone(e);
    params.sourceText = e.scriptInstance.sourceText;
    dispatch ("find-sourcetext", params);
}

function cmdFindSourceText (e)
{
    function cb(status)
    {
        if (status != Components.results.NS_OK)
        {
            display (getMsg (MSN_ERR_SOURCE_LOAD_FAILED,
                             [e.sourceText.url, status], MT_ERROR));
            return;
        }
        
        var params = {
            sourceText: e.sourceText,
            rangeStart: e.rangeStart,
            rangeEnd: e.rangeEnd,
            targetLine: (e.targetLine != null) ? e.targetLine : e.rangeStart,
            details:    e.details
        };
        
        if (e.command.name.indexOf("soft") != -1)
            dispatch ("hook-display-sourcetext-soft", params);
        else
            dispatch ("hook-display-sourcetext", params);
    };

    console.currentSourceText = e.sourceText;
    console.currentDetails = e.details;
    
    if (!e.sourceText || e.sourceText.isLoaded)
        cb(Components.results.NS_OK);
    else
        e.sourceText.loadSource(cb);
}

function cmdFindURL (e)
{
    if (!e.url)
    {
        dispatch ("find-sourcetext", { sourceText: null });
        return;
    }

    var sourceText;

    if (e.url.indexOf("x-jsd:") == 0)
    {
        switch (e.url)
        {
            case "x-jsd:help":
                sourceText = console.sourceText;
                break;
                
            default:
                display (getMsg(MSN_ERR_INVALID_PARAM, ["url", e.url]),
                         MT_ERROR);
                return;
        }
    }
    else if (e.url in console.scriptManagers)
    {
        sourceText = console.scriptManagers[e.url].sourceText;
    }
    else if (e.url in console.files)
    {
        sourceText = console.files[e.url];
    }
    else
    {
        sourceText = console.files[e.url] = new SourceText (e.url);
    }

    var params = {
        sourceText: sourceText,
        rangeStart: e.rangeStart,
        rangeEnd: e.rangeEnd,
        targetLine: e.targetLine,
        details: e.details
    };

    if (e.command.name.indexOf("soft") == -1)
        dispatch ("find-sourcetext", params);
    else
        dispatch ("find-sourcetext-soft", params);
}

function cmdFClear (e)
{
    var found = false;

    for (var b in console.fbreaks)
    {
        var brk = console.fbreaks[b];
        if (!e.lineNumber || e.lineNumber == brk.lineNumber &&
            brk.url.search(e.urlPattern) != -1)
        {
            found = true;
            clearFutureBreakpoint (brk.url, e.lineNumber);
            feedback (e, getMsg(MSN_FBP_CLEARED, [brk.url, e.lineNumber]));
        }
    }

    if (!found)
    {
        feedback (e, getMsg(MSN_ERR_BP_NODICE, [e.urlPattern, e.lineNumber]),
                  MT_ERROR);
    }
}

function cmdFClearAll (e)
{
    if (!("isInteractive" in e))
        e.isInteractive = false;

    var breakWrapperList = new Array();
    
    for (var i in console.fbreaks)
        breakWrapperList.push (console.fbreaks[i]);

    if (breakWrapperList.length)
    {
        dispatch ("clear-fbreak", { isInteractive: e.isInteractive,
                                    breakWrapper: breakWrapperList[0],
                                    breakWrapperList: breakWrapperList });
    }
}

function cmdFrame (e)
{
    if (e.frameIndex != null)
    {
        if (e.frameIndex < 0 || e.frameIndex >= console.frames.length)
        {
            display (getMsg(MSN_ERR_INVALID_PARAM,
                            ["frameIndex", e.frameIndex]),
                     MT_ERROR);
            return false;
        }
        setCurrentFrameByIndex(e.frameIndex);
    }
    else
    {
        e.frameIndex = getCurrentFrameIndex();
    }    

    if (!("isInteractive" in e))
        e.isInteractive = false;
    
    dispatch ("find-frame", { frameIndex: e.frameIndex,
                              isInteractive: e.isInteractive });
    return true;
}

function cmdGC()
{
    console.jsds.GC();    
}

function cmdHelp (e)
{
    var ary;
    if (!e.pattern)
    {
        openTopWin ("x-jsd:help", "venkman-help");
        return true;
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

function cmdHook(e)
{
    /* empty function used for "hook" commands. */
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
            console._loader = cls.getService(mozIJSSubScriptLoader);
    }
    
    var obj = ("scope" in e) ? e.scope : null;
    try
    {
        var rvStr;
        var rv = rvStr = console._loader.loadSubScript(e.url, obj);
        if (typeof rv == "function")
            rvStr = MSG_TYPE_FUNCTION;
        feedback(e, getMsg(MSN_SUBSCRIPT_LOADED, [e.url, rvStr]), MT_INFO);
        return rv;
    }
    catch (ex)
    {
        display (getMsg(MSN_ERR_SCRIPTLOAD, e.url));
        display (formatException(ex), MT_ERROR);
    }
    
    return null;
}

function cmdMoveView (e)
{
    if (!e.viewId || !(e.viewId in console.views))
    {
        display (getMsg(MSN_ERR_INVALID_PARAM, ["viewId", e.viewId]), MT_ERROR);
        return;    
    }

    var parsedLocation = console.viewManager.parseLocation (e.locationUrl);
    if (!parsedLocation)
    {
        display (getMsg(MSN_ERR_INVALID_PARAM, ["locationURL", e.locationURL]),
                 MT_ERROR);
        return;    
    }
    
    console.viewManager.moveView (parsedLocation, e.viewId);
}

function cmdMozillaHelp ()
{
    if (typeof openHelp == "undefined")
        toOpenWindowByType('mozilla:help', 'chrome://help/content/help.xul');
    else
        openHelp();
}

function cmdNext ()
{
    // we don't setStopState(false) here because if the line we're at
    // doesn't call out to a function, /next is the same as /step.
    console._stepOverDepth = console.frames.length;
    dispatch ("step");
    console.jsds.functionHook = console.callHook;
    return;
}

function cmdOpenDialog (e)
{
    if (!e.windowFlags)
        e.windowFlags = "chrome,menubar,toolbar,resizable,dialog=no";
    
    return openDialog (e.url, e.windowName, e.windowFlags);
}

function cmdOpenURL (e)
{
    var url = prompt (MSG_OPEN_URL, "http://");
    if (url && url != "http://")
        return dispatch ("find-url", { url: url });

    return null;
}    

function cmdPPrint (e)
{
    var state, params;
    
    if (e.toggle != null)
    {
        e.toggle = getToggle (e.toggle, console.prefs["prettyprint"]);
        console.prefs["prettyprint"] = e.toggle;
        
        var tb = document.getElementById("maintoolbar:toggle-pprint");
        if (e.toggle)
        {
            tb.setAttribute("state", "true");
        }
        else
        {
            tb.removeAttribute("state");
        }

        if ("currentDetails" in console &&
            console.currentDetails instanceof ScriptWrapper)
        {
            dispatch ("find-script", { scriptWrapper: console.currentDetails });
        }
    }

    feedback (e, getMsg(MSN_FMT_PPRINT, console.prefs["prettyprint"] ?
                        MSG_VAL_ON : MSG_VAL_OFF));
    
    return true;
}

function cmdPref (e)
{
    if (e.prefName && e.prefName[0] == "-")
    {
        e.prefName = e.prefName.substr(1);
        e.deletePref = true;
    }
        
    if (e.deletePref)
    {
        try
        {
            console.prefManager.prefBranch.clearUserPref(e.prefName);
            console.prefManager.dirtyPrefs[e.prefName] = true;
            console.prefManager.prefService.savePrefFile(null);
        }
        catch (ex)
        {
            // ignore exception generated by clear of nonexistant pref
            if (!("result" in ex) ||
                ex.result != Components.results.NS_ERROR_UNEXPECTED)
            {
                throw ex;
            }
        }

        var prefValue = console.prefs[e.prefName];
        feedback (e, getMsg(MSN_FMT_PREFVALUE,
                            [e.prefName, console.prefs[e.prefName]]));
        return true;
    }

    if (e.prefValue)
    {
        var type = typeof console.prefs[e.prefName];
        switch (type)
        {
            case "number":
                e.prefValue = Number(e.prefValue);
                break;
            case "boolean":
                e.prefValue = (e.prefValue.toLowerCase() == "true");
                break;
            case "string":
                break;
            default:
                e.prefValue = String(e.prefValue);
                break;
        }

        console.prefs[e.prefName] = e.prefValue;
        feedback (e, getMsg(MSN_FMT_PREFVALUE, [e.prefName, e.prefValue]));
    }
    else
    {
        var ary = console.prefManager.listPrefs(e.prefName);
        if (ary.length == 0)
        {
            display (getMsg(MSN_ERR_UNKNOWN_PREF, [e.prefName]),
                     MT_ERROR);
            return false;
        }

        for (var i = 0; i < ary.length; ++i)
        {
            feedback (e, getMsg(MSN_FMT_PREFVALUE,
                                [ary[i], console.prefs[ary[i]]]));
        }
    }

    return true;
}

function cmdProfile(e)
{
    var currentState = console.jsds.flags & COLLECT_PROFILE_DATA;

    if (e.toggle != null)
    {
        e.toggle = getToggle (e.toggle, currentState);
        setProfileState(e.toggle);
    }
    else
    {
        e.toggle = currentState;
    }
    
    feedback (e, getMsg(MSN_PROFILE_STATE,
                        [e.toggle ? MSG_VAL_ON : MSG_VAL_OFF]));
}

function cmdProps (e)
{
    var v;
    var debuggerScope = (e.command.name == "propsd");

    if (debuggerScope)
    {
        v = console.jsds.wrapValue(evalInDebuggerScope (e.expression));
    }
    else
    {
        if (!("currentEvalObject" in console))
        {
            display (MSG_ERR_NO_EVAL_OBJECT, MT_ERROR);
            return false;
        }

        if (console.currentEvalObject instanceof jsdIStackFrame)
        {
            v = evalInTargetScope (e.expression);
        }
        else
        {
            v = console.doEval.apply(console.currentEvalObject,
                                     [e.expression, parent]);
            v = console.jsds.wrapValue(v);
        }
    }
    
    if (!(v instanceof jsdIValue) || v.jsType != jsdIValue.TYPE_OBJECT)
    {
        var str = (v instanceof jsdIValue) ? formatValue(v) : String(v)
        display (getMsg(MSN_ERR_INVALID_PARAM, [MSG_VAL_EXPRESSION, str]),
                 MT_ERROR);
        return false;
    }
    
    display (getMsg(debuggerScope ? MSN_PROPSD_HEADER : MSN_PROPS_HEADER,
                    e.expression));
    displayProperties(v);
    return true;
}

function cmdQuit()
{
    goQuitApplication();
}

function cmdRestoreLayout (e)
{   
    if (!e.name)
    {
        var list = console.prefManager.listPrefs("layoutState.");
        for (var i = 0; i < list.length; ++i)
            list[i] = list[i].substr(12);
        list.push("factory");
        display (getMsg(MSN_LAYOUT_LIST, list.sort().join(MSG_COMMASP)));
        return;
    }
    
    var layout;
    e.name = e.name.toLowerCase();
    if (e.name == "factory")
    {
        layout = DEFAULT_VURLS;
    }
    else
    {
        var prefName = "layoutState." + e.name;
        if (!(prefName in console.prefs))
        {
            display (getMsg(MSN_ERR_INVALID_PARAM, ["name", e.name]), MT_ERROR);
            return;
        }
        else
        {
            layout = console.prefs[prefName];
        }
    }
    
    console.viewManager.destroyWindows();
    console.viewManager.reconstituteVURLs (layout.split (/\s*;\s*/));
}

function cmdRestoreSettings(e)
{
    if (!e.settingsFile || e.settingsFile == "?")
    {
        var rv = pickOpen(MSG_OPEN_FILE, "*.js");
        if (rv.reason == PICK_CANCEL)
            return;
        e.settingsFile = getURLSpecFromFile(rv.file);
    }
    else if (e.settingsFile.indexOf("file:") != 0)
    {
        e.settingsFile = getURLSpecFromFile(e.settingsFile);
    }

    dispatch("loadd", { url: e.settingsFile });
}

function cmdReleaseNotes (e)
{
    openTopWin(MSG_RELEASE_URL);
}

function cmdRunTo (e)
{
    if (!e.scriptWrapper.jsdScript.isValid)
        return;
    
    var breakpoint = e.scriptWrapper.setBreakpoint(e.pc);
    if (breakpoint)
        breakpoint.oneTime = true;
    dispatch ("cont");
}

function cmdSaveBreakpoints(e)
{
    var needClose = false;
    var file = e.settingsFile;
    
    if (!file || file == "?")
    {
        var rv = pickSaveAs(MSG_SAVE_FILE, "*.js");
        if (rv.reason == PICK_CANCEL)
            return;
        e.settingsFile = file = fopen(rv.file, ">");
        needClose = true;
    }
    else if (typeof file == "string")
    {
        e.settingsFile = file = fopen(file, ">");
        needClose = true;
    }

    var fbp, bp;
    var params;

    file.write ("\n//Breakpoint settings start...\n");
    
    for (i in console.fbreaks)
    {
        var wroteVar;
        
        fbp = console.fbreaks[i];
        params = {
            urlPattern: fbp.url,
            lineNumber: fbp.lineNumber,
            properties: fbp.getProperties()
        };

        if (keys(fbp.childrenBP).length)
        {
            if (!wroteVar)
            {
                file.write ("var fbreak;\n\n");
                wroteVar = true;
            }
            file.write("fbreak = ");
        }            

        file.write("dispatch('fbreak', " + params.toSource() + ");\n\n");

        for (i in fbp.childrenBP)
        {
            bp = fbp.childrenBP[i];
            
            file.write("dispatch('break', {" +
                       "urlPattern:" + bp.url.quote() + ", " +
                       "lineNumber:" + bp.lineNumber + ", " +
                       "parent:fbreak, " +
                       "properties:" + bp.getProperties().toSource() +
                       "});\n");
        };

        file.write("\n");
    }
    
    for (i in console.breaks)
    {
        bp = console.breaks[i];
        if (!bp.parentBP)
        {
            params = {
                urlPattern: bp.url,
                lineNumber: bp.lineNumber,
                properties: bp.getProperties()
            };
            
            file.write("dispatch('break', " + params.toSource() + ");\n");
        }
    }
    
    file.write (MSG_BREAKPOINTS_RESTORED.quote() + ";\n");

    if (needClose)
        file.close();
}
    
function cmdSaveLayout(e)
{
    if (!e.name)
    {
        var list = console.prefManager.listPrefs("layoutState.");
        for (var i = 0; i < list.length; ++i)
            list[i] = list[i].substr(12);
        list.push("factory");
        display (getMsg(MSN_LAYOUT_LIST, list.sort().join(MSG_COMMASP)));
        return;
    }
    
    e.name = e.name.toLowerCase();
    if (e.name == "factory")
    {
        display (getMsg(MSN_ERR_INVALID_PARAM, ["name", e.name]), MT_ERROR);
        return;    
    }
    
    var ary = console.viewManager.getLayoutState ();
    var prefName = "layoutState." + e.name;
    console.prefManager.addPref(prefName, "");
    console.prefs[prefName] = ary.join ("; ");
}

function cmdSaveProfile (e)
{
    function onComplete (i)
    {
        var msg = getMsg(MSN_PROFILE_SAVED, getURLSpecFromFile(file.localFile));
        display (msg);
        console.status = msg;
        file.close();
    };
        
    var templatePfx = "profile.template.";
    var i;
    var ext;
    
    if (!e.targetFile || e.targetFile == "?")
    {
        var list = console.prefManager.listPrefs(templatePfx);
        var extList = "";
        for (i = 0; i < list.length; ++i)
        {
            ext = list[i].substr(templatePfx.length);
            if (list[i].search(/html|htm|xml|txt/i) == -1)
                extList += "*." + ext + " ";
        }
        var rv = pickSaveAs(MSG_SAVE_PROFILE, "$html $xml $text " + 
                            extList + "$all");
        if (rv.reason == PICK_CANCEL)
            return null;
        e.targetFile = rv.file;
    }
    
    var file = fopen (e.targetFile, ">");

    var templateName;
    var ary = file.localFile.path.match(/\.([^.]+)$/);

    if (ary)
        ext = ary[1].toLowerCase();
    else
        ext = "txt";

    templateName = templatePfx + ext;
    var templateFile;
    if (templateName in console.prefs)
        templateFile = console.prefs[templateName];
    else
        templateFile = console.prefs[templatePfx + "txt"];
    
    var reportTemplate = console.profiler.loadTemplate(templateFile);

    var scriptInstanceList = new Array();
    
    var j;

    if (!("urlList" in e) || e.urlList.length == 0)
    {
        if ("url" in e && e.url)
            e.urlList = [e.url];
        else
            e.urlList = keys(console.scriptManagers);
    }
    
    e.urlList = e.urlList.sort();
    
    for (i = 0; i < e.urlList.length; ++i)
    {
        var url = e.urlList[i];
        if (!ASSERT (url in console.scriptManagers, "url not loaded"))
            continue;
        var manager = console.scriptManagers[url];
        for (j in manager.instances)
            scriptInstanceList.push (manager.instances[j]);
    }

    var rangeList;
    if (("profile.ranges." + ext) in console.prefs)
        rangeList = console.prefs["profile.ranges." + ext].split(",");
    else
        rangeList = console.prefs["profile.ranges.default"].split(",");
    
    var profileReport = new ProfileReport (reportTemplate, file, rangeList,
                                           scriptInstanceList);
    profileReport.onComplete = onComplete;
    
    console.profiler.generateReport (profileReport);

    return file.localFile;
}

function cmdSaveSettings(e)
{
    if (!e.settingsFile || e.settingsFile == "?")
    {
        var rv = pickSaveAs(MSG_SAVE_FILE, "*.js");
        if (rv.reason == PICK_CANCEL)
            return;
        e.settingsFile = fopen(rv.file, ">");
    }
    else if (typeof e.settingsFile == "string")
    {
        e.settingsFile = fopen(e.settingsFile, ">");
    }
  
    dispatch ("save-breakpoints", { settingsFile: e.settingsFile });
    dispatch ("save-watches", { settingsFile: e.settingsFile });

    e.settingsFile.write("\n" + MSG_SETTINGS_RESTORED.quote() + ";\n");
    
    e.settingsFile.close();
}

function cmdScanSource (e)
{
    e.scriptInstance.scanForMetaComments();
}

function cmdScope ()
{
    if (getCurrentFrame().scope.propertyCount == 0)
        display (getMsg (MSN_NO_PROPERTIES, MSG_VAL_SCOPE));
    else
        displayProperties (getCurrentFrame().scope);
    
    return true;
}

function cmdThisExpr(e)
{
    if (e.expression == "debugger")
    {
        rv = console.jsdConsole;
    }
    else if (console.currentEvalObject instanceof jsdIStackFrame)
    {
        rv = evalInTargetScope (e.expression);
    }
    else
    {
        rv = console.doEval.apply(console.currentEvalObject,
                                  [e.expression, parent]);
    }

    if (!(rv instanceof jsdIValue))
        rv = console.jsds.wrapValue(rv);
    
    if (rv.jsType != TYPE_OBJECT)
    {
        display (MSG_ERR_THIS_NOT_OBJECT, MT_ERROR);
        return false;
    }
    
    dispatch ("set-eval-obj", { jsdValue: rv });
    dispatch ("hook-eval-done");
    return true;
}    

function cmdToggleSomething (e)
{
    var ary = e.command.name.match (/(.*)-(on|off|toggle)$/);
    if (!ary)
        return;
    
    var newEvent = Clone(e);
    
    if (ary[2] == "on")
        newEvent.toggle = true;
    else if (ary[2] == "off")
        newEvent.toggle = false;
    else
        newEvent.toggle = "toggle";

    dispatch (ary[1], newEvent);
}

function cmdSetEvalObj (e)
{
    if (!(e.jsdValue instanceof jsdIStackFrame))
    {
        e.jsdValue = e.jsdValue.getWrappedValue();
    }
    
    if (e.jsdValue == console.currentEvalObject)
    {
        var frame = getCurrentFrame();
        if (frame)
            e.jsdValue = frame;
        else
            e.jsdValue = console;
    }
    
    console.currentEvalObject = e.jsdValue;
}

function cmdSetScriptFlag (e)
{
    function setFlag (scriptWrapper)
    {
        var jsdScript = scriptWrapper.jsdScript;
        
        if (!jsdScript.isValid)
            return;
        
        if (e.toggle == "toggle")
        {
            jsdScript.flags ^= flag;
            if (flag == SCRIPT_NODEBUG)
            {
                if (jsdScript.flags & flag)
                    ++scriptWrapper.scriptInstance.disabledScripts;
                else
                    --scriptWrapper.scriptInstance.disabledScripts;
            }
        }
        else if (e.toggle)
        {
            jsdScript.flags |= flag;
            if (flag == SCRIPT_NODEBUG)
                ++scriptWrapper.scriptInstance.disabledScripts;
        }
        else
        {
            jsdScript.flags &= ~flag;
            if (flag == SCRIPT_NODEBUG)
                --scriptWrapper.scriptInstance.disabledScripts;
        }
    };
    
    function setFlagInstance (scriptInstance)
    {
        if (scriptInstance.topLevel)
            setFlag (scriptInstance.topLevel);
        
        for (var f in scriptInstance.functions)
            setFlag (scriptInstance.functions[f]);
    };

    var flag;
    var i;
    
    if (e.command.name.search(/^profile/) == 0)
        flag = SCRIPT_NOPROFILE;
    else
        flag = SCRIPT_NODEBUG;
    
    if (e.command.name.search (/script$/) != -1)
    {
        if ("scriptWrapperList" in e)
        {
            for (i = 0; i < e.scriptWrapperList.length; ++i)
                setFlag(e.scriptWrapperList[i]);
        }
        else
        {
            setFlag(e.scriptWrapper);
        }
    }
    else
    {
        if ("scriptInstanceList" in e)
        {
            for (i = 0; i < e.scriptInstanceList.length; ++i)
                setFlagInstance(e.scriptInstanceList[i]);
        }
        else
        {
            setFlagInstance(e.scriptInstance);
        }
    }
}

function cmdSetTransientFlag (e)
{
    function setFlag (url)
    {
        if (!(url in console.scriptManagers))
            return;

        var scriptManager = console.scriptManagers[url];
        if (e.toggle == "toggle")
            scriptManager.disableTransients = !scriptManager.disableTransients;
        else
            scriptManager.disableTransients = e.toggle;
    };
    
    if ("urlList" in e)
    {
        for (var i = 0; i < e.urlList.length; ++i)
            setFlag (e.urlList[i]);
    }
    else
    {
        setFlag (e.url);
    }
}

function cmdStartupInit (e)
{
    dd ("startup-init " + e.toggle);
    
    if (e.toggle != null)
    {
        e.toggle = getToggle (e.toggle, console.jsds.initAtStartup);
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
    console._stepPast = console.frames.length + topFrame.script.fileName;

    if (console.prefs["prettyprint"])
    {
        console._stepPast +=
            topFrame.script.pcToLine(topFrame.pc, PCMAP_PRETTYPRINT);
    }
    else
    {
        console._stepPast += topFrame.line;
    }
    disableDebugCommands()
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
                console.jsds.throwHook = console.executionHook;
                console.throwMode = TMODE_TRACE;
                break;
            case "break":
                console.jsds.throwHook = console.executionHook;
                console.throwMode = TMODE_BREAK;
                break;
            default:
                display (getMsg(MSN_ERR_INVALID_PARAM, ["mode", e.mode]),
                         MT_ERROR);
                return false;
        }

        console.prefs["lastThrowMode"] = e.mode;
    }
    
    switch (console.throwMode)
    {
        case TMODE_IGNORE:
            feedback (e, MSG_TMODE_IGNORE);
            break;
        case TMODE_TRACE:
            feedback (e, MSG_TMODE_TRACE);
            break;
        case TMODE_BREAK:
            feedback (e, MSG_TMODE_BREAK);
            break;
    }

    return true;
}

function cmdToggleFloat (e)
{
    if (!e.viewId in console.views || 
        typeof console.views[e.viewId] != "object")
    {
        display (getMsg(MSN_ERR_NO_SUCH_VIEW, e.viewId), MT_ERROR);
        return;
    }

    var locationUrl;
    var view = console.views[e.viewId];
    var parsedLocation = 
        console.viewManager.computeLocation(view.currentContent);

    if (parsedLocation.windowId == VMGR_MAINWINDOW)
    {
        /* already in the main window, float the view. */
        locationUrl = VMGR_VURL_NEW
    }
    else
    {
        /* already floated, put it back. */
        if ("previousLocation" in view)
            locationUrl = view.previousLocation;
        else
            locationUrl = VMGR_VURL_GUTTER;
    }

    dispatch ("move-view", { viewId: e.viewId, locationUrl: locationUrl });
}

function cmdTogglePref (e)
{
    var state = !console.prefs[e.prefName];
    console.prefs[e.prefName] = state;
    feedback (e, getMsg (MSN_FMT_PREFVALUE, 
                         [e.prefName, state ? MSG_VAL_ON : MSG_VAL_OFF]));
}
                     
function cmdToggleView (e)
{
    if (!e.viewId in console.views || typeof console.views[e.viewId] != "object")
    {
        display (getMsg(MSN_ERR_NO_SUCH_VIEW, e.viewId), MT_ERROR);
        return;
    }
    
    var view = console.views[e.viewId];
    var url;
    
    if ("currentContent" in view)
    {
        url = VMGR_VURL_HIDDEN;
    }
    else
    {
        if ("previousLocation" in view)
            url = view.previousLocation;
        else
            url = VMGR_VURL_GUTTER;
    }
    
    console.viewManager.moveViewURL (url, e.viewId);
}    

function cmdVersion ()
{
    display(MSG_HELLO, MT_HELLO);
    display(getMsg(MSN_VERSION, __vnk_version + __vnk_versionSuffix), MT_HELLO);
}

function cmdWhere ()
{
    displayCallStack();
    return true;
}
