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

function initMenus()
{    
    function onMenuCommand (event, window)
    {
        var params;
        var commandName = event.originalTarget.getAttribute("commandname");
        if ("cx" in console.menuManager && console.menuManager.cx)
        {
            console.menuManager.cx.sourceWindow = window;
            params = console.menuManager.cx;
        }
        else
        {
            params = { sourceWindow: window };
        }
            
        dispatch (commandName, params);

        delete console.menuManager.cx;
    };
    
    console.onMenuCommand = onMenuCommand;
    console.menuSpecs = new Object();
    var menuManager = 
        console.menuManager = new MenuManager(console.commandManager,
                                              console.menuSpecs,
                                              getCommandContext,
                                              "console.onMenuCommand(event, " +
                                              "window);");

    console.menuSpecs["maintoolbar"] = {
        items:
        [
         ["stop"],
         ["-"],
         ["cont"],
         ["next"],
         ["step"],
         ["finish"],
         ["-"],
         ["profile-tb"],
         ["toggle-pprint"]
        ]
    };

    console.menuSpecs["mainmenu:file"] = {
        label: MSG_MNU_FILE,
        items:
        [
         ["open-url"],
         ["find-file"],
         ["close-source-tab",
                 {enabledif: "console.views.source2.currentContent && " +
                             "console.views.source2.sourceTabList.length"}],
         ["close"],
         ["-"],
         ["save-source-tab", { enabledif: "console.views.source2.canSave()" }],
         ["save-profile"],
         ["-"],
         ["save-settings"],
         ["restore-settings"],
         ["toggle-save-settings",
                 {type: "checkbox",
                  checkedif: "console.prefs['saveSettingsOnExit']"}],
         ["-"],
         [navigator.platform.search(/win/i) == -1 ? "quit" : "exit"]
        ]
    };

    console.menuSpecs["mainmenu:view"] = {
        label: MSG_MNU_VIEW,
        items:
        [
         [">popup:showhide"],
         ["-"],
         ["reload-source-tab"],
         ["toggle-source-coloring",
                 {type: "checkbox",
                  checkedif: "console.prefs['services.source.colorize']"} ],
         ["toggle-pprint",
                 {type: "checkbox",
                  checkedif: "console.prefs['prettyprint']"}],
         ["-"],
         ["clear-session"],
         [">session:colors"],
         ["-"],
         ["save-default-layout"],
         ["toggle-save-layout",
                 {type: "checkbox",
                  checkedif: "console.prefs['saveLayoutOnExit']"}]
        ]
    };
    
    console.menuSpecs["mainmenu:debug"] = {
        label: MSG_MNU_DEBUG,
        items:
        [
         ["stop",
                 {type: "checkbox",
                  checkedif: "console.jsds.interruptHook"}],
         ["cont"],
         ["next"],
         ["step"],
         ["finish"],
         ["-"],
         [">popup:emode"],
         [">popup:tmode"],
         ["-"],
         ["toggle-chrome",
                 {type: "checkbox",
                  checkedif: "console.prefs['enableChromeFilter']"}]
         /*
         ["toggle-ias",
                 {type: "checkbox",
                  checkedif: "console.jsds.initAtStartup"}]
         */
        ]
    };

    console.menuSpecs["mainmenu:profile"] = {
        label: MSG_MNU_PROFILE,
        items:
        [
         ["toggle-profile",
                 {type: "checkbox",
                  checkedif: "console.jsds.flags & COLLECT_PROFILE_DATA"}],
         ["clear-profile"],
         ["save-profile"]
        ]
    };

    /* Mac expects a help menu with this ID, and there is nothing we can do
     * about it. */
    console.menuSpecs["mainmenu:help"] = {
        label: MSG_MNU_HELP,
        domID: "menu_Help",
        items:
        [
         ["version"],
         ["-"],
         ["help"]
        ]
    };
    
    console.menuSpecs["popup:emode"] = {
        label: MSG_MNU_EMODE,
        items:
        [
         ["em-ignore",
                 {type: "radio", name: "em",
                  checkedif: "console.errorMode == EMODE_IGNORE"}],
         ["em-trace",
                 {type: "radio", name: "em",
                  checkedif: "console.errorMode == EMODE_TRACE"}],
         ["em-break",
                 {type: "radio", name: "em",
                  checkedif: "console.errorMode == EMODE_BREAK"}]
        ]
    };
    
    console.menuSpecs["popup:tmode"] = {
        label: MSG_MNU_TMODE,
        items:
        [
         ["tm-ignore",
                 {type: "radio", name: "tm",
                  checkedif: "console.throwMode == TMODE_IGNORE"}],
         ["tm-trace",
                 {type: "radio", name: "tm",
                  checkedif: "console.throwMode == TMODE_TRACE"}],
         ["tm-break",
                 {type: "radio", name: "tm",
                  checkedif: "console.throwMode == TMODE_BREAK"}]
        ]
    };

    console.menuSpecs["popup:showhide"] = {
        label: MSG_MNU_SHOWHIDE,
        items: [ /* filled by initViews() */ ]
    };
}

console.createMainMenu = createMainMenu;
function createMainMenu(document)
{
    var mainmenu = document.getElementById("mainmenu");
    var menuManager = console.menuManager;
    for (var id in console.menuSpecs)
    {
        var domID;
        if ("domID" in console.menuSpecs[id])
            domID = console.menuSpecs[id].domID;
        else
            domID = id;
        
        if (id.indexOf("mainmenu:") == 0)
            menuManager.createMenu (mainmenu, null, id, domID);
    }

    mainmenu.removeAttribute ("collapsed");
    var toolbox = document.getElementById("main-toolbox");
    toolbox.removeAttribute ("collapsed");
}

console.createMainToolbar = createMainToolbar;
function createMainToolbar(document)
{
    var maintoolbar = document.getElementById("maintoolbar");
    var menuManager = console.menuManager;
    var spec = console.menuSpecs["maintoolbar"];
    for (var i in spec.items)
    {
        menuManager.appendToolbarItem (maintoolbar, null, spec.items[i][0]);
    }

    maintoolbar = document.getElementById("maintoolbar-outer");
    maintoolbar.removeAttribute ("collapsed");
    maintoolbar.className = "toolbar-primary chromeclass-toolbar";
    var toolbox = document.getElementById("main-toolbox");
    toolbox.removeAttribute ("collapsed");
}

function getCommandContext (id, event)
{
    var cx = { originalEvent: event };
    
    if (id in console.menuSpecs)
    {
        if ("getContext" in console.menuSpecs[id])
            cx = console.menuSpecs[id].getContext(cx);
        else if ("cx" in console.menuManager) 
        {
            //dd ("using existing context");
            cx = console.menuManager.cx;
        }
        else
        {
            //dd ("no context at all");
        }
    }
    else
    {
        dd ("getCommandContext: unknown menu id " + id);
    }

    if (typeof cx == "object")
    {
        if (!("menuManager" in cx))
            cx.menuManager = console.menuManager;
        if (!("contextSource" in cx))
            cx.contextSource = id;
        if ("dbgContexts" in console && console.dbgContexts)
            dd ("context '" + id + "'\n" + dumpObjectTree(cx));
    }

    return cx;
}
