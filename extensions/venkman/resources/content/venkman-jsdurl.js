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

const JSD_URL_SCHEME       = "x-jsd:";
const JSD_URL_PREFIX       = /x-jsd:/i;
const JSD_SCHEME_LEN       = JSD_URL_SCHEME.length;

const JSD_SERVICE_HELP     = "help";
const JSD_SERVICE_SOURCE   = "source";
const JSD_SERVICE_PPRINT   = "pprint";
const JSD_SERVICE_PPBUFFER = "ppbuffer";

function initJSDURL()
{   
    var prefs =
        [
         ["services.help.css", "chrome://venkman/skin/venkman-help.css"],
         ["services.help.template", "chrome://venkman/locale/venkman-help.tpl"],
         ["services.source.css", "chrome://venkman/skin/venkman-source.css"],
         ["services.source.colorize", true],
         ["services.source.colorizeLimit", 1500]
        ];

    console.prefManager.addPrefs(prefs);
}

/*
 * x-jsd:<service>[?[<property>=<value>[(&<property>=<value>) ...]]][#anchor]
 *
 * <service> is one of...
 *
 *   x-jsd:help   - Help system
 *                   properties are:
 *                     search   - text to search for
 *                     within   - bitmap of fields to search within
 *                                 0x01 - search in command names
 *                                 0x02 - search in ui labels
 *                                 0x04 - search in help text
 *
 *   x-jsd:source - Source text
 *                   properties are:
 *                     url      - source url
 *                     instance - index of source instance
 *
 *   x-jsd:pprint - Pretty Printed source text
 *                   properties are:
 *                     script   - tag of script to pretty print
 *
 *   x-jsd:ppbuffer - URL of a function created internally for pretty printing.
 *                    You may come across this in jsdIScript objects, but it
 *                    should not be used elsewhere.
 *                     properties are:
 *                      type - "function" or "script"
 */

