/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is JSIRC Library.
 *
 * The Initial Developer of the Original Code is
 * New Dimensions Consulting, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@ndcico.com, original author
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

function getAccessKey (str)
{
    var i = str.indexOf("&");
    if (i == -1)
        return "";
    return str[i + 1];
}

function CommandRecord (name, func, usage, help, label, flags, keystr, tip)
{
    this.name = name;
    this.func = func;
    this._usage = usage;
    this.scanUsage();
    this.help = help;
    this.label = label ? label : name;
    this.labelstr = label.replace ("&", "");
    this.tip = tip;
    this.flags = flags;
    this._enabled = true;
    this.keyNodes = new Array();
    this.keystr = keystr;
    this.uiElements = new Array();
}

CommandRecord.prototype.__defineGetter__ ("enabled", cr_getenable);
function cr_getenable ()
{
    return this._enabled;
}

CommandRecord.prototype.__defineSetter__ ("enabled", cr_setenable);
function cr_setenable (state)
{
    for (var i = 0; i < this.uiElements.length; ++i)
    {
        if (state)
            this.uiElements[i].removeAttribute ("disabled");
        else
            this.uiElements[i].setAttribute ("disabled", "true");
    }
    return (this._enabled = state);
}

CommandRecord.prototype.__defineSetter__ ("usage", cr_setusage);
function cr_setusage (usage)
{
    this._usage = usage;
    this.scanUsage();
}

CommandRecord.prototype.__defineGetter__ ("usage", cr_getusage);
function cr_getusage()
{
    return this._usage;
}

/**
 * Internal use only.
 *
 * Scans the argument spec, in the format "<a1> <a2> [<o1> <o2>]", into an
 * array of strings.
 */
CommandRecord.prototype.scanUsage =
function cr_scanusage()
{
    var spec = this._usage;
    var currentName = "";
    var inName = false;
    var len = spec.length;
    var capNext = false;
    
    this._usage = spec;
    this.argNames = new Array();

    for (var i = 0; i < len; ++i)
    {
        switch (spec[i])
        {
            case '[':
                this.argNames.push (":");
                break;
                
            case '<':
                inName = true;
                break;
                
            case '-':
                capNext = true;
                break;
                
            case '>':
                inName = false;
                this.argNames.push (currentName);
                currentName = "";
                capNext = false;
                break;
                
            default:
                if (inName)
                    currentName += capNext ? spec[i].toUpperCase() : spec[i];
                capNext = false;
                break;
        }
    }
}

/**
 * Returns the command formatted for presentation as part of online
 * documentation.
 */
CommandRecord.prototype.getDocumentation =
function cr_getdocs(flagFormatter)

{
    var str;
    str  = getMsg(MSN_DOC_COMMANDLABEL,
                  [this.label.replace("&", ""), this.name]) + "\n";
    str += getMsg(MSN_DOC_KEY, this.keystr ? this.keystr : MSG_VAL_NA) + "\n";
    str += getMsg(MSN_DOC_SYNTAX, [this.name, this.usage]) + "\n";
    str += MSG_DOC_NOTES + "\n";
    str += (flagFormatter ? flagFormatter(this.flags) : this.flags) + "\n\n";
    str += MSG_DOC_DESCRIPTION + "\n";
    str += wrapText(this.help, 75) + "\n";
    return str;
}

CommandRecord.prototype.argNames = new Array();

function CommandManager (defaultBundle)
{
    this.commands = new Object();
    this.defaultBundle = defaultBundle;
}

CommandManager.prototype.defineCommands =
function cmgr_defcmds (cmdary)
{
    var len = cmdary.length;
    var commands = new Object();
    var bundle =  ("stringBundle" in cmdary ?
                   cmdary.stringBundle : 
                   this.defaultBundle);
    
    for (var i = 0; i < len; ++i)
    {
        var name  = cmdary[i][0];
        var func  = cmdary[i][1];
        var flags = cmdary[i][2];
        var usage = getMsgFrom(bundle, "cmd." + name + ".params", null, "");

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
            else
                aliasFor = null;
            helpDefault = getMsg (MSN_DEFAULT_ALIAS_HELP, func); 
            if (aliasFor)
                labelDefault = getMsgFrom (bundle, "cmd." + aliasFor + ".label",
                                           null, name);
        }

        var label = getMsgFrom(bundle, "cmd." + name + ".label", null,
                               labelDefault);
        var help  = getMsgFrom(bundle, "cmd." + name + ".help", null,
                               helpDefault);
        var keystr = getMsgFrom (bundle, "cmd." + name + ".key", null, "");
        var tip = getMsgFrom (bundle, "cmd." + name + ".tip", null, "");
        var command = new CommandRecord (name, func, usage, help, label, flags,
                                         keystr, tip);
        if (aliasFor)
            command.aliasFor = aliasFor;
        this.addCommand(command);
        commands[name] = command;
    }

    return commands;
}

