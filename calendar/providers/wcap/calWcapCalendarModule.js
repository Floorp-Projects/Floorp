/* -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2006 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Daniel Boelzle (daniel.boelzle@sun.com)
 *
 * Contributor(s):
 *
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

var calWcapCalendarModule = {
    
    WcapCalendarInfo: {
        classDescription: "Sun Java System Calendar Server WCAP Provider",
        contractID: "@mozilla.org/calendar/calendar;1?type=wcap",
        classID: Components.ID("{CF4D93E5-AF79-451a-95F3-109055B32EF0}")
    },
    
    registerSelf:
    function( compMgr, fileSpec, location, type )
    {
        compMgr = compMgr.QueryInterface(
            Components.interfaces.nsIComponentRegistrar );
        compMgr.registerFactoryLocation(
            this.WcapCalendarInfo.classID,
            this.WcapCalendarInfo.classDescription,
            this.WcapCalendarInfo.contractID,
            fileSpec, location, type );
    },
    
    m_scriptsLoaded: false,
    getClassObject:
    function( compMgr, cid, iid )
    {
        if (! this.m_scriptsLoaded) {
            this.m_scriptsLoaded = true;
            // load scripts:
            const scripts = [ "calWcapUtils.js", "calWcapErrors.js",
                              "calWcapRequest.js", "calWcapSession.js",
                              "calWcapCalendar.js", "calWcapCalendarItems.js",
                              "calWcapCachedCalendar.js" ];
            var scriptLoader =
                Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                .createInstance(Components.interfaces.mozIJSSubScriptLoader);
            var ioService =
                Components.classes["@mozilla.org/network/io-service;1"]
                .getService(Components.interfaces.nsIIOService);
            for each ( var script in scripts ) {
                var scriptFile = __LOCATION__.parent.clone();
                scriptFile.append(script);
                scriptLoader.loadSubScript(
                    ioService.newFileURI(scriptFile).spec, null );
            }
            init(); // init first time
        }
        
        if (! cid.equals( calWcapCalendar.prototype.classID ))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        if (! iid.equals( Components.interfaces.nsIFactory ))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        
        return {
            createInstance:
            function( outer, iid ) {
                if (outer != null)
                    throw Components.results.NS_ERROR_NO_AGGREGATION;
                var cal;
                switch (CACHE) {
                case "memory":
                case "storage":
                    cal = new calWcapCachedCalendar();
                    break;
                default:
                    cal = new calWcapCalendar();
                    break;
                }
                return cal.QueryInterface( iid );
            }
        };
    },
    
    canUnload: function( compMgr ) { return true; }
};

/** module export */
function NSGetModule( compMgr, fileSpec ) {
    return calWcapCalendarModule;
}

