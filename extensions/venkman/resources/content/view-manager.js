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

const VMGR_VURL_SCHEME = "x-vloc:";
const VMGR_SCHEME_LEN  = VMGR_VURL_SCHEME.length;

const VMGR_HIDDEN      = "hidden";
const VMGR_NEW         = "new";
const VMGR_MAINWINDOW  = "mainwindow";
const VMGR_GUTTER      = "gutter";

const VMGR_VURL_HIDDEN     = VMGR_VURL_SCHEME + "/" + VMGR_HIDDEN;
const VMGR_VURL_NEW        = VMGR_VURL_SCHEME + "/" + VMGR_NEW;
const VMGR_VURL_MAINWINDOW = VMGR_VURL_SCHEME + "/" + VMGR_MAINWINDOW;
const VMGR_VURL_GUTTER     = VMGR_VURL_MAINWINDOW + "/gutter";
const VMGR_DEFAULT_CONTAINER = "initial-container";

var VMGR_GUTTER_CONTAINER = VMGR_VURL_MAINWINDOW + "?target=container&" +
    "id=" + VMGR_GUTTER + "&type=vertical";

function ViewManager(commandManager, mainWindow)
{
    this.commandManager = commandManager;    
    this.multiMoveDepth = 0;
    this.floaterSequence = 0;
    this.containerSequence = 0;
    this.dirtyContainers = new Object();
    this.windows = new Object();
    this.mainWindow = mainWindow;
    this.windows[VMGR_MAINWINDOW] = mainWindow;
    this.views = new Object();
}

ViewManager.prototype.realizeViews =
function vmgr_realizeviews (views)
{
    for (var v in views)
        this.realizeView(views[v]);
}    

ViewManager.prototype.realizeView =
function vmgr_realizeview (view)
{
    var entry;
    var key = view.viewId;
    this.views[key] = view;
    if ("init" in view && typeof view.init == "function")
        view.init();
    
    var toggleName = "toggle-" + key;
    if (!(key in this.commandManager.commands))
    {
        entry = [toggleName, "toggle-view " + key, CMD_CONSOLE];
        
        if ("cmdary" in view)
            view.cmdary.unshift(entry);
        else
            view.cmdary = [entry];
    }
    
    if ("cmdary" in view)
    {
        var commands = this.commandManager.defineCommands(view.cmdary);
        for (var i in this.windows)
            this.commandManager.installKeys (this.windows[i].document,
                                             commands);
        view.commands = commands;
    }
    if ("hooks" in view)
        this.commandManager.addHooks (view.hooks, key);
}

ViewManager.prototype.unrealizeViews =
function vmgr_unrealizeviews (views)
{
    for (var v in views)
        this.unrealizeView(views[v]);
}    

ViewManager.prototype.unrealizeView =
function vmgr_unrealizeview (view)
{
    if ("commands" in view)
        this.commandManager.uninstallKeys(view.commands);
    if ("hooks" in view)
        this.commandManager.removeHooks (view.hooks, view.viewId);

    if ("destroy" in view && typeof view.destroy == "function")
        view.destroy();

    delete this.views[view.viewId];
}

ViewManager.prototype.startMultiMove =
function vmrg_startmm()
{
    ++this.multiMoveDepth;
}

ViewManager.prototype.endMultiMove =
function vmrg_endmm()
{
    --this.multiMoveDepth;

    ASSERT(this.multiMoveDepth >= 0, "mismatched multi move calls: " +
           this.multiMoveDepth);

    var container;
    
    if (!this.multiMoveDepth)
    {
        // close any empty windows that may have been left open during the
        // multi move.
        for (var w in this.windows)
        {
            if (w != VMGR_MAINWINDOW)
            {
                var window = this.windows[w];
                container = 
                    window.document.getElementById(VMGR_DEFAULT_CONTAINER);
                if (container && container.viewCount == 0)
                    window.close();
            }
        }

        // deal with any single child tab containers too
        for (var viewId in this.views)
        {
            var view = this.views[viewId];
            if ("currentContent" in view && view.currentContent)
            {
                container = view.currentContent.parentNode;
                if (container.getAttribute("type") == "tab" &&
                    container.viewCount == 1)
                {
                    var parentLocation = this.computeLocation(container);
                    container.viewCount = 0;
                    this.moveView(parentLocation, viewId);
                    container.parentNode.removeChild(container);
                }
            }
        }
    }
}
    
ViewManager.prototype.getWindowById =
function vmgr_getwindow (windowId)
{
    if (windowId in this.windows)
        return this.windows[windowId];

    return null;
}

ViewManager.prototype.createWindow =
function vmgr_createwindow (windowId, cb)
{
    var viewManager = this;
    
    function onWindowLoaded (window)
    {
        var cm = viewManager.commandManager;
        cm.installKeys (window.document, cm.commands);
        if (typeof cb == "function")
            cb(window);
    };
    
    if (!ASSERT(!(windowId in this.windows), "window " + windowId +
                " already exists"))
    {
        return null;
    }
    
    var win = openDialog ("chrome://venkman/content/venkman-floater.xul?id=" +
                          encodeURIComponent(windowId), "_blank",
                          "chrome,menubar,toolbar,resizable,dialog=no",
                          onWindowLoaded);
    this.windows[windowId] = win;
    return win;
}

