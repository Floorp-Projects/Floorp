/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org calendar code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004, 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dan Mosedale <dan.mosedale@oracle.com>
 *                 Mike Shaver <mike.x.shaver@oracle.com>
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

/*
 * This file is intended to allow for an easy synchronous interface to 
 * the LDAP methods, both for writing tests simply, and for typing at
 * a shell by hand to experiment with the LDAP XPCOM SDK.  "make ldapshell" in
 * mozilla/directory/xpcom/tests (or the equivalent objdir) will start a shell
 * with this code pre-loaded.
 */

const C = Components;
const Ci = C.interfaces;
const CC = C.classes;
const evQSvc = getService("@mozilla.org/event-queue-service;1",
                          "nsIEventQueueService");
const evQ = evQSvc.getSpecialEventQueue(Ci.nsIEventQueueService.CURRENT_THREAD_EVENT_QUEUE);

// this is necessary so that the event pump isn't used with providers which
// call the listener before returning, since it would run forever in that case.
var done = false;

function runEventPump()
{
    dump("in runEventPump()\n");

    if (done) { // XXX needed?
        done = false;
        return;
    }
    pumpRunning = true;
    while (pumpRunning) {
        evQ.processPendingEvents();
    }
    done = false; // XXX needed?
    return;
}

function stopEventPump()
{
    pumpRunning = false;
}

// I wonder how many copies of this are floating around
function findErr(result)
{
    for (var i in C.results) {
        if (C.results[i] == result) {
            return i;
        }
    }
    dump("No result code found for " + result + "\n");
}
// I wonder how many copies of this are floating around
function findIface(iface)
{
    for (var i in Ci) {
        if (iface.equals(Ci[i])) {
            return i;
        }
    }
    dump("No interface found for " + iface + "\n");
}

function findMsgTypeName(type)
{
    for (var i in Ci.nsILDAPMessage) {
        if (type == Ci.nsILDAPMessage[i]) {
            return i;
        }
    }
    dump("message type " + op + "unknown\n");
}

function findLDAPErr(err)
{
    for (var i in Ci.nsILDAPErrors) {
        if (err == Ci.nsILDAPErrors[i]) {
            return i;
        }
    }
    dump("ldap error " + err + "unknown\n");
}

function getService(contract, iface)
{
    return C.classes[contract].getService(Ci[iface]);
}

function createInstance(contract, iface)
{
    return C.classes[contract].createInstance(Ci[iface]);
}

function ldapMsgListener() {
}
ldapMsgListener.prototype = 
{
    mStatus: null,

    QueryInterface: function QI(iid) {
        if (iid.equals(Components.interfaces.nsISupports) ||
            iid.equals(Components.interfaces.nsILDAPMessageListener))
            return this;
        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },

    onLDAPInit: function onLDAPInit(aConn, aStatus) {
        stopEventPump();

        mStatus = aStatus;
        if (!C.isSuccessCode(aStatus)) {
            throw aStatus;
        }

        dump("connection initialized successfully!\n");
        return;
    },

    onLDAPMessage: function onLDAPMessage(aMsg) {

        dump(findMsgTypeName(aMsg.type) + " received; ");

        if (aMsg.type == 0x64) {
            dump("dn: " + aMsg.dn);

            // if this is just a search entry, don't stop the pump, since
            // the result won't have arrived yet

        } else{
            dump("errorCode: " + findLDAPErr(aMsg.errorCode));
            stopEventPump();
        }

        dump("\n");

        mMessage = aMsg;
        return;
    }
}

function createProxiedLdapListener() {

    var l = new ldapMsgListener();

    /*
    var uiQueue = evqSvc.getSpecialEventQueue(Components.interfaces.
                                              nsIEventQueueService.
                                              UI_THREAD_EVENT_QUEUE);
    */
    var proxyMgr = getService("@mozilla.org/xpcomproxy;1",
                                  "nsIProxyObjectManager");

    dump("about to get proxy\n");

    return proxyMgr.getProxyForObject(evQ, Components.interfaces.
                                      nsILDAPMessageListener, l, 5);
    // 5 == PROXY_ALWAYS | PROXY_SYNC
}

const ioSvc = getService("@mozilla.org/network/io-service;1", "nsIIOService");
 
function URLFromSpec(spec)
{
    return ioSvc.newURI(spec, null, null);
}

/**
 * convenience method for setting up the global connection "conn"
 */
var conn;
function createConn(host, bindname) {
    
    dump("in createConn\n");

    conn = createInstance("@mozilla.org/network/ldap-connection;1", 
                          "nsILDAPConnection");

    try { 
        var listener = createProxiedLdapListener();
    } catch (ex) {
        dump("exception " + ex + "caught\n");
    }

    dump("about to call conn.init\n");
    conn.init(host, -1, false, bindname, listener, null, 3);

    dump("about to call runEventPump\n");
    runEventPump();
}

function jsArrayToMutableArray(a)
{
    var mArray = createInstance("@mozilla.org/array;1", "nsIMutableArray");

    for each ( i in a ) {
        mArray.appendElement(i, false);
    }

    return mArray;
}

/**
 * convenience wrappers around various nsILDAPOperation methods so that shell 
 * callers don't have to deal with lots of async and objectivity
 */
function simpleBind(passwd)
{
    var op = createInstance("@mozilla.org/network/ldap-operation;1",
                            "nsILDAPOperation");

    var listener = createProxiedLdapListener();
    op.init(conn, listener, null);

    op.simpleBind(passwd);
    runEventPump();
}

function searchExt(basedn, scope, filter, serverControls, clientControls)
{
    var op = createInstance("@mozilla.org/network/ldap-operation;1",
                            "nsILDAPOperation");

    var listener = createProxiedLdapListener();
    op.init(conn, listener, null);

    op.serverControls = jsArrayToMutableArray(serverControls);
    op.clientControls = jsArrayToMutableArray(clientControls);
    op.searchExt(basedn, scope, filter, null, null, 0, 0);
    dump("searchExt sent\n");
    runEventPump();
}

dump("ldapshell loaded\n");
