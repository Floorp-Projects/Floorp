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

const DEFAULT_VURLS =
("x-vloc:/mainwindow/initial-container?target=container&id=outer&type=horizontal;" +
 // left vert (gutter)
 ("x-vloc:/mainwindow/outer?target=container&id=gutter&width=231&before=vright&type=vertical;" +
  // top tab
  ("x-vloc:/mainwindow/gutter?target=container&id=top-tab&height=177&before=vm-container-5&type=tab;" +
   "x-vloc:/mainwindow/top-tab?target=view&id=scripts&height=177&before=windows;" +
   "x-vloc:/mainwindow/top-tab?target=view&id=windows;"
   ) +
  // middle tab
  ("x-vloc:/mainwindow/gutter?target=container&id=mid-tab&height=121&type=tab;" +
   "x-vloc:/mainwindow/mid-tab?target=view&id=locals;" +
   "x-vloc:/mainwindow/mid-tab?target=view&id=watches;"
   ) +
  // bottom tab
  ("x-vloc:/mainwindow/gutter?target=container&id=bot-tab&height=100&type=tab;" +
   "x-vloc:/mainwindow/bot-tab?target=view&id=breaks&height=100&;" +
   "x-vloc:/mainwindow/bot-tab?target=view&id=stack&height=75;"
   )
  ) +
 // right vert
 ("x-vloc:/mainwindow/outer?target=container&id=vright&width=560&type=vertical;" +
  "x-vloc:/mainwindow/vright?target=view&id=source2&before=session;" +
  "x-vloc:/mainwindow/vright?target=view&id=session&width=677"
  )
 )

function initViews()
{
    var prefs =
        [
         ["layoutState.default", DEFAULT_VURLS],
         ["saveLayoutOnExit", true]
        ];
    
    console.prefManager.addPrefs (prefs);

    const ATOM_CTRID = "@mozilla.org/atom-service;1";
    const nsIAtomService = Components.interfaces.nsIAtomService;
    console.atomService =
        Components.classes[ATOM_CTRID].getService(nsIAtomService);

    console.viewManager = new ViewManager (console.commandManager,
                                           console.mainWindow);
    console.viewManager.realizeViews (console.views,
                                      console.menuSpecs["popup:showhide"]);
    console.views = console.viewManager.views;

    for (var viewId in console.views)
    {
        if (("enableMenuItem" in console.views[viewId]) &&
            !console.views[viewId].enableMenuItem)
        {
            continue;
        }
        
        var toggleCommand = "toggle-" + viewId;
        if (toggleCommand in console.commandManager.commands)
        {
            var entry = [toggleCommand,
                    {
                        type: "checkbox",
                        checkedif: "'currentContent' in console.views." + viewId
                    }];
                         
            console.menuSpecs["popup:showhide"].items.push(entry);
        }
    }
}

function destroyViews()
{
    console.viewManager.destroyWindows ();
    console.viewManager.unrealizeViews (console.views);
}

/**
 * Sync a XUL tree with the view that it represents.
 *
 * XUL trees seem to take a small amount of time to initialize themselves to
 * the point where they're willing to accept a view object.  This method will
 * call itself back after a small delay if the XUL tree is not ready yet.
 */
function syncTreeView (treeContent, treeView, cb)
{
    function ondblclick(event) { treeView.onRouteDblClick(event); };
    function onkeypress(event) { treeView.onRouteKeyPress(event); };
    function onfocus(event)    { treeView.onRouteFocus(event); };
    function onblur(event)     { treeView.onRouteBlur(event); };
    
    function tryAgain()
    {
        if ("treeBoxObject" in treeContent)
        {
            syncTreeView(treeContent, treeView, cb);
        }
        else
        {
            //dd ("trying to sync " + treeContent.getAttribute("id"));
            setTimeout (tryAgain, 500);
        }
    };

    try
    {
        if (!("treeBoxObject" in treeContent))
            throw "tantrum";
        
        treeContent.treeBoxObject.view = treeView;
        if (treeView.selection)
        {
            treeView.selection.tree = treeContent.treeBoxObject;
        }
    }
    catch (ex)
    {
        setTimeout (tryAgain, 500);
        return;
    }

    if (!treeView)
    {
        if ("_listenersInstalled" in treeContent &&
            treeContent._listenersInstalled)
        {
            treeContent._listenersInstalled = false;
        
            treeContent.removeEventListener("dblclick", ondblclick, false);
            treeContent.removeEventListener("keypress", onkeypress, false);
            treeContent.removeEventListener("focus", onfocus, false);
            treeContent.removeEventListener("blur", onblur, false);
        }
    }
    else if (!("_listenersInstalled" in treeContent) ||
             !treeContent._listenersInstalled)
    {
        try
        {
            treeContent.addEventListener("dblclick", ondblclick, false);
            treeContent.addEventListener("keypress", onkeypress, false);
            treeContent.addEventListener("focus", onfocus, false);
            treeContent.addEventListener("blur", onblur, false);
        }
        catch (ex)
        {
            dd ("caught exception setting listeners: " + ex);
        }

        treeContent._listenersInstalled = true;
    }
    
    if (typeof cb == "function")
        cb();
}

function getTreeContext (view, cx, recordContextGetter)
{
    //dd ("getTreeContext {");

    var i = 0;
    var selection = view.tree.view.selection;
    var row = selection.currentIndex;
    var rec;
    
    if (view instanceof XULTreeView)
    {
        rec = view.childData.locateChildByVisualRow (row);
        if (!rec)
        {
            //dd ("} no record at currentIndex " + row);
            return cx;
        }
    }
    else
    {
        rec = row;
    }
    
    cx.target = rec;

    recordContextGetter(cx, rec, i++);
    var rangeCount = selection.getRangeCount();

    //dd ("walking ranges {");
    for (var range = 0; range < rangeCount; ++range)
    {
        var min = new Object();
        var max = new Object();
        selection.getRangeAt(range, min, max);
        min = min.value;
        max = max.value;

        for (row = min; row <= max; ++row)
        {
            if (view instanceof XULTreeView)
            {
                rec = view.childData.locateChildByVisualRow(row);
                if (!rec)
                    dd ("no record at range index " + row);
            }
            else
            {
                rec = row;
            }

            recordContextGetter(cx, rec, i++);
        }
    }
    //dd ("}");
    //dd ("cleaning up {");
    /* delete empty arrays as that may have been left behind. */
    for (var p in cx)
    {
        if (cx[p] instanceof Array && !cx[p].length)
            delete cx[p];
    }
    //dd ("}");
    //dd ("}");
    
    return cx;
}

function initContextMenu (document, id)
{
    if (!document.getElementById(id))
    {
        if (!ASSERT(id in console.menuSpecs, "unknown context menu " + id))
            return;

        var dp = document.getElementById("dynamic-popups");
        var popup = console.menuManager.appendPopupMenu (dp, null, id, id);
        var items = console.menuSpecs[id].items;
        console.menuManager.createMenuItems (popup, null, items);
    }
}

console.viewDragProxy = new Object();

console.viewDragProxy.onDragStart =
Prophylactic(console.viewDragProxy, vpxy_dragstart);
function vpxy_dragstart (event, transferData, action)
{
    console.viewManager.onDragStart (event, transferData, action);
    return true;
}

console.viewDropProxy = new Object();

console.viewDropProxy.onDrop =
function vpxy_drop (event, transferData, session)
{
    console.viewManager.onViewDrop (event, transferData, session);
    /* when moving into a new view, the parent view won't repaint until
     * the mouse moves, or we do call the paintHack.
     */
    paintHack();
    return true;
}

console.viewDropProxy.onDragOver =
function vpxy_dragover (event, flavor, session)
{
    console.viewManager.onViewDragOver (event, flavor, session);
    return true;
}

console.viewDropProxy.onDragExit =
function vpxy_dragexit (event, session)
{
    console.viewManager.onViewDragExit (event, session);
}
        
console.viewDropProxy.getSupportedFlavours =
function vpxy_getflavors ()
{
    return console.viewManager.getSupportedFlavours();
}

console.views = new Object();

/*******************************************************************************
 * Breakpoints View
 ******************************************************************************/

console.views.breaks = new XULTreeView();

const VIEW_BREAKS = "breaks"
console.views.breaks.viewId = VIEW_BREAKS;

console.views.breaks.hooks = new Object();

console.views.breaks.hooks["hook-fbreak-set"] =
function bv_hookFBreakSet (e)
{
    var breakRecord = new BPRecord (e.fbreak);
    breakRecord.reserveChildren();
    e.fbreak.breakRecord = breakRecord;
    console.views.breaks.childData.appendChild(breakRecord);        
}

console.views.breaks.hooks["hook-fbreak-clear"] =
function bv_hookFBreakClear (e)
{
    var breakRecord = e.fbreak.breakRecord;
    delete e.fbreak.breakRecord;
    console.views.breaks.childData.removeChildAtIndex(breakRecord.childIndex);
    for (var i in breakRecord.childData)
    {
        console.views.breaks.childData.appendChild(breakRecord.childData[i]);
    }
}

console.views.breaks.hooks["hook-break-set"] =
function bv_hookBreakSet (e)
{
    var breakRecord = new BPRecord (e.breakWrapper);
    e.breakWrapper.breakRecord = breakRecord;
    if (e.breakWrapper.parentBP)
    {
        var parentRecord = e.breakWrapper.parentBP.breakRecord;
        parentRecord.appendChild(breakRecord);
    }
    else
    {
        console.views.breaks.childData.appendChild(breakRecord);
    }
}

console.views.breaks.hooks["hook-break-clear"] =
function bv_hookBreakClear (e)
{
    var breakRecord = e.breakWrapper.breakRecord;
    if (e.breakWrapper.parentBP)
    {
        var parentRecord = e.breakWrapper.parentBP.breakRecord;
        parentRecord.removeChildAtIndex(breakRecord.childIndex);
    }
    else
    {
        var idx = breakRecord.childIndex;
        console.views.breaks.childData.removeChildAtIndex(idx);
    }
}

console.views.breaks.init =
function bv_init ()
{
    console.menuSpecs["context:breaks"] = {
        getContext: this.getContext,
        items:
        [
         ["find-url"],
         ["-"],
         ["clear-break",  { enabledif: "has('hasBreak')" }],
         ["clear-fbreak", { enabledif: "has('hasFBreak')" }],
         ["-"],
         ["clear-all"],
         ["fclear-all"],
         ["-"],
         ["save-breakpoints"],
         ["restore-settings"],
         ["-"],
         ["break-props"]
        ]
    };

    this.atomBreak = console.atomService.getAtom("item-breakpoint");
    this.atomFBreak = console.atomService.getAtom("future-breakpoint");

    this.caption = MSG_VIEW_BREAKS;
}

console.views.breaks.onShow =
function bv_show()
{
    syncTreeView (getChildById(this.currentContent, "break-tree"), this);
    initContextMenu(this.currentContent.ownerDocument, "context:breaks");
}

console.views.breaks.onHide =
function bv_hide()
{
    syncTreeView (getChildById(this.currentContent, "break-tree"), null);
}

console.views.breaks.onRowCommand =
function bv_rowcommand (rec)
{
    if (rec instanceof BPRecord)
        dispatch ("find-bp", {breakpoint: rec.breakWrapper});
}

console.views.breaks.onKeyPress =
function bv_keypress(rec, e)
{
    if (e.keyCode == 46)
    {
        cx = this.getContext({});
        if ("hasBreak" in cx)
            dispatch ("clear-break", cx);
        if ("hasFBreak" in cx)
            dispatch ("clear-fbreak", cx);
    }
}

console.views.breaks.getContext =
function bv_getcx(cx)
{
    cx.breakWrapperList = new Array();
    cx.urlList = new Array();
    cx.lineNumberList = new Array();
    cx.pcList = new Array();
    cx.scriptWrapperList = new Array();
    
    function recordContextGetter (cx, rec, i)
    {
        if (i == 0)
        {
            cx.breakWrapper = rec.breakWrapper;
            if (rec.type == "instance")
            {
                cx.hasBreak = true;
                cx.scriptWrapper = rec.breakWrapper.scriptWrapper;
                cx.pc = rec.breakWrapper.pc;
            }
            else if (rec.type == "future")
            {
                cx.hasFBreak = true;
                cx.url = rec.breakWrapper.url;
                cx.lineNumber = rec.breakWrapper.lineNumber;
            }
        }
        else
        {
            cx.breakWrapperList.push(rec.breakWrapper);
            if (rec.type == "instance")
            {
                cx.hasBreak = true;
                cx.scriptWrapperList.push(rec.breakWrapper.scriptWrapper);
                cx.pcList.push(rec.breakWrapper.pc);
            }
            else if (rec.type == "instance")
            {
                cx.hasFBreak = true;
                cx.urlList.push(rec.breakWrapper.url);
                cx.lineNumberList.push(rec.breakWrapper.lineNumber);
            }
        }
    };

    return getTreeContext (console.views.breaks, cx, recordContextGetter);
}

console.views.breaks.getCellProperties =
function bv_cellprops (index, col, properties)
{
    if (col.id == "breaks:col-0")
    {
        var row = this.childData.locateChildByVisualRow(index);
        if (row.type == "future")
            properties.AppendElement (this.atomFBreak);
        else
            properties.AppendElement (this.atomBreak);
    }
}

/*******************************************************************************
 * Locals View
 ******************************************************************************/

console.views.locals = new XULTreeView();

const VIEW_LOCALS = "locals";
console.views.locals.viewId = VIEW_LOCALS;

console.views.locals.init =
function lv_init ()
{
    var prefs =
        [
         ["localsView.autoOpenMax", 25],
         ["localsView.savedStatesMax", 20]
        ];
    
    console.prefManager.addPrefs(prefs);

    this.cmdary =
        [
         ["copy-qual-name", cmdCopyQualName, 0]
        ];

    console.menuSpecs["context:locals"] = {
        getContext: this.getContext,
        items:
        [
         ["change-value", {enabledif: "cx.parentValue"}],
         ["watch-expr"],
         ["copy-qual-name", {enabledif: "has('expression')"}],
         ["-"],
         ["set-eval-obj", {type: "checkbox",
                           checkedif: "has('jsdValue') && " +
                                      "cx.jsdValue.getWrappedValue() == " +
                                      "console.currentEvalObject",
                           enabledif: "has('jsdValue') && " +
                                      "cx.jsdValue.jsType == TYPE_OBJECT"}],
         ["-"],
         ["find-creator",
                 {enabledif: "cx.target instanceof ValueRecord && " +
                  "cx.target.jsType == jsdIValue.TYPE_OBJECT  && " +
                  "cx.target.value.objectValue.creatorURL"}],
         ["find-ctor",
                 {enabledif: "cx.target instanceof ValueRecord && " +
                  "cx.target.jsType == jsdIValue.TYPE_OBJECT  && " +
                  "cx.target.value.objectValue.constructorURL"}],
         ["-"],
         ["toggle-functions",
                 {type: "checkbox",
                  checkedif: "ValueRecord.prototype.showFunctions"}],
         ["toggle-ecmas",
                 {type: "checkbox",
                  checkedif: "ValueRecord.prototype.showECMAProps"}],
         ["toggle-constants",
                 {type: "checkbox",
                  checkedif: "ValueRecord.prototype.showConstants"}]
        ]
    };

    this.caption = MSG_VIEW_LOCALS;

    this.jsdFrame = null;
    this.savedStates = new Object();
    this.stateTags = new Array();
}

