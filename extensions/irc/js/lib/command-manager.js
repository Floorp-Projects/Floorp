/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is JSIRC Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

function CCommandManager ()
{

    this.commands = new Array();

}

CCommandManager.prototype.add =
function cmgr_add (name, func, usage, help)
{
    function compare (a, b)
    {
        if (a.name == b.name)
            return 0;
        else
            if (a.name > b.name)
                return 1;
            else
                return -1;
    }
    
    this.commands.push ({name: name, func: func, usage: usage, help: help});
    this.commands = this.commands.sort(compare);
    
}

CCommandManager.prototype.list =
function cmgr_list (partialName)
{
    /* returns array of command objects which look like |partialName|, or
     * all commands if |partialName| is not specified */

    if ((typeof partialName == "undefined") ||
        (String(partialName) == ""))
        return this.commands;

    var ary = new Array();

    for (var i in this.commands)
    {
        if (this.commands[i].name.indexOf(partialName) == 0)
                ary.push (this.commands[i]);
    }

    return ary;

}

CCommandManager.prototype.listNames =
function cmgr_listnames (partialName)
{
    var cmds = this.list(partialName);
    var cmdNames = new Array();
    
    for (var c in cmds)
        cmdNames.push (cmds[c].name);
    
    return (cmdNames.length > 0) ? cmdNames.sort() : [];
}
