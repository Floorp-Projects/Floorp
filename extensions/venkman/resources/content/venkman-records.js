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

function initRecords()
{
    var cmdary =
        [/* "real" commands */
         ["show-functions",  cmdShowFunctions,                     CMD_CONSOLE],
         ["show-ecmas",      cmdShowECMAs,                         CMD_CONSOLE],
         ["show-constants",  cmdShowConstants,                     CMD_CONSOLE],

         /* aliases */
         ["toggle-functions",     "show-functions toggle",                   0],
         ["toggle-ecmas",         "show-ecma toggle",                        0],
         ["toggle-constants",     "show-constants toggle",                   0]
        ];

    console.commandManager.defineCommands (cmdary);

    var atomsvc = console.atomService;

    WindowRecord.prototype.property         = atomsvc.getAtom("item-window");

    FileContainerRecord.prototype.property  = atomsvc.getAtom("item-files");

    FileRecord.prototype.property           = atomsvc.getAtom("item-file");

    FrameRecord.prototype.property    = atomsvc.getAtom("item-frame");
    FrameRecord.prototype.atomCurrent = atomsvc.getAtom("current-frame-flag");

    ScriptInstanceRecord.prototype.atomUnknown = atomsvc.getAtom("item-unk");
    ScriptInstanceRecord.prototype.atomHTML    = atomsvc.getAtom("item-html");
    ScriptInstanceRecord.prototype.atomJS      = atomsvc.getAtom("item-js");
    ScriptInstanceRecord.prototype.atomXUL     = atomsvc.getAtom("item-xul");
    ScriptInstanceRecord.prototype.atomXML     = atomsvc.getAtom("item-xml");
    ScriptInstanceRecord.prototype.atomDisabled = 
        atomsvc.getAtom("script-disabled");

    ScriptRecord.prototype.atomFunction   = atomsvc.getAtom("file-function");
    ScriptRecord.prototype.atomBreakpoint = atomsvc.getAtom("item-has-bp");
    ScriptRecord.prototype.atomDisabled   = atomsvc.getAtom("script-disabled");

    var prefs =
        [
         ["valueRecord.showFunctions", false],
         ["valueRecord.showECMAProps", false],
         ["valueRecord.showConstants", false],
         ["valueRecord.brokenObjects", "^JavaPackage$"]
        ];

    console.prefManager.addPrefs(prefs);
    
    try
    {
        ValueRecord.prototype.brokenObjects = 
            new RegExp (console.prefs["valueRecord.brokenObjects"]);
    }
    catch (ex)
    {
        display (MSN_ERR_INVALID_PREF,
                 ["valueRecord.brokenObjects",
                  console.prefs["valueRecord.brokenObjects"]], MT_ERROR);
        display (formatException(ex), MT_ERROR);
        ValueRecord.prototype.brokenObjects = /^JavaPackage$/;
    }
                  
    ValueRecord.prototype.atomVoid      = atomsvc.getAtom("item-void");
    ValueRecord.prototype.atomNull      = atomsvc.getAtom("item-null");
    ValueRecord.prototype.atomBool      = atomsvc.getAtom("item-bool");
    ValueRecord.prototype.atomInt       = atomsvc.getAtom("item-int");
    ValueRecord.prototype.atomDouble    = atomsvc.getAtom("item-double");
    ValueRecord.prototype.atomString    = atomsvc.getAtom("item-string");
    ValueRecord.prototype.atomFunction  = atomsvc.getAtom("item-function");
    ValueRecord.prototype.atomObject    = atomsvc.getAtom("item-object");
    ValueRecord.prototype.atomError     = atomsvc.getAtom("item-error");
    ValueRecord.prototype.atomException = atomsvc.getAtom("item-exception");
    ValueRecord.prototype.atomHinted    = atomsvc.getAtom("item-hinted");
}

/*******************************************************************************
 * Breakpoint Record.
 * One prototype for all breakpoint types, works only in the Breakpoints View.
 *******************************************************************************/

