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
        var sels = this.boxObject.selection,a=new Object(),b=new Object(),k;
        var rowResource, name, path;
        enablePrivilege('UniversalXPConnect');
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
        enablePrivilege('UniversalXPConnect');
        var rowResource = this.builder.getResourceAtIndex(current);
        var item = itemCache.getItem(rowResource);
        DumpDOM(item.mSourceDoc);
        DumpDOM(item.mStyleDoc);
    },
    browseForRDF : function()
    {
        enablePrivilege('UniversalXPConnect');
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
        enablePrivilege('UniversalXPConnect');
        var enumi = this.database.GetSources(krTypeSucc, kGood, true);
        var k = 0;
        while (enumi.hasMoreElements()) {
            k += 1;
            dump(enumi.getNext().QueryInterface(nsIRDFResource).Value+"\n");
        }
        dump("found "+k+" good tests\n");
    },
    prune_ds : function()
    {
        enablePrivilege('UniversalXPConnect');
        this.unassert(this.database.GetSources(krTypeSucc, kGood, true),
                      kGood);
        this.unassert(this.database.GetSources(krTypeSucc, kBad, true),
                      kBad);
        this.unassert(this.database.GetSources(krTypeSucc, kMixed, true),
                      kMixed);
        runQueue.mArray = new Array();
        itemCache.mArray = new Array();
    },
    unassert : function(aEnum, aResult)
    {
        enablePrivilege('UniversalXPConnect');
        var k = 0, item;
        while (aEnum.hasMoreElements()) {
            k += 1;
            var item = aEnum.getNext();
            try {
                item = item.QueryInterface(nsIRDFResource);
                this.database.Unassert(item, krTypeSucc, aResult, true);
            } catch (e) {
                dump("Can't unassert "+item+"\n");
            }
        }
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
        dump(baseSpec+"\n");
        var currentSources = this.database.GetDataSources();
        while (currentSources.hasMoreElements()) {
            var aSrc = currentSources.getNext().
                QueryInterface(nsIRDFDataSource);
            this.database.RemoveDataSource(aSrc);
        }
        var ds = kRDFSvc.GetDataSource(baseSpec);
        if (!ds) {
            alert("Unable do load DataSource: "+baseSpec);
            return;
        }
        view.memoryDataSource = doCreate(kRDFInMemContractID,
                                         nsIRDFDataSource);
        if (!view.memoryDataSource) {
            alert("Unable to create write-protect InMemDatasource,"+
                  " not adding "+ baseSpec);
        }
        else {
            view.database.AddObserver(new regressionStats());
            view.database.AddDataSource(ds);
            view.database.AddDataSource(view.memoryDataSource);
        }
        view.builder.rebuild();
        document.getElementById("xalan_rdf").value = baseSpec;
        runItem.prototype.kXalan.init(runItem.prototype.kXalan.URLTYPE_STANDARD,
                                      0, baseSpec, null, null);
    },
    fillItemContext : function()
    {
        enablePrivilege('UniversalXPConnect');
        var index = view.boxObject.selection.currentIndex;
        var res = view.builder.getResourceAtIndex(index);
        var purp = view.database.GetTarget(res, krTypePurp, true);
        return (purp != null);
    }
}

function state()
{
}
state.prototype =
{
    mSucceeded: 0,
    mFailed: 0,
    mMixed: 0
}
function regressionStats()
{
}
regressionStats.prototype =
{
    QueryInterface: function(iid) {
        if (!iid.equals(Components.interfaces.nsISupports) &&
            !iid.equals(Components.interfaces.nsIRDFObserver))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        return this;
    },
    assertNewCount: function(aSource, aArc, aIncrement)
    {
        var count = 0;
        var parent = view.database.GetTarget(aSource, krTypeParent, true);
        while (parent) {
            var countRes = view.database.GetTarget(parent, aArc, true);
            if (countRes) {
                count = countRes.QueryInterface(nsIRDFInt).Value;
            }
    
            var newCountRes = kRDFSvc.GetIntLiteral(count + aIncrement);
            if (!newCountRes) {
                return;
            }
    
            if (countRes) {
                view.database.Change(parent, aArc, countRes, newCountRes);
            }
            else {
                view.database.Assert(parent, aArc, newCountRes, true);
            }
            parent = view.database.GetTarget(parent, krTypeParent, true);
        }
    },
    onAssert: function(aDataSource, aSource, aProperty, aTarget)
    {
        if (aProperty.EqualsNode(krTypeOrigSucc)) {
            var arc = (aTarget.EqualsNode(kGood)) ? krTypeOrigSuccCount : krTypeOrigFailCount;
            this.assertNewCount(aSource, arc, 1);
        }
        else if (aProperty.Value.substr(0, 44) == "http://www.w3.org/1999/02/22-rdf-syntax-ns#_") {
            view.database.Assert(aTarget, krTypeParent, aSource, true);
        }
    },
    onUnassert: function(aDataSource, aSource, aProperty, aTarget)
    {
        if (aProperty.EqualsNode(krTypeSucc)) {
            var arc = (aTarget.EqualsNode(kGood)) ? krTypeSuccCount : krTypeFailCount;
            this.assertNewCount(aSource, arc, -1);
        }
    },
    onChange: function(aDataSource, aSource, aProperty, aOldTarget, aNewTarget)
    {
    },
    onMove: function(aDataSource, aOldSource, aNewSource, aProperty, aTarget)
    {
    },
    beginUpdateBatch: function(aDataSource)
    {
    },
    endUpdateBatch: function(aDataSource)
    {
    }
}

function rdfObserve(aSubject, aTopic, aData)
{
    if (aTopic == "success") {
        var target = (aData == "true") ? kGood : kBad;
        view.database.Assert(aSubject, krTypeSucc, target, true);

        var parent = view.database.GetTarget(aSubject, krTypeParent, true);
        while (parent) {
            var count = 0;
            var arc = (aData == "true") ? krTypeSuccCount : krTypeFailCount;
            var countRes = view.database.GetTarget(parent, arc, true);
            if (countRes) {
                count = countRes.QueryInterface(nsIRDFInt).Value;
            }
    
            var newCountRes = kRDFSvc.GetIntLiteral(++count);
            if (newCountRes) {
                if (countRes) {
                    view.database.Change(parent, arc, countRes, newCountRes);
                }
                else {
                    view.database.Assert(parent, arc, newCountRes, true);
                }
            }
             parent = view.database.GetTarget(parent, krTypeParent, true);
        }
    }
}

runItem.prototype.kObservers.push(rdfObserve);
