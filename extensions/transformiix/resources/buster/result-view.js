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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

var currentResultItem = new Object();

function onNewResultView(event)
{
    const db = runItem.prototype.kDatabase;
    const kXalan = runItem.prototype.kXalan;
    var index = view.boxObject.selection.currentIndex;
    enablePrivilege('UniversalXPConnect');
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
    currentResultItem.testpath = xalan_fl;
    currentResultItem.refpath = xalan_ref;
    dump(name.Value+" links to "+xalan_fl+"\n");
    var win = window.openDialog('result-view.xul','txBusterResult',
                                'chrome,width=800,height=800,dialog=no',
                                currentResultItem);
}

function onResultViewLoad(event)
{
    var arg = window.arguments[0];
    document.getElementById('src').setAttribute('src', 'view-source:'+
        arg.testpath+'.xml');
    document.getElementById('style').setAttribute('src', 'view-source:'+
        arg.testpath+'.xsl');
    document.getElementById('ref').setAttribute('src', arg.refpath+'.out');
    return true;
}