function BPRecord (breakWrapper)
{
    this.setColumnPropertyName ("col-0", "name");
    this.setColumnPropertyName ("col-1", "line");

    this.breakWrapper = breakWrapper;
    
    if ("pc" in breakWrapper)
    {
        this.type = "instance";
        this.name = breakWrapper.scriptWrapper.functionName;
        this.line = getMsg(MSN_FMT_PC, String(breakWrapper.pc));
    }
    else if (breakWrapper instanceof FutureBreakpoint)
    {
        this.type = "future";
        var ary = breakWrapper.url.match(/\/([^\/?]+)(\?|$)/);
        if (ary)
            this.name = ary[1];
        else
            this.name = breakWrapper.url;

        this.line = breakWrapper.lineNumber;
    }    
}

BPRecord.prototype = new XULTreeViewRecord(console.views.breaks.share);

/*******************************************************************************
 * Stack Frame Record.
 * Represents a jsdIStackFrame, for use in the Stack View only.
 *******************************************************************************/

function FrameRecord (jsdFrame)
{
    if (!(jsdFrame instanceof jsdIStackFrame))
        throw new BadMojo (ERR_INVALID_PARAM, "value");

    this.setColumnPropertyName ("col-0", "functionName");
    this.setColumnPropertyName ("col-1", "location");

    this.functionName = jsdFrame.functionName;
    if (!jsdFrame.isNative)
    {
        this.scriptWrapper = getScriptWrapper(jsdFrame.script);
        this.location = getMsg(MSN_FMT_FRAME_LOCATION,
                               [getFileFromPath(jsdFrame.script.fileName),
                                jsdFrame.line, jsdFrame.pc]);
        this.functionName = this.scriptWrapper.functionName;
    }
    else
    {
        this.scriptWrapper = null;
        this.location = MSG_URL_NATIVE;
    }
    
    this.jsdFrame = jsdFrame;
}

FrameRecord.prototype = new XULTreeViewRecord (console.views.stack.share);

/*******************************************************************************
 * Window Record.
 * Represents a DOM window with files and child windows.  For use in the
 * Windows View.
 *******************************************************************************/

function WindowRecord (win, baseURL)
{
    function none() { return ""; };
    this.setColumnPropertyName ("col-0", "displayName");

    this.window = win;
    this.url = win.location.href;
    if (this.url.search(/^\w+:/) == -1)
    {
        if (this.url[0] == "/")
        {
            this.url = win.location.protocol + "//" + win.location.host + 
                this.url;
        }
        else
        {
            this.url = baseURL + this.url;
        }
        this.baseURL = baseURL;
    }
    else
    {
        this.baseURL = getPathFromURL(this.url);
        if (this.baseURL.indexOf("file:///") == 0)
            this.baseURL = "file:/" + this.baseURL.substr(8)
    }
    
    this.reserveChildren(true);
    this.shortName = getFileFromPath (this.url);
    if (console.prefs["enableChromeFilter"] && 
        (this.shortName == "navigator.xul" || this.shortName == "browser.xul"))
    {
        this.displayName = MSG_NAVIGATOR_XUL;
    }
    else
    {
        this.filesRecord = new FileContainerRecord(this);
        this.displayName = this.shortName;
    }
    
}

WindowRecord.prototype = new XULTreeViewRecord(console.views.windows.share);

WindowRecord.prototype.onPreOpen =
function wr_preopen()
{   
    this.childData = new Array();

    if ("filesRecord" in this)
        this.appendChild(this.filesRecord);    

    var framesLength = this.window.frames.length;
    for (var i = 0; i < framesLength; ++i)
    {
        this.appendChild(new WindowRecord(this.window.frames[i].window,
                                          this.baseURL));
    }
}
    
/*******************************************************************************
 * "File Container" Record.
 * List of script tags found in the parent record's |window.document| property.
 * For use in the Windows View, as a child of a WindowRecord.
 *******************************************************************************/

function FileContainerRecord (windowRecord)
{
    function files() { return MSG_FILES_REC; }
    function none() { return ""; }
    this.setColumnPropertyName ("col-0", files);
    this.windowRecord = windowRecord;
    this.reserveChildren(true);
}

FileContainerRecord.prototype =
    new XULTreeViewRecord(console.views.windows.share);