function cmdCopyQualName (e)
{
    const CLIPBOARD_CTRID = "@mozilla.org/widget/clipboardhelper;1";
    const nsIClipboardHelper = Components.interfaces.nsIClipboardHelper;

    var clipboardHelper =
        Components.classes[CLIPBOARD_CTRID].getService(nsIClipboardHelper);

    clipboardHelper.copyString(e.expression);
}

console.views.locals.clear =
function lv_clear ()
{
    while (this.childData.childData.length)
        this.childData.removeChildAtIndex(0);
}

console.views.locals.changeFrame =
function lv_renit (jsdFrame)
{
    this.clear();
    
    if (!jsdFrame)
    {
        delete this.jsdFrame;
        return;
    }
    
    this.jsdFrame = jsdFrame;
    var state;

    if (jsdFrame.script)
    {
        var tag = jsdFrame.script.tag;
        if (tag in this.savedStates)
            state = this.savedStates[tag];
    }
    
    if (jsdFrame.scope)
    {
        this.scopeRecord = new ValueRecord (jsdFrame.scope, MSG_VAL_SCOPE, "");
        this.scopeRecord.onPreRefresh = null;
        this.childData.appendChild(this.scopeRecord);
        if (!state && jsdFrame.scope.propertyCount <
            console.prefs["localsView.autoOpenMax"])
        {
            this.scopeRecord.open();
        }
        
    }
    else
    {
        this.scopeRecord = new XTLabelRecord ("locals:col-0", MSV_VAL_SCOPE,
                                              ["locals:col-1", "locals:col-2",
                                               "locals:col-3"]);
        this.scopeRecord.property = ValueRecord.prototype.atomObject;
        this.childData.appendChild(this.scopeRecord);
    }
    
    if (jsdFrame.thisValue)
    {
        this.thisRecord = new ValueRecord (jsdFrame.thisValue, MSG_VAL_THIS,
                                           "");
        this.thisRecord.onPreRefresh = null;
        this.childData.appendChild(this.thisRecord);
        if (!state && jsdFrame.thisValue.propertyCount < 
            console.prefs["localsView.autoOpenMax"])
        {
            this.scopeRecord.open();
        }
    }    
    else
    {
        this.thisRecord = new XTLabelRecord ("locals:col-0", MSV_VAL_THIS,
                                             ["locals:col-1", "locals:col-2",
                                              "locals:col-3"]);
        this.thisRecord.property = ValueRecord.prototype.atomObject;
        this.childData.appendChild(this.thisRecord);
    }

    if (state)
        this.restoreState(state);
}

console.views.locals.restoreState =
function lv_restore (state)
{
    this.freeze();
    if ("scopeState" in state)
    {
        this.scopeRecord.open();
        this.restoreBranchState (this.scopeRecord.childData, state.scopeState,
                                 true);
    }
    if ("thisState" in state)
    {
        this.thisRecord.open();
        this.restoreBranchState (this.thisRecord.childData, state.thisState,
                                 true);
    }
    this.thaw();
    this.scrollTo (state.firstVisible, -1);
}

console.views.locals.saveState =
function sv_save ()
{
    if (!ASSERT(this.jsdFrame, "no frame"))
        return;

    if (!this.jsdFrame.script)
        return;
    
    var tag = this.jsdFrame.script.tag;    

    if (!tag in this.savedStates &&
        this.stateTags.length == console.prefs["localsView.maxSavedStates"])
    {
        delete this.savedStates[this.stateTags.shift()];
        this.stateTags.push(tag);
    }
        
    var state = this.savedStates[tag] = new Object();
    if (this.tree)
        state.firstVisible = this.tree.getFirstVisibleRow() + 1;
    else
        state.firstVisible = 1;

    if (this.scopeRecord && this.scopeRecord.isContainerOpen)
    {
        state.scopeState = { name: "scope" };
        this.saveBranchState (state.scopeState, this.scopeRecord.childData,
                              true);
    }
    
    if (this.thisRecord && this.thisRecord.isContainerOpen)
    {
        state.thisState = { name: "this" };
        this.saveBranchState (state.thisState, this.thisRecord.childData,
                              true);
    }
    
    //dd ("saved as\n" + dumpObjectTree(this.savedState, 10));
}

console.views.locals.hooks = new Object()

console.views.locals.hooks["hook-debug-stop"] =
function lv_hookDebugStop (e)
{
    console.views.locals.changeFrame(console.frames[0]);
}

console.views.locals.hooks["hook-eval-done"] =
function lv_hookEvalDone (e)
{
    console.views.locals.refresh();
}

console.views.locals.hooks["find-frame"] =
function lv_hookFindFrame (e)
{
    console.views.locals.changeFrame(console.frames[e.frameIndex]);
}

console.views.locals.hooks["hook-debug-continue"] =
function lv_hookDebugContinue (e)
{
    console.views.locals.saveState();
    console.views.locals.clear();
}

console.views.locals.onShow =
function lv_show ()
{
    syncTreeView (getChildById(this.currentContent, "locals-tree"), this);
    initContextMenu(this.currentContent.ownerDocument, "context:locals");
}

console.views.locals.onHide =
function lv_hide ()
{
    syncTreeView (getChildById(this.currentContent, "locals-tree"), null);
}

console.views.locals.onRowCommand =
function lv_rowcommand(rec)
{
    if (this.isContainerEmpty(rec.childIndex) && "value" in rec.parentRecord)
    {
        dispatch ("change-value", 
                  {parentValue: rec.parentRecord.value, 
                   propertyName: rec.displayName});
    }
}

console.views.locals.getCellProperties =
function lv_cellprops (index, col, properties)
{
    if (col.id != "locals:col-0")
        return null;
    
    var row = this.childData.locateChildByVisualRow(index);
    if (row)
    {
        if ("getProperties" in row)
            return row.getProperties (properties);

        if (row.property)
            return properties.AppendElement (row.property);
    }

    return null;
}

console.views.locals.getContext =
function lv_getcx(cx)
{
    var locals = console.views.locals;
    
    cx.jsdValueList = new Array();
    
    function recordContextGetter (cx, rec, i)
    {
        if (i == 0)
        {
            cx.jsdValue = rec.value;
            cx.expression = rec.displayName;
            
            if ("value" in rec.parentRecord)
            {
                cx.parentValue = rec.parentRecord.value;
                var cur = rec.parentRecord;
                while (cur != locals.childData &&
                       cur != locals.scopeRecord)
                {
                    cx.expression = cur.displayName + "." + cx.expression;
                    cur = cur.parentRecord;
                }
            }
            else
            {
                cx.parentValue = null;
            }
            cx.propertyName = rec.displayName;
        }
        else
        {
            cx.jsdValueList.push(rec.value);
        }
        return cx;
    };
    
    return getTreeContext (console.views.locals, cx, recordContextGetter);
}

console.views.locals.refresh =
function lv_refresh()
{
    if (!this.tree)
        return;

    var rootRecord = this.childData;    
    if (!"childData" in rootRecord)
        return;
    
    this.freeze();
    
    for (var i = 0; i < rootRecord.childData.length; ++i)
        rootRecord.childData[i].refresh();

    this.thaw();
    /* the refresh may have changed a property without altering the
     * size of the tree, so thaw might not invalidate. */
    this.tree.invalidate();
}

/*******************************************************************************
 * Scripts View
 ******************************************************************************/

console.views.scripts = new XULTreeView ();

const VIEW_SCRIPTS = "scripts";
console.views.scripts.viewId = VIEW_SCRIPTS;

console.views.scripts.init =
function scv_init ()
{
    var debugIf = "'scriptWrapper' in cx && " +
        "cx.scriptWrapper.jsdScript.isValid && " +
        "cx.scriptWrapper.jsdScript.flags & SCRIPT_NODEBUG";
    var profileIf = "'scriptWrapper' in cx && " +
        "cx.scriptWrapper.jsdScript.isValid && " +
        "cx.scriptWrapper.jsdScript.flags & SCRIPT_NOPROFILE";
    var transientIf = "'scriptInstance' in cx && " +
        "cx.scriptInstance.scriptManager.disableTransients";
    
    var prefs =
        [
         ["scriptsView.groupFiles", true],
         ["scriptsView.showMostRecent", true]
        ];
    
    console.prefManager.addPrefs(prefs);

    this.cmdary =
        [
         ["show-most-recent",          cmdShowMostRecent,          CMD_CONSOLE],
         ["search-scripts",            cmdSearchScripts,           CMD_CONSOLE],
         ["toggle-scripts-search-box", cmdToggleScriptsSearchBox,  CMD_CONSOLE],

         ["toggle-show-most-recent",   "show-most-recent toggle",            0]
        ];

    console.menuSpecs["context:scripts"] = {
        getContext: this.getContext,
        items:
        [
         ["find-scriptinstance", {visibleif: "!has('scriptWrapper')"}],
         ["find-script",    {visibleif: "has('scriptWrapper')"}],
         ["set-break"],
         ["clear-instance", {visibleif: "!has('scriptWrapper')"}],
         ["clear-script",   {visibleif: "has('scriptWrapper')",
                             enabledif: "cx.scriptWrapper.breakpointCount"}],
         ["scan-source"],
         ["-"],
         [">scripts:instance-flags"],
         [">scripts:wrapper-flags", {enabledif: "has('scriptWrapper')"}],
         ["-"],
         ["save-profile"],
         ["clear-profile"],
         ["-"],
         ["toggle-show-most-recent",
                 {type: "checkbox",
                  checkedif: "console.prefs['scriptsView.showMostRecent']"}],
         ["toggle-chrome",
                 {type: "checkbox",
                  checkedif: "console.prefs['enableChromeFilter']"}]
        ]
    };

    console.menuSpecs["scripts:instance-flags"] = {
        label: MSG_MNU_SCRIPTS_INSTANCE,
        items:
        [
         ["debug-transient", {type: "checkbox", checkedif: transientIf}],
         ["-"],
         ["debug-instance-on"],
         ["debug-instance-off"],
         ["debug-instance"],
         ["-"],
         ["profile-instance-on"],
         ["profile-instance-off"],
         ["profile-instance"],
        ]
    };

    console.menuSpecs["scripts:wrapper-flags"] = {
        label: MSG_MNU_SCRIPTS_WRAPPER,
        items:
        [
         ["debug-script", {type: "checkbox", checkedif: debugIf}],
         ["profile-script", {type: "checkbox", checkedif: profileIf}],
        ]
    };

    this.caption = MSG_VIEW_SCRIPTS;

    this.childData.setSortColumn("baseLineNumber");
    this.groupFiles = console.prefs["scriptsView.groupFiles"];
}

function cmdSearchScripts (e)
{
    var scriptsView = console.views.scripts;
    var rootNode = scriptsView.childData;
    var i;
    
    for (i = 0; i < rootNode.childData.length; ++i)
    {
        var scriptInstanceRecord = rootNode.childData[i];
        if (e.pattern && (!scriptInstanceRecord.url ||
                          scriptInstanceRecord.url.indexOf(e.pattern) == -1))
        {
            scriptInstanceRecord.searchExclude = true;
            if (!scriptInstanceRecord.isHidden)
                scriptInstanceRecord.hide();
        }
        else
        {
            delete scriptInstanceRecord.searchExclude;
            
            if (!("recentExclude" in scriptInstanceRecord) &&
                scriptInstanceRecord.isHidden)
            {
                scriptInstanceRecord.unHide();
            }
        }
    }

    if (scriptsView.currentContent)
    {
        var textbox = getChildById(scriptsView.currentContent,
                                   "scripts-search");
        textbox.value = e.pattern;
    }
}

function cmdShowMostRecent (e)
{
    e.toggle = getToggle (e.toggle,
                          console.prefs["scriptsView.showMostRecent"]);

    console.prefs["scriptsView.showMostRecent"] = e.toggle;
    
    var rootNode = console.views.scripts.childData;
    var i;
    var scriptInstanceRecord;
    
    if (e.toggle)
    {
        /* want to hide duplicate script instances */
        for (i = 0; i < rootNode.childData.length; ++i)
        {
            scriptInstanceRecord = rootNode.childData[i];
            var scriptInstance = scriptInstanceRecord.scriptInstance;
            var instances = scriptInstance.scriptManager.instances;
            var length = instances.length;
            if (scriptInstance != instances[length - 1])
            {
                scriptInstanceRecord.recentExclude = true;
                if (!scriptInstanceRecord.isHidden)
                    scriptInstanceRecord.hide();
            }
        }
    }
    else
    {
        /* show all script instances */
        for (i = 0; i < rootNode.childData.length; ++i)
        {
            scriptInstanceRecord = rootNode.childData[i];
            delete scriptInstanceRecord.recentExclude;
            if (scriptInstanceRecord.isHidden &&
                !("searchExclude" in scriptInstanceRecord))
            {
                scriptInstanceRecord.unHide();
            }
        }
    }
}

function cmdToggleScriptsSearchBox(e)
{
    var scriptsView = console.views.scripts;

    if (scriptsView.currentContent)
    {
        var box = getChildById(scriptsView.currentContent,
                               "scripts-search-box");
        if (box.hasAttribute("hidden"))
            box.removeAttribute("hidden");
        else
            box.setAttribute("hidden", "true");
    }
}
        
console.views.scripts.hooks = new Object();

console.views.scripts.hooks["chrome-filter"] =
function scv_hookChromeFilter(e)
{
    var scriptsView = console.views.scripts;
    var nodes = scriptsView.childData;
    scriptsView.freeze();

    //dd ("e.toggle is " + e.toggle);

    for (var m in console.scriptManagers)
    {
        if (console.scriptManagers[m].url.search(/^chrome:/) == 0 ||
            ("componentPath" in console &&
             console.scriptManagers[m].url.indexOf(console.componentPath) == 0))
        {
        
            for (var i in console.scriptManagers[m].instances)
            {
                var instance = console.scriptManagers[m].instances[i];
                if ("scriptInstanceRecord" in instance)
                {
                    var rec = instance.scriptInstanceRecord;
                    
                    if (e.toggle)
                    {
                        if (!ASSERT("parentRecord" in rec, "Record for " +
                                    console.scriptManagers[m].url + 
                                    " is already removed"))
                        {
                            continue;
                        }
                        /* filter is on, remove chrome file from scripts view */
                        /*
                          dd ("removing " + console.scriptManagers[m].url + 
                          " kid at " + rec.childIndex);
                        */
                        nodes.removeChildAtIndex(rec.childIndex);
                    }
                    else
                    {
                        if ("parentRecord" in rec)
                            continue;
                        //dd ("cmdChromeFilter: append " +
                        //    tov_formatRecord(rec, ""));
                        nodes.appendChild(rec);
                    }
                }
            }
        }
    }

    scriptsView.thaw();
    if (scriptsView.tree)
        scriptsView.tree.invalidate();
}

console.views.scripts.hooks["hook-break-set"] =
console.views.scripts.hooks["hook-break-clear"] =
console.views.scripts.hooks["hook-fbreak-set"] =
console.views.scripts.hooks["hook-fbreak-clear"] =
function sch_hookBreakChange(e)
{
    if (console.views.scripts.tree)
        console.views.scripts.tree.invalidate();
}