ViewManager.prototype.destroyWindows =
function vmgr_destroywindows ()
{
    this.startMultiMove();
    
    for (var w in this.windows)
    {
        if (w == VMGR_MAINWINDOW)
            this.destroyWindow(w);
        else
            this.windows[w].close();
    }
    
    this.endMultiMove();
}

ViewManager.prototype.destroyWindow =
function vmgr_destroywindow (windowId)
{
    if (!ASSERT(windowId in this.windows, "unknown window id"))
        return;

    var document = this.windows[windowId].document;
    
    for (var viewId in this.views)
    {
        try
        {
            var view = this.views[viewId];
            
            if ("currentContent" in view &&
                view.currentContent.ownerDocument == document)
            {
                this.moveViewURL(VMGR_VURL_HIDDEN, viewId);
            }
        }
        catch (ex)
        {
            dd ("caught exception while moving view ``" + ex + "''");
        }
    }

    var container = document.getElementById (VMGR_DEFAULT_CONTAINER);
    var child = container.firstChild;
    while (child)
    {
        var next = child.nextSibling;
        container.removeChild (child);
        child = next;
    }

    if (windowId != VMGR_MAINWINDOW)
        delete this.windows[windowId];
}

ViewManager.prototype.destroyContainer =
function vmgr_destroycontainer (container)
{
    content = container.firstChild;
    
    while (content)
    {
        if (content.hasAttribute("grout"))
        {
            content.parentNode.removeChild(content);
        }
        else
        {
            if (content.localName == "viewcontainer")
            {
                this.destroyContainer(content);
            }
            else
            {
                var viewId = content.getAttribute("id");
                this.moveViewURL(VMGR_VURL_HIDDEN, viewId);
            }
        }
        
        content = container.firstChild;
    }
    
    container.parentNode.removeChild(container);
}
                
ViewManager.prototype.changeContainer =
function vmgr_changecontainer (container, newType)
{
    if (!ASSERT(newType != container.getAttribute("type"),
                "requested useless change, ``" + newType + "''"))
    {
        return;
    }
    
    this.hideContainer(container);
    container.setAttribute("type", newType);
    this.showContainer(container);
    
    this.groutContainer(container);
}
    
ViewManager.prototype.createContainer =
function vmgr_createcontainer (parsedLocation, containerId, type)
{
    if (!type)
        type = "vertical";
    
    var parentContainer = this.getLocation(parsedLocation);
    
    if (!ASSERT(parentContainer, "location is hidden or does not exist"))
        return null;

    var document = parentContainer.ownerDocument;
    var container = document.createElement("viewcontainer");
    container.setAttribute ("id", containerId);
    container.setAttribute ("flex", "1");
    container.setAttribute ("type", type);
    if ("width" in parsedLocation)
        container.setAttribute ("width", parsedLocation.width);
    if ("height" in parsedLocation)
        container.setAttribute ("height", parsedLocation.height);
    container.setAttribute ("collapsed", "true");
    var before;
    if ("before" in parsedLocation)
        before = getChildById(parentContainer, parsedLocation.before);
    parentContainer.insertBefore(container, before);
    this.groutContainer(parentContainer);

    return container;
}

/*
 * x-vloc:/<window-id>[/<container-id>][?<option>[&<option>...]]
 *
 * <option> is one of...
 *   before=<view-id>
 *   type=(horizontal|vertical|tab)
 *   id=(<view-id>|<container-id>)
 *   target=(view|container)
 *   width=<number>
 *   height=<number>
 *
 * parseResult
 *  +- url
 *  +- windowId
 *  +- containerId
 *  +- <option>
 *  +- ...
 */

ViewManager.prototype.parseLocation =
function vmgr_parselocation (locationURL)
{
    var parseResult;
    var ary;
    
    if (locationURL.indexOf(VMGR_VURL_SCHEME) != 0)
        return null;
    
    ary =
        locationURL.substr(VMGR_SCHEME_LEN).match(/([^\/?]+)(?:\/([^\/?]+))?/);
    if (!ary)
        return null;

    parseResult = {
        url: locationURL,
        windowId: ary[1],
        containerId: arrayHasElementAt(ary, 2) ? ary[2] : VMGR_DEFAULT_CONTAINER
    };

    var rest = RegExp.rightContext.substr(1);

    ary = rest.match(/([^&]+)/);

    while (ary)
    {
        rest = RegExp.rightContext.substr(1);
        var assignment = ary[1];
        ary = assignment.match(/(.+)=(.*)/);
        if (ASSERT(ary, "error parsing ``" + assignment + "'' from " +
                   locationURL))
        {
            /* only set the property the first time we see it */
            if (arrayHasElementAt(ary, 2) && !(ary[1] in parseResult))
                parseResult[ary[1]] = ary[2];
        }
        ary = rest.match(/([^&]+)/);
    }
    
    if (parseResult.windowId == VMGR_NEW)
    {
        var id = "floater-" + this.floaterSequence++;

        while (id in this.windows)
            id = "floater-" + this.floaterSequence++;

        parseResult.windowId = id;
    }

    //dd (dumpObjectTree(parseResult));
    return parseResult;
}

ViewManager.prototype.getWindowId =
function getwindowid (window)
{
    for (var m in this.windows)
    {
        if (this.windows[m] == window)
            return m;
    }

    ASSERT (0, "unknown window");
    return null;
}   