FileContainerRecord.prototype.onPreOpen =
function fcr_getkids ()
{
    if (!("parentRecord" in this))
        return;
    
    this.childData = new Array();
    var doc = this.windowRecord.window.document;
    var loc = this.windowRecord.window.location;
    var nodeList = doc.getElementsByTagName("script");

    for (var i = 0; i < nodeList.length; ++i)
    {
        var url = nodeList.item(i).getAttribute("src");
        if (url)
        {
            if (url.search(/^\w+:/) == -1)
            {
                if (url[0] == "/")
                    url = loc.protocol + "//" + loc.host + url;
                else
                    url = this.windowRecord.baseURL + url;
            }
            else
            {
                this.baseURL = getPathFromURL(url);
            }

            this.appendChild(new FileRecord(url));
        }
    }
}

/*******************************************************************************
 * File Record
 * Represents a URL, for use in the Windows View only.
 *******************************************************************************/

function FileRecord (url)
{
    function none() { return ""; }
    this.setColumnPropertyName ("col-0", "shortName");
    this.url = url;
    this.shortName = getFileFromPath(url);
}

FileRecord.prototype = new XULTreeViewRecord(console.views.windows.share);

/*******************************************************************************
 * Script Instance Record.
 * Represents a ScriptInstance, for use in the Scripts View only.
 *******************************************************************************/

function ScriptInstanceRecord(scriptInstance)
{
    if (!ASSERT(scriptInstance.isSealed,
                "Attempt to create ScriptInstanceRecord for unsealed instance"))
    {
        return null;
    }
    
    this.setColumnPropertyName ("col-0", "displayName");
    this.setColumnPropertyValue ("col-1", "");
    this.setColumnPropertyValue ("col-2", "");
    this.reserveChildren(true);
    this.url = scriptInstance.url;
    var sv = console.views.scripts;
    this.fileType = this.atomUnknown;
    this.shortName = this.url;
    this.group = 4;
    this.scriptInstance = scriptInstance;
    this.lastScriptCount = 0;
    this.sequence = scriptInstance.sequence;
    
    this.shortName = getFileFromPath(this.url);
    var ary = this.shortName.match (/\.(js|html|xul|xml)$/i);
    if (ary)
    {
        switch (ary[1].toLowerCase())
        {
            case "js":
                this.fileType = this.atomJS;
                this.group = 0;
                break;
            
            case "html":
                this.group = 1;
                this.fileType = this.atomHTML;
                break;
            
            case "xul":
                this.group = 2;
                this.fileType = this.atomXUL;
                break;
            
            case "xml":
                this.group = 3;
                this.fileType = this.atomXML;
                break;
        }
    }
    
    this.displayName = this.shortName;
    this.sortName = this.shortName.toLowerCase();
    return this;
}

ScriptInstanceRecord.prototype =
    new XULTreeViewRecord(console.views.scripts.share);

ScriptInstanceRecord.prototype.onDragStart =
function scr_dragstart (e, transferData, dragAction)
{        
    transferData.data = new TransferData();
    transferData.data.addDataForFlavour("text/x-venkman-file", this.fileName);
    transferData.data.addDataForFlavour("text/x-moz-url", this.fileName);
    transferData.data.addDataForFlavour("text/unicode", this.fileName);
    transferData.data.addDataForFlavour("text/html",
                                        "<a href='" + this.fileName +
                                        "'>" + this.fileName + "</a>");
    return true;
}    

ScriptInstanceRecord.prototype.getProperties =
function scr_getprops (properties)
{
    properties.AppendElement(this.fileType);
    
    if (this.scriptInstance.disabledScripts > 0)
        properties.AppendElement (this.atomDisabled);
}

ScriptInstanceRecord.prototype.super_resort = XTRootRecord.prototype.resort;

ScriptInstanceRecord.prototype.resort =
function scr_resort ()
{
    console._groupFiles = console.prefs["scriptsView.groupFiles"];
    this.super_resort();
    delete console._groupFiles;
}

ScriptInstanceRecord.prototype.sortCompare =
function scr_compare (a, b)
{
    if (0)
    {
        if (a.group < b.group)
            return -1;
    
        if (a.group > b.group)
            return 1;
    }
    
    if (a.sortName < b.sortName)
        return -1;

    if (a.sortName > b.sortName)
        return 1;

    if (a.sequence < b.sequence)
        return -1;

    if (a.sequence > b.sequence)
        return 1;
    
    dd ("ack, all equal?");
    return 0;
}