CommandManager.prototype.installKeys =
function cmgr_instkeys (document, commands)
{
    var parentElem = document.getElementById("dynamic-keys");
    if (!parentElem)
    {
        parentElem = document.createElement("keyset");
        parentElem.setAttribute ("id", "dynamic-keys");
        document.firstChild.appendChild (parentElem);
    }

    if (!commands)
        commands = this.commands;
    
    for (var c in commands)
        this.installKey (parentElem, commands[c]);
}

/**
 * Create a <key> node relative to a DOM node.  Usually called once per command,
 * per document, so that accelerator keys work in all application windows.
 *
 * @param parentElem  A reference to the DOM node which should contain the new
 *                    <key> node.
 * @param command     reference to the CommandRecord to install.
 */
CommandManager.prototype.installKey =
function cmgr_instkey (parentElem, command)
{
    if (!command.keystr)
        return;

    var ary = command.keystr.match (/(.*\s)?([\S]+)$/);
    if (!ASSERT(ary, "couldn't parse key string ``" + command.keystr +
                "'' for command ``" + command.name + "''"))
    {
        return;
    }
    
    var key = document.createElement ("key");
    key.setAttribute ("id", "key:" + command.name);
    key.setAttribute ("oncommand", "dispatch('" + command.name +
                      "', {isInteractive: true});");
    if (ary[1])
        key.setAttribute ("modifiers", ary[1]);
    if (ary[2].indexOf("VK_") == 0)
        key.setAttribute ("keycode", ary[2]);
    else
        key.setAttribute ("key", ary[2]);
    
    parentElem.appendChild(key);
    command.keyNodes.push(key);
}

CommandManager.prototype.uninstallKeys =
function cmgr_uninstkeys (commands)
{
    if (!commands)
        commands = this.commands;
    
    for (var c in commands)
        this.uninstallKey (commands[c]);
}

CommandManager.prototype.uninstallKey =
function cmgr_uninstkey (command)
{
    for (var i in command.keyNodes)
    {
        try
        {
            /* document may no longer exist in a useful state. */
            command.keyNodes[i].parentNode.removeChild(command.keyNodes[i]);
        }
        catch (ex)
        {
            dd ("*** caught exception uninstalling key node: " + ex);
        }
    }
}

/**
 * Register a new command with the manager.
 */
CommandManager.prototype.addCommand =
function cmgr_add (command)
{
    this.commands[command.name] = command;
}

CommandManager.prototype.removeCommands = 
function cmgr_removes (commands)
{
    for (var c in commands)
        this.removeCommand(commands[c]);
}

CommandManager.prototype.removeCommand = 
function cmgr_remove (command)
{
    delete this.commands[command.name];
}

/**
 * Register a hook for a particular command name.  |id| is a human readable
 * identifier for the hook, and can be used to unregister the hook.  If you
 * re-use a hook id, the previous hook function will be replaced.
 * If |before| is |true|, the hook will be called *before* the command executes,
 * if |before| is |false|, or not specified, the hook will be called *after*
 * the command executes.
 */
CommandManager.prototype.addHook =
function cmgr_hook (commandName, func, id, before)
{
    if (!ASSERT(commandName in this.commands,
                "Unknown command '" + commandName + "'"))
    {
        return;
    }
    
    var command = this.commands[commandName];
    
    if (before)
    {
        if (!("beforeHooks" in command))
            command.beforeHooks = new Object();
        command.beforeHooks[id] = func;
    }
    else
    {
        if (!("afterHooks" in command))
            command.afterHooks = new Object();
        command.afterHooks[id] = func;
    }
}

