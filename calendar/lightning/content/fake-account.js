
function ltndbg(s)
{
    dump("LTN: " + s + "\n");
}

var ltnAccount;
const LTN_FAKE_ACCOUNT_KEY = "lightning-fake-account";
const LTN_SERVER_NAME = "Calendars"; // XXX l10n
const LTN_FAKE_USER = "lightning-fake-user";
const LTN_SERVER_HOSTNAME = "lightning";

function ltnEnsureFakeAccount()
{
    ltnAccount = /*TBG*/accountManager.getAccount(LTN_FAKE_ACCOUNT_KEY);
    if (ltnAccount.incomingServer.username == LTN_FAKE_USER) {
        ltndbg("found fake account: " + ltnAccount);
        return;
    }

    ltndbg("creating fake account");
    var server = /*TBG*/accountManager.
        createIncomingServer(LTN_FAKE_USER, LTN_SERVER_HOSTNAME, "none");

    server.prettyName = LTN_SERVER_NAME;
    ltnAccount.incomingServer = server;

    /*TBG*/accountManager.saveAccountInfo();
    ltndbg("created: " + ltnAccount);
}

ltnEnsureFakeAccount();

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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@off.net>
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
/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