ScriptInstanceRecord.prototype.onPreOpen =
function scr_preopen ()
{
    if (!this.scriptInstance.sourceText.isLoaded)
        this.scriptInstance.sourceText.loadSource();
    
    if (this.lastScriptCount != this.scriptInstance.scriptCount)
    {
        var sr;

        console.views.scripts.freeze();
        this.childData = new Array();
        var scriptWrapper = this.scriptInstance.topLevel;
        if (scriptWrapper)
        {
            sr = new ScriptRecord(scriptWrapper);
            scriptWrapper.scriptRecord = sr;
            this.appendChild(sr);
        }

        var functions = this.scriptInstance.functions;
        for (var f in functions)
        {
            if (functions[f].jsdScript.isValid)
            {
                sr = new ScriptRecord(functions[f]);
                functions[f].scriptRecord = sr;
                this.appendChild(sr);
            }
        }
        console.views.scripts.thaw();
        this.lastScriptCount = this.scriptInstance.scriptCount;
    }
}

/*******************************************************************************
 * Script Record.
 * Represents a ScriptWrapper, for use in the Scripts View only.
 *******************************************************************************/

function ScriptRecord(scriptWrapper) 
{
    this.setColumnPropertyName ("col-0", "functionName");
    this.setColumnPropertyName ("col-1", "baseLineNumber");
    this.setColumnPropertyName ("col-2", "lineExtent");

    this.functionName = scriptWrapper.functionName
    this.baseLineNumber = scriptWrapper.jsdScript.baseLineNumber;
    this.lineExtent = scriptWrapper.jsdScript.lineExtent;
    this.scriptWrapper = scriptWrapper;

    this.jsdurl = "jsd:sourcetext?url=" +
        encodeURIComponent(this.scriptWrapper.jsdScript.fileName) + 
        "&base=" + this.baseLineNumber + "&extent=" + this.lineExtent +
        "&name=" + this.functionName;
}

ScriptRecord.prototype = new XULTreeViewRecord(console.views.scripts.share);

ScriptRecord.prototype.onDragStart =
function sr_dragstart (e, transferData, dragAction)
{        
    var fileName = this.script.fileName;
    transferData.data = new TransferData();
    transferData.data.addDataForFlavour("text/x-jsd-url", this.jsdurl);
    transferData.data.addDataForFlavour("text/x-moz-url", fileName);
    transferData.data.addDataForFlavour("text/unicode", fileName);
    transferData.data.addDataForFlavour("text/html",
                                        "<a href='" + fileName +
                                        "'>" + fileName + "</a>");
    return true;
}

ScriptRecord.prototype.getProperties =
function sr_getprops (properties)
{
    properties.AppendElement (this.atomFunction);

    if (this.scriptWrapper.breakpointCount)
        properties.AppendElement (this.atomBreakpoint);

    if (this.scriptWrapper.jsdScript.isValid &&
        this.scriptWrapper.jsdScript.flags & SCRIPT_NODEBUG)
    {
        properties.AppendElement (this.atomDisabled);
    }
}

/*******************************************************************************
 * Value Record.
 * Use this to show a jsdIValue in any tree.
 *******************************************************************************/

function cmdShowFunctions (e)
{
    if (e.toggle != null)
    {
        e.toggle = getToggle(e.toggle, ValueRecord.prototype.showFunctions);
        ValueRecord.prototype.showFunctions = e.toggle;
        console.prefs["valueRecord.showFunctions"] = e.toggle;
    }

    if ("isInteractive" in e && e.isInteractive)
        dispatch("pref valueRecord.showFunctions", { isInteractive: true });
}

function cmdShowECMAs (e)
{
    if (e.toggle != null)
    {
        e.toggle = getToggle(e.toggle, ValueRecord.prototype.showECMAProps);
        ValueRecord.prototype.showECMAProps = e.toggle;
        console.prefs["valueRecord.showECMAProps"] = e.toggle;
    }

    if ("isInteractive" in e && e.isInteractive)
        dispatch("pref valueRecord.showECMAProps", { isInteractive: true });
}