CommandManager.prototype.addHooks =
function cmgr_hooks (hooks, prefix)
{
    if (!prefix)
        prefix = "";
    
    for (var h in hooks)
    {
        this.addHook(h, hooks[h], prefix + ":" + h, 
                     ("_before" in hooks[h]) ? hooks[h]._before : false);
    }
}

CommandManager.prototype.removeHooks =
function cmgr_remhooks (hooks, prefix)
{
    if (!prefix)
        prefix = "";
    
    for (var h in hooks)
    {
        this.removeHook(h, prefix + ":" + h, 
                        ("before" in hooks[h]) ? hooks[h].before : false);
    }
}

CommandManager.prototype.removeHook =
function cmgr_unhook (commandName, id, before)
{
    var command = this.commands[commandName];

    if (before)
        delete command.beforeHooks[id];
    else
        delete command.afterHooks[id];
}

/**
 * Return an array of all CommandRecords which start with the string
 * |partialName|, sorted by |label| property.
 *
 * @param   partialName Prefix to search for.
 * @param   flags       logical ANDed with command flags.
 * @returns array       Array of matching commands, sorted by |label| property.
 */
CommandManager.prototype.list =
function cmgr_list (partialName, flags)
{
    /* returns array of command objects which look like |partialName|, or
     * all commands if |partialName| is not specified */
    function compare (a, b)
    {
        a = a.labelstr.toLowerCase();
        b = b.labelstr.toLowerCase();

        if (a == b)
            return 0;
 
        if (a > b)
            return 1;

        return -1;
    }

    var ary = new Array();
    var commandNames = keys(this.commands);

    /* a command named "eval" wouldn't show up in the result of keys() because
     * eval is not-enumerable, even if overwritten. */
    if ("eval" in this.commands && typeof this.commands.eval == "object")
        commandNames.push ("eval");

    for (var i in commandNames)
    {
        var name = commandNames[i];
        if (!flags || (this.commands[name].flags & flags))
        {
            if (!partialName ||
                this.commands[name].name.indexOf(partialName) == 0)
            {
                if (partialName && 
                    partialName.length == this.commands[name].name.length)
                {
                    /* exact match */
                    return [this.commands[name]];
                }
                else
                {
                    ary.push (this.commands[name]);
                }
            }
        }
    }

    ary.sort(compare);
    return ary;
}

/**
 * Return a sorted array of the command names which start with the string
 * |partialName|.
 *
 * @param   partialName Prefix to search for.
 * @param   flags       logical ANDed with command flags.
 * @returns array       Sorted Array of matching command names.
 */
CommandManager.prototype.listNames =
function cmgr_listnames (partialName, flags)
{
    var cmds = this.list(partialName, flags);
    var cmdNames = new Array();
    
    for (var c in cmds)
        cmdNames.push (cmds[c].name);

    cmdNames.sort();
    return cmdNames;
}

/**
 * Internal use only.
 *
 * Called to parse the arguments stored in |e.inputData|, as properties of |e|,
 * for the CommandRecord stored on |e.command|.
 *
 * @params e  Event object to be processed.
 */
CommandManager.prototype.parseArguments =
function cmgr_parseargs (e)
{
    var rv = this.parseArgumentsRaw(e);
    //dd("parseArguments '" + e.command.usage + "' " + 
    //   (rv ? "passed" : "failed") + "\n" + dumpObjectTree(e));
    delete e.currentArgIndex;
    return rv;
}

/**
 * Internal use only.
 *
 * Don't call parseArgumentsRaw directly, use parseArguments instead.
 *
 * Parses the arguments stored in the |inputData| property of the event object,
 * according to the format specified by the |command| property.
 *
 * On success this method returns true, and propery names corresponding to the
 * argument names used in the format spec will be created on the event object.
 * All optional parameters will be initialized to |null| if not already present
 * on the event.
 *
 * On failure this method returns false and a description of the problem
 * will be stored in the |parseError| property of the event.
 *
 * For example...
 * Given the argument spec "<int> <word> [ <word2> <word3> ]", and given the
 * input string "411 foo", stored as |e.command.usage| and |e.inputData|
 * respectively, this method would add the following propertys to the event
 * object...
 *   -name---value--notes-   
 *   e.int    411   Parsed as an integer
 *   e.word   foo   Parsed as a string
 *   e.word2  null  Optional parameters not specified will be set to null.
 *   e.word3  null  If word2 had been provided, word3 would be required too.
 *
 * Each parameter is parsed by calling the function with the same name, located
 * in this.argTypes.  The first parameter is parsed by calling the function
 * this.argTypes["int"], for example.  This function is expected to act on
 * e.unparsedData, taking it's chunk, and leaving the rest of the string.
 * The default parse functions are...
 *   <word>    parses contiguous non-space characters.
 *   <int>     parses as an int.
 *   <rest>    parses to the end of input data.
 *   <state>   parses yes, on, true, 1, 0, false, off, no as a boolean.
 *   <toggle>  parses like a <state>, except allows "toggle" as well.
 *   <...>     parses according to the parameter type before it, until the end
 *             of the input data.  Results are stored in an array named
 *             paramnameList, where paramname is the name of the parameter
 *             before <...>.  The value of the parameter before this will be
 *             paramnameList[0].
 *
 * If there is no parse function for an argument type, "word" will be used by
 * default.  You can alias argument types with code like...
 * commandManager.argTypes["my-integer-name"] = commandManager.argTypes["int"];
 */