ViewManager.prototype.getLayoutState =
function vmgr_getlayout ()
{
    var ary = new Array();
    
    for (var w in this.windows)
    {
        var container = 
            this.windows[w].document.getElementById (VMGR_DEFAULT_CONTAINER);
        this.getContainerContents (container, true, ary);
    }
    
    return ary;
}        
        
ViewManager.prototype.getContainerContents =
function vmgr_getcontents (container, recurse, ary)
{
    if (!ary)
        ary = new Array();
    var child = container.firstChild;
    
    while (child)
    {
        if (child.localName == "viewcontainer" && "viewCount" in child &&
            child.viewCount > 0)
        {
            ary.push (this.computeLocation(child));
            if (recurse)
                this.getContainerContents(child, true, ary);
        }
        else if (child.localName == "floatingview")
        {
            ary.push (this.computeLocation(child));
        }
        child = child.nextSibling;
    }

    return ary;
}

ViewManager.prototype.reconstituteVURLs =
function vmgr_remake (ary, cb, startIndex, recalled)
{
    var viewManager = this;
    
    function onWindowLoaded (window)
    {
        viewManager.reconstituteVURLs (ary, cb, i, true);
    };
    
    if (!startIndex)
        startIndex = 0;

    if (!recalled)
        this.startMultiMove();

    for (var i = startIndex; i < ary.length; ++i)
    {
        var parsedLocation = this.parseLocation (ary[i]);
        if (!ASSERT(parsedLocation, "can't parse " + ary[i]) ||
            !ASSERT("target" in parsedLocation, "no target in " + ary[i]) ||
            !ASSERT("id" in parsedLocation, "no id in " + ary[i]))
        {
            continue;
        }

        if (parsedLocation.target == "container")
        {
            if (!(parsedLocation.windowId in this.windows))
            {
                //dd ("loading window " + parsedLocation.windowId);
                this.createWindow (parsedLocation.windowId, onWindowLoaded);
                return;
            }
                
            if (!ASSERT("type" in parsedLocation, "no type in " + ary[i]))
                parsedLocation.type = "vertical";
            this.createContainer (parsedLocation, parsedLocation.id,
                                  parsedLocation.type);
        }
        else if (parsedLocation.target == "view")
        {
            var height = ("height" in parsedLocation ?
                         parsedLocation.height : null);
            var width = ("width" in parsedLocation ?
                         parsedLocation.width : null);
            this.moveView (parsedLocation, parsedLocation.id, height, width);
        }
        else
        {
            ASSERT(0, "unknown target in " + ary[i]);
        }
    }

    if (typeof cb == "function")
        cb();
    
    this.endMultiMove();
}

ViewManager.prototype.computeLocation =
function vmgr_computelocation (element)
{
    if (!ASSERT(element, "missig parameter"))
        return null;
    
    if (!element.parentNode)
        return VMGR_VURL_HIDDEN;
    
    var target;
    var containerId;
    var type;
    
    if (element.localName == "floatingview")
    {
        target = "view";
        if (element.parentNode.localName != "viewcontainer")
            containerId = VMGR_HIDDEN;
        else
            containerId = element.parentNode.getAttribute("id");
    }
    else if (element.localName == "viewcontainer")
    {
        target      = "container";
        containerId = element.parentNode.getAttribute("id");
        type        = element.getAttribute("type");
    }
    else
    {
        ASSERT(0, "can't get location for unknown element " +
               element.localName);
        return null;
    }

    var before;
    
    if (containerId != VMGR_HIDDEN)
    {
        var beforeNode = element.nextSibling;
        if (beforeNode)
        {
            if (beforeNode.hasAttribute("grout"))
                beforeNode = beforeNode.nextSibling;
            
            if (ASSERT(beforeNode, "nothing before the grout?"))
                before = beforeNode.getAttribute("id");
        }
    }

    var windowId = this.getWindowId(element.ownerWindow);
    var id       = element.getAttribute("id");
    var height   = element.getAttribute("height");
    var width    = element.getAttribute("width");
    var url      = VMGR_VURL_SCHEME + "/" + windowId + "/" + containerId +
        "?target=" + target + "&id=" + id;
    if (height)
        url += "&height=" + height;
    if (width)
        url += "&width=" + width;
    if (before)
        url += "&before=" + before;
    if (type)
        url += "&type=" + type;
        
    var result = new String(url);
    result.url = url;
    result.windowId = windowId;
    result.containerId = containerId;
    result.target = target;
    result.id = id;
    result.height = height;
    result.width = width;
    result.before = before;
    result.type = type;

    return result;
}

ViewManager.prototype.locationExists =
function vmgr_islocation (parsedLocation)
{
    if (parsedLocation.windowId == VMGR_HIDDEN)
        return true;
    
    if (this.getLocation(parsedLocation))
        return true;

    return false;
}

ViewManager.prototype.getLocation =
function vmgr_getlocation (parsedLocation)
{
    if (parsedLocation.windowId == VMGR_HIDDEN)
    {
        //dd ("getLocation: location is hidden");
        return null;
    }
    
    var window = this.getWindowById(parsedLocation.windowId);
    if (!ASSERT(window, "unknown window id " + parsedLocation.windowId))
        return false;

    if (!parsedLocation.containerId)
        parsedLocation.containerId = VMGR_DEFAULT_CONTAINER;
    
    var container = window.document.getElementById(parsedLocation.containerId);
    return container;
}    