function cmdShowConstants (e)
{
    if (e.toggle != null)
    {
        e.toggle = getToggle(e.toggle, ValueRecord.prototype.showConstants);
        ValueRecord.prototype.showConstants = e.toggle;
        console.prefs["valueRecord.showConstants"] = e.toggle;
    }

    if ("isInteractive" in e && e.isInteractive)
        dispatch("pref valueRecord.showConstants", { isInteractive: true });
}

function ValueRecord (value, name, flags)
{
    if (!(value instanceof jsdIValue))
        throw new BadMojo (ERR_INVALID_PARAM, "value", String(value));

    this.setColumnPropertyName ("col-0", "displayName");
    this.setColumnPropertyName ("col-1", "displayType");
    this.setColumnPropertyName ("col-2", "displayValue");
    this.setColumnPropertyName ("col-3", "displayFlags");    
    this.displayName = name;
    this.displayFlags = formatFlags(flags);
    this.name = name;
    this.flags = flags;
    this.value = value;
    this.jsType = null;
    this.onPreRefresh = false;
    this.refresh();
    delete this.onPreRefresh;
}

ValueRecord.prototype = new XULTreeViewRecord (null);

ValueRecord.prototype.__defineGetter__("_share", vr_getshare);
function vr_getshare()
{
    if ("__share" in this)
        return this.__share;
     
    if ("parentRecord" in this)
        return this.__share = this.parentRecord._share;
 
    ASSERT (0, "ValueRecord cannot be the root of a visible tree.");
    return null;
}

ValueRecord.prototype.showFunctions = false;
ValueRecord.prototype.showECMAProps = false;
ValueRecord.prototype.showConstants = false;

ValueRecord.prototype.getProperties =
function vr_getprops (properties)
{
    if ("valueIsException" in this || this.flags & PROP_EXCEPTION)
        properties.AppendElement (this.atomException);

    if (this.flags & PROP_ERROR)
        properties.AppendElement (this.atomError);

    if (this.flags & PROP_HINTED)
        properties.AppendElement (this.atomHinted);

    properties.AppendElement (this.property);
}

ValueRecord.prototype.onPreRefresh =
function vr_prerefresh ()
{
    if (!ASSERT("parentRecord" in this, "onPreRefresh with no parent"))
        return;
    
    if ("isECMAProto" in this)
    {
        if (this.parentRecord.value.jsPrototype)
            this.value = this.parentRecord.value.jsPrototype;
        else
            this.value = console.jsds.wrapValue(null);
    }
    else if ("isECMAParent" in this)
    {
        if (this.parentRecord.value.jsParent)
            this.value = this.parentRecord.value.jsParent;
        else
            this.value = console.jsds.wrapValue(null);
    }
    else
    {
        var value = this.parentRecord.value;
        var prop = value.getProperty (this.name);
        if (prop)
        {
            this.flags = prop.flags;
            this.value = prop.value;
        }
        else
        {
            var jsval = value.getWrappedValue();
            this.value = console.jsds.wrapValue(jsval[this.name]);
            this.flags = PROP_ENUMERATE | PROP_HINTED;
        }
    }
}
    