CommandManager.prototype.parseArgumentsRaw =
function parse_parseargsraw (e)
{
    var argc = e.command.argNames.length;
    
    function initOptionals()
    {
        for (var i = 0; i < argc; ++i)
        {
            if (e.command.argNames[i] != ":" && 
                e.command.argNames[i] != "..."  && 
                !(e.command.argNames[i] in e))
            {
                e[e.command.argNames[i]] = null;
            }
        }
    }
        
    if ("inputData" in e && e.inputData)
    {
        /* if data has been provided, parse it */
        e.unparsedData = e.inputData;
        var parseResult;
        var currentArg;
        e.currentArgIndex = 0;

        if (argc)
        {
            currentArg = e.command.argNames[e.currentArgIndex];
        
            while (e.unparsedData)
            {
                if (currentArg != ":")
                {
                    if (!this.parseArgument (e, currentArg))
                        return false;
                }
                if (++e.currentArgIndex < argc)
                    currentArg = e.command.argNames[e.currentArgIndex];
                else
                    break;
            }

            if (e.currentArgIndex < argc && currentArg != ":")
            {
                /* parse loop completed because it ran out of data.  We haven't
                 * parsed all of the declared arguments, and we're not stopped
                 * at an optional marker, so we must be missing something
                 * required... */
                e.parseError = getMsg(MSN_ERR_REQUIRED_PARAM, 
                                      e.command.argNames[e.currentArgIndex]);
                return false;
            }
        }
        
        if (e.unparsedData)
        {
            /* parse loop completed with unparsed data, which means we've
             * successfully parsed all arguments declared.  Whine about the
             * extra data... */
            display (getMsg(MSN_EXTRA_PARAMS, e.unparsedData), MT_WARN);
        }
        else 
        {
            /* we've got no unparsed data, and we're not missing a required
             * argument, go back and fill in |null| for any optional arguments
             * not present on the comand line. */
            initOptionals();
        }
    }
    else
    {
        /* if no data was provided, check to see if the event is good enough
         * on its own. */
        var rv = this.isCommandSatisfied (e);
        if (rv)
            initOptionals();
        return rv;
    }

    return true;
}

/**
 * Returns true if |e| has the properties required to call the command
 * |command|.
 *
 * If |command| is not provided, |e.command| is used instead.
 *
 * @param e        Event object to test against the command.
 * @param command  Command to test.
 */
CommandManager.prototype.isCommandSatisfied =
function cmgr_isok (e, command)
{
    if (typeof command == "undefined")
        command = e.command;
    
    if (!command.enabled)
        return false;

    for (var i = 0; i < command.argNames.length; ++i)
    {
        if (command.argNames[i] == ":")
            return true;
        
        if (!(command.argNames[i] in e))
        {
            e.parseError = getMsg(MSN_ERR_REQUIRED_PARAM, command.argNames[i]);
            //dd("command '" + command.name + "' unsatisfied: " + e.parseError);
            return false;
        }
    }

    //dd ("command '" + command.name + "' satisfied.");
    return true;
}

/**
 * Internal use only.
 * See parseArguments above and the |argTypes| object below.
 *
 * Parses the next argument by calling an appropriate parser function, or the
 * generic "word" parser if none other is found.
 *
 * @param e     event object.
 * @param name  property name to use for the parse result.
 */