console.parseJSDURL = parseJSDURL;
function parseJSDURL (url)
{
    var ary;
    
    if (url.search(JSD_URL_PREFIX) != 0)
        return null;
    
    ary = url.substr(JSD_SCHEME_LEN).match(/([^?#]+)(?:\?(.*))?/);
    if (!ary)
        return null;

    var parseResult = new Object();
    parseResult.spec = url;
    parseResult.service = ary[1].toLowerCase();
    var rest = ary[2];
    if (rest)
    {
        ary = rest.match(/([^&#]+)/);

        while (ary)
        {
            rest = RegExp.rightContext.substr(1);
            var assignment = ary[1];
            ary = assignment.match(/(.+)=(.*)/);
            if (ASSERT(ary, "error parsing ``" + assignment + "'' from " + url))
            {
                var name = decodeURIComponent(ary[1]);
                /* only set the property the first time we see it */
                if (arrayHasElementAt(ary, 2) && !(name in parseResult))
                    parseResult[name] = decodeURIComponent(ary[2]);
            }
            ary = rest.match(/([^&#]+)/);
        }
    }
    
    //dd (dumpObjectTree(parseResult));
    return parseResult;
}

console.loadServiceTemplate =
function con_loadservicetpl (name, sections, callback)
{
    function onComplete (data, url, status)
    {
        if (status == Components.results.NS_OK)
        {
            var tpl = parseSections (data, sections);
            for (var p in sections)
            {
                if (!(p in tpl))
                {
                    display (getMsg (MSN_ERR_NO_SECTION, [sections[p], url]),
                             MT_ERROR);
                    callback(name, Components.results.NS_ERROR_FAILURE);
                    return;
                }
            }
            console.serviceTemplates[name] = tpl;
        }
        callback(name, status);
    };
        
    if (name in console.serviceTemplates)
    {
        callback(name, Components.results.NS_OK);
        return;
    }

    var prefName = "services." + name + ".template";
    if (!(prefName in console.prefs))
    {
        display (getMsg (MSN_ERR_NO_TEMPLATE, prefName), MT_ERROR);
        callback(name, Components.results.NS_ERROR_FILE_NOT_FOUND);
        return;
    }
    
    var url = console.prefs[prefName];
    loadURLAsync (url, { onComplete: onComplete });
}
        
console.asyncOpenJSDURL = asyncOpenJSDURL;
function asyncOpenJSDURL (channel, streamListener, context)
{    
    function onTemplateLoaded (name, status)
    {
        if (status != Components.results.NS_OK)
        {
            response.start();
            response.append(getMsg(MSN_JSDURL_ERRPAGE,
                                   [safeHTML(url),
                                    getMsg(MSN_ERR_JSDURL_TEMPLATE, name)]));
            response.end();
        }
        else
        {
            tryService();
        }
    };
    
    function tryService ()
    {
        var serviceObject = console.services[service];
        if ("requiredTemplates" in serviceObject)
        {
            for (var i = 0; i < serviceObject.requiredTemplates.length; ++i)
            {
                var def = serviceObject.requiredTemplates[i];
                if (!(def[0] in console.serviceTemplates))
                {
                    console.loadServiceTemplate (def[0], def[1],
                                                 onTemplateLoaded);
                    return;
                }
            }                
        }

        console.services[service](response, parseResult);
    };

    var url = channel.URI.spec;
    var response = new JSDResponse (channel, streamListener, context);
    var parseResult = parseJSDURL (url);
    if (!parseResult)
    {
        response.start();
        response.append(getMsg(MSN_JSDURL_ERRPAGE, [safeHTML(url),
                                                    MSG_ERR_JSDURL_PARSE]));
        response.end();
        return;
    }

    var service = parseResult.service.toLowerCase();
    if (!(service in console.services))
        service = "unknown";

    tryService();
}

console.serviceTemplates = new Object();
console.services = new Object();

console.services["unknown"] =
function svc_nosource (response, parsedURL)
{
    response.start();
    response.append(getMsg(MSN_JSDURL_ERRPAGE, [safeHTML(parsedURL.spec),
                                                MSG_ERR_JSDURL_NOSERVICE]));
    response.end();
}

console.services["ppbuffer"] =
function svc_nosource (response)
{
    response.start();
    response.append(getMsg(MSN_JSDURL_ERRPAGE, [safeHTML(parsedURL.spec),
                                                MSG_ERR_JSDURL_NOSOURCE]));
    response.end();
}

console.services["help"] =
function svc_help (response, parsedURL)
{
    const CHUNK_DELAY = 100;
    const CHUNK_SIZE  = 150;
    
    function processHelpChunk (start)
    {
        var stop = Math.min (commandList.length, start + CHUNK_SIZE);

        for (var i = start; i < stop; ++i)
        {
            command = commandList[i];
            if (!("htmlHelp" in command))
            {
                function replaceBold (str, p1)
                {
                    return "<b>" + p1 + "</b>";
                };
                
                function replaceParam (str, p1)
                {
                    return "&lt;<span class='param'>" + p1 + "</span>&gt;";
                };
                
                function replaceCommand (str, p1)
                {
                    if (p1.indexOf(" ") != -1)
                    {
                        var ary = p1.split (" ");
                        for (var i = 0; i < ary.length; ++i)
                        {
                            ary[i] = replaceCommand (null, ary[i]);
                        }

                        return ary.join (" ");
                    }
                    
                    if (p1 != command.name &&
                        (p1 in console.commandManager.commands))
                    {
                        return ("<a class='command-name' " +
                                "href='" + JSD_URL_SCHEME + "help?search=" +
                                p1 + "'>" + p1 + "</a>");
                    }

                    return "<tt>" + p1 + "</tt>";
                };
                    
                
                var htmlUsage = command.usage.replace(/<([^\s>]+)>/g,
                                                      replaceParam);
                var htmlDesc = command.help.replace(/<([^\s>]+)>/g,
                                                    replaceParam);
                htmlDesc = htmlDesc.replace (/\*([^\*]+)\*/g, replaceBold);
                htmlDesc = htmlDesc.replace (/\|([^\|]+)\|/g, replaceCommand);

                // remove trailing access key (non en-US locales) and ...
                var trimmedLabel = 
                    command.labelstr.replace(/(\([a-zA-Z]\))?(\.\.\.)?$/, "");

                vars = {
                    "\\$command-name": command.name,
                    "\\$ui-label-safe": encodeURIComponent(trimmedLabel),
                    "\\$ui-label": fromUnicode(command.labelstr,
                                               MSG_REPORT_CHARSET),
                    "\\$params": fromUnicode(htmlUsage, MSG_REPORT_CHARSET),
                    "\\$key": command.keystr,
                    "\\$desc": fromUnicode(htmlDesc, MSG_REPORT_CHARSET)
                };
                
                command.htmlHelp = replaceStrings (section, vars);
            }
            
            response.append(command.htmlHelp);
        }

        if (i != commandList.length)
        {
            setTimeout (processHelpChunk, CHUNK_DELAY, i);
        }
        else
        {
            response.append(tpl["footer"]);
            response.end();
        }
    };
    
    function compare (a, b)
    {
        if (parsedURL.within & WITHIN_LABEL)
        {
            a = a.labelstr.toLowerCase();
            b = b.labelstr.toLowerCase();
        }
        else
        {
            a = a.name;
            b = b.name;
        }    

        if (a == b)
            return 0;
 
        if (a > b)
            return 1;

        return -1;
    };
    
    var command;
    var commandList = new Array();
    var hasSearched;
    var tpl = console.serviceTemplates["help"];

    var WITHIN_NAME     = 0x01;
    var WITHIN_LABEL    = 0x02;
    var WITHIN_DESC     = 0x04;

    if ("search" in parsedURL)
    {
        try
        {
            parsedURL.search = new RegExp (parsedURL.search, "i");
        }
        catch (ex)
        {
            response.start();
            response.append(getMsg(MSN_JSDURL_ERRPAGE,
                                   [parsedURL.spec, MSG_ERR_JSDURL_SEARCH]));
            response.end();
            return;
        }
        
        dd ("searching for " + parsedURL.search);
        
        if (!("within" in parsedURL) ||
            !((parsedURL.within = parseInt(parsedURL.within)) & 0x07))
        {
            parsedURL.within = WITHIN_NAME;
        }
        
        for (var c in console.commandManager.commands)
        {
            command = console.commandManager.commands[c];
            if ((parsedURL.within & WITHIN_NAME) &&
                command.name.search(parsedURL.search) != -1)
            {
                commandList.push (command);
            }
            else if ((parsedURL.within & WITHIN_LABEL) &&
                     command.labelstr.search(parsedURL.search) != -1)
            {
                commandList.push (command);
            }
            else if ((parsedURL.within & WITHIN_DESC) &&
                     command.help.search(parsedURL.search) != -1)
            {
                commandList.push (command);
            }
        }

        hasSearched = commandList.length > 0 ? "true" : "false";
    }
    else
    {
        commandList = console.commandManager.list ("", CMD_CONSOLE);
        hasSearched = "false";
    }

    commandList.sort(compare);
    
    response.start();

    var vars = {
        "\\$css": console.prefs["services.help.css"],
        "\\$match-count": commandList.length,
        "\\$has-searched": hasSearched,
        "\\$report-charset": MSG_REPORT_CHARSET
    };
            
    response.append(replaceStrings(tpl["header"], vars));

    if (commandList.length == 0)
    {
        response.append(tpl["nomatch"]);
        response.append(tpl["footer"]);
        response.end();
    }
    else
    {
        var section = tpl["command"];
        processHelpChunk(0)
    }
}

console.services["help"].requiredTemplates =
[
 ["help", { "header"  : /@-header-end/mi,
            "command" : /@-command-end/mi,
            "nomatch" : /@-nomatch-end/mi,
            "footer"  : 0 }
 ]
];

const OTHER   = 0;
const COMMENT = 1;
const STRING1 = 2;
const STRING2 = 3;
const WORD    = 4;
const NUMBER  = 5;

var keywords = {
    "abstract": 1, "boolean": 1, "break": 1, "byte": 1, "case": 1, "catch": 1,
    "char": 1, "class": 1, "const": 1, "continue": 1, "debugger": 1,
    "default": 1, "delete": 1, "do": 1, "double": 1, "else": 1, "enum": 1,
    "export": 1, "export": 1, "extends": 1, "false": 1, "final": 1,
    "finally": 1, "float": 1, "for": 1, "function": 1, "goto": 1, "if": 1,
    "implements": 1, "import": 1, "in": 1, "instanceof": 1, "int": 1,
    "interface": 1, "long": 1, "native": 1, "new": 1, "null": 1,
    "package": 1, "private": 1, "protected": 1, "public": 1, "return": 1,
    "short": 1, "static": 1, "switch": 1, "synchronized": 1, "this": 1,
    "throw": 1, "throws": 1, "transient": 1, "true": 1, "try": 1,
    "typeof": 1, "var": 1, "void": 1, "while": 1, "with": 1
};

var specialChars = /[&<>]/g;

var wordStart = /[\w\\\$]/;
var numberStart = /[\d]/;

var otherEnd = /[\w\$\"\']|\\|\/\/|\/\*/;
var wordEnd = /[^\w\$]/;
var string1End = /\'/;
var string2End = /\"/;
var commentEnd = /\*\//;
var numberEnd = /[^\d\.]/;

function escapeSpecial (p1)
{
    switch (p1)
    {
        case "&":
            return "&amp;";
        case "<":
            return "&lt;";
        case ">":
            return "&gt;";
    }

    return p1;
};
    
function escapeSourceLine (line)
{
    return { line: line.replace (specialChars, escapeSpecial),
             previousState: 0 };
}

function colorizeSourceLine (line, previousState)
{
    function closePhrase (phrase)
    {
        if (!phrase)
        {
            previousState = OTHER;
            return;
        }

        switch (previousState)
        {
            case COMMENT:
                result += "<c>" + phrase.replace (specialChars, escapeSpecial) +
                    "</c>";
                break;            
            case STRING1:
            case STRING2:
                result += "<t>" + phrase.replace (specialChars, escapeSpecial) +
                    "</t>";
                break;
            case WORD:
                if (phrase in keywords)
                    result += "<k>" + phrase + "</k>";
                else
                    result += phrase.replace (specialChars, escapeSpecial);
                break;
            case OTHER:
                phrase = phrase.replace (specialChars, escapeSpecial);
                /* fall through */
            case NUMBER:
                result += phrase;
                break;
        }
    };
    
    var result = "";
    var pos;
    var ch, ch2;
    var expr;
    
    while (line.length > 0)
    {
        /* scan a line of text.  |pos| always one *past* the end of the
         * phrase we just scanned. */

        switch (previousState)
        {
            case OTHER:
                /* look for the end of an uncalssified token, like whitespace
                 * or an operator. */
                pos = line.search (otherEnd);
                break;

            case WORD:
                /* look for the end of something that qualifies as
                 * an identifier. */
                pos = line.search(wordEnd);
                while (pos > -1 && line[pos] == "\\")
                {
                    /* if we ended with a \ character, then the slash
                     * and the character after it are part of this word.
                     * the characters following may also be part of the
                     * word. */
                    pos += 2;
                    var newPos = line.substr(pos).search(wordEnd);
                    if (newPos > -1)
                        pos += newPos;
                }
                break;

            case STRING1:
            case STRING2:
                /* look for the end of a single or double quoted string. */
                if (previousState == STRING1)
                {
                    ch = "'";
                    expr = string1End;
                }
                else
                {
                    ch = "\"";
                    expr = string2End;
                }

                if (line[0] == ch)
                {
                    pos = 1;
                }
                else
                {
                    pos = line.search (expr);
                    if (pos > 0 && line[pos - 1] == "\\")
                    {
                        /* arg, the quote we found was escaped, fall back
                         * to scanning a character at a time. */
                        var done = false;
                        for (pos = 0; !done && pos < line.length; ++pos)
                        {
                            if (line[pos] == "\\")
                                ++pos;
                            else if (line[pos] == ch)
                                done = true;
                        }        
                    }
                    else
                    {
                        if (pos != -1)
                            ++pos;
                    }
                }
                break;

            case COMMENT:
                /* look for the end of a slash-star comment,
                 * slash-slash comments are handled down below, because
                 * we know for sure that it's the last phrase on this line.
                 */
                pos = line.search (commentEnd);
                if (pos != -1)
                    pos += 2;
                break;

            case NUMBER:
                /* look for the end of a number */
                pos = line.search (numberEnd);
                break; 
        }

        if (pos == -1)
        {
            /* couldn't find an end for the current state, close out the 
             * rest of the line.
             */
            closePhrase(line);
            line = "";
        }
        else
        {
            /* pos has a non -1 value, close out what we found, and move
             * along. */
            if (previousState == STRING1 || previousState == STRING2)
            {
                /* strings are a special case because they actually are
                 * considered to start *after* the leading quote,
                 * and they end *before* the trailing quote. */
                if (pos == 1)
                {
                    /* empty string */
                    previousState = OTHER;
                }
                else
                {
                    /* non-empty string, close out the contents of the
                     * string. */
                    closePhrase(line.substr (0, pos - 1));
                    previousState = OTHER;
                }

                /* close the trailing quote. */
                result += line[pos - 1];
            }
            else
            {
                /* non-string phrase, close the whole deal. */
                closePhrase(line.substr (0, pos));
                previousState = OTHER;
            }

            if (pos)
                line = line.substr (pos);
        }

        if (line)
        {
            /* figure out what the next token looks like. */
            ch = line[0];
            ch2 = (line.length > 1) ? line[1] : "";
            
            if (ch.search (wordStart) == 0)
            {
                previousState = WORD;
            }
            else if (ch == "'")
            {
                result += "'";
                line = line.substr(1);
                previousState = STRING1;
            }
            else if (ch == "\"")
            {
                result += "\"";
                line = line.substr(1);
                previousState = STRING2;
            }
            else if (ch == "/" && ch2 == "*")
            {
                previousState = COMMENT;
            }
            else if (ch == "/" && ch2 == "/")
            {
                /* slash-slash comment, the last thing on this line. */
                previousState = COMMENT;
                closePhrase(line);
                previousState = OTHER;
                line = "";
            }
            else if (ch.search (numberStart) == 0)
            {
                previousState = NUMBER;
            }
        }
    }

    return { previousState: previousState, line: result };
}

console.respondWithSourceText =
function con_respondsourcetext (response, sourceText)
{
    const CHUNK_DELAY = 50;
    const CHUNK_SIZE  = 250;
    var sourceLines = sourceText.lines;
    var resultSource;
    var tenSpaces = "          ";
    var maxDigits;
    
    var previousState = 0;

    var mungeLine;

    if (console.prefs["services.source.colorize"] && 
        sourceLines.length <= console.prefs["services.source.colorizeLimit"])
    {
        mungeLine = colorizeSourceLine;
    }
    else
    {
        mungeLine = escapeSourceLine;
    }

    function processSourceChunk (start)
    {
        dd ("processSourceChunk " + start + " {");
                    
        var stop = Math.min (sourceLines.length, start + CHUNK_SIZE);
        
        for (var i = start; i < stop; ++i)
        {
            var padding;
            if (i != 999)
            {
                padding =
                    tenSpaces.substr(0, maxDigits -
                                     Math.floor(Math.log(i + 1) / Math.LN10));
            }
            else
            {
                /* at exactly 1000, a rounding error gets us. */
                padding = tenSpaces.substr(0, maxDigits - 3);
            }    

            var isExecutable;
            var marginContent;
            if ("lineMap" in sourceText && i in sourceText.lineMap)
            {
                if (sourceText.lineMap[i] & LINE_BREAKABLE)
                {
                    isExecutable = "t";
                    marginContent = " - ";
                }
                else
                {
                    isExecutable = "f";
                    marginContent = "   ";
                }
            }
            else
            {
                isExecutable = "f";
                marginContent = "   ";
            }
            
            var o = mungeLine(sourceLines[i], previousState);
            var line = o.line;
            previousState = o.previousState;

            resultSource += "<line><margin x='" + isExecutable +"'>" +
                marginContent + "</margin>" +
                "<num>" + padding + (i + 1) +
                "</num> " + line + "</line>\n";
               
        }

        if (i != sourceLines.length)
        {
            setTimeout (processSourceChunk, CHUNK_DELAY, i);
        }
        else
        {
            resultSource += "</source-listing>";
            //resultSource += "</source-listing></body></html>";
            response.append(resultSource);
            response.end();
            sourceText.markup = resultSource;
            dd ("}");
        }

        dd ("}");
    };
    
    if ("charset" in sourceText)
        response.channel.contentCharset = sourceText.charset;
    
    if ("markup" in sourceText)
    {
        response.channel.contentType = "text/xml";
        response.start();
        response.append(sourceText.markup);
        response.end();
    }
    else
    {
        maxDigits = Math.floor(Math.log(sourceLines.length) / Math.LN10) + 1;
        dd ("OFF building response {");
        response.channel.contentType = "text/xml";
        resultSource = "<?xml version='1.0'";
        //        if ("charset" in sourceText)
        //    resultSource += " encoding=\"" + sourceText.charset + "\"";
        resultSource += "?>\n" +
            "<?xml-stylesheet type='text/css' href='" +
            console.prefs["services.source.css"] + "' ?>\n" +
            "<source-listing id='source-listing'>\n";
        
        /*
          resultSource = "<html><head>\n" +
          "<link rel='stylesheet' type='text/css' href='" +
          console.prefs["services.source.css"] + "'><body>\n" +
          "<source-listing id='source-listing'>\n";
        */
        response.start();
        
        processSourceChunk (0);
    }
}

console.services["pprint"] =
function svc_pprint (response, parsedURL)
{    
    var err;
    
    if (!("scriptWrapper" in parsedURL))
    {
        err = getMsg(MSN_ERR_REQUIRED_PARAM, "scriptWrapper");
        response.start();
        response.append(getMsg(MSN_JSDURL_ERRPAGE, [safeHTML(parsedURL.spec),
                                                    err]));
        response.end();
        return;
    }

    if (!(parsedURL.scriptWrapper in console.scriptWrappers))
    {
        err = getMsg(MSN_ERR_INVALID_PARAM,
                         ["scriptWrapper", parsedURL.scriptWrapper]);
        response.start();
        response.append(getMsg(MSN_JSDURL_ERRPAGE, [safeHTML(parsedURL.spec),
                                                    err]));
        response.end();
        return;
    }

    var sourceText = console.scriptWrappers[parsedURL.scriptWrapper].sourceText;
    console.respondWithSourceText (response, sourceText);
}

console.services["source"] =
function svc_source (response, parsedURL)
{
    function onSourceTextLoaded (status)
    {
        if (status != Components.results.NS_OK)
        {
            response.start();
            response.append(getMsg(MSN_JSDURL_ERRPAGE,
                                   [safeHTML(parsedURL.spec), status]));
            response.end();
            display (getMsg (MSN_ERR_SOURCE_LOAD_FAILED,
                             [parsedURL.spec, status]),
                     MT_ERROR);
            return;
        }

        console.respondWithSourceText (response, sourceText);
    }

    if (!("location" in parsedURL) || !parsedURL.location)
    {
        var err = getMsg(MSN_ERR_REQUIRED_PARAM, "location");
        response.start();
        response.append(getMsg(MSN_JSDURL_ERRPAGE, [safeHTML(parsedURL.spec),
                                                    err]));
        response.end();
        return;
    }
        
    var sourceText;
    var targetURL = parsedURL.location;
    
    if (targetURL in console.scriptManagers)
    {
        var scriptManager = console.scriptManagers[targetURL];
        if ("instance" in parsedURL)
        {
            var instance =
                scriptManager.getInstanceBySequence(parsedURL.instance);
            if (instance)
                sourceText = instance.sourceText;
        }
        
        if (!sourceText)
            sourceText = scriptManager.sourceText;
    }
    else
    {
        if (targetURL in console.files)
            sourceText = console.files[targetURL];
        else
            sourceText = console.files[targetURL] = new SourceText (targetURL);
    }

    if (!sourceText)
    {
        response.start();
        response.append(getMsg(MSN_JSDURL_ERRPAGE, [safeHTML(parsedURL.spec),
                                                    MSG_ERR_JSDURL_SOURCETEXT]));
        response.end();
        return;
    }

    if (!sourceText.isLoaded)
        sourceText.loadSource (onSourceTextLoaded);
    else
        onSourceTextLoaded(Components.results.NS_OK);    
}

function JSDResponse (channel, streamListener, context)
{
    this.hasStarted     = false;
    this.hasEnded       = false;
    this.channel        = channel;
    this.streamListener = streamListener;
    this.context        = context;
}

JSDResponse.prototype.start =
function jsdr_start()
{
    if (!ASSERT(!this.hasStarted, "response already started"))
        return;
    
    this.streamListener.onStartRequest (this.channel, this.context);
    this.hasStarted = true;
}

JSDResponse.prototype.append =
function jsdr_append (str)
{
    //dd ("appending\n" + str);

    const STRING_STREAM_CTRID = "@mozilla.org/io/string-input-stream;1";
    const nsIStringInputStream = Components.interfaces.nsIStringInputStream;
    const I_LOVE_NECKO = 2152398850;

    var clazz = Components.classes[STRING_STREAM_CTRID];
    var stringStream = clazz.createInstance(nsIStringInputStream);

    var len = str.length;
    stringStream.setData (str, len);
    try
    {
        this.streamListener.onDataAvailable (this.channel, this.context, 
                                             stringStream, 0, len);
    }
    catch (ex)
    {
        if ("result" in ex && ex.result == I_LOVE_NECKO)
        {
            /* ignore this exception, it means the caller doesn't want the
             * data, or something.
             */
        }
        else
        {
            throw ex;
        }
    }
}

JSDResponse.prototype.end =
function jsdr_end ()
{
    if (!ASSERT(this.hasStarted, "response hasn't started"))
        return;
    
    if (!ASSERT(!this.hasEnded, "response has already ended"))
        return;
    
    var ok = Components.results.NS_OK;
    this.streamListener.onStopRequest (this.channel, this.context, ok);
    if (this.channel.loadGroup)
        this.channel.loadGroup.removeRequest (this.channel, null, ok);
    else
        dd ("channel had no load group");
    this.channel._isPending = false;
    this.hasEnded = true;
    //dd ("response ended");
}