ViewManager.prototype.ensureLocation =
function vmgr_ensurelocation (parsedLocation, cb)
{
    var viewManager = this;
    
    function onWindowLoaded (window)
    {
        var container = 
            window.document.getElementById (parsedLocation.containerId);
        if (!container && parsedLocation.containerId == VMGR_GUTTER)
        {
            viewManager.reconstituteVURLs ([VMGR_GUTTER_CONTAINER]);
            container = 
                window.document.getElementById (parsedLocation.containerId);
        }
        cb (window, container);
    };
    
    if (parsedLocation.windowId == VMGR_HIDDEN)
    {
        cb(null, null);
        return;
    }
    
    var window = this.getWindowById(parsedLocation.windowId);
    if (window)
    {
        onWindowLoaded(window);
        return;
    }
    
    this.createWindow (parsedLocation.windowId, onWindowLoaded);
}

/**
 * This only half-assed affects the contents.  It calls the show/hide events
 * on the contained views, but does not actually re-home the content.  This
 * should only be used to quickly move views within the same window.
 */
ViewManager.prototype.showContainer =
function vbgr_showctr (container, state)
{
    if (typeof state == "undefined")
        state = true;
    
    var content = container.firstChild;
    while (content)
    {
        if (content.localName == "floatingview")
        {
            var viewId = content.getAttribute("id");
            if (!ASSERT(viewId in this.views, "unknown view"))
            {
                content = content.nextSibling;
                continue;
            }

            if (state)
                this.views[viewId].onShow();
            else
                this.views[viewId].onHide();
        }
        else if (content.localName == "viewcontainer")
        {
            this.showContainer(content, state);
        }

        content = content.nextSibling;
    }
}

ViewManager.prototype.hideContainer =
function vbgr_hidectr (container)
{
    this.showContainer(container, false);
}

ViewManager.prototype.moveContainer =
function vmgr_movectr (parsedLocation, sourceContainer)
{
    var viewManager = this;
    
    function onLocationFound (window, container)
    {
        if (!window)
        {
            ASSERT(false, "not implemented");
            return;
            // everything gets hidden
        }
        
        var beforeNode;
        if ("before" in parsedLocation)
            beforeNode = window.document.getElementById(parsedLocation.before);

        if (!container)
        {
            /* container does not exist. */
            if (beforeNode)
            {
                // if we know who it's supposed to be before, then drop it
                // there.
                container = beforeNode.parentNode;
            }
            else
            {
                // otherwise, toss it in the gutter
                var gutterContainer = Clone(parsedLocation);
                gutterContainer.containerId = VMGR_GUTTER;
                container = viewManager.getLocation(gutterContainer);
                if (!container)
                {
                    gutterContainer.containerId = VMGR_DEFAULT_CONTAINER;
                    container = viewManager.createContainer(gutterContainer,
                                                            VMGR_GUTTER,
                                                            "vertical");
                }
            }
            if (!ASSERT(container, "can't locate default container"))
                return;
        }
        else if (beforeNode && beforeNode.parentNode != container)
        {
            //dd ("before is not in container");
            /* container portion of url wins in a mismatch situation */
            beforeNode = null;
        }

        viewManager.hideContainer(sourceContainer);

        var previousParent = sourceContainer.parentNode;
        previousParent.removeChild(sourceContainer);
        viewManager.groutContainer(previousParent);

        dd ("beforeNode is " + (beforeNode ? beforeNode.getAttribute("id") :
            null));
        
        container.insertBefore(sourceContainer, beforeNode);
        viewManager.showContainer(sourceContainer);
        viewManager.groutContainer(sourceContainer);
    };

    var sourceLocation = this.computeLocation(sourceContainer);
    if (!ASSERT (sourceLocation.windowId == parsedLocation.windowId,
                 "moving containers between windows is not supported"))
    {
        return;
    }
    
    this.ensureLocation (parsedLocation, onLocationFound);
}

ViewManager.prototype.moveViewURL =
function vmgr_moveurl (locationURL, viewId)
{
    var parsedLocation = this.parseLocation(locationURL);
    if (!ASSERT(parsedLocation, "can't parse " + locationURL))
        return;
    
    this.moveView(parsedLocation, viewId);
}