CommandManager.prototype.parseArgument =
function cmgr_parsearg (e, name)
{
    var parseResult;
    
    if (name in this.argTypes)
        parseResult = this.argTypes[name](e, name, this);
    else
        parseResult = this.argTypes["word"](e, name, this);

    if (!parseResult)
        e.parseError = getMsg(MSN_ERR_INVALID_PARAM,
                              [name, e.unparsedData]);

    return parseResult;
}

CommandManager.prototype.argTypes = new Object();

/**
 * Convenience function used to map a list of new types to an existing parse
 * function.
 */
CommandManager.prototype.argTypes.__aliasTypes__ =
function at_alias (list, type)
{
    for (var i in list)
    {
        this[list[i]] = this[type];
    }
}

/**
 * Internal use only.
 *
 * Parses an integer, stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["int"] =
function parse_int (e, name)
{
    var ary = e.unparsedData.match (/(\d+)(?:\s+(.*))?$/);
    if (!ary)
        return false;
    e[name] = Number(ary[1]);
    e.unparsedData = arrayHasElementAt(ary, 2) ? ary[2] : "";
    return true;
}

/**
 * Internal use only.
 *
 * Parses a word, which is defined as a list of nonspace characters.
 *
 * Stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["word"] =
function parse_word (e, name)
{
    var ary = e.unparsedData.match (/(\S+)(?:\s+(.*))?$/);
    if (!ary)
        return false;
    e[name] = ary[1];
    e.unparsedData = arrayHasElementAt(ary, 2) ? ary[2] : "";
    return true;
}

/**
 * Internal use only.
 *
 * Parses a "state" which can be "true", "on", "yes", or 1 to indicate |true|,
 * or "false", "off", "no", or 0 to indicate |false|.
 *
 * Stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["state"] =
function parse_state (e, name)
{
    var ary =
        e.unparsedData.match (/(true|on|yes|1|false|off|no|0)(?:\s+(.*))?$/i);
    if (!ary)
        return false;
    if (ary[1].search(/true|on|yes|1/i) != -1)
        e[name] = true;
    else
        e[name] = false;
    e.unparsedData = arrayHasElementAt(ary, 2) ? ary[2] : "";
    return true;
}

/**
 * Internal use only.
 *
 * Parses a "toggle" which can be "true", "on", "yes", or 1 to indicate |true|,
 * or "false", "off", "no", or 0 to indicate |false|.  In addition, the string
 * "toggle" is accepted, in which case |e[name]| will be the string "toggle".
 *
 * Stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["toggle"] =
function parse_toggle (e, name)
{
    var ary = e.unparsedData.match
        (/(toggle|true|on|yes|1|false|off|no|0)(?:\s+(.*))?$/i);

    if (!ary)
        return false;
    if (ary[1].search(/toggle/i) != -1)
        e[name] = "toggle";
    else if (ary[1].search(/true|on|yes|1/i) != -1)
        e[name] = true;
    else
        e[name] = false;
    e.unparsedData = arrayHasElementAt(ary, 2) ? ary[2] : "";
    return true;
}

/**
 * Internal use only.
 *
 * Returns all unparsed data to the end of the line.
 *
 * Stores result in |e[name]|.
 */
CommandManager.prototype.argTypes["rest"] =
function parse_rest (e, name)
{
    e[name] = e.unparsedData;
    e.unparsedData = "";
    return true;
}

/**
 * Internal use only.
 *
 * Parses the rest of the unparsed data the same way the previous argument was
 * parsed.  Can't be used as the first parameter.  if |name| is "..." then the
 * name of the previous argument, plus the suffix "List" will be used instead.
 *
 * Stores result in |e[name]| or |e[lastName + "List"]|.
 */
CommandManager.prototype.argTypes["..."] =
function parse_repeat (e, name, cm)
{
    ASSERT (e.currentArgIndex > 0, "<...> can't be the first argument.");
    
    var lastArg = e.command.argNames[e.currentArgIndex - 1];
    if (lastArg == ":")
        lastArg = e.command.argNames[e.currentArgIndex - 2];

    var listName = lastArg + "List";
    e[listName] = [e[lastArg]];
    
    while (e.unparsedData)
    {
        if (!cm.parseArgument(e, lastArg))
            return false;
        e[listName].push(e[lastArg]);
    }    

    e[lastArg] = e[listName][0];
    return true;
}
