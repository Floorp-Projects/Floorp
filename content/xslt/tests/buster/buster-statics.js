/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
 *   Peter Van der Beken <peterv@propagandism.org>
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

// helper function to shortcut component creation
function doCreate(aContract, aInterface)
{
    return Components.classes[aContract].createInstance(aInterface);
}

// for the items, loading a text file
const IOSERVICE_CTRID = "@mozilla.org/network/io-service;1";
const nsIIOService    = Components.interfaces.nsIIOService;
const SIS_CTRID       = "@mozilla.org/scriptableinputstream;1"
const nsISIS          = Components.interfaces.nsIScriptableInputStream;

// rdf foo, onload handler
const kRDFSvcContractID = "@mozilla.org/rdf/rdf-service;1";
const kRDFInMemContractID = 
    "@mozilla.org/rdf/datasource;1?name=in-memory-datasource";
const kRDFContUtilsID = "@mozilla.org/rdf/container-utils;1";
const kRDFXMLSerializerID = "@mozilla.org/rdf/xml-serializer;1";
const kIOSvcContractID  = "@mozilla.org/network/io-service;1";
const kStandardURL = Components.classes["@mozilla.org/network/standard-url;1"];
const nsIURL = Components.interfaces.nsIURL;
const nsIStandardURL = Components.interfaces.nsIStandardURL;
const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsIXULTreeBuilder = Components.interfaces.nsIXULTreeBuilder;
const nsIXULTemplateBuilder = Components.interfaces.nsIXULTemplateBuilder;
const kIOSvc = Components.classes[kIOSvcContractID]
    .getService(Components.interfaces.nsIIOService);
const nsIRDFService = Components.interfaces.nsIRDFService;
const nsIRDFDataSource = Components.interfaces.nsIRDFDataSource;
const nsIRDFRemoteDataSource = Components.interfaces.nsIRDFRemoteDataSource;
const nsIRDFPurgeableDataSource =
    Components.interfaces.nsIRDFPurgeableDataSource;
const nsIRDFResource = Components.interfaces.nsIRDFResource;
const nsIRDFLiteral = Components.interfaces.nsIRDFLiteral;
const nsIRDFInt = Components.interfaces.nsIRDFInt;
const nsIRDFContainerUtils = Components.interfaces.nsIRDFContainerUtils;
const nsIRDFXMLSerializer = Components.interfaces.nsIRDFXMLSerializer;
const nsIRDFXMLSource = Components.interfaces.nsIRDFXMLSource;
const kRDFSvc =
    Components.classes[kRDFSvcContractID].getService(nsIRDFService);
const krTypeCat = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#category");
const krTypeFailCount = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#failCount");
const krTypeName = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#name");
const krTypeSucc = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#succ");
const krTypeOrigSucc = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#orig_succ");
const krTypeOrigFailCount = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#orig_failCount");
const krTypeOrigSuccCount = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#orig_succCount");
const krTypePath = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#path");
const krTypeParent = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#parent");
const krTypePurp = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#purp");
const krTypeSuccCount = kRDFSvc.GetResource("http://home.netscape.com/NC-rdf#succCount");
const kGood  = kRDFSvc.GetLiteral("yes");
const kBad   = kRDFSvc.GetLiteral("no");
const kMixed = kRDFSvc.GetLiteral("+-");
const kContUtils = doCreate(kRDFContUtilsID, nsIRDFContainerUtils);

function doCreateRDFFP(aTitle, aMode)
{
    var fp = doCreate("@mozilla.org/filepicker;1", nsIFilePicker);
    fp.init(window, aTitle, aMode);
    fp.appendFilter('*.rdf', '*.rdf');
    fp.appendFilters(nsIFilePicker.filterAll);
    return fp;
}

function goDoCommand(aCommand)
{
    try {
        var controller = 
            top.document.commandDispatcher.getControllerForCommand(aCommand);
        if (controller && controller.isCommandEnabled(aCommand))
            controller.doCommand(aCommand);
    }
    catch(e) {
        dump("An error "+e+" occurred executing the "+aCommand+" command\n");
    }
}

function registerController(aController)
{
    top.controllers.appendController(aController);
}