ViewManager.prototype.moveView =
function vmgr_move (parsedLocation, viewId, height, width)
{
    var viewManager = this;
    
    function moveContent (content, newParent, before)
    {
        if (!("originalParent" in content))
        {
            if (!ASSERT(content.parentNode, "no original parent to record"))
                return;
            content.originalParent = content.parentNode;
        }
        
        //dd ("OFF moveContent {");
        
        var previousParent = content.parentNode;
        previousParent.removeChild(content);
        if (previousParent.localName == "viewcontainer")
            viewManager.groutContainer (previousParent);

        if (newParent)
        {
            //dd ("placing in new parent");
            newParent.insertBefore (content, before);
            viewManager.groutContainer (newParent);
        }
        else
        {
            //dd ("hiding");
            content.originalParent.appendChild(content);
            content.setAttribute ("containertype", "hidden");
        }

        //dd ("}");
    };

    function onLocationFound (window, container)
    {
        if (!window)
        {
            /* view needs to be hidden. */
            if (currentContent)
            {
                if ("onHide" in view)
                    view.onHide();

                if ("onViewRemoved" in view.currentContent.ownerWindow)
                    view.currentContent.ownerWindow.onViewRemoved(view);

                moveContent (view.currentContent, null);
                delete view.currentContent;
            }
            return;
        }
        
        var content = window.document.getElementById(viewId);
        if (!ASSERT(content, "no content for ``" + viewId + "'' found in " +
                    parsedLocation.windowId))
        {
            return;
        }

        var beforeNode;
        if ("before" in parsedLocation)
        {
            beforeNode = window.document.getElementById(parsedLocation.before);
            if (beforeNode &&
                beforeNode.parentNode.localName != "viewcontainer")
            {
                beforeNode = null;
            }
        }
        
        if (!container)
        {
            /* container does not exist. */
            if (beforeNode)
            {
                // if we know who it's supposed to be before, then drop it
                // there.
                container = beforeNode.parentNode;
            }
            else
            {
                // otherwise, toss it in the gutter
                var gutterContainer = Clone(parsedLocation);
                gutterContainer.containerId = VMGR_GUTTER;
                container = viewManager.getLocation(gutterContainer);
                if (!container)
                {
                    gutterContainer.containerId = VMGR_DEFAULT_CONTAINER;
                    container = viewManager.createContainer(gutterContainer,
                                                            VMGR_GUTTER,
                                                            "vertical");
                }
            }
            if (!ASSERT(container, "can't locate default container"))
                return;
        }
        else if (beforeNode && beforeNode.parentNode != container)
        {
            //dd ("before is not in container");
            /* container portion of url wins in a mismatch situation */
            beforeNode = null;
        }   
        
        if (currentContent)
        {
            //dd ("have content");
            /* view is already visible, fire an onHide().  If we're destined for
             * a new document, return the current content to it's hiding place.
             */
            if (beforeNode == currentContent ||
                (currentContent.nextSibling == beforeNode &&
                 container == currentContent.parentNode))
            {
                /* unless we're already where we are supposed to be. */
                return;
            }   
            
            if ("onHide" in view)
                view.onHide();

            if ("onViewRemoved" in currentContent.ownerWindow)
                currentContent.ownerWindow.onViewRemoved(view);

            if (currentContent != content)
            {
                //dd ("returning content to home");
                moveContent (currentContent, null);
            }
        }        
        
        moveContent (content, container, beforeNode);
        view.currentContent = content;
        if (height)
            content.setAttribute ("height", height);
        if (width)
            content.setAttribute ("width", width);
        
        if ("onShow" in view)
            view.onShow();

        if ("onViewAdded" in window)
            window.onViewAdded(view);
    };

    var view = this.views[viewId];
    var currentContent = ("currentContent" in view ?
                          view.currentContent : null);
    if (currentContent && currentContent.ownerWindow == this.mainWindow)
        view.previousLocation = this.computeLocation(currentContent);

    this.ensureLocation (parsedLocation, onLocationFound);
}

ViewManager.prototype.groutContainer =
function vmgr_groutcontainer (container)
{
    if (!container.parentNode)
        return;
    
    var type = container.getAttribute ("type");
    
    if (!ASSERT(type in this.groutFunctions, "unknown container type ``" +
                type + "''"))
    {
        return;
    }

    if (!container.hasAttribute("grout-in-progress"))
    {
        container.setAttribute("grout-in-progress", "true");

        this.groutFunctions[type](this, container);

        var location = this.computeLocation(container);
        if (location in this.dirtyContainers)
            delete this.dirtyContainers[location];

        container.removeAttribute("grout-in-progress");
    }
}

ViewManager.prototype.groutFunctions = new Object();

