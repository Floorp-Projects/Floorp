/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is TransforMiiX XSLT Processor.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
 *  Peter Van der Beken <peterv@netscape.com>
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

var view = 
{
    onRun : function()
    {
        runQueue.mArray = new Array();
        var sels = this.boxObject.selection,a=new Object(),b=new Object(),k;
        var rowResource, name, path;
        for (k=0;k<sels.getRangeCount();k++){
            sels.getRangeAt(k,a,b);
            for (var l=a.value;l<=b.value;l++) {
                rowResource = this.builder.getResourceAtIndex(l);
                itemCache.getItem(rowResource);
            }
        }
        runQueue.run();
    },
    displayTest : function()
    {
        var current = this.boxObject.selection.currentIndex;
        var rowResource = this.builder.getResourceAtIndex(current);
        var item = itemCache.getItem(rowResource);
    },
    browseForRDF : function()
    {
        var fp = doCreateRDFFP('Xalan Description File',
                               nsIFilePicker.modeOpen);
        var res = fp.show();

        if (res == nsIFilePicker.returnOK) {
            var furl = fp.fileURL;
            this.setDataSource(fp.fileURL.spec);
        }
    },
    dump_Good : function()
    {
        var enumi = this.mResultDS.GetSources(krTypeSucc, kGood, true);
        var k = 0;
        while (enumi.hasMoreElements()) {
            k += 1;
            dump(enumi.getNext().QueryInterface(nsIRDFResource).Value+"\n");
        }
        dump("found "+k+" good tests\n");
    },
    prune_ds : function()
    {
        if (this.mResultDS) {
            this.mResultDS.QueryInterface(nsIRDFPurgeableDataSource).Sweep();
        }
        regressionStats.init()
        itemCache.mArray = new Array();
    },
    setDataSource : function(aSpec)
    {
        var baseSpec;
        if (aSpec) {
            baseSpec = aSpec;
        }
        else {
            baseSpec = document.getElementById("xalan_rdf").value;
        }
        if (this.mXalanDS && this.mXalanDS.URI == baseSpec) {
            this.mXalanDS.QueryInterface(nsIRDFRemoteDataSource);
            this.mXalanDS.Refresh(true);
        }
        else {
            if (this.mXalanDS) {
                this.database.RemoveDataSource(view.mXalanDS);
            }
            this.mXalanDS = kRDFSvc.GetDataSourceBlocking(baseSpec);
            if (!this.mXalanDS) {
                alert("Unable do load DataSource: "+baseSpec);
                return;
            }
            this.database.AddDataSource(this.mXalanDS);
        }
        regressionStats.init();
        if (!this.mResultDS) {
            this.mResultDS = doCreate(kRDFInMemContractID,
                                      nsIRDFDataSource);
            this.database.AddDataSource(view.mResultDS);
            if (!this.mResultDS) {
                alert("Unable to create result InMemDatasource");
                return;
            }
        }

        this.builder.rebuild();
        document.getElementById("xalan_rdf").value = baseSpec;
        runItem.prototype.kXalan.init(runItem.prototype.kXalan.URLTYPE_STANDARD,
                                      0, baseSpec, null, null);
    },
    loadHtml : function(aUrl, aListener)
    {
        const nsIIRequestor = Components.interfaces.nsIInterfaceRequestor;
        const nsIWebProgress = Components.interfaces.nsIWebProgress;
        var req = this.mIframe.webNavigation.QueryInterface(nsIIRequestor);
        var prog = req.getInterface(nsIWebProgress);
        prog.addProgressListener(aListener, nsIWebProgress.NOTIFY_ALL);
        this.mIframe.webNavigation.loadURI(aUrl, 0,null,null,null);
    },
    fillItemContext : function()
    {
        var index = view.boxObject.selection.currentIndex;
        var res = view.builder.getResourceAtIndex(index);
        var purp = view.mXalanDS.GetTarget(res, krTypePurp, true);
        return (purp != null);
    }
}

regressionStats =
{
    observe: function(aSubject, aTopic, aData)
    {
        if (aTopic != 'success') {
            return;
        }
        var arc = (aData == "true") ? krTypeSuccCount : krTypeFailCount;
        this.assertNewCount(aSubject, arc, 1);
    },
    init: function()
    {
        if (this.mRegressionDS) {
            this.mRegressionDS.QueryInterface(nsIRDFPurgeableDataSource).Sweep();
        }
        else {
            this.mRegressionDS = 
                doCreate(kRDFInMemContractID, nsIRDFDataSource);
            view.database.AddDataSource(this.mRegressionDS);
        }
    },
    getParent: function(aDS, aSource)
    {
        // parent chached?
        var parent = this.mRegressionDS.GetTarget(aSource, krTypeParent, true);
        if (!parent) {
            var labels = view.mXalanDS.ArcLabelsIn(aSource);
            while (labels.hasMoreElements()) {
                var arc = labels.getNext().QueryInterface(nsIRDFResource);
                if (arc.Value.match(this.mChildRE)) {
                    parent = view.mXalanDS.GetSource(arc, aSource, true);
                    // cache the parent
                    this.mRegressionDS.Assert(aSource, krTypeParent,
                                              parent, true);
                }
            }
        }
        return parent;
    },
    assertNewCount: function(aSource, aArc, aIncrement)
    {
        var root = kRDFSvc.GetResource('urn:root');
        var count = 0;
        // parent chached?
        var parent = this.getParent(view.XalanDS, aSource);
        while (parent && !parent.EqualsNode(root)) {
            var countRes = view.mResultDS.GetTarget(parent, aArc, true);
            if (countRes) {
                count = countRes.QueryInterface(nsIRDFInt).Value;
            }
            var newCountRes = kRDFSvc.GetIntLiteral(count + aIncrement);
            if (!newCountRes) {
                return;
            }

            if (countRes) {
                view.mResultDS.Change(parent, aArc, countRes, newCountRes);
            }
            else {
                view.mResultDS.Assert(parent, aArc, newCountRes, true);
            }
            parent = this.getParent(view.XalanDS, parent);
        }
    },
    mRegressionDS: 0,
    mChildRE: /http:\/\/www\.w3\.org\/1999\/02\/22-rdf-syntax-ns#_/
}

function rdfObserve(aSubject, aTopic, aData)
{
    if (aTopic == "success") {
        var target = (aData == "true") ? kGood : kBad;
        view.mResultDS.Assert(aSubject, krTypeSucc, target, true);

        regressionStats.observe(aSubject, aTopic, aData);
    }
}

runItem.prototype.kObservers.push(rdfObserve);