ValueRecord.prototype.refresh =
function vr_refresh ()
{
    if (this.onPreRefresh)
    {
        try
        {
            this.onPreRefresh();
            delete this.valueIsException;
        }
        catch (ex)
        {
            /*
            dd ("caught exception refreshing " + this.displayName);
            if (typeof ex == "object")
                dd (dumpObjectTree(ex));
            else
                dd(ex);
            */
            if (!(ex instanceof jsdIValue))
                ex = console.jsds.wrapValue(ex);
            
            this.value = ex;
            this.valueIsException = true;
        }
    }

    this.jsType = this.value.jsType;
    delete this.alwaysHasChildren;

    var strval;
    
    switch (this.jsType)
    {
        case TYPE_VOID:
            this.displayValue = MSG_TYPE_VOID
            this.displayType  = MSG_TYPE_VOID;
            this.property     = this.atomVoid;
            break;
        case TYPE_NULL:
            this.displayValue = MSG_TYPE_NULL;
            this.displayType  = MSG_TYPE_NULL;
            this.property     = this.atomNull;
            break;
        case TYPE_BOOLEAN:
            this.displayValue = this.value.stringValue;
            this.displayType  = MSG_TYPE_BOOLEAN;
            this.property     = this.atomBool;
            break;
        case TYPE_INT:
            this.displayValue = this.value.intValue;
            this.displayType  = MSG_TYPE_INT;
            this.property     = this.atomInt;
            break;
        case TYPE_DOUBLE:
            this.displayValue = this.value.doubleValue;
            this.displayType  = MSG_TYPE_DOUBLE;
            this.property     = this.atomDouble;
            break;
        case TYPE_STRING:
            strval = this.value.stringValue;
            if (strval.length > console.prefs["maxStringLength"])
                strval = getMsg(MSN_FMT_LONGSTR, strval.length);
            else
                strval = strval.quote();
            this.displayValue = strval;
            this.displayType  = MSG_TYPE_STRING;
            this.property     = this.atomString;
            break;
        case TYPE_FUNCTION:
        case TYPE_OBJECT:
            this.displayType = MSG_TYPE_OBJECT;
            this.property = this.atomObject;

            this.alwaysHasChildren = true;
            this.value.refresh();

            var ctor = this.value.jsClassName;
            strval = null;
            
            switch (ctor)
            {
                case "Function":
                    this.displayType  = MSG_TYPE_FUNCTION;
                    ctor = (this.value.isNative ? MSG_CLASS_NATIVE_FUN :
                            MSG_CLASS_SCRIPT_FUN);
                    this.property = this.atomFunction;
                    break;

                case "Object":
                    if (this.value.jsConstructor)
                        ctor = this.value.jsConstructor.jsFunctionName;
                    break;

                case "XPCWrappedNative_NoHelper":
                    ctor = MSG_CLASS_CONST_XPCOBJ;
                    break;

                case "XPC_WN_ModsAllowed_Proto_JSClass":
                    ctor = MSG_CLASS_XPCOBJ;
                    break;

                case "String":
                    strval = this.value.stringValue;
                    if (strval.length > console.prefs["maxStringLength"])
                        strval = getMsg(MSN_FMT_LONGSTR, strval.length);
                    else
                        strval = strval.quote();
                    break;

                case "Number":
                case "Boolean":
                    strval = this.value.stringValue;
                    break;
            }

            if (strval != null)
            {
                this.displayValue = strval;
            }
            else
            {
                if (0) {
                    /* too slow... */
                    var propCount;
                    if (this.brokenObjects.test(ctor))
                    {
                        /* XXX these objects do Bad Things when you enumerate
                         * over them. */
                        propCount = 0;
                    }
                    else
                    {
                        propCount = this.countProperties();
                    }
                
                    this.displayValue = getMsg (MSN_FMT_OBJECT_VALUE,
                                                [ctor, propCount]);
                }
                else
                {
                    this.displayValue = "{" + ctor + "}";
                }
            }
            
            /* if we have children, refresh them. */
            if ("childData" in this && this.childData.length > 0)
            {
                if (!this.refreshChildren())
                {
                    dd ("refreshChilren failed for " + this.displayName);
                    delete this.childData;
                    this.close();
                }
            }
            break;

        default:
            ASSERT (0, "invalid value");
    }
}

ValueRecord.prototype.countProperties =
function vr_countprops ()
{
    var c = 0;
    var jsval = this.value.getWrappedValue();
    try
    {
        for (var p in jsval)
            ++c;
    }
    catch (ex)
    {
        dd ("caught exception counting properties\n" + ex);
    }
    
    return c;
}