ViewManager.prototype.groutFunctions["tab"] =
function vmgr_grouttab (viewManager, container)
{
    function onTabClick(event)
    {
        var target = event.target;
        while (target && target.localName != "little-tab")
            target = target.parentNode;
            
        var index = target.getAttribute("index");
        container.deck.selectedIndex = index;
        for (var i = 0; i < container.tabs.childNodes.length; ++i)
            container.tabs.childNodes[i].removeAttribute("selected");

        container.tabs.childNodes[index].setAttribute("selected", "true");
    };
    
    function closeWindow (window)
    {
        window.close();
    };
    
    if (!ASSERT(container, "null container"))
        return;

    ASSERT(container.localName == "viewcontainer",
           "Attempt to grout something that is not a view container");

    var content = container.firstChild;
    var document = container.ownerDocument;
    container.viewCount = 0;

    var lastSelected = container.deck.selectedIndex;

    while (content)
    {
        var nextContent = content.nextSibling;

        if (content.hasAttribute("grout"))
        {
            container.removeChild(content);
        }
        else
        {
            var viewId = content.getAttribute("id");

            if (!ASSERT(content.localName != "viewcontainer",
                        "tab containers cannot contain other containers"))
            {
                viewManager.destroyContainer(content);
                content = nextContent;
                continue;
            }
            
            if (!ASSERT(viewId in viewManager.views, "unknown view id ``" +
                        viewId + "''"))
            {
                content = nextContent;
                continue;
            }
            
            var view = viewManager.views[viewId];
            
            var tab = (container.viewCount >= container.tabs.childNodes.length)
                      ? null : container.tabs.childNodes[container.viewCount];
            var label;
            
            if (!tab || tab.hasAttribute("needinit"))
            {
                if (!tab)
                    tab = document.createElement("little-tab");
                else
                    tab.removeAttribute("needinit");
                
                tab.setAttribute("index", container.viewCount);
                tab.setAttribute("flex", "1");
                container.tabs.appendChild(tab);
                tab.addEventListener("click", onTabClick, false);
            }
            
            tab.setAttribute("label", view.caption);

            ++container.viewCount;
        }

        content = nextContent;
    }

    if (container.parentNode.localName == "viewcontainer")
        viewManager.groutContainer(container.parentNode);

    if (container.viewCount == 0)
    {
        //dd ("tab container is empty, hiding");
        container.setAttribute("collapsed", "true");
        if (viewManager.multiMoveDepth == 0 &&
            container.parentNode.localName == "window" &&
            container.ownerWindow != viewManager.mainWindow &&
            !("isClosing" in container.ownerWindow) &&
            lastViewCount > 0)
        {
            setTimeout (closeWindow, 1, container.ownerWindow);
            //dd ("} no children, closing window");
        }
        return;
    }
    
    //dd ("unhiding tab container");
    container.removeAttribute("collapsed");

    if (container.viewCount == 1 && !viewManager.multiMoveDepth)
    {
        var parentLocation =
            viewManager.computeLocation(view.currentContent.parentNode);
        container.viewCount = 0;
        viewManager.moveView(parentLocation, viewId);
        container.parentNode.removeChild(container);
        return;
    }
    
    while (container.tabs.childNodes.length > container.viewCount)
        container.tabs.removeChild(container.tabs.lastChild);

    if (lastSelected >= container.viewCount)
        lastSelected = container.viewCount - 1;

    if (typeof lastSelected == "number" && lastSelected > 0)
        container.deck.selectedIndex = lastSelected;
    else
        container.tabs.firstChild.setAttribute("selected", "true");
    
}

ViewManager.prototype.groutFunctions["horizontal"] =
ViewManager.prototype.groutFunctions["vertical"] =
function vmgr_groutbox (viewManager, container)
{
    function closeWindow (window)
    {
        window.close();
    };
    
    function isContainerEmpty (container)
    {
        var rv = container.localName == "viewcontainer" && 
            "viewCount" in container && container.viewCount == 0;
        //dd ("isContainerEmpty: " + container.getAttribute("id") + ": " + rv);
        return rv;
    };
    
    if (!ASSERT(container, "null container"))
        return;

    if (!ASSERT(container.parentNode, "container has no parent ``" +
                container.getAttribute("id") + "''"))
    {
        return;
    }
    
    ASSERT(container.localName == "viewcontainer",
           "Attempt to grout something that is not a view container");

    //dd ("OFF grouting: " + container.getAttribute("id") +" {");

    var lastViewCount = ("viewCount" in container) ? container.viewCount : 0;
    container.viewCount = 0;
    var doc = container.ownerDocument;
    var content = container.firstChild;
    var orient = container.getAttribute("type");
    
    var previousContent;
    var nextContent;
    
    while (content)
    {
        previousContent = content.previousSibling;
        nextContent = content.nextSibling;

        while (nextContent && nextContent.localName == "viewcontainer" &&
               isContainerEmpty(nextContent))
        {
            //skip over empty containers
            nextContent = nextContent.nextSibling;
        }

        while (previousContent &&
               previousContent.localName == "viewcontainer" &&
               isContainerEmpty(previousContent))
        {
            //skip over empty containers
            previousContent = previousContent.previousSibling;
        }

        if (content.hasAttribute("grout"))
        {
            if (!previousContent || !nextContent ||
                previousContent.hasAttribute("grout") ||
                nextContent.hasAttribute("grout") ||
                isContainerEmpty(previousContent))
            {
                container.removeChild(content);
            }
            else
            {
                content.setAttribute("orient", orient);
            }
        }
        else
        {
            if (content.localName == "floatingview")
                ++container.viewCount;
            else if (content.localName == "viewcontainer")
            {
                if ("viewCount" in content)
                    container.viewCount += content.viewCount;
                //else
                    //dd ("no viewCount in " + content.getAttribute("id"));
            }

            if (nextContent && !nextContent.hasAttribute("grout") &&
                !isContainerEmpty(nextContent) && !isContainerEmpty(content))
            {
                var collapse;
                if (content.hasAttribute("splitter-collapse"))
                    collapse = content.getAttribute("splitter-collapse");
                else
                    collapse = "after";
                var split = doc.createElement("splitter");
                split.setAttribute("grout", "true");
                split.setAttribute("collapse", collapse);
                split.appendChild(doc.createElement("grippy"));
                container.insertBefore(split, nextContent);
            }
        }
        content = nextContent;
    }

    if (previousContent && previousContent.hasAttribute("grout") &&
        isContainerEmpty(previousContent.nextSibling))
    {
        container.removeChild(previousContent);
    }
            

    if (isContainerEmpty(container))
    {
        //dd ("container is empty, hiding");
        container.setAttribute("collapsed", "true");
        if (viewManager.multiMoveDepth == 0 &&
            container.parentNode.localName == "window" &&
            container.ownerWindow != viewManager.mainWindow &&
            !("isClosing" in container.ownerWindow) &&
            lastViewCount > 0)
        {
            setTimeout (closeWindow, 1, container.ownerWindow);
            //dd ("} no children, closing window");
            return;
        }
    }
    else
    {
        //dd ("unhiding");
        container.removeAttribute("collapsed");
    }

    if (container.parentNode.localName == "viewcontainer")
        viewManager.groutContainer(container.parentNode);

    //dd ("} " + container.getAttribute("id") +
    //    " view count: " + container.viewCount);

}

