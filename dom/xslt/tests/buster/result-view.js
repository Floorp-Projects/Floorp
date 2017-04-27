/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
const SYSTEMPRINCIPAL = Services.scriptSecurityManager.getSystemPrincipal();

function onNewResultView(event)
{
    dump("onNewResultView\n");
    const db = runItem.prototype.kDatabase;
    const kXalan = runItem.prototype.kXalan;
    var index = view.boxObject.view.selection.currentIndex;
    var res = view.builder.getResourceAtIndex(index);
    var name = db.GetTarget(res, krTypeName, true);
    if (!name) {
        return false;
    }
    var cat = db.GetTarget(res, krTypeCat, true);
    var path = db.GetTarget(res, krTypePath, true);
    cat = cat.QueryInterface(nsIRDFLiteral);
    name = name.QueryInterface(nsIRDFLiteral);
    path = path.QueryInterface(nsIRDFLiteral);
    xalan_fl  = kXalan.resolve(cat.Value+"/"+path.Value);
    xalan_ref  = kXalan.resolve(cat.Value+"-gold/"+path.Value);
    var currentResultItem = new Object();
    currentResultItem.testpath = xalan_fl;
    currentResultItem.refpath = xalan_ref;
    var currentRunItem = itemCache.getItem(res);
    // XXX todo, keep a list of these windows, so that we can close them.
    resultWin = window.openDialog('result-view.xul','_blank',
                                   'chrome,resizable,dialog=no',
                                   currentResultItem, currentRunItem);
    return true;
}

var refInspector;
var resInspector;

function onResultViewLoad(event)
{
    dump("onResultViewLoad\n");
    aResultItem = window.arguments[0];
    aRunItem = window.arguments[1];
    var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
    document.getElementById('src').webNavigation.loadURI('view-source:'+
        aResultItem.testpath+'.xml', loadFlags, null, null, null,
        SYSTEMPRINCIPAL);
    document.getElementById('style').webNavigation.loadURI('view-source:'+
        aResultItem.testpath+'.xsl', loadFlags, null, null, null,
        SYSTEMPRINCIPAL);

    if (aRunItem && aRunItem.mRefDoc && aRunItem.mResDoc) {
        document.getElementById("refSourceBox").setAttribute("class", "hidden");
        refInspector = new ObjectApp();
        refInspector.initialize("refInsp", aRunItem.mRefDoc);
        resInspector = new ObjectApp();
        resInspector.initialize("resInsp", aRunItem.mResDoc);
    }
    else {
        document.getElementById("inspectorBox").setAttribute("class", "hidden");
        document.getElementById('ref').webNavigation.loadURI('view-source:'+
            aResultItem.refpath+'.out', loadFlags, null, null, null,
            SYSTEMPRINCIPAL);
    }
    return true;
}

function onResultViewUnload(event)
{
    dump("onResultUnload\n");
}

function ObjectApp()
{
}

ObjectApp.prototype = 
{
    mDoc: null,
    mPanelSet: null,

    initialize: function(aId, aDoc)
    {
        this.mDoc = aDoc;
        this.mPanelSet = document.getElementById(aId).contentDocument.getElementById("bxPanelSet");
        this.mPanelSet.addObserver("panelsetready", this);
        this.mPanelSet.initialize();
    },

    doViewerCommand: function(aCommand)
    {
        this.mPanelSet.execCommand(aCommand);
    },

    getViewer: function(aUID)
    {
        return this.mPanelSet.registry.getViewerByUID(aUID);
    },

    onEvent: function(aEvent)
    {
        switch (aEvent.type) {
            case "panelsetready":
            {
                this.mPanelSet.getPanel(0).subject = this.mDoc;
            }
        }
    }
};
