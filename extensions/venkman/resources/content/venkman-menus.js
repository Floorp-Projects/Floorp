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

function initMenus()
{
    var lastMenu;
    var cm = console.commandManager;

    /*
     * The C(), M(), m(), and t() functions are defined below.  They are
     * Venkman specific wrappers around calls the the CommandManager.
     *
     * C(id, name)
     *  Creates a new context menu attached to the object with the id |id|.
     *  The label will be derived from the string named "mnu." + |name|.
     *
     * M(parent, name)
     *  Creates a new menu dropdown in an existing menu bar with the id |parent|.
     *  The label will be derived from the string named "mnu." + |name|.
     *
     * m(commandName)
     *  Creates a menuitem for the command named commandName.
     *
     * t(parent, commandName)
     *  Creates a toolbaritem for the command named |commandName| in the toolbar
     *  with the id |parent|.
     */

    /* main toolbar */
    t("maintoolbar", "stop");
    t("maintoolbar", "-");
    t("maintoolbar", "cont");
    t("maintoolbar", "next");
    t("maintoolbar", "step");
    t("maintoolbar", "finish");
    t("maintoolbar", "-");
    t("maintoolbar", "profile-tb");
    t("maintoolbar", "pprint");


    M("mainmenu", "file");
     m("open-url");
     m("find-file");
     m("-");
     m("close");
     m("save-source");
     m("save-profile");
     m("-");
     m("quit");
    
    /* View menu */
    M("mainmenu", "view");
     m("reload");
     m("pprint",        {type: "checkbox",
                         checkedif: "console.sourceView.prettyPrint"});
     m("-");
     m("toggle-chrome", {type: "checkbox",
                         checkedif: "console.enableChromeFilter"});
     
    /* Debug menu */
    M("mainmenu", "debug");
     m("stop", {type: "checkbox",
                checkedif: "console.jsds.interruptHook"});
     m("cont");
     m("next");
     m("step");
     m("finish");
     m("-");
     m("em-ignore", {type: "radio", name: "em",
                     checkedif: "console.errorMode == EMODE_IGNORE"});
     m("em-trace",  {type: "radio", name: "em",
                     checkedif: "console.errorMode == EMODE_TRACE"});
     m("em-break",  {type: "radio", name: "em",
                     checkedif: "console.errorMode == EMODE_BREAK"});
     m("-");
     m("tm-ignore", {type: "radio", name: "tm",
                     checkedif: "console.throwMode == TMODE_IGNORE"});
     m("tm-trace",  {type: "radio", name: "tm",
                     checkedif: "console.throwMode == TMODE_TRACE"});
     m("tm-break",  {type: "radio", name: "tm",
                     checkedif: "console.throwMode == TMODE_BREAK"});
     m("-");
     m("toggle-ias",
         {type: "checkbox",
          checkedif: "console.jsds.initAtStartup"});

    M("mainmenu", "profile");
     m("toggle-profile", {type: "checkbox",
                          checkedif:
                            "console.jsds.flags & COLLECT_PROFILE_DATA"});
     m("clear-profile");
     m("save-profile");

    /* Context menu for console view */
    C("output-iframe", "console");
     m("stop", {type: "checkbox",
                checkedif: "console.jsds.interruptHook"});
     m("cont");
     m("next");
     m("step");
     m("finish");
     m("-");
     m("em-ignore", {type: "radio", name: "em",
                     checkedif: "console.errorMode == EMODE_IGNORE"});
     m("em-trace",  {type: "radio", name: "em",
                     checkedif: "console.errorMode == EMODE_TRACE"});
     m("em-break",  {type: "radio", name: "em",
                     checkedif: "console.errorMode == EMODE_BREAK"});
     m("-");
     m("tm-ignore", {type: "radio", name: "tm",
                     checkedif: "console.throwMode == TMODE_IGNORE"});
     m("tm-trace",  {type: "radio", name: "tm",
                     checkedif: "console.throwMode == TMODE_TRACE"});
     m("tm-break",  {type: "radio", name: "tm",
                     checkedif: "console.throwMode == TMODE_BREAK"});
     
    /* Context menu for project view */
    C("project-tree", "project");
     m("find-url");
     m("-");
     m("clear-all", {enabledif:
                     "cx.target instanceof BPRecord || " +
                     "(has('breakpointLabel') && cx.target.childData.length)"});
     m("clear");
     m("-");
     m("save-profile", {enabledif: "has('url')"});

    /* Context menu for source view */
    C("source-tree", "source");
     m("save-source");
     m("-");
     m("break",  {enabledif: "cx.lineIsExecutable && !has('breakpointRec')"});
     m("fbreak", {enabledif: "!cx.lineIsExecutable && !has('breakpointRec')"});
     m("clear");
     m("-");
     m("cont");
     m("next");
     m("step");
     m("finish");
     m("-");
     m("pprint", {type: "checkbox",
                  checkedif: "console.sourceView.prettyPrint"});

    /* Context menu for script view */
    C("script-list-tree", "script");
     m("find-url");
     m("find-script");
     m("clear-script", {enabledif: "cx.target.bpcount"});
     m("-");
     m("save-profile");
     m("clear-profile");
     
    /* Context menu for stack view */
    C("stack-tree", "stack");
     m("frame",        {enabledif: "cx.target instanceof FrameRecord"});
     m("find-creator", {enabledif: "cx.target instanceof ValueRecord && " +
                                   "cx.target.jsType == jsdIValue.TYPE_OBJECT"});
     m("find-ctor",    {enabledif: "cx.target instanceof ValueRecord && " +
                                   "cx.target.jsType == jsdIValue.TYPE_OBJECT"});
    
    function M(parent, id, attribs)
    {
        lastMenu = parent + ":" + id;
        console.commandManager.appendSubMenu(parent, lastMenu,
                                             getMsg("mnu." + id));
    }

    function C(elementId, id)
    {
        lastMenu = "popup:" + id;
        console.commandManager.appendPopupMenu("dynamicPopups", lastMenu,
                                               getMsg("popup." + id));
        var elemObject = document.getElementById(elementId);
        elemObject.setAttribute ("context", lastMenu);
    }
        
    function m(command, attribs)
    {            
        if (command != "-")
        {
            if (!(command in cm.commands))
            {
                dd("no such command: " + command)
                    return;
            }
            command = cm.commands[command];
        }
        console.commandManager.appendMenuItem(lastMenu, command, attribs);
    }
    
    function t(parent, command, attribs)
    {
        if (command != "-")
            command = cm.commands[command];
        console.commandManager.appendToolbarItem(parent, command, attribs);
    }
}
