/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

// using the rdf service extensively here
var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);

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

// Given an email address (like "joe@isp.com") return an associative
// array with all the defaults for that domain
function getDomainDefaults(email) {

    var emailData = email.split('@');
    dump("emailData.length == " + emailData.length + "\n");
    if (emailData.length != 2) {
        dump("bad e-mail address!\n");
        return null;
    }
    
    if (!ispDefaults) 
        ispDefaults = rdf.GetDataSource("rdf:ispdefaults");
    
    // all the variables we'll be returning
    var username = emailData[0];
    var domain = emailData[1];
    
    domainURI = "domain:" + domain;
    domainRes = rdf.GetResource(domainURI);

    var result = dataSourceToObject(ispDefaults, domainRes);
    
    // no such ISP, just set up the defaults
    if (!ispInfo)
        return result;

    // here's where we'd do smart things

    
    return result;
}

// this function will extract an entire datasource into a giant
// associative array for easy retrieval from JS
var NClength = NC.length;
function dataSourceToObject(datasource, root)
{
    var result = null;
    var arcs = datasource.ArcLabelsOut(root);

    while (arcs.HasMoreElements()) {
        var arc = arcs.GetNext().QueryInterface(nsIRDFResource);

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
        else
            value = target.Value;
        
        // add this value
        result[arcName] = value;
    }
    return result;
}