ViewManager.prototype.findFloatingView =
function vmgr_findview (element)
{
    while (element && element.localName != "floatingview")
    {
        if (element.localName == "little-tab")
        {
            // this is probably a tab from a tab view container
            var container = element.parentNode.parentNode.parentNode;
            if (container.localName == "viewcontainer")
                return container.childNodes[element.getAttribute("index")];
        }
        
        element = element.parentNode;
    }
    
    return element;
}

ViewManager.prototype.findViewTab =
function vmgr_findtab (viewContent)
{
    var container = viewContent.parentNode;
    if (!ASSERT (container.localName == "viewcontainer", "not part of a view"))
        return null;
    
    if (container.getAttribute("type") != "tab")
        return null;
    
    var len = container.childNodes.length;
    for (var i = 0; i < len; ++i)
    {
        if (container.childNodes[i] == viewContent)
            break;
    }
    
    if (!ASSERT(i < len, "view not in its container?"))
        return null;
    
    return container.tabs.childNodes[i];
}

ViewManager.prototype.getDropDirection =
function vmgr_getdropdir (event, floatingView)
{
    var eventX = event.screenX - floatingView.boxObject.screenX;
    var eventY = event.screenY - floatingView.boxObject.screenY;
    var viewW = floatingView.boxObject.width;
    var viewH = floatingView.boxObject.height;

    if ((event.target.localName == "image" &&
         event.target.getAttribute("class") == "view-title-pop"))
    {
        // dropping on the pop icon
        return "new-tab";
    }
    
    if (eventY > viewH)
    {
        // out of bounds we must be in the tab strip of a tab view
        var tab = this.findViewTab(floatingView);
        if (!ASSERT(tab, "view has no tab"))
            return "new-tab";
        
        if (event.screenX > tab.boxObject.screenX + (tab.boxObject.width / 2))
            return "next-tab";
        
        return "prev-tab";
    }
    
    var rv;
    var dir;

    var m = viewH / viewW;
    if (eventY < m * eventX)
        dir = -1;
    else
        dir = 1;
    
    m = -m;
    
    if (eventY < m * eventX + viewH)
    {
        if (dir < 0)
            rv = "up";
        else
            rv = "left";
    }
    else
    {
        if (dir < 0)
            rv = "right";
        else
            rv = "down";
    }

    return rv;
}

ViewManager.prototype.onDragStart =
function vmgr_dragstart (event, transferData, action)
{
    var floatingView = this.findFloatingView (event.originalTarget);
    if (!floatingView)
        return false;
    
    var viewId = floatingView.getAttribute("id");
    if (!ASSERT(viewId in this.views, "unknown view " + viewId))
        return false;
    
    var view = this.views[viewId];
    
    if (!ASSERT("currentContent" in view, "view is not visible " + viewId))
        return false;
    
    var locationURL = this.computeLocation (view.currentContent);
    transferData.data = new TransferData();
    transferData.data.addDataForFlavour("text/x-vloc", locationURL);

    return true;
}

ViewManager.prototype.getSupportedFlavours =
function vmgr_nicename ()
{
    var flavours = new FlavourSet();
    flavours.appendFlavour("text/x-vloc");
    return flavours;
}

ViewManager.prototype.onViewDragOver =
function vmgr_dragover (event, flavor, session)
{
    session.canDrop = false;
    
    if (flavor.contentType != "text/x-vloc")
        return false;

    var target = this.findFloatingView(event.originalTarget);
    if (!target)
        return false;

    /* COVER YOUR EYES!! */
    var data = nsTransferable.get (this.getSupportedFlavours(),
                                   nsDragAndDrop.getDragData, true);
    data = data.first.first;
    
    var parsedLocation = this.parseLocation(data.data);
    if (target.getAttribute("id") == parsedLocation.id)
        return false;
    
    var direction = this.getDropDirection(event, target);

    if (target.parentNode.getAttribute("type") == "tab" &&
        direction.indexOf("tab") == -1)
    {
        // for tab views, up, down, left, and right are relative to the
        // tab container, *not* the view itself.
        target.parentNode.setAttribute ("dragover", direction);
    }
    else
    {
        target.setAttribute ("dragover", direction);
        target.proxyIcon.setAttribute ("dragover", direction);

        var tab = this.findViewTab(target);
        if (tab)
            tab.setAttribute("dragover", direction);
    }
    
    session.canDrop = true;
    return true;
}