ValueRecord.prototype.listProperties =
function vr_listprops ()
{
    // the ":" prefix for keys in the propMap avoid collisions with "real"
    // pseudo-properties, such as __proto__.  If we were to actually assign
    // to those we would introduce bad side affects.

    //dd ("listProperties {");
    var i;
    var jsval = this.value.getWrappedValue();
    var propMap = new Object();

    /* get the enumerable properties */
    
    for (var p in jsval)
    {
        var value;
        try
        {
            value = console.jsds.wrapValue(jsval[p]);
            if (this.showFunctions || value.jsType != TYPE_FUNCTION)
            {
                propMap[":" + p] = { name: p, value: value,
                                     flags: PROP_ENUMERATE | PROP_HINTED };
            }
            else
            {
                //dd ("not including function " + name);
                propMap[":" + p] = null;
            }
        }
        catch (ex)
        {
            propMap[":" + p] = { name: p, value: console.jsds.wrapValue(ex),
                                 flags: PROP_EXCEPTION };
        }

        //dd ("jsval props: adding " + p + ", " + propMap[":" + p]);

    }
    
    /* get the local properties, may or may not be enumerable */
    var localProps = new Object()
    this.value.getProperties(localProps, {});
    localProps = localProps.value;
    var len = localProps.length;
    for (i = 0; i < len; ++i)
    {
        var prop = localProps[i];
        var name = prop.name.stringValue;
        
        if (!((":" + name) in propMap))
        {
            if ((this.showFunctions || prop.value.jsType != TYPE_FUNCTION) && 
                (this.showConstants || !(prop.flags & PROP_READONLY)))
            {
                //dd ("localProps: adding " + name + ", " + prop);
                propMap[":" + name] = { name: name, value: prop.value,
                                        flags: prop.flags };
            }
            else
            {
                //dd ("not including function " + name);
                propMap[":" + name] = null;
            }
        }
        else
        {
            if (!this.showConstants && (prop.flags & PROP_READONLY))
                propMap[":" + name] = null;
            if (propMap[":" + name])
                propMap[":" + name].flags = prop.flags;
        }
    }
    
    /* sort the property list */
    var nameList = keys(propMap);
    //dd ("nameList is " + nameList);
    
    nameList.sort();
    var propertyList = new Array();
    for (i = 0; i < nameList.length; ++i)
    {
        name = nameList[i];
        if (propMap[name])
            propertyList.push (propMap[name]);
    }
    
    //dd ("} " + propertyList.length + " properties");

    return propertyList;
}

ValueRecord.prototype.refreshChildren =
function vr_refreshkids ()
{
    var leadingProps = 0;
    
    this.propertyList = this.listProperties();

    for (var i = 0; i < this.childData.length; ++i)
    {
        if ("isECMAParent" in this.childData[i] ||
            "isECMAProto" in this.childData[i])
        {
            ++leadingProps;
        }
        else if (this.childData.length - leadingProps != 
                 this.propertyList.length)
        {
            dd ("refreshChildren: property length mismatch");
            return false;
        }
        else if (this.childData[i]._colValues["col-0"] !=
                 this.propertyList[i - leadingProps].name)
        {
            dd ("refreshChildren: " +
                this.childData[i]._colValues["col-0"] + " != " +
                this.propertyList[i - leadingProps].name);
            return false;
        }

        this.childData[i].refresh();
    }

    if (this.childData.length - leadingProps != 
        this.propertyList.length)
    {
        dd ("refreshChildren: property length mismatch");
        return false;
    }
    return true;
}

ValueRecord.prototype.onPreOpen =
function vr_preopen()
{
    try
    {
        if (!ASSERT(this.value.jsType == TYPE_OBJECT || 
                    this.value.jsType == TYPE_FUNCTION,
                    "onPreOpen called for non object?"))
        {
            return;
        }
        
        this.childData = new Array();
        this.propertyList = this.listProperties();
        
        if (this.showECMAProps)
        {
            var rec;
            if (this.value.jsPrototype)
            {
                rec = new ValueRecord(this.value.jsPrototype,
                                      MSG_VAL_PROTO);
                rec.isECMAProto = true;
                this.appendChild (rec);
            }
            
            if (this.value.jsParent)
            {
                rec = new ValueRecord(this.value.jsParent,
                                      MSG_VAL_PARENT);
                rec.isECMAParent = true;
                this.appendChild (rec);
            }
        }
        
        if (!this.childData.length && !this.propertyList.length)
        {
            rec = new XTLabelRecord ("col-0", MSG_VAL_NONE,
                                     ["col-1", "col-2", "col-3"]);
            this.appendChild(rec);
            return;
        }
        
        for (var i = 0; i < this.propertyList.length; ++i)
        {
            var prop = this.propertyList[i];
            this.appendChild(new ValueRecord(prop.value,
                                             prop.name,
                                             prop.flags));
        }
    }
    catch (ex)
    {
        display (getMsg (MSN_ERR_FAILURE, ex), MT_ERROR);
    }
}

ValueRecord.prototype.onPostClose =
function vr_destroy()
{
    this.childData = new Array();
    delete this.propertyList;
}
