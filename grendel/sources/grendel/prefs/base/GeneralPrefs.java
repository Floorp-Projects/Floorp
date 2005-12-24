/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Edwin Woudt
 * <edwin@woudt.nl>.  Portions created by Edwin Woudt are
 * Copyright (C) 1999 Edwin Woudt. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

package grendel.prefs.base;
import grendel.prefs.accounts.Account_SMTP;
import grendel.prefs.Preferences;

public class GeneralPrefs {
    
    private static GeneralPrefs MasterGeneralPrefs;
    
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    public static synchronized GeneralPrefs GetMaster() {
        if (MasterGeneralPrefs == null) {
            MasterGeneralPrefs = new GeneralPrefs();
        }
        return MasterGeneralPrefs;
    }
    
    /*Preferences prefs;*/
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    private GeneralPrefs() {
        /*prefs = PreferencesFactory.Get();
        readPrefs();*/
    }
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    public void readPrefs() {
        //setSMTPServer(prefs.getString("general.smtpserver",""));
        //writePrefs();
    }
    /**
     *
     * @deprecated New preferences architecture does not require this method.
     */
    public void writePrefs() {
        //prefs.putString("general.smtpserver",getSMTPServer());
        Preferences.save();
    }
    
    public String getSMTPServer() {
        Account_SMTP a = (Account_SMTP) Preferences.getPreferances().getAccounts().getAccount(0);
        return a.getHost();
    }
    
    public void setSMTPServer(String aSMTPServer) {
        Account_SMTP a = (Account_SMTP) Preferences.getPreferances().getAccounts().getAccount(0);
        a.setHost(aSMTPServer);
    }
    
}