console.views.scripts.hooks["hook-guess-complete"] =
function sch_hookGuessComplete(e)
{
    if (!("scriptInstanceRecord" in e.scriptInstance))
        return;
    
    var rec = e.scriptInstance.scriptInstanceRecord;
    if (!rec.childData.length)
        return;

    for (var i in rec.childData)
    {
        rec.childData[i].functionName =
            rec.childData[i].scriptWrapper.functionName;
    }

    if (console.views.scripts.tree)
        console.views.scripts.tree.invalidate();
}

console.views.scripts.hooks["hook-script-instance-sealed"] =
function scv_hookScriptInstanceSealed (e)
{
    if (!e.scriptInstance.url ||
        e.scriptInstance.url.search (/^(x-jsd|javascript)/) == 0)
    {
        return;
    }
    //dd ("instance sealed: " + e.scriptInstance.url);
    
    var scr = new ScriptInstanceRecord (e.scriptInstance);
    e.scriptInstance.scriptInstanceRecord = scr;

    if (console.prefs["enableChromeFilter"] &&
        (e.scriptInstance.url.search(/^chrome:/) == 0 ||
         "componentPath" in console &&
         e.scriptInstance.url.indexOf(console.componentPath) == 0))
    {
        return;
    }
    
    console.views.scripts.childData.appendChild(scr);

    var length = e.scriptInstance.scriptManager.instances.length;
    
    if (console.prefs["scriptsView.showMostRecent"] && length > 1)
    {        
        var previous = e.scriptInstance.scriptManager.instances[length - 2];
        if ("scriptInstanceRecord" in previous)
        {
            var record = previous.scriptInstanceRecord;
            record.recentExclude = true;
            if (!record.isHidden)
                record.hide();
        }
    }
}

console.views.scripts.hooks["hook-script-instance-destroyed"] =
function scv_hookScriptInstanceDestroyed (e)
{
    if (!("scriptInstanceRecord" in e.scriptInstance))
        return;
    
    var rec = e.scriptInstance.scriptInstanceRecord;
    if ("parentRecord" in rec)
        console.views.scripts.childData.removeChildAtIndex(rec.childIndex);

    var instances = e.scriptInstance.scriptManager.instances;
    if (instances.length)
    {
        var previous = instances[instances.length - 1];
        if ("scriptInstanceRecord" in previous)
        {
            var record = previous.scriptInstanceRecord;
            if (!("searchExclude" in record))
                record.unHide();
            delete record.recentExclude;
        }
    }
}

console.views.scripts.hooks["debug-script"] =
console.views.scripts.hooks["debug-instance"] =
function scv_hookDebugFlagChanged (e)
{
    if (console.views.scripts.tree)
        console.views.scripts.tree.invalidate();
}

console.views.scripts.onSearchInput =
function scv_oninput (event)
{
    var scriptsView = this;

    function onTimeout ()
    {
        var textbox = getChildById(scriptsView.currentContent,
                                   "scripts-search");
        dispatch ("search-scripts", { pattern: textbox.value });
    };
    
    if ("searchTimeout" in this)
        clearTimeout(this.searchTimeout);
    
    this.searchTimeout = setTimeout (onTimeout, 500);
}

console.views.scripts.onSearchClear =
function scv_onclear (event)
{
    dispatch ("search-scripts");
}

console.views.scripts.onShow =
function scv_show ()
{
    syncTreeView (getChildById(this.currentContent, "scripts-tree"), this);
    initContextMenu(this.currentContent.ownerDocument, "context:scripts");
}

console.views.scripts.onHide =
function scv_hide ()
{
    syncTreeView (getChildById(this.currentContent, "scripts-tree"), null);
}

console.views.scripts.onRowCommand =
function scv_rowcommand (rec)
{
    if (rec instanceof ScriptRecord)
        dispatch ("find-script", { scriptWrapper: rec.scriptWrapper });
    else if (rec instanceof ScriptInstanceRecord)
        dispatch ("find-sourcetext",
                  { sourceText: rec.scriptInstance.sourceText });
}

console.views.scripts.onClick =
function scv_click (e)
{
    if (e.originalTarget.localName == "treecol")
    {
        /* resort by column */
        var rowIndex = new Object();
        var col = new Object();
        var childElt = new Object();
        
        var treeBox = console.views.scripts.tree;
        treeBox.getCellAt(e.clientX, e.clientY, rowIndex, col, childElt);
        var prop;
        switch (col.value.id)
        {
            case "scripts:col-0":
                prop = "functionName";
                break;
            case "scripts:col-1":
                prop = "baseLineNumber";
                break;
            case "scripts:col-2":
                prop = "lineExtent";
                break;
        }

        var scriptsRoot = console.views.scripts.childData;
        var dir = (prop == scriptsRoot._share.sortColumn) ?
            scriptsRoot._share.sortDirection * -1 : 1;
        //dd ("sort direction is " + dir);
        scriptsRoot.setSortColumn (prop, dir);
    }
}

console.views.scripts.onDragStart = Prophylactic(console.views.scripts,
                                                 scv_dstart);
function scv_dstart (e, transferData, dragAction)
{
    var row = this.tree.getRowAt(e.clientX, e.clientY);
    if (row == -1)
        return false;
    
    row = this.childData.locateChildByVisualRow (row);
    var rv = false;
    if (row && ("onDragStart" in row))
        rv = row.onDragStart (e, transferData, dragAction);

    return rv;
}

console.views.scripts.fullNameMode = false;

console.views.scripts.setFullNameMode =
function scv_setmode (flag)
{
    this.fullNameMode = flag;
    for (var i = 0; i < this.childData.length; ++i)
        this.childData[i].setFullNameMode (flag);
}

console.views.scripts.getCellProperties =
function scv_cellprops (index, col, properties)
{
    if (col.id != "scripts:col-0")
        return null;
    
    var row = this.childData.locateChildByVisualRow(index);
    if (row)
    {
        if ("getProperties" in row)
            return row.getProperties (properties);

        if (row.property)
            return properties.AppendElement (row.property);
    }

    return null;
}

console.views.scripts.getContext =
function scv_getcx(cx)
{
    cx.urlList = new Array();
    cx.scriptInstanceList = new Array();
    cx.scriptWrapperList = new Array();
    cx.lineNumberList = new Array();
    cx.toggle = "toggle";
    
    function recordContextGetter (cx, rec, i)
    {
        if (i == 0)
        {
            if (rec instanceof ScriptInstanceRecord)
            {
                cx.urlPattern = cx.url = rec.url;
                cx.scriptInstance = rec.scriptInstance;
            }
            else if (rec instanceof ScriptRecord)
            {
                cx.scriptWrapper = rec.scriptWrapper;
                cx.scriptInstance = rec.scriptWrapper.scriptInstance;
                cx.urlPattern = cx.url = cx.scriptInstance.url;
                cx.lineNumber = rec.baseLineNumber;
            }
        }
        else
        {
            if (rec instanceof ScriptInstanceRecord)
            {
                cx.urlList.push (rec.url);
                cx.scriptInstanceList.push(rec.scriptInstance);
            }
            else if (rec instanceof ScriptRecord)
            {
                cx.scriptWrapperList.push (rec.scriptWrapper);
                cx.lineNumberList.push (rec.baseLineNumber);
            }
            
        }
        return cx;
    };
    
    return getTreeContext (console.views.scripts, cx, recordContextGetter);
}    

/*******************************************************************************
 * Session View
 ******************************************************************************/

console.views.session = new Object();

const VIEW_SESSION = "session";
console.views.session.viewId = VIEW_SESSION;

