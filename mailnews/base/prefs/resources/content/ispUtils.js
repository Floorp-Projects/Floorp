/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// using the rdf service extensively here
var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

// all the RDF resources we'll be retrieving
var NC = "http://home.netscape.com/NC-rdf#";
var Server              = rdf.GetResource(NC + "Server");
var SmtpServer          = rdf.GetResource(NC + "SmtpServer");
var ServerHost          = rdf.GetResource(NC + "ServerHost");
var ServerType          = rdf.GetResource(NC + "ServerType");
var PrefixIsUsername    = rdf.GetResource(NC + "PrefixIsUsername");
var UseAuthenticatedSmtp= rdf.GetResource(NC + "UseAuthenticatedSmtp");

// this is possibly expensive, not sure what to do here
var ispDefaults;

var nsIRDFResource = Components.interfaces.nsIRDFResource;
var nsIRDFLiteral = Components.interfaces.nsIRDFLiteral;

var ispRoot = rdf.GetResource("NC:ispinfo");

// given an ISP's domain URI, look up all relevant information about it
function getIspDefaultsForUri(domainURI)
{
    if (!ispDefaults) 
        ispDefaults = rdf.GetDataSource("rdf:ispdefaults");
    
    var domainRes = rdf.GetResource(domainURI);

    var result = dataSourceToObject(ispDefaults, domainRes);

    if (!result) return null;
    
    // add this extra attribute which is the domain itself
    var domainData = domainURI.split(':');
    if (domainData.length > 1)
    result.domain = domainData[1];
    
    return result;
}

// construct an ISP's domain URI based on it's domain
// (i.e. turns isp.com -> domain:isp.com)
function getIspDefaultsForDomain(domain) {
    domainURI = "domain:" + domain;
    return getIspDefaultsForUri(domainURI);
}

// Given an email address (like "joe@isp.com") look up 
function getIspDefaultsForEmail(email) {

    var emailData = getEmailInfo(email);

    var ispData;
    if (emailData)
        ispData = getIspDefaultsForDomain(emailData.domain);

    prefillIspData(ispData, email);

    return ispData;
}

// given an email address, split it into username and domain
// return in an associative array
function getEmailInfo(email) {
    if (!email) return null;
    
    var result = new Object;
    
    var emailData = email.split('@');
    
    if (emailData.length != 2) {
        dump("bad e-mail address!\n");
        return null;
    }
    
    // all the variables we'll be returning
    result.username = emailData[0];
    result.domain = emailData[1];

    return result;
}

function prefillIspData(ispData, email, fullName) {
    if (!ispData) return;

    // make sure these objects exist
    if (!ispData.identity) ispData.identity = new Object;
    if (!ispData.incomingServer) ispData.incomingServer = new Object;

    // fill in e-mail if it's not already there
    if (email && !ispData.identity.email)
        ispData.identity.email = email;

    var emailData = getEmailInfo(email);
    if (emailData) {

        // fill in the username (assuming the ISP doesn't prevent it)
        if (!ispData.incomingServer.userName &&
            !ispData.incomingServer.noDefaultUsername)
            ispData.incomingServer.username = emailData.username;
    }
    
}

// this function will extract an entire datasource into a giant
// associative array for easy retrieval from JS
var NClength = NC.length;
function dataSourceToObject(datasource, root)
{
    var result = null;
    var arcs = datasource.ArcLabelsOut(root);

    while (arcs.hasMoreElements()) {
        var arc = arcs.getNext().QueryInterface(nsIRDFResource);

        var arcName = arc.Value;
        arcName = arcName.substring(NClength, arcName.length);

        if (!result) result = new Object;
        
        var target = datasource.GetTarget(root, arc, true);
        
        var value;
        var targetHasChildren = false;
        try {
            target = target.QueryInterface(nsIRDFResource);
            targetHasChildren = true;
        } catch (ex) {
            target = target.QueryInterface(nsIRDFLiteral);
        }

        if (targetHasChildren)
            value = dataSourceToObject(datasource, target);
        else {
            value = target.Value;

            // fixup booleans/numbers/etc
            if (value == "true") value = true;
            else if (value == "false") value = false;
            else {
                var num = Number(value);
                if (!isNaN(num)) value = num;
            }
        }
            
        
        // add this value
        result[arcName] = value;
    }
    return result;
}