ViewManager.prototype.onViewDragExit =
function vmgr_dragexit (event, session)
{
    var target = this.findFloatingView(event.originalTarget);
    if (target)
    {
        if (target.parentNode.getAttribute("type") == "tab")
        {
            // for tab views, up, down, left, and right are relative to the
            // tab container, *not* the view itself.
            target.parentNode.removeAttribute ("dragover");
        }
        
        target.removeAttribute ("dragover");
        target.proxyIcon.removeAttribute ("dragover");
        var tab = this.findViewTab(target);
        if (tab)
            tab.removeAttribute("dragover");
    }
}

ViewManager.prototype.onViewDrop =
function vmgr_ondrop (event, transferData, session)
{
    var viewManager = this;
    
    function callProcessDrop ()
    {
        viewManager.processDrop (sourceView, targetView, direction);
    };
    
    var floatingView = this.findFloatingView (event.originalTarget);
    var viewId = floatingView.getAttribute ("id");
    var targetView = this.views[viewId];
    if (!ASSERT("currentContent" in targetView,
                "view is not visible " + viewId))
    {
        return false;
    }

    var parsedSource = this.parseLocation(transferData.data);
    if (!ASSERT(parsedSource, "unable to parse " + transferData.data))
        return false;
    
    if (parsedSource.id == viewId)
    {
        //dd ("wanker");
        return false;
    }

    var sourceView = this.views[parsedSource.id];
    
    var direction = this.getDropDirection (event, floatingView);

    /* performing the actual drop action may involve moving DOM nodes
     * that are involved in the actual drag-drop operation.  Mozilla
     * doesn't like it when you do that before drag-drop is done, so we
     * call ourselves back on a timer, to allow the drag-drop wind down before
     * modifying the DOM.
     */
    setTimeout (callProcessDrop, 1);
    return true;
}

ViewManager.prototype.processDrop =
function vmgr_dewdropinn (sourceView, targetView, direction)
{    
    var viewManager = this;

    function reparent (child, type)
    {
        if (!ASSERT(child.parentNode.localName == "viewcontainer",
                    "can't reparent child with unknown parent"))
        {
            return null;
        }
        
        var childLocation = viewManager.computeLocation(child);
        var id = "vm-container-" + ++viewManager.containerSequence;
        while (child.ownerDocument.getElementById(id))
            id = "vm-container-" + ++viewManager.containerSequence;
        var container = viewManager.createContainer (childLocation, id, type);
        var newLocation = { windowId: parsedTarget.windowId, containerId: id };

        if (child.localName == "floatingview")
        {
            viewManager.moveView (newLocation, childLocation.id);
        }
        else
        {
            viewManager.moveContainer (newLocation, child);
        }    
            
        return container;
    };

    try {
        
    var destContainer;
    var destBefore;
    var currentContent   = targetView.currentContent;
    var parsedTarget     = this.computeLocation (currentContent);
    var currentContainer = currentContent.parentNode;
    var parentType       = currentContainer.getAttribute("type");

    this.startMultiMove();

    if (direction.search(/^(new|next|prev)-tab$/) == 0)
    {
        if (parentType != "tab")
            destContainer = reparent(currentContent, "tab");
        else
            destContainer = currentContainer;

        if (direction == "new-tab")
            destBefore = null;
        else if (direction == "prev-tab")
            destBefore = currentContent;
        else if (direction == "next-tab")
            destBefore = currentContent.nextSibling;
    }
    else
    {
        if (parentType == "tab")
        {
            // tab containers can't accept up, down, left, right drops
            // directly.  instead we defer to the tab container's container.
            currentContent = currentContainer;
            currentContainer = currentContainer.parentNode;
            if (!ASSERT(currentContainer.localName == "viewcontainer"))
            {
                this.endMultiMove();
                return;
            }
            parentType = currentContainer.getAttribute("type");
        }    

        if (parentType == "vertical")
        {
            switch (direction)
            {
                case "up":
                    destContainer = currentContainer;
                    destBefore = currentContent;
                    break;
                    
                case "down":
                    destContainer = currentContainer;
                    destBefore = currentContent.nextSibling;
                    if (destBefore && destBefore.hasAttribute("grout"))
                        destBefore = destBefore.nextSibling;
                    break;
                    
                case "left":
                    destContainer = reparent (currentContent, "horizontal");
                    destBefore = currentContent;
                    break;
                    
                case "right":
                    destContainer = reparent (currentContent, "horizontal");
                    break;
            }
        }
        else if (parentType == "horizontal")
        {
            switch (direction)
            {
                case "up":
                    destContainer = reparent (currentContent, "vertical");
                    destBefore = currentContent;
                    break;
                    
                case "down":
                    destContainer = reparent (currentContent, "vertical");
                    break;
                    
                case "left":
                    destContainer = currentContainer;
                    destBefore = currentContent;
                    break;
                    
                case "right":
                    destContainer = currentContainer;
                    destBefore = currentContent.nextSibling;
                    if (destBefore && destBefore.hasAttribute("grout"))
                        destBefore = destBefore.nextSibling;
                    break;
            }
        }
    }
    
    var dest = new Object();
    dest.windowId = parsedTarget.windowId;
    dest.containerId = destContainer.getAttribute ("id");
    dest.before = destBefore ? destBefore.getAttribute("id") : null;
    this.moveView(dest, sourceView.viewId);
    this.endMultiMove();
    
    }
    catch (ex)
    {
        dd("caught exception: " + ex);
        dd(formatException(ex));
    }
}