console.views.session.init =
function ss_init ()
{
    function currentScheme (scheme)
    {
        var css;
        
        switch (scheme)
        {
            case "default":
                css = "console.prefs['sessionView.defaultCSS']";
                break;
            case "dark":
                css = "console.prefs['sessionView.darkCSS']";
                break;
            case "light":
                css = "console.prefs['sessionView.lightCSS']";
                break;
        }

        return "console.prefs['sessionView.currentCSS'] == " + css;
    };
    
    var prefs =
        [
         ["sessionView.requireSlash", true],
         ["sessionView.commandHistory", 20],
         ["sessionView.dtabTime", 500],
         ["sessionView.maxHistory", 500],
         ["sessionView.outputWindow",
          "chrome://venkman/content/venkman-output-window.html?$css"],
         ["sessionView.currentCSS",
          "chrome://venkman/skin/venkman-output-dark.css"],
         ["sessionView.defaultCSS",
          "chrome://venkman/skin/venkman-output-default.css"],
         ["sessionView.darkCSS",
          "chrome://venkman/skin/venkman-output-dark.css"],
         ["sessionView.lightCSS",
          "chrome://venkman/skin/venkman-output-light.css"]
        ];
    
    console.prefManager.addPrefs(prefs);
    
    console.menuSpecs["context:session"] = {
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
         ["clear-session"],
         [">session:colors"]
        ]
    };

    console.menuSpecs["session:colors"] = {
        label: MSG_MNU_SESSION_COLORS,
        items:
        [
         ["session-css-default", {type: "checkbox",
                                  checkedif: currentScheme ("default")}],
         ["session-css-dark", {type: "checkbox",
                               checkedif: currentScheme ("dark")}],
         ["session-css-light", {type: "checkbox",
                                checkedif: currentScheme ("light")}]
        ]
    };
        
    this.cmdary =
        [
         ["session-css",          cmdSessionCSS,             CMD_CONSOLE],

         ["session-css-default",  "session-css default",     0],
         ["session-css-dark",     "session-css dark",        0],
         ["session-css-light",    "session-css light",       0]
        ];

    this.caption = MSG_VIEW_SESSION;

    /* input history (up/down arrow) related vars */
    this.inputHistory = new Array();
    this.lastHistoryReferenced = -1;
    this.incompleteLine = "";

    /* tab complete */
    this.lastTabUp = new Date();

    this.messageCount = 0;

    this.outputTBody = new htmlTBody({ id: "session-output-tbody" });
    this.zapDisplayFrame();
        
    this.munger = new CMunger();
    this.munger.enabled = true;
    this.munger.addRule ("quote", /(``|'')/, insertQuote);
    this.munger.addRule
        ("link", /((\w+):\/\/[^<>()\'\"\s]+|www(\.[^.<>()\'\"\s]+){2,}|x-jsd:help[\w&\?%]*)/,
         insertLink);
    this.munger.addRule ("word-hyphenator",
                         new RegExp ("(\\S{" + MAX_WORD_LEN + ",})"),
                         insertHyphenatedWord);

}

function cmdSessionCSS (e)
{
    if (e.css)
    {
        var url;
        
        switch (e.css.toLowerCase())
        {
            case "default":
                url = console.prefs["sessionView.defaultCSS"];
                break;
                
            case "dark":
                url = console.prefs["sessionView.darkCSS"];
                break;
                
            case "light":
                url = console.prefs["sessionView.lightCSS"];
                break;

            default:
                url = e.css;
                break;
                
        }
        
        console.views.session.changeDisplayCSS (url);
    }    
    
    feedback (e, getMsg(MSN_SESSION_CSS,
                        console.prefs["sessionView.currentCSS"]));
}

console.views.session.hooks = new Object();

console.views.session.hooks["clear-session"] =
function ss_clear(e)
{
    var sessionView = console.views.session;
    sessionView.outputTBody = new htmlTBody({ id: "session-output-tbody" });
    sessionView.messageCount = 0;
    if (sessionView.currentContent)
        sessionView.syncDisplayFrame();
}

console.views.session.hooks["focus-input"] =
function ss_hookFocus(e)
{
    var sessionView = console.views.session;

    if ("inputElement" in sessionView)
        sessionView.inputElement.focus();
}

console.views.session.hooks["set-eval-obj"] =
function ss_setThisObj (e)
{
    var urlFile;
    var functionName;
    
    if (console.currentEvalObject instanceof jsdIStackFrame)
    {
        if (console.currentEvalObject.script)
        {
            urlFile = 
                getFileFromPath(console.currentEvalObject.script.fileName);
            var tag = console.currentEvalObject.script.tag;
            if (tag in console.scriptWrappers)
                functionName = console.scriptWrappers[tag].functionName;
            else
                functionName = MSG_VAL_UNKNOWN;
        }
        else
        {
            urlFile = "x-jsd:native";
            functionName = console.currentEvalObject.functionName;
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

        functionName = String(parent);
    }

    var sessionView = console.views.session;
    if (sessionView.currentContent)
    {
        var title = abbreviateWord(urlFile, 30);
        sessionView.currentContent.setAttribute ("title",
                                                 getMsg(MSN_SESSION_TITLE,
                                                        [title, functionName]));
    }
}
    
console.views.session.hooks["hook-session-display"] =
function ss_hookDisplay (e)
{
    var message = e.message;
    var msgtype = (e.msgtype) ? e.msgtype : MT_INFO;
    var monospace;
    var mtname;
    
    if (msgtype[0] == "#")
    {
        monospace = true;
        mtname = msgtype.substr(1);    
    }
    else
    {
        mtname = msgtype;
    }
    
    function setAttribs (obj, c, attrs)
    {
        if (attrs)
        {
            for (var a in attrs)
                obj.setAttribute (a, attrs[a]);
        }
        obj.setAttribute("class", c);
        obj.setAttribute("msg-type", mtname);
        if (monospace)
            obj.setAttribute("monospace", "true");
    }

    var msgRow = htmlTR("msg");
    setAttribs(msgRow, "msg");

    var msgData = htmlTD();
    setAttribs(msgData, "msg-data");
    if (typeof message == "string")
        msgData.appendChild(console.views.session.stringToDOM(message,
                                                              msgtype));
    else
        msgData.appendChild(message);

    msgRow.appendChild(msgData);

    var sessionView = console.views.session;
    if (sessionView.messageCount == console.prefs["sessionView.maxHistory"])
        sessionView.outputTBody.removeChild(sessionView.outputTBody.firstChild);
    
    sessionView.outputTBody.appendChild(msgRow);
    sessionView.scrollDown();
}

console.views.session.hooks["hook-window-resized"] =
function ss_hookResize (e)
{
    console.views.session.scrollDown();
}

console.views.session.changeDisplayCSS =
function ss_changecss (url)
{
    console.prefs["sessionView.currentCSS"] = url;
    if (this.outputDocument)
    {
        this.zapDisplayFrame();
        this.syncDisplayFrame();
    }
}

console.views.session.zapDisplayFrame =
function ss_zap ()
{
    if (this.outputTBody && this.outputTBody.parentNode)
        this.outputTBody.parentNode.removeChild(this.outputTBody);
    if ("iframe" in this && this.iframe)
        this.iframe.setAttribute ("src", "about:blank");
    this.iframe = null;
    this.outputTable = null;
    this.outputWindow = null;
    this.outputDocument = null;
    this.intputElement = null;
}

console.views.session.syncDisplayFrame =
function ss_syncframe ()
{
    var sessionView = this;

    function tryAgain ()
    {
        //dd ("session view trying again...");
        sessionView.syncDisplayFrame();
    };

    var doc = this.currentContent.ownerDocument;
    
    try
    {
        this.iframe = doc.getElementById("session-output-iframe");
        if ("contentDocument" in this.iframe)
        {
            /* iframe looks ready */
            this.outputDocument = this.iframe.contentDocument;

            if (this.iframe.getAttribute ("src") == "about:blank")
            {
                /* but it doesn't point to the right place yet */
                var docURL = console.prefs["sessionView.outputWindow"];
                var css = console.prefs["sessionView.currentCSS"];
                docURL = docURL.replace ("$css", css);
                this.iframe.setAttribute("src", docURL);
            }
            else
            {
                /* now it is, get the DOM nodes */
                var win = this.currentContent.ownerWindow;
                for (var f = 0; f < win.frames.length; ++f)
                {
                    if (win.frames[f].document == this.outputDocument)
                    {
                        this.outputWindow = win.frames[f];
                        break;
                    }
                }

                this.outputTable =
                    this.outputDocument.getElementById("session-output-table");
                this.inputElement = doc.getElementById("session-sl-input");
                this.inputLabel = doc.getElementById("session-input-label");
                this.hooks["set-eval-obj"]();
            }
        }
    }
    catch (ex)
    {
        //dd ("caught exception showing session view, will try again later.");
        //dd (dumpObjectTree(ex));
    }
    
    if (!this.outputDocument || !this.outputTable || !this.inputElement)
    {
        setTimeout (tryAgain, 500);
        return;
    }

    if (this.outputTable.firstChild)
        this.outputTable.removeChild (this.outputTable.firstChild);

    this.outputTable.appendChild (this.outputTBody);
    this.scrollDown();
}

console.views.session.scrollDown =
function ss_scroll()
{
    if (this.outputWindow)
        this.outputWindow.scrollTo(0, this.outputDocument.height);
}

console.views.session.onShow =
function ss_show ()
{
    this.syncDisplayFrame();
    this.hooks["focus-input"]();
    initContextMenu(this.currentContent.ownerDocument, "context:session");
}

console.views.session.onHide =
function ss_hide ()
{
    this.zapDisplayFrame();
}

console.views.session.stringToDOM = 
function ss_stringToDOM (message, msgtype)
{
    var ary = message.split ("\n");
    var span = htmlSpan();
    
    for (var l in ary)
    {
        this.munger.munge(ary[l], span, msgtype);
        span.appendChild (htmlBR());
    }

    return span;
}

console.views.session.onInputCompleteLine =
function ss_icline (e)
{    
    if (this.inputHistory.length == 0 || this.inputHistory[0] != e.line)
        this.inputHistory.unshift (e.line);
    
    if (this.inputHistory.length > console.prefs["sessionView.commandHistory"])
        this.inputHistory.pop();
    
    this.lastHistoryReferenced = -1;
    this.incompleteLine = "";

    if (console.prefs["sessionView.requireSlash"])
    {
        if (e.line[0] == "/")
            e.line = e.line.substr(1);
        else
            e.line = "eval " + e.line;
    }
    
    var ev = {isInteractive: true, initialEvent: e};
    dispatch (e.line, ev, CMD_CONSOLE);
}

console.views.session.onSLKeyPress =
function ss_slkeypress (e)
{
    var w = this.outputWindow;
    var newOfs;
    
    switch (e.keyCode)
    {
        case 13:
            if (!e.target.value)
                return;
            e.line = e.target.value;
            this.onInputCompleteLine (e);
            e.target.value = "";
            break;

        case 38: /* up */
            e.preventDefault()
            if (this.lastHistoryReferenced == -2)
            {
                this.lastHistoryReferenced = -1;
                e.target.value = this.incompleteLine;
            }
            else if (this.lastHistoryReferenced <
                     this.inputHistory.length - 1)
            {
                e.target.value =
                    this.inputHistory[++this.lastHistoryReferenced];
            }
            break;

        case 40: /* down */
            e.preventDefault()
            if (this.lastHistoryReferenced > 0)
                e.target.value =
                    this.inputHistory[--this.lastHistoryReferenced];
            else if (this.lastHistoryReferenced == -1)
            {
                e.target.value = "";
                this.lastHistoryReferenced = -2;
            }
            else
            {
                this.lastHistoryReferenced = -1;
                e.target.value = this.incompleteLine;
            }
            break;
            
        case 33: /* pgup */
            newOfs = w.pageYOffset - (w.innerHeight / 2);
            if (newOfs > 0)
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, 0);
            break;
            
        case 34: /* pgdn */
            newOfs = w.pageYOffset + (w.innerHeight / 2);
            if (newOfs < (w.innerHeight + w.pageYOffset))
                w.scrollTo (w.pageXOffset, newOfs);
            else
                w.scrollTo (w.pageXOffset, (w.innerHeight + w.pageYOffset));
            break;

        case 9: /* tab */
            e.preventDefault();
            this.onTabCompleteRequest(e);
            break;       

        default:
            this.lastHistoryReferenced = -1;
            this.incompleteLine = e.target.value;
            break;
    }
}


console.views.session.onTabCompleteRequest =
function ss_tabcomplete (e)
{
    var selStart = e.target.selectionStart;
    var selEnd   = e.target.selectionEnd;            
    var v        = e.target.value;
    
    if (selStart != selEnd) 
    {
        /* text is highlighted, just move caret to end and exit */
        e.target.selectionStart = e.target.selectionEnd = v.length;
        return;
    }

    var firstSpace = v.indexOf(" ");
    if (firstSpace == -1)
        firstSpace = v.length;

    var pfx;
    var d;
    if ((selStart <= firstSpace))
    {
        /* The cursor is positioned before the first space, so we're completing
         * a command
         */

        var startPos;
        var slash;        
        if (console.prefs["sessionView.requireSlash"])
        {
            if (v[0] != "/")
                return;
            startPos = 1;
            slash = "/";
        }
        else
        {
            startPos = 0;
            slash = "";
        }
            
        var partialCommand = v.substring(startPos, firstSpace).toLowerCase();
        var cmds = console.commandManager.listNames(partialCommand, CMD_CONSOLE);

        if (cmds.length == 0)
        {
            /* partial didn't match a thing */
            display (getMsg(MSN_NO_CMDMATCH, partialCommand), MT_ERROR);
        }
        else if (cmds.length == 1)
        {
            /* partial matched exactly one command */
            pfx = slash + cmds[0];
            if (firstSpace == v.length)
                v =  pfx + " ";
            else
                v = pfx + v.substr (firstSpace);
            
            e.target.value = v;
            e.target.selectionStart = e.target.selectionEnd = pfx.length + 1;
        }
        else if (cmds.length > 1)
        {
            /* partial matched more than one command */
            d = new Date();
            if ((d - this.lastTabUp) <= console.prefs["sessionView.dtabTime"])
                display (getMsg (MSN_CMDMATCH,
                                 [partialCommand, cmds.join(MSG_COMMASP)]));
            else
                this.lastTabUp = d;
            
            pfx = slash + getCommonPfx(cmds);
            if (firstSpace == v.length)
                v =  pfx;
            else
                v = pfx + v.substr (firstSpace);
            
            e.target.value = v;
            e.target.selectionStart = e.target.selectionEnd = pfx.length;
            
        }
                
    }

}

/*******************************************************************************
 * Stack View
 ******************************************************************************/

console.views.stack = new XULTreeView();

const VIEW_STACK = "stack";
console.views.stack.viewId = VIEW_STACK;

console.views.stack.init =
function skv_init()
{
    var debugIf = "'scriptWrapper' in cx && " +
        "cx.scriptWrapper.jsdScript.isValid && " +
        "cx.scriptWrapper.jsdScript.flags & SCRIPT_NODEBUG";
    var profileIf = "'scriptWrapper' in cx && " +
        "cx.scriptWrapper.jsdScript.isValid && " +
        "cx.scriptWrapper.jsdScript.flags & SCRIPT_NOPROFILE";

    this.cmdary =
        [
         ["copy-frames",               cmdCopyFrames,                      0]
        ];

    console.menuSpecs["context:stack"] = {
        getContext: this.getContext,
        items:
        [
         ["find-script"],
         ["copy-frames"],
         ["-"],
         ["set-break"],
         ["clear-script", {enabledif: "cx.scriptWrapper.breakpointCount"}],
         ["-"],
         ["debug-script", {type: "checkbox", checkedif: debugIf}],
         ["profile-script", {type: "checkbox", checkedif: profileIf}],
        ]
    };

    this.caption = MSG_VIEW_STACK;

}

function cmdCopyFrames (e)
{
    var stackString = "";

    var numFrames = e.jsdFrameList.length;
    var startSearch = 0;

    for (var i = 0; i < numFrames; ++i)
    {
        var jsdFrame = e.jsdFrameList[i];
        var frameIdx = arrayIndexOf(console.frames, jsdFrame, startSearch);

        if (!ASSERT(frameIdx != -1,
                    "e.jsdFrameList[" + i + "] is not in console.frames!"))
        {
            return;
        }

        stackString += getMsg(MSN_FMT_FRAME_LINE,
                              [frameIdx, formatFrame(jsdFrame)]) + "\n";

        startSearch = frameIdx + 1;
    }

    const CLIPBOARD_CTRID = "@mozilla.org/widget/clipboardhelper;1";
    const nsIClipboardHelper = Components.interfaces.nsIClipboardHelper;

    var clipboardHelper =
        Components.classes[CLIPBOARD_CTRID].getService(nsIClipboardHelper);

    clipboardHelper.copyString(stackString);
}

console.views.stack.hooks = new Object();

console.views.stack.hooks["hook-debug-stop"] =
function skv_hookDebugStop (e)
{
    var frameRec;
    
    for (var i = 0; i < console.frames.length; ++i)
    {
        frameRec = new FrameRecord(console.frames[i]);
        console.views.stack.childData.appendChild (frameRec);
    }

    console.views.stack.hooks["find-frame"]({ frameIndex: 0 });
}

console.views.stack.hooks["hook-debug-continue"] =
function skv_hookDebugCont (e)
{
    while (console.views.stack.childData.childData.length)
        console.views.stack.childData.removeChildAtIndex(0);

    delete console.views.stack.lastFrameIndex;
}

console.views.stack.hooks["hook-guess-complete"] =
function skv_hookGuessComplete(e)
{
    var frameRecs = console.views.stack.childData.childData;
    
    for (var i = 0; i < frameRecs.length; ++i)
    {
        if (!frameRecs[i].jsdFrame.isNative)
            frameRecs[i].functionName = frameRecs[i].scriptWrapper.functionName;
    }

    if (console.views.stack.tree)
        console.views.stack.tree.invalidate();
}

console.views.stack.hooks["find-frame"] =
function skv_hookFrame (e)
{
    var stackView = console.views.stack;    
    var childData = stackView.childData.childData;

    if ("lastFrameIndex" in stackView)
        delete childData[stackView.lastFrameIndex].property;
    childData[e.frameIndex].property = FrameRecord.prototype.atomCurrent;

    stackView.lastFrameIndex = e.frameIndex;

    if (console.views.stack.tree)
    {
        stackView.scrollTo (e.frameIndex, 0);
        stackView.tree.view.selection.currentIndex = e.frameIndex;
        stackView.tree.invalidate();
    }
}

console.views.stack.onShow =
function skv_show()
{
    syncTreeView (getChildById(this.currentContent, "stack-tree"), this);
    initContextMenu(this.currentContent.ownerDocument, "context:stack");
}

console.views.stack.onHide =
function skv_hide()
{
    syncTreeView (getChildById(this.currentContent, "stack-tree"), null);
}

console.views.stack.onRowCommand =
function skv_rowcommand (rec)
{
    var rowIndex = rec.childIndex;
    
    if (rowIndex >= 0 && rowIndex < console.frames.length)
        dispatch ("frame", { frameIndex: rowIndex });
}

console.views.stack.getContext =
function sv_getcx(cx)
{
    cx.jsdFrameList = new Array();
    cx.urlList = new Array();
    cx.scriptInstanceList = new Array();
    cx.scriptWrapperList = new Array();
    cx.lineNumberList = new Array();
    cx.toggle = "toggle";
    
    function recordContextGetter (cx, rec, i)
    {
        var line;
        if (rec.scriptWrapper.jsdScript.isValid)
            line = rec.scriptWrapper.jsdScript.baseLineNumber

        if (i == 0)
        {
            cx.jsdFrame = rec.jsdFrame;

            if (rec.scriptWrapper)
            {
                cx.scriptWrapper = rec.scriptWrapper;
                cx.scriptInstance = rec.scriptWrapper.scriptInstance;
                cx.urlPattern = cx.url = cx.scriptInstance.url;
                cx.lineNumber = line;
            }
        }
        else
        {
            cx.jsdFrameList.push (rec.jsdFrame);

            if (rec.scriptWrapper)
            {
                cx.scriptWrapperList.push (rec.scriptWrapper);
                cx.scriptInstanceList.push(rec.scriptWrapper.scriptInstance);
                cx.urlList.push (rec.scriptWrapper.scriptInstance.url);
                cx.lineNumberList.push(line);
            }
        }
        return cx;
    };
    
    return getTreeContext (console.views.stack, cx, recordContextGetter);
}    

console.views.stack.getCellProperties =
function sv_cellprops (index, col, properties)
{
    if (col.id != "stack:col-0")
        return;

    var row = this.childData.locateChildByVisualRow(index);
    if (row)
    {
        if ("getProperties" in row)
            row.getProperties (properties);
        else if (row.property)
            properties.AppendElement (row.property);
    }

    return;
}

/*******************************************************************************
 * Source2 View
 *******************************************************************************/

console.views.source2 = new Object();

const VIEW_SOURCE2 = "source2";
console.views.source2.viewId = VIEW_SOURCE2;

console.views.source2.init =
function ss_init ()
{
    var prefs =
        [
         ["source2View.maxTabs", 5],
         ["source2View.showCloseButton", true],
         ["source2View.middleClickClose", false]
        ];
    
    console.prefManager.addPrefs(prefs);

    this.cmdary =
        [
         ["close-source-tab",          cmdCloseTab,              CMD_CONSOLE],
         ["find-string",               cmdFindString,            CMD_CONSOLE],
         ["save-source-tab",           cmdSaveTab,               CMD_CONSOLE],
         ["reload-source-tab",         cmdReloadTab,             CMD_CONSOLE],
         ["source-coloring",           cmdToggleColoring,                  0],
         ["toggle-source-coloring",    "source-coloring toggle",           0]
        ];

    console.menuSpecs["context:source2"] = {
        getContext: this.getContext,
        items:
        [
         ["close-source-tab"],
         ["reload-source-tab"],
         ["save-source-tab", {enabledif: "console.views.source2.canSave()"}],
         ["find-string"],
         ["-"],
         ["break", 
                 {enabledif: "cx.lineIsExecutable && !has('hasBreak')"}],
         ["clear",
                 {enabledif: "has('hasBreak')"}],
         ["set-fbreak",
                 {enabledif: "!has('hasFBreak')"}],
         ["fclear",
                 {enabledif: "has('hasFBreak')"}],
         ["-"],
         ["run-to"],
         ["cont"],
         ["next"],
         ["step"],
         ["finish"],
         ["-"],
         ["toggle-source-coloring",
                 {type: "checkbox",
                  checkedif: "console.prefs['services.source.colorize']"}],
         ["toggle-pprint",
                 {type: "checkbox",
                  checkedif: "console.prefs['prettyprint']"}],
         ["-"],
         ["break-props"]
        ]
    };

    this.caption = MSG_VIEW_SOURCE2;

    this.deck  = null;
    this.tabs  = null;
    this.heading  = null;
    this.sourceTabList = new Array();
    this.highlightTab = null;
    this.highlightNodes = new Array();
}

function cmdCloseTab (e)
{
    var source2View = console.views.source2;

    if (source2View.tabs && e.index == null)
    {
        e.index = source2View.tabs.selectedIndex;
    }
    else if (!source2View.tabs || e.index < 0 ||
             e.index > source2View.sourceTabList.length - 1)
    {
        display (MSN_ERR_INVALID_PARAM, ["index", e.index], MT_ERROR);
        return;
    }
    
    source2View.removeSourceTabAtIndex (e.index);
}

function cmdFindString (e)
{
    var source2View = console.views.source2;
    if (!source2View.tabs)
        return;

    var index = source2View.tabs.selectedIndex;
    if (!(index in source2View.sourceTabList))
        return;

    var browser = source2View.sourceTabList[index].iframe;

    if (typeof nsFindInstData != "undefined")
    {
        if (!("findData" in console))
            console.findData = new nsFindInstData();

        console.findData.browser = browser;
        console.findData.rootSearchWindow = browser.contentWindow;
        console.findData.currentSearchWindow = browser.contentWindow;
        findInPage(console.findData);
    }
    else
    {
        findInPage(browser, browser.contentWindow, browser.contentWindow);
    }
}

function cmdReloadTab (e)
{
    const WINDOW = Components.interfaces.nsIWebProgress.NOTIFY_STATE_WINDOW;
    
    if (console.views.source2.sourceTabList.length == 0)
        return;

    var source2View = console.views.source2;
    
    function cb(status)
    {
        sourceTab.iframe.addProgressListener (source2View.progressListener,
                                              WINDOW);
        sourceTab.iframe.reload();
    };

    if (source2View.tabs && e.index == null)
    {
        e.index = source2View.tabs.selectedIndex;
    }
    else if (!source2View.tabs || e.index < 0 ||
             e.index > source2View.sourceTabList.length - 1)
    {
        display (MSN_ERR_INVALID_PARAM, ["index", e.index], MT_ERROR);
        return;
    }
    
    var sourceTab = console.views.source2.sourceTabList[e.index];
    source2View.onSourceTabUnloaded (sourceTab, Components.results.NS_OK);
    sourceTab.sourceText.reloadSource(cb);
}

function cmdSaveTab (e)
{
    var source2View = console.views.source2;

    if (source2View.tabs && e.index == null)
    {
        e.index = source2View.tabs.selectedIndex;
        if (!(e.index in source2View.sourceTabList))
            return null;
    }
    else if (!source2View.tabs || e.index < 0 ||
             e.index > source2View.sourceTabList.length - 1)
    {
        display (MSN_ERR_INVALID_PARAM, ["index", e.index], MT_ERROR);
        return null;
    }
    
    var sourceText = source2View.sourceTabList[e.index].sourceText;
    var fileString;
    
    if (!e.targetFile || e.targetFile == "?")
    {
        var fileName = sourceText.url;
        //dd ("fileName is " + fileName);
        if (fileName.search(/^\w+:/) != -1)
        {
            var shortName = getFileFromPath(fileName);
            if (fileName != shortName)
                fileName = shortName;
            else
                fileName = "";
        }
        else
            fileName = "";

        var rv = pickSaveAs(MSG_SAVE_SOURCE, "*.js $xml $text $all", fileName);
        if (rv.reason == PICK_CANCEL)
            return null;
        e.targetFile = rv.file;
        fileString = rv.file.leafName;
    }
    else
    {
        fileString = e.targetFile;
    }

    var file = fopen (e.targetFile, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE);
    if (fileString.search (/xml$/i) == -1)
    {
        for (var i = 0; i < sourceText.lines.length; ++i)
            file.write(sourceText.lines[i] + "\n");
    }
    else
    {
        if ("markup" in sourceText)
            file.write (sourceText.markup);
        else
            display (MSG_ERR_FORMAT_NOT_AVAILABLE, MT_ERROR);
    }
    
    file.close();

    return file.localFile.path;
}

function cmdToggleColoring (e)
{
    if (e.toggle != null)
    {
        if (e.toggle == "toggle")
            e.toggle = !console.prefs["services.source.colorize"];

        console.prefs["services.source.colorize"] = e.toggle;
    }    
    
    if ("isInteractive" in e && e.isInteractive)
        dispatch("pref services.source.colorize", { isInteractive: true });
}

console.views.source2.canSave =
function s2v_cansave ()
{
    return this.tabs && this.sourceTabList.length > 0;
}

console.views.source2.getContext =
function s2v_getcontext (cx)
{
    var source2View = console.views.source2;
    
    cx.lineIsExecutable = false;
    var sourceText;

    if (document.popupNode.localName == "tab")
    {
        cx.index = source2View.getIndexOfTab(document.popupNode);
        sourceText = source2View.sourceTabList[cx.index].sourceText;
        cx.url = cx.urlPattern = sourceText.url;
    }
    else
    {
        cx.index = source2View.tabs.selectedIndex;
        sourceText = source2View.sourceTabList[cx.index].sourceText;
        cx.url = cx.urlPattern = sourceText.url;

        var target = document.popupNode;
        while (target)
        {
            if (target.localName == "line")
                break;
            target = target.parentNode;
        }

        if (target)
        {
            cx.lineNumber = parseInt(target.childNodes[1].firstChild.data);

            var row = cx.lineNumber - 1;

            if (sourceText.lineMap && sourceText.lineMap[row] & LINE_BREAKABLE)
            {
                cx.lineIsExecutable = true;
                if ("scriptInstance" in sourceText)
                {
                    var scriptInstance = sourceText.scriptInstance;
                    var scriptWrapper = 
                        scriptInstance.getScriptWrapperAtLine(cx.lineNumber);
                    if (scriptWrapper && scriptWrapper.jsdScript.isValid)
                    {
                        cx.scriptWrapper = scriptWrapper;
                        cx.pc =
                            scriptWrapper.jsdScript.lineToPc(cx.lineNumber,
                                                             PCMAP_SOURCETEXT);
                    }
                }
                else if ("scriptWrapper" in sourceText &&
                         sourceText.scriptWrapper.jsdScript.isValid)
                {
                    cx.scriptWrapper = sourceText.scriptWrapper;
                    cx.pc = 
                        cx.scriptWrapper.jsdScript.lineToPc(cx.lineNumber,
                                                            PCMAP_PRETTYPRINT);
                }
            }
            
            if (sourceText.lineMap && sourceText.lineMap[row] & LINE_BREAK)
            {
                cx.hasBreak = true;
                if ("scriptInstance" in sourceText)
                {
                    cx.breakWrapper =
                        sourceText.scriptInstance.getBreakpoint(cx.lineNumber);
                }
                else if ("scriptWrapper" in sourceText && "pc" in cx)
                {
                    cx.breakWrapper = 
                        sourceText.scriptWrapper.getBreakpoint(cx.pc);
                }
                
                if ("breakWrapper" in cx && cx.breakWrapper.parentBP)
                    cx.hasFBreak = true;
            }
            else if (sourceText.lineMap && sourceText.lineMap[row] & LINE_FBREAK)
            {
                cx.hasFBreak = true;
                cx.breakWrapper = getFutureBreakpoint(cx.url, cx.lineNumber);
            }
        }
    }    
    
    return cx;
}

/*
 * scroll the source so |line| is at either the top, center, or bottom
 * of the view, depending on the value of |align|.
 *
 * line is the one based target line.
 * if align is negative, the line will be scrolled to the top, if align is
 * zero the line will be centered, and if align is greater than 0 the line
 * will be scrolled to the bottom.  0 is the default.
 */
console.views.source2.scrollTabTo =
function s2v_scrollto (sourceTab, line, align)
{
    //dd ("scrolling to " + line + ", " + align);
    
    if (!sourceTab.content)
    {
        //dd ("scrollTabTo: sourceTab not loaded yet");
        sourceTab.scrollPosition = [line, align];
        return;
    }
    
    var window = sourceTab.iframe.contentWindow;
    var viewportHeight = window.innerHeight;
    var style = window.getComputedStyle(sourceTab.content, null);
    var cssValue = style.getPropertyCSSValue("height");
    var documentHeight = cssValue.getFloatValue(CSSPrimitiveValue.CSS_PX);
    var lineCount = sourceTab.sourceText.lines.length;
    var lineHeight = documentHeight / lineCount;

    var targetPos;
    
    if (align < 0)
        targetPos = (line - 1) * lineHeight;
    else if (align > 0)
        targetPos = (line + 1) * lineHeight - viewportHeight;
    else
        targetPos = (line - 1) * lineHeight - viewportHeight / 2;
    
    if (targetPos < 0)
        targetPos = 0;
    else if (targetPos > documentHeight)
        targetPos = documentHeight;
    
    window.scrollTo (window.pageXOffset, targetPos);
}

console.views.source2.updateLineMargin =
function s2v_updatemargin (sourceTab, line)
{
    if (!("lineMap" in sourceTab.sourceText))
        return;
    
    if (!sourceTab.content)
        return;
    
    var node = sourceTab.content.childNodes[line * 2 - 1];
    if (!ASSERT(node, "no node at line " + line))
        return;
    
    node = node.childNodes[0];
    var lineMap = sourceTab.sourceText.lineMap;
    var data = MSG_MARGIN_NONE;
    
    //dd ("updateLineMargin: line " + line);
    --line;

    if (line in lineMap)
    {
        var flags = lineMap[line];
        
        //dd ("updateLineMargin: flags " + flags);
        
        if (flags & LINE_BREAKABLE)
        {
            node.setAttribute ("x", "t");
            data = MSG_MARGIN_BREAKABLE;
        }
        else
        {
            node.removeAttribute ("x");
        }

        if (flags & LINE_FBREAK)
        {
            node.setAttribute ("f", "t");
            data = MSG_MARGIN_FBREAK;
        }
        else
        {
            node.removeAttribute ("f");
        }

        if (flags & LINE_BREAK)
        {
            node.setAttribute ("b", "t");
            data = MSG_MARGIN_BREAK;
        }
        else
        {
            node.removeAttribute ("b");
        }
    }
    else
    {
        node.removeAttribute ("x");
        node.removeAttribute ("f");
        node.removeAttribute ("b");
    }
    
    //dd ("node data was " + node.firstChild.data + ", wil be ``" + data + "''");
    //dd (dumpObjectTree(node));
    
    node.firstChild.data = data;
}

console.views.source2.initMargin =
function s2v_initmargin (sourceTab)
{
    if (!("lineMap" in sourceTab.sourceText))
        return;
    
    var CHUNK_SIZE  = 1000;
    var CHUNK_DELAY = 100;
    var lineMap = sourceTab.sourceText.lineMap;
    var source2View = this;
    
    function initChunk (start)
    {
        var stop = Math.min (lineMap.length, start + CHUNK_SIZE);
        for (var i = start; i < stop; ++i)
        {
            if (i in lineMap && lineMap[i] != LINE_BREAKABLE)
                source2View.updateLineMargin (sourceTab, i + 1);
        }
        
        if (i != lineMap.length)
            setTimeout (initChunk, CHUNK_DELAY, i);
    };
    
    initChunk (0);
}

console.views.source2.markStopLine =
function s2v_marktab (sourceTab, currentFrame)
{
    if ("stopNode" in sourceTab)
        this.unmarkStopLine(sourceTab);
    
    if (!currentFrame)
        return;
    
    if (!sourceTab.content)
    {
        //dd ("markStopLine: sourceTab not loaded yet");
        return;
    }

    if (!ASSERT(currentFrame.script, "current frame has no script"))
        return;
    
    var tag = currentFrame.script.tag;
    var sourceText = sourceTab.sourceText;
    var line = -1;

    if ("scriptInstance" in sourceText)
    {
        if (sourceText.scriptInstance.containsScriptTag (tag) ||
            tag in sourceText.scriptInstance.scriptManager.transients)
        {
            line = currentFrame.line;
        }
    }
    else if ("scriptWrapper" in sourceText &&
             sourceText.scriptWrapper.tag == tag)
    {
        line = currentFrame.script.pcToLine (currentFrame.pc,
                                             PCMAP_PRETTYPRINT);
    }

    if (line > 0)
    {
        //dd ("marking stop line " + line);
        var stopNode = sourceTab.content.childNodes[line * 2 - 1];
        if (!ASSERT(stopNode, "no node at line " + line))
            return;
        //dd ("stopNode: " + stopNode.localName);
        stopNode.setAttribute ("stoppedAt", "true");
        sourceTab.stopNode = stopNode;
    }    
}

console.views.source2.unmarkStopLine =
function s2v_unmarktab (sourceTab)
{
    if ("stopNode" in sourceTab)
    {
        //dd ("unmarking stop line");
        sourceTab.stopNode.removeAttribute ("stoppedAt");
        delete sourceTab.stopNode;
    }
    else
    {
        //dd ("unmarkStopLine: sourceTab had no stop line");
    }
}

console.views.source2.markHighlight =
function s2v_markhigh ()
{
    if (!this.highlightTab.content)
    {
        //dd ("highlight tab is not loaded yet");
        return;
    }

    if (!ASSERT(this.highlightNodes.length == 0,
                "something is already highlighted"))
    {
        return;
    }

    for (var i = this.highlightStart; i <= this.highlightEnd; ++i)
    {
        var node = this.highlightTab.content.childNodes[i * 2 - 1];
        if (!ASSERT(node, "no node at line " + i))
            return;
        node.setAttribute ("highlighted", "true");
        this.highlightNodes.push (node);
    }    
}

console.views.source2.unmarkHighlight =
function s2v_unmarkhigh ()
{
    if (this.highlightTab)
    {
        while (this.highlightNodes.length)
            this.highlightNodes.pop().removeAttribute ("highlighted");
    }
}

console.views.source2.clearHighlight =
function s2v_clearhigh ()
{
    this.unmarkHighlight();    
    this.highlightTab   = null;
    this.highlightStart = -1;
    this.highlightEnd   = -1;
}

console.views.source2.onSourceClick =
function s2v_sourceclick (event)
{
    var source2View = console.views.source2;
    if (!source2View.tabs)
        return;
    
    var sourceTab = source2View.sourceTabList[source2View.tabs.selectedIndex];
    var sourceText = sourceTab.sourceText;
    
    if (!("onMarginClick" in sourceText))
        return;

    var target = event.target;
    while (target)
    {
        if (target.localName == "margin")
            break;
        target = target.parentNode;
    }
    
    if (target && target.localName == "margin")
    {
        var line = parseInt (target.nextSibling.firstChild.data);
        sourceText.onMarginClick (event, line);
    }
}

console.views.source2.onSourceTabUnloaded =
function s2v_tabunloaded (sourceTab, status)
{
    this.unmarkStopLine (sourceTab);
    sourceTab.content = null;
    if (sourceTab == this.highlightTab)
        this.unmarkHighlight();
}

console.views.source2.onSourceTabLoaded =
function s2v_tabloaded (sourceTab, status)
{
    var collection = 
        sourceTab.iframe.contentDocument.getElementsByTagName("source-listing");
    sourceTab.content = collection[0];

    if (!sourceTab.content)
    {
        //dd ("tab loaded, but had no content, about:blank crap?");
        return;
    }
    
    this.markStopLine (sourceTab, getCurrentFrame());

    if (sourceTab == this.highlightTab)
        this.markHighlight();

    if ("scrollPosition" in sourceTab)
    {
        this.scrollTabTo (sourceTab, sourceTab.scrollPosition[0],
                          sourceTab.scrollPosition[1]);
    }

    this.initMargin (sourceTab);
}

console.views.source2.syncOutputFrame =
function s2v_syncframe (iframe)
{
    const nsIWebProgress = Components.interfaces.nsIWebProgress;
    const ALL = nsIWebProgress.NOTIFY_ALL;
    const DOCUMENT = nsIWebProgress.NOTIFY_STATE_DOCUMENT;
    const WINDOW = nsIWebProgress.NOTIFY_STATE_WINDOW;

    var source2View = this;

    function tryAgain ()
    {
        dd ("source2 view trying again...");
        source2View.syncOutputFrame(iframe);
    };

    var doc = this.currentContent.ownerDocument;
    
    try
    {
        if ("contentDocument" in iframe && "webProgress" in iframe)
        {
            var listener = console.views.source2.progressListener;
            iframe.addProgressListener (listener, WINDOW);
            iframe.loadURI (iframe.getAttribute ("targetSrc"));

            if (iframe.hasAttribute ("raiseOnSync"))
            {
                var i = this.getIndexOfDOMWindow (iframe.webProgress.DOMWindow);
                this.showTab(i);
                iframe.removeAttribute ("raiseOnSync");
            }
        }
        else
        {
            setTimeout (tryAgain, 500);
        }
    }
    catch (ex)
    {
        dd ("caught exception showing session view, will try again later.");
        dd (ex);
        dd (dumpObjectTree(ex));
        setTimeout (tryAgain, 500);
    }
}

console.views.source2.syncOutputDeck =
function s2v_syncdeck ()
{
    if (!ASSERT("currentContent" in this, "not inserted anywhere"))
        return;

    for (var i = 0; i < this.sourceTabList.length; ++i)
    {
        this.createFrameFor (this.sourceTabList[i], i,
                             ("lastSelectedTab" in this && 
                              this.lastSelectedTab == i));
    }

    delete this.lastSelectedTab;
}

console.views.source2.clearOutputDeck =
function s2v_cleardeck ()
{
    for (var i = 0; i < this.sourceTabList.length; ++i)
    {
        var sourceTab = this.sourceTabList[i];
        sourceTab.tab = null;
        sourceTab.iframe = null;
        this.onSourceTabUnloaded (this.sourceTabList[i],
                                  Components.results.NS_OK);
    }

    if (this.tabs)
    {
        i = this.tabs.childNodes.length;
        while (i > 0)
            this.tabs.removeChild(this.tabs.childNodes[--i]);

        i = this.deck.childNodes.length;
        while (i > 0)
            this.deck.removeChild (this.deck.childNodes[--i]);

        var bloke = document.createElement ("tab");
        bloke.setAttribute ("id", "source2-bloke");
        bloke.setAttribute ("hidden", "true");
        this.tabs.appendChild (bloke);
        bloke.selected = true;
    }
}

console.views.source2.createFrameFor =
function s2v_createframe (sourceTab, index, raiseFlag)
{
    if (!ASSERT(this.deck, "we're deckless"))
        return null;

    var bloke = getChildById (this.tabs, "source2-bloke");
    if (bloke)
        this.tabs.removeChild(bloke);

    var document = this.currentContent.ownerDocument;

    var tab = document.createElement ("tab");    
    tab.selected = false;
    tab.setAttribute ("class", "source2-tab");
    tab.setAttribute ("context", "context:source2");
    tab.setAttribute ("align", "center");
    tab.setAttribute ("flex", "1");
    tab.setAttribute("onclick",
                     "console.views.source2.onTabClick(event)");    

    var tabicon = document.createElement("image");
    tabicon.setAttribute("class", "tab-icon");
    tab.appendChild(tabicon); 
    var label = document.createElement("label");
    tab.label = label;
    label.setAttribute("value", sourceTab.sourceText.shortName);
    label.setAttribute("crop", "center");
    label.setAttribute("flex", "1");
    tab.appendChild(label);
    tab.appendChild(document.createElement("spacer"));
    if (console.prefs["source2View.showCloseButton"])
    {
        var closeButton = document.createElement("image");
        closeButton.setAttribute("class", "tab-close-button");
        closeButton.setAttribute("onclick",
                                 "console.views.source2.onCloseButton(event)");
        tab.appendChild(closeButton);
    }

    if (index < this.tabs.childNodes.length)
        this.tabs.insertBefore (tab, this.tabs.childNodes[index]);
    else
        this.tabs.appendChild (tab);

    if (!this.tabs.firstChild.hasAttribute("first-tab"))
    {
        this.tabs.firstChild.setAttribute("first-tab", "true");
        if (this.tabs.firstChild.nextSibling)
            this.tabs.firstChild.nextSibling.removeAttribute("first-tab");
    }

    sourceTab.tab = tab;

    var tabPanel = document.createElement("tabpanel");
    var iframe = document.createElement("browser");
    iframe.setAttribute ("flex", "1");
    iframe.setAttribute ("context", "context:source2");
    iframe.setAttribute ("disablehistory", "true");
    iframe.setAttribute ("disablesecurity", "true");
    iframe.setAttribute ("type", "content");
    iframe.setAttribute ("onclick",
                         "console.views.source2.onSourceClick(event);");
    iframe.setAttribute ("targetSrc", sourceTab.sourceText.jsdURL);
    if (raiseFlag)
        iframe.setAttribute ("raiseOnSync", "true");

    tabPanel.appendChild (iframe);
    if (this.deck.childNodes.length > index)
        this.deck.insertBefore (tabPanel, this.deck.childNodes[index]);
    else
        this.deck.appendChild(tabPanel);

    sourceTab.iframe = iframe;
    this.syncOutputFrame (iframe);

    return iframe;
}

console.views.source2.loadSourceTextAtIndex =
function s2v_loadsource (sourceText, index)
{
    var sourceTab;
    
    if (index in this.sourceTabList)
    {
        sourceTab = this.sourceTabList[index];
        sourceTab.content = null;
        sourceTab.sourceText = sourceText;
        
        if ("currentContent" in this)
        {
            if (!ASSERT(sourceTab.tab, 
                        "existing sourcetab not fully initialized"))
            {
                return null;
            }
            sourceTab.tab.label.setAttribute("value", sourceText.shortName);
            sourceTab.iframe.setAttribute("targetSrc", sourceText.jsdURL);
            sourceTab.iframe.setAttribute("raiseOnSync", "true");
            this.syncOutputFrame(sourceTab.iframe);
        }
        
    }
    else
    {
        sourceTab = {
            sourceText: sourceText,
            tab: null,
            iframe: null,
            content: null
        };

        this.sourceTabList[index] = sourceTab;
    
        if ("currentContent" in this)
        {
            this.createFrameFor(sourceTab, index, true);
        }
    }

    return sourceTab;
}

console.views.source2.addSourceText =
function s2v_addsource (sourceText)
{
    var index = this.getIndexOfSourceText (sourceText);
    if (index != -1)
    {
        this.showTab(index);
        return this.sourceTabList[index];
    }
    
    if (this.sourceTabList.length < console.prefs["source2View.maxTabs"])
        index = this.sourceTabList.length;
    else
        index = 0;

    return this.loadSourceTextAtIndex (sourceText, index);
}

console.views.source2.onCloseButton =
function s2v_onclose(e)
{
    var index = this.getIndexOfTab(e.target.parentNode);
    this.removeSourceTabAtIndex(index);    
    e.preventBubble()
}

console.views.source2.onTabClick =
function s2v_ontab(e)
{
    var tab;
    if (e.target.localName == "tab")
        tab = e.target;
    else
        tab = e.target.parentNode;
    
    var index = this.getIndexOfTab(tab);
    if (e.which == 2 && console.prefs["source2View.middleClickClose"])
        this.removeSourceTabAtIndex(index);
    else
        this.showTab(index);
}

console.views.source2.removeSourceText =
function s2v_removetext (sourceText)
{
    var index = this.getIndexOfSourceText (sourceText);
    if (index != -1)
        this.removeSourceTabAtIndex (index);
}

console.views.source2.removeSourceTabAtIndex =
function s2v_removeindex (index)
{
    var sourceTab = this.sourceTabList[index];
    if (this.highlightTab == sourceTab)
        this.unmarkHighlight();
    sourceTab.tab = null;
    sourceTab.iframe = null;
    sourceTab.content = null;
    sourceTab.stopNode = null;
    arrayRemoveAt (this.sourceTabList, index);

    if (this.tabs)
    {
        var lastIndex = this.tabs.selectedIndex;        
        this.tabs.removeItemAt(index);
        this.deck.removeChild(this.deck.childNodes[index]);
        if (lastIndex >= this.sourceTabList.length)
            lastIndex = this.sourceTabList.length - 1;
        if (lastIndex >= 0)
            this.showTab(lastIndex);
        if (index == 0 && this.tabs.firstChild)
            this.tabs.firstChild.setAttribute("first-tab", "true");
    }
    else if (this.lastSelectedTab > this.sourceTabList.length)
    {
        this.lastSelectedTab = this.sourceTabList.length;
    }
    
    if (this.sourceTabList.length == 0)
        this.clearOutputDeck();
}

console.views.source2.getIndexOfSourceTab =
function s2v_getstabindex (sourceTab)
{
    var index = -1;
    
    for (var i = 0; i < this.sourceTabList.length; ++i)
    {
        if (this.sourceTabList[i] == sourceTab)
        {
            index = i;
            break;
        }
    }

    return index;
}

console.views.source2.getIndexOfSourceText =
function s2v_getstextindex (sourceText)
{
    var index = -1;
    
    for (var i = 0; i < this.sourceTabList.length; ++i)
    {
        if (this.sourceTabList[i].sourceText == sourceText)
        {
            index = i;
            break;
        }
    }

    return index;
}

console.views.source2.getIndexOfTab =
function s2v_gettabindex (tab)
{
    var child = this.tabs.firstChild;
    var i = 0;
    
    while (child)
    {
        if (child == tab)
            return i;
        ++i;
        child = child.nextSibling;
    }
    
    return -1;
}

console.views.source2.getIndexOfDOMWindow =
function s2v_getindex (window)
{
    var child = this.deck.firstChild;
    var i = 0;
    
    while (child)
    {
        if (child.firstChild.webProgress.DOMWindow == window)
            return i;
        
        ++i;
        child = child.nextSibling;
    }
    
    return -1;
}

console.views.source2.getSourceTabForDOMWindow =
function s2v_getbutton (window)
{
    var i = this.getIndexOfDOMWindow(window);
    if (i == -1)
        return null;
    
    return this.sourceTabList[i];
}

console.views.source2.showTab =
function s2v_showtab (index)
{
    //dd ("show tab " + index);
    if (this.tabs)
    {
        for (var i = 0; i < this.tabs.childNodes.length; ++i)
        {
            var tab = this.tabs.childNodes[i];
            tab.selected = false;
            tab.removeAttribute("selected");
            tab.removeAttribute("beforeselected");
            tab.removeAttribute("afterselected");
        }
        
        tab = this.tabs.childNodes[index];
        tab.selected = true;
        tab.setAttribute("selected", "true");
        if (tab.previousSibling)
            tab.previousSibling.setAttribute("beforeselected", "true");
        if (tab.nextSibling)
            tab.nextSibling.setAttribute("afterselected", "true");
        
        this.tabs.selectedItem = tab;
        if (!ASSERT(index <= (this.deck.childNodes.length - 1) && index >= 0,
                    "index ``" + index + "'' out of range"))
        {
            return;
        }
        this.deck.selectedIndex = index;
        this.heading.value = this.sourceTabList[index].sourceText.url;
    }
    else
        this.lastSelectedTab = index;
}

console.views.source2.progressListener = new Object();

console.views.source2.progressListener.QueryInterface =
function s2v_qi ()
{
    //dd ("qi!");
    return this;
}

console.views.source2.progressListener.onStateChange =
function s2v_statechange (webProgress, request, stateFlags, status)
{
    const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
    const START = nsIWebProgressListener.STATE_START;
    const STOP = nsIWebProgressListener.STATE_STOP;

    //dd ("state change " + stateFlags + ", " + status);
    
    var source2View = console.views.source2;
    var sourceTab;
    if (stateFlags & START)
    {
        //dd ("start load");
        sourceTab = source2View.getSourceTabForDOMWindow(webProgress.DOMWindow);
        if (!ASSERT(sourceTab, "can't find tab for window"))
        {
            webProgress.removeProgressListener (this);
            return;
        }

        sourceTab.tab.setAttribute ("loading", "true");
    }
    else if (stateFlags == 786448)
    {
        /*
        dd ("stop load " + stateFlags + " " + 
            webProgress.DOMWindow.location.href);
        */

        sourceTab = source2View.getSourceTabForDOMWindow(webProgress.DOMWindow);
        if (webProgress.DOMWindow.location.href !=
            sourceTab.iframe.getAttribute("targetSrc"))
        {
            return;
        }
        
        webProgress.removeProgressListener (this);
        if (!ASSERT(sourceTab, "can't find tab for window"))
            return;
        
        sourceTab.tab.removeAttribute ("loading");
        source2View.onSourceTabLoaded (sourceTab, status);
    }    
}

console.views.source2.progressListener.onProgressChange =
function s2v_progresschange (webProgress, request, currentSelf, totalSelf,
                             currentMax, selfMax)
{
    //dd ("progress change ");
}

console.views.source2.progressListener.onLocationChange =
function s2v_statechange (webProgress, request, uri)
{
    //dd ("location change " + uri);
}

console.views.source2.progressListener.onStatusChange =
function s2v_statechange (webProgress, request, status, message)
{
    //dd ("status change " + status + ", " + message);
}

console.views.source2.progressListener.onSecurityChange =
function s2v_statechange (webProgress, request, state)
{
    //dd ("security change");
}

console.views.source2.hooks = new Object();

console.views.source2.hooks["hook-script-instance-destroyed"] =
function s2v_hookScriptInstanceDestroyed (e)
{
    if ("_sourceText" in e.scriptInstance)
        console.views.source2.removeSourceText (e.scriptInstance.sourceText);
}

console.views.source2.hooks["hook-debug-stop"] =
console.views.source2.hooks["frame"] =
function s2v_hookDebugStop (e)
{
    var source2View = console.views.source2;

    var currentFrame = getCurrentFrame();
    for (var i = 0; i < source2View.sourceTabList.length; ++i)
        source2View.markStopLine (source2View.sourceTabList[i], currentFrame);
}

console.views.source2.hooks["hook-debug-continue"] =
function s2v_hookDebugCont (e)
{
    var source2View = console.views.source2;

    for (var i = 0; i < source2View.sourceTabList.length; ++i)
        source2View.unmarkStopLine (source2View.sourceTabList[i]);
}

console.views.source2.hooks["hook-break-set"] =
console.views.source2.hooks["hook-break-clear"] =
function s2v_hookBreakSet (e)
{
    var source2View = console.views.source2;

    for (var i = 0; i < source2View.sourceTabList.length; ++i)
    {
        var jsdScript;
        var line = -1;
        var sourceTab = source2View.sourceTabList[i];
        var sourceText = sourceTab.sourceText;

        if ("scriptInstance" in sourceText &&
            sourceText.scriptInstance ==
            e.breakWrapper.scriptWrapper.scriptInstance)
        {
            jsdScript = e.breakWrapper.scriptWrapper.jsdScript;
            if (jsdScript.isValid)
                line = jsdScript.pcToLine (e.breakWrapper.pc, PCMAP_SOURCETEXT);
        }
        else if ("scriptWrapper" in sourceText &&
                 sourceText.scriptWrapper == e.breakWrapper.scriptWrapper)
        {
            jsdScript = e.breakWrapper.scriptWrapper.jsdScript;
            if (jsdScript.isValid)
                line = jsdScript.pcToLine (e.breakWrapper.pc, PCMAP_PRETTYPRINT);
        }

        if (line > 0)
            source2View.updateLineMargin (sourceTab, line);
    }
}

console.views.source2.hooks["hook-fbreak-set"] =
console.views.source2.hooks["hook-fbreak-clear"] =
function s2v_hookBreakSet (e)
{
    var source2View = console.views.source2;

    for (var i = 0; i < source2View.sourceTabList.length; ++i)
    {
        var sourceTab = source2View.sourceTabList[i];
        var sourceText = sourceTab.sourceText;

        if (sourceText.url.indexOf(e.fbreak.url) != -1)
            source2View.updateLineMargin (sourceTab, e.fbreak.lineNumber);
    }
}

console.views.source2.hooks["hook-display-sourcetext"] =
console.views.source2.hooks["hook-display-sourcetext-soft"] =
function s2v_hookDisplay (e)
{
    var source2View = console.views.source2;

    source2View.unmarkHighlight();
    
    var sourceTab = source2View.addSourceText (e.sourceText);
    if (e.rangeStart)
    {
        source2View.highlightStart = e.rangeStart;
        source2View.highlightEnd = e.rangeEnd;
        source2View.highlightTab = sourceTab;
        source2View.markHighlight();
    }
    
    source2View.scrollTabTo (sourceTab, e.targetLine, 0);
}

console.views.source2.onShow =
function s2v_show ()
{
    var source2View = this;
    function tryAgain ()
    {
        //dd ("source2 view trying again...");
        source2View.onShow();
    };

    //var version;
    //var help;
    
    try
    {
        this.tabs  = getChildById (this.currentContent, "source2-tabs");
        this.deck  = getChildById (this.currentContent, "source2-deck");
        this.heading = getChildById(this.currentContent, "source2-heading");
        //this.bloke = getChildById (this.currentContent, "source2-bloke");
        //version = getChildById (this.currentContent, "source2-version-label");
        //help = getChildById (this.currentContent, "source2-help-label");
    }
    catch (ex)
    {
        //dd ("caught exception showing source2 view...");
        //dd (dumpObjectTree(ex));
    }
    
    if (!this.deck || !this.tabs)
    {
        setTimeout (tryAgain, 500);
        return;
    }

    //version.setAttribute ("value", console.userAgent);
    //help.setAttribute ("value", MSG_SOURCE2_HELP);
    this.syncOutputDeck();
    initContextMenu(this.currentContent.ownerDocument, "context:source2");
}

console.views.source2.onHide =
function s2v_hide ()
{
    if (this.sourceTabList.length > 0)
        this.lastSelectedTab = this.tabs.selectedIndex;
    this.clearOutputDeck();
    this.deck = null;
    this.tabs = null;
    this.heading = null;
}

/*******************************************************************************
 * Source View
 *******************************************************************************/
console.views.source = new BasicOView();

const VIEW_SOURCE = "source";
console.views.source.viewId = VIEW_SOURCE;

console.views.source.details = null;
console.views.source.prettyPrint = false;

console.views.source.init =
function sv_init()
{
    this.savedState = new Object();

    console.prefManager.addPref("sourceView.enableMenuItem", false);    
    this.enableMenuItem = console.prefs["sourceView.enableMenuItem"];

    console.menuSpecs["context:source"] = {
        getContext: this.getContext,
        items:
        [
         ["break", 
                 {enabledif: "cx.lineIsExecutable && !has('hasBreak')"}],
         ["clear",
                 {enabledif: "has('hasBreak')"}],
         ["fbreak",
                 {enabledif: "!has('hasFBreak')"}],
         ["fclear",
                 {enabledif: "has('hasFBreak')"}],
         ["-"],
         ["run-to"],
         ["cont"],
         ["next"],
         ["step"],
         ["finish"],
         ["-"],
         ["toggle-pprint",
                 {type: "checkbox",
                  checkedif: "console.prefs['prettyprint']"}],
         ["-"],
         ["break-props"]
        ]
    };
    
    this.caption = MSG_VIEW_SOURCE;
    var atomsvc = console.atomService;

    this.atomCurrent        = atomsvc.getAtom("current-line");
    this.atomHighlightStart = atomsvc.getAtom("highlight-start");
    this.atomHighlightRange = atomsvc.getAtom("highlight-range");
    this.atomHighlightEnd   = atomsvc.getAtom("highlight-end");
    this.atomBreak          = atomsvc.getAtom("breakpoint");
    this.atomFBreak         = atomsvc.getAtom("future-breakpoint");
    this.atomCode           = atomsvc.getAtom("code");
    this.atomPrettyPrint    = atomsvc.getAtom("prettyprint");
    this.atomWhitespace     = atomsvc.getAtom("whitespace");
}

console.views.source.hooks = new Object();

console.views.source.hooks["hook-break-set"] =
console.views.source.hooks["hook-break-clear"] =
console.views.source.hooks["hook-fbreak-set"] =
console.views.source.hooks["hook-fbreak-clear"] =
function sv_hookBreakChange(e)
{
    if (console.views.source.tree)
        console.views.source.tree.invalidate();
}

console.views.source.hooks["hook-debug-continue"] =
function sv_hookDebugCont (e)
{
    /* Invalidate on continue to remove the highlight line. */
    if (console.views.source.tree)
        console.views.source.tree.invalidate();
}

/**
 * Display requested sourcetext.
 */
console.views.source.hooks["hook-display-sourcetext"] =
console.views.source.hooks["hook-display-sourcetext-soft"] =
function sv_hookDisplay (e)
{
    var sourceView = console.views.source;
    sourceView.details = e.details;
    sourceView.displaySourceText(e.sourceText, Boolean(e.targetLine));
    sourceView.rangeStart = e.rangeStart - 1;
    sourceView.rangeEnd   = e.rangeEnd - 1;
    var targetLine;
    if (e.targetLine == null)
        targetLine = sourceView.rangeStart;
    else
        targetLine = e.targetLine - 1;

    if (sourceView.tree)
    {
        if (e.targetLine && e.command.name == "hook-display-sourcetext-soft")
        {
            sourceView.softScrollTo (targetLine);
        }
        else if (sourceView.rangeEnd && 
                 (targetLine > e.rangeStart && targetLine <= e.rangeEnd) ||
                 (e.rangeStart == e.rangeEnd))
        {
            /* if there is a range, and the target is in the range,
             * scroll target to the center */
            sourceView.scrollTo (targetLine, 0);
        }
        else
        {
            /* otherwise scroll near the top */
            sourceView.scrollTo (targetLine - 2, -1);
        }
    }
    else
    {
        var url = e.sourceText.url;
        if (!(url in sourceView.savedState))
            sourceView.savedState[url] = new Object();
        sourceView.savedState[url].topLine = targetLine;
    }   

    console.views.source.currentLine = targetLine + 1;
    console.views.source.syncHeader();
}

/**
 * Sync with the pretty print state as it changes.
 */
console.views.source.hooks["hook-source-load-complete"] =
function sv_hookLoadComplete (e)
{
    console.views.source.syncTreeView();
}

console.views.source.hooks["pprint"] =
function sv_hookLoadComplete (e)
{
    console.views.source.prettyPrint = e.toggle;
}

console.views.source.onShow =
function sv_show ()
{
    var sourceView = this;
    function cb ()
    {
        if ("childData" in sourceView)
        {
            var sourceText = sourceView.childData;
            if (sourceText && sourceText.url in sourceView.savedState)
            {
                //dd ("clearing lastRowCount");
                delete sourceView.savedState[sourceText.url].lastRowCount;
            }
        }
        
        sourceView.syncTreeView();
        sourceView.urlLabel = getChildById(sourceView.currentContent,
                                           "source-url");
        sourceView.syncHeader();
    };
    
    syncTreeView (getChildById(this.currentContent, "source-tree"), this, cb);
    initContextMenu(this.currentContent.ownerDocument, "context:source");
}

console.views.source.onHide =
function sv_hide ()
{
    syncTreeView (getChildById(this.currentContent, "source-tree"), null);
    delete this.urlLabel;
}

console.views.source.onClick =
function sv_click (e)
{
    var target = e.originalTarget;
    
    if (target.localName == "treechildren")
    {
        var row = new Object();
        var col = new Object();
        var childElt = new Object();
        
        var treeBox = console.views.source.tree;
        treeBox.getCellAt(e.clientX, e.clientY, row, col, childElt);
        if (row.value == -1)
          return;
        
        colID = col.value.id;
        row = row.value;
        
        if (colID == "source:col-0")
        {
            if ("onMarginClick" in console.views.source.childData)
                console.views.source.childData.onMarginClick (e, row.value + 1);
        }
    }

}

console.views.source.onSelect =
function sv_select (e)
{
    var sourceView = console.views.source;
    sourceView.currentLine = sourceView.tree.view.selection.currentIndex + 1;
    console.views.source.syncHeader();
}

console.views.source.super_scrollTo = BasicOView.prototype.scrollTo;

console.views.source.scrollTo =
function sv_scrollto (line, align)
{
    if (!("childData" in this))
        return;

    if (!this.childData.isLoaded)
    {
        /* the source hasn't been loaded yet, store line/align for processing
         * when the load is done. */
        this.childData.pendingScroll = line;
        this.childData.pendingScrollType = align;
        return;
    }
    this.super_scrollTo(line, align);

    if (this.tree)
        this.tree.invalidate();
}

console.views.source.syncHeader =
function sv_synch ()
{
    if ("urlLabel" in this)
    {
        var value;
        if ("childData" in this)
        {
            value = getMsg(MSN_SOURCEHEADER_URL,
                           [this.childData.url, this.currentLine]);
        }
        else
        {
            value = MSG_VAL_NONE;
        }

        this.urlLabel.setAttribute ("value", value);    
    }
}
    
console.views.source.syncTreeView =
function sv_synct (skipScrollRestore)
{    
    if (this.tree)
    {
        if (!("childData" in this) || !this.childData)
        {
            this.rowCount = 0;
            this.tree.rowCountChanged (0, 0);
            this.tree.invalidate();
            return;
        };

        var url = this.childData.url;
        var state;
        if (url in this.savedState)
            state = this.savedState[url];
        else
        {
            //dd ("making new state");
            state = this.savedState[url] = new Object();
        }

        if (!("lastRowCount" in state) || state.lastRowCount != this.rowCount)
        {
            //dd ("notifying new row count " + this.rowCount);
            this.tree.rowCountChanged(0, this.rowCount);
        }

        state.lastRowCount = this.rowCount;

        if (!skipScrollRestore && "topLine" in state)
            this.scrollTo (state.topLine, 1);

        this.tree.invalidate();
        delete state.topLine;
    }
}

/*
 * pass in a SourceText to be displayed on this tree
 */
console.views.source.displaySourceText =
function sv_dsource (sourceText, skipScrollRestore)
{
    var sourceView = this;

    if (!sourceText)
    {
        delete this.childData;
        this.rowCount = 0;
        this.syncTreeView();
        return;
    }

    if (!ASSERT(sourceText.isLoaded,
                "Source text for '" + sourceText.url + "' has not been loaded."))
    {
        return;
    }
    
    if ("childData" in this && sourceText == this.childData)
        return;
    
    /* save the current position before we change to another source */
    if ("childData" in this && this.tree)
    {
        this.savedState[this.childData.url].topLine = 
            this.tree.getFirstVisibleRow() + 1;
    }
    
    
    this.childData = sourceText;
    this.rowCount = sourceText.lines.length;
    //var hdr = document.getElementById("source-line-text");
    //hdr.setAttribute ("label", sourceText.fileName);

    this.syncTreeView(skipScrollRestore);
}

/*
 * "soft" scroll to a line number in the current source.  soft, in this
 * case, means that if the target line somewhere in the center of the
 * source view already, then we can just exit.  otherwise, we'll center on the
 * target line.  this is used when single stepping through source, when constant
 * one-line scrolls would be distracting.
 *
 * the line parameter is one based.
 */
console.views.source.softScrollTo =
function sv_lscroll (line)
{
    if (!("childData" in this))
        return;
    
    if (!this.childData.isLoaded)
    {
        /* the source hasn't been loaded yet, queue the scroll for later. */
        this.childData.pendingScroll = line;
        this.childData.pendingScrollType = 0;
        return;
    }

    delete this.childData.pendingScroll;
    delete this.childData.pendingScrollType;

    var first = this.tree.getFirstVisibleRow();
    var last = this.tree.getLastVisibleRow();
    var fuzz = 2;
    if (line < (first + fuzz) || line > (last - fuzz))
        this.scrollTo (line - 2, -1);
    else
        this.tree.invalidate(); /* invalidate to show the new currentLine if
                                 * we don't have to scroll. */
}    

/**
 * Create a context object for use in the sourceView context menu.
 */
console.views.source.getContext =
function sv_getcx(cx)
{
    if (!cx)
        cx = new Object();

    var sourceText = console.views.source.childData;
    cx.url = cx.urlPattern = sourceText.url;
    cx.breakWrapperList = new Array();
    cx.lineNumberList = new Array();
    cx.lineIsExecutable = null;

    function rowContextGetter (cx, row, i)
    {
        if (!("lineMap" in sourceText))
            return cx;
        
        if (i == 0)
        {
            cx.lineNumber = row + 1;
            
            if (sourceText.lineMap[row] & LINE_BREAKABLE)
            {
                cx.lineIsExecutable = true;

                if ("scriptInstance" in sourceText)
                {
                    var scriptInstance = sourceText.scriptInstance;
                    var scriptWrapper = 
                        scriptInstance.getScriptWrapperAtLine(cx.lineNumber);
                    if (scriptWrapper)
                    {
                        cx.scriptWrapper = scriptWrapper;
                        cx.pc =
                            scriptWrapper.jsdScript.lineToPc(cx.lineNumber,
                                                             PCMAP_SOURCETEXT);
                    }
                }
                else if ("scriptWrapper" in sourceText)
                {
                    cx.scriptWrapper = sourceText.scriptWrapper;
                    cx.pc = 
                        cx.scriptWrapper.jsdScript.lineToPc(cx.lineNumber,
                                                            PCMAP_PRETTYPRINT);
                }
            }
            
            if (sourceText.lineMap[row] & LINE_BREAK)
            {
                cx.hasBreak = true;
                cx.breakWrapper =
                    sourceText.scriptInstance.getBreakpoint(row + 1);
                if (cx.breakWrapper.parentBP)
                    cx.hasFBreak = true;
            }
            else if (sourceText.lineMap[row] & LINE_FBREAK)
            {
                cx.hasFBreak = true;
                cx.breakWrapper = getFutureBreakpoint(cx.url, row + 1);
            }
        }
        else
        {
            if (sourceText.lineMap[row] & LINE_BREAK)
            {
                cx.hasBreak = true;
                var wrapper = sourceText.scriptInstance.getBreakpoint(row + 1);
                if (wrapper.parentBP)
                    cx.hasFBreak = true;
                cx.breakWrapperList.push(wrapper);
            }
            else if (sourceText.lineMap[row] & LINE_FBREAK)
            {
                cx.hasFBreak = true;
                cx.breakWrapperList.push(getFutureBreakpoint(cx.url, row + 1));
            }
        }
        
        return cx;
    };
    
    getTreeContext (console.views.source, cx, rowContextGetter);

    return cx;
}    

/* nsITreeView */
console.views.source.getRowProperties =
function sv_rowprops (row, properties)
{
    var prettyPrint = console.prefs["prettyprint"];
    
    if ("frames" in console)
    {
        if (((!prettyPrint && row == console.stopLine - 1) ||
             (prettyPrint && row == console.pp_stopLine - 1)) &&
            console.stopFile == this.childData.url)
        {
            properties.AppendElement(this.atomCurrent);
        }
    }
}

/* nsITreeView */
console.views.source.getCellProperties =
function sv_cellprops (row, col, properties)
{
    if (!("childData" in this) || !this.childData.isLoaded ||
        row < 0 || row >= this.childData.lines.length)
        return;

    var line = this.childData.lines[row];
    if (!line)
        return;
    
    if (col.id == "source:col-0")
    {
        if ("lineMap" in this.childData && row in this.childData.lineMap)
        {
            var flags = this.childData.lineMap[row];
            if (flags & LINE_BREAK)
                properties.AppendElement(this.atomBreak);
            if (flags & LINE_FBREAK)
                properties.AppendElement(this.atomFBreak);
            if (flags & LINE_BREAKABLE)
                properties.AppendElement(this.atomCode);
        }
    }
    
    if (this.rangeEnd != null)
    {
        if (row == this.rangeStart)
        {
            properties.AppendElement(this.atomHighlightStart);
        }
        else if (row == this.rangeEnd)
        {
            properties.AppendElement(this.atomHighlightEnd);
        }
        else if (row > this.rangeStart && row < this.rangeEnd)
        {
            properties.AppendElement(this.atomHighlightRange);
        }        
    }

    if ("frames" in console)
    {
        if (((!this.prettyPrint && row == console.stopLine - 1) ||
             (this.prettyPrint && row == console.pp_stopLine - 1)) &&
            console.stopFile == this.childData.url)
        {
            properties.AppendElement(this.atomCurrent);
        }
    }
}

/* nsITreeView */
console.views.source.getCellText =
function sv_getcelltext (row, col)
{    
    if (!this.childData.isLoaded || 
        row < 0 || row > this.childData.lines.length)
        return "";
    
    var ary = col.id.match (/:(.*)/);
    if (ary)
        col = ary[1];
    
    switch (col)
    {
        case "col-2":
            return this.childData.lines[row];

        case "col-1":
            return row + 1;
            
        default:
            return "";
    }
}

/*******************************************************************************
 * Watch View
 *******************************************************************************/

console.views.watches = new XULTreeView();

const VIEW_WATCHES = "watches";
console.views.watches.viewId = VIEW_WATCHES;

console.views.watches.init =
function wv_init()
{
    this.cmdary =
        [
         ["watch-expr",     cmdWatchExpr,          CMD_CONSOLE],
         ["watch-exprd",    cmdWatchExpr,          CMD_CONSOLE],
         ["remove-watch",   cmdUnwatch,            CMD_CONSOLE],
         ["save-watches",   cmdSaveWatches,        CMD_CONSOLE],
         ["watch-property", cmdWatchProperty,      0],
        ];

    console.menuSpecs["context:watches"] = {
        getContext: this.getContext,
        items:
        [
         ["change-value", {enabledif: "cx.parentValue"}],
         ["-"],
         ["watch-expr"],
         ["remove-watch"],
         ["set-eval-obj", {type: "checkbox",
                           checkedif: "has('jsdValue') && " +
                                      "cx.jsdValue.getWrappedValue() == " +
                                      "console.currentEvalObject",
                           enabledif: "has('jsdValue') && " +
                                      "cx.jsdValue.jsType == TYPE_OBJECT"}],
         ["-"],
         ["find-creator",
                 {enabledif: "cx.target instanceof ValueRecord && " +
                  "cx.target.jsType == jsdIValue.TYPE_OBJECT  && " +
                  "cx.target.value.objectValue.creatorURL"}],
         ["find-ctor",
                 {enabledif: "cx.target instanceof ValueRecord && " +
                  "cx.target.jsType == jsdIValue.TYPE_OBJECT  && " +
                  "cx.target.value.objectValue.constructorURL"}],
         ["-"],
         ["toggle-functions",
                 {type: "checkbox",
                  checkedif: "ValueRecord.prototype.showFunctions"}],
         ["toggle-ecmas",
                 {type: "checkbox",
                  checkedif: "ValueRecord.prototype.showECMAProps"}],
         ["-"],
         ["save-watches"],
         ["restore-settings"]
        ]
    };

    this.caption = MSG_VIEW_WATCHES;

}

console.views.watches.destroy =
function lv_destroy ()
{
    var childRecords = this.childData.childData;
    for (var i = 0; i < childRecords.length; ++i)
        delete childRecords[i].onPreRefresh;
    
    delete this.childData;
}

console.views.watches.hooks = new Object();

console.views.watches.hooks["hook-debug-stop"] =
console.views.watches.hooks["hook-eval-done"] =
function lv_hookEvalDone (e)
{
    console.views.watches.refresh();
}


console.views.watches.onShow =
function wv_show()
{
    syncTreeView (getChildById(this.currentContent, "watch-tree"), this);
    initContextMenu(this.currentContent.ownerDocument, "context:watches");
}

console.views.watches.onHide =
function onHide()
{
    syncTreeView (getChildById(this.currentContent, "watch-tree"), null);
}

console.views.watches.getCellProperties =
function wv_cellprops (index, col, properties)
{
    if (col.id != "watches:col-0")
        return null;
    
    var row = this.childData.locateChildByVisualRow(index);
    if (row)
    {
        if ("getProperties" in row)
            return row.getProperties (properties);

        if (row.property)
            return properties.AppendElement (row.property);
    }

    return null;
}

console.views.watches.getContext =
function wv_getcx(cx)
{
    cx.jsdValueList = new Array();
    cx.indexList    = new Array();

    function recordContextGetter (cx, rec, i)
    {
        if (rec instanceof ValueRecord)
        {
            if (i == 0)
            {
                cx.jsdValue = rec.value;
                if ("value" in rec.parentRecord)
                    cx.parentValue = rec.parentRecord.value;
                else
                    cx.parentValue = null;
                cx.propertyName = rec.displayName;
                if (rec.parentRecord == console.views.watches.childData)
                    cx.index = rec.childIndex;
            }
            else
            {
                cx.jsdValueList.push(rec.value);
                if (rec.parentRecord == console.views.watches.childData)
                    cx.indexList.push(rec.childIndex);
            }
        }
    };
    
    return getTreeContext (console.views.watches, cx, recordContextGetter);
}

console.views.watches.refresh =
function wv_refresh()
{
    if (!this.tree)
        return;
    
    var rootRecord = this.childData;
    if (!"childData" in rootRecord)
        return;
    
    this.freeze();
    for (var i = 0; i < rootRecord.childData.length; ++i)
        rootRecord.childData[i].refresh()

    this.thaw();
    /* the refresh may have changed a property without altering the
     * size of the tree, so thaw might not invalidate. */
    this.tree.invalidate();
}

console.views.watches.onRowCommand =
function wv_rowcommand(rec)
{
    if ("value" in rec.parentRecord)
    {
        dispatch ("change-value", 
                  {parentValue: rec.parentRecord.value,
                   propertyName: rec.displayName});
    }
}

console.views.watches.onKeyPress =
function wv_keypress(rec, e)
{
    if (e.keyCode == 46)
    {
        var cx = this.getContext({});
        if ("index" in cx)
            dispatch ("remove-watch", cx);        
    }
}

function cmdUnwatch (e)
{
    var watches = console.views.watches.childData;

    function unwatch (index)
    {
        if (!index in watches)
        {
            display (getMsg(MSN_ERR_INVALID_PARAM, ["index", index]), MT_ERROR);
            return;
        }

        watches.removeChildAtIndex(index);
    };
    
    if (e.indexList)
    {
        e.indexList = e.indexList.sort();
        for (var i = e.indexList.length - 1; i >= 0; --i)
            unwatch(e.indexList[i]);
    }
    else
    {
        unwatch (e.index);
    }
}

function cmdWatchExpr (e)
{
    var watches = console.views.watches;
    
    if (!e.expression)
    {
        if ("isInteractive" in e && e.isInteractive)
        {
            var watchData = console.views.watches.childData.childData;
            var len = watchData.length;
            if (len == 0)
            {
                display (getMsg(MSG_NO_WATCHES_SET));
                return null;
            }
            
            display (getMsg(MSN_WATCH_HEADER, len));
            for (var i = 0; i < len; ++i)
            {
                display (getMsg(MSN_FMT_WATCH_ITEM,
                                [i, watchData[i].displayName,
                                 watchData[i].displayValue]));
            }
            return null;
        }

        var parent;
            
        if (watches.currentContent)
            parent = watches.currentContent.ownerWindow;
        else
            parent = window;
            
        e.expression = prompt(MSG_ENTER_WATCH, "", parent);
        if (!e.expression)
            return null;
    }
    
    var refresher;
    if (e.command.name == "watch-expr")
    {
        if (!("currentEvalObject" in console))
        {
            display (MSG_ERR_NO_EVAL_OBJECT, MT_ERROR);
            return null;
        }

        refresher = function () {
                        if ("frames" in console)
                            this.value = evalInTargetScope(e.expression, true);
                        else
                            throw MSG_VAL_NA;
                    };
    }
    else
    {
        refresher = function () {
                        var rv = evalInDebuggerScope(e.expression, true);
                        this.value = console.jsds.wrapValue(rv);
                    };
    }
    
    var rec = new ValueRecord(console.jsds.wrapValue(null), e.expression, 0);
    rec.onPreRefresh = refresher;
    rec.refresh();
    watches.childData.appendChild(rec);
    watches.refresh();
    return rec;
}

function cmdWatchProperty (e)
{
    var rec = new ValueRecord(console.jsds.wrapValue(null),
                              e.propertyName, 0);
    rec.onPreRefresh = function () {
                           var prop = e.jsdValue.getProperty(e.propertyName);
                           this.value = prop.value;
                           this.flags = prop.flags;
                           this.displayFlags = this.flags;
                       };
    rec.onPreRefresh();
    console.views.watches.childData.appendChild(rec);
    return rec;
}

function cmdSaveWatches(e)
{
    var needClose = false;
    var file = e.settingsFile;
    
    if (!file || file == "?")
    {
        rv = pickSaveAs(MSG_SAVE_FILE, "*.js");
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

    file.write ("\n//Watch settings start...\n");
    
    var watchData = console.views.watches.childData.childData;
    var len = watchData.length;
    for (var i = 0; i < len; ++i)
    {
        file.write("dispatch('watch-expr " + watchData[i].displayName + "');\n");
    }

    file.write ("\n" + MSG_WATCHES_RESTORED.quote() + ";\n");    

    if (needClose)
        file.close();
}

/*******************************************************************************
 * Windows View
 *******************************************************************************/

console.views.windows = new XULTreeView();

const VIEW_WINDOWS = "windows";
console.views.windows.viewId = VIEW_WINDOWS;

console.views.windows.init = 
function winv_init ()
{
    console.menuSpecs["context:windows"] = {
        getContext: this.getContext,
        items:
        [
         ["find-url"],
         ["set-eval-obj", {type: "checkbox",
                           checkedif: "has('jsdValue') && " +
                                      "cx.jsdValue.getWrappedValue() == " +
                                      "console.currentEvalObject",
                           enabledif: "has('jsdValue') && " +
                                       "cx.jsdValue.jsType == TYPE_OBJECT"}]
        ]
    };

    this.caption = MSG_VIEW_WINDOWS;

}

console.views.windows.hooks = new Object();

console.views.windows.hooks["hook-window-opened"] =
function winv_hookWindowOpened (e)
{
    console.views.windows.childData.appendChild (new WindowRecord(e.window, ""));
}

console.views.windows.hooks["hook-window-closed"] =
function winv_hookWindowClosed (e)
{
    var winRecord = console.views.windows.locateChildByWindow(e.window);
    if (!ASSERT(winRecord, "Can't find window record for closed window."))
        return;
    console.views.windows.childData.removeChildAtIndex(winRecord.childIndex);
}

console.views.windows.hooks["chrome-filter"] =
function scv_hookChromeFilter(e)
{
    if (e.toggle != null)
    {
        var windowsView = console.views.windows;
        var rootRecord = console.views.windows.childData;

        windowsView.freeze();
        rootRecord.childData = new Array();
        var enumerator = console.windowWatcher.getWindowEnumerator();
        while (enumerator.hasMoreElements())
        {
            var window = enumerator.getNext();
            if (!isWindowFiltered(window))
                rootRecord.appendChild (new WindowRecord(window, ""));
        }
        windowsView.thaw();
        if (windowsView.tree)
            windowsView.tree.invalidate();
    }

}

console.views.windows.onShow =
function winv_show ()
{
    syncTreeView (getChildById(this.currentContent, "windows-tree"), this);
    initContextMenu(this.currentContent.ownerDocument, "context:windows");
}

console.views.windows.onHide =
function winv_hide ()
{
    syncTreeView (getChildById(this.currentContent, "windows-tree"), null);
}

console.views.windows.getCellProperties =
function winv_cellprops (index, col, properties)
{
    if (col.id == "windows:col-0")
    {
        var row = this.childData.locateChildByVisualRow(index);
        if (row)
            properties.AppendElement (row.property);
    }

    return;
}

console.views.windows.onRowCommand =
function winv_rowcommand (rec)
{
    if ("url" in rec)
        dispatch ("find-url", { url: rec.url });
}

console.views.windows.getContext =
function winv_getcx(cx)
{
    cx.jsdValueList = new Array();
    cx.urlList    = new Array();

    function recordContextGetter (cx, rec, i)
    {
        if (i == 0)
        {
            if (rec instanceof WindowRecord || rec instanceof FileRecord)
            {
                if ("window" in rec)
                    cx.jsdValue = console.jsds.wrapValue(rec.window);
                cx.url = rec.url;
            }
        }
        else
        {
            if (rec instanceof WindowRecord || rec instanceof FileRecord)
            {
                if ("window" in rec)
                    cx.jsdValueList.push(console.jsds.wrapValue(rec.window));
                cx.urlList.push (rec.url);
            }
        }
    };
    
    return getTreeContext (console.views.windows, cx, recordContextGetter);
}

console.views.windows.locateChildByWindow =
function winv_find (win)
{
    var children = this.childData.childData;
    for (var i = 0; i < children.length; ++i)
    {
        var child = children[i];
        if (child.window == win)
            return child;
    }

    return null;
}

/************************/

function formatRecord (rec, indent)
{
    var str = "";
    
    for (var i in rec._colValues)
        str += rec._colValues[i] + ", ";
    
    str += "[";
    
    str += rec.calculateVisualRow() + ", ";
    str += rec.childIndex + ", ";
    str += rec.level + ", ";
    str += rec.visualFootprint + ", ";
    str += rec.isContainerOpen + ", ";
    str += rec.isHidden + "]";
    
    dd (indent + str);
}

function formatBranch (rec, indent)
{
    for (var i = 0; i < rec.childData.length; ++i)
    {
        formatRecord (rec.childData[i], indent);
        if (rec.childData[i].childData)
            formatBranch(rec.childData[i], indent + "  ");
    }
}
