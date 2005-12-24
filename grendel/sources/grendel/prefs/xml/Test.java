/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Test.java
 *
 * Created on 09 August 2005, 21:15
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

package grendel.prefs.xml;
import grendel.prefs.Preferences;
import grendel.prefs.accounts.Account_IMAP;

/**
 *
 * @author hash9
 */
public final class Test {
    
    public static void main(String... args) throws Exception {
        /*Prefs p = new Prefs();
        try {
            p.loadFromXML(new FileReader("C:\\test.xml"));
        } catch (FileNotFoundException fnfe) {}
        p.setProperty("test","102");
        Accounts accounts = new Accounts();
        accounts.addAccount(new Account_SMTP("Main_SMTP"));
        accounts.addAccount(new Account_IMAP("Main_IMAP"));
        accounts.getAccount("Main_IMAP").addIdentity(new Identity("Hash9","hash9@localhost","Main"));
        p.setProperty("accounts",accounts);
        p.setProperty("accounts_1",accounts);
        Prefs p_1 = new Prefs();
        p_1.setProperty("gui","false");
        p.setProperty("ui",p_1);
        p.storeToXML(new FileWriter("C:\\test_1.xml"));
        System.out.println(p.toString());*/
        /*Accounts accounts = XMLPreferences.getPreferances().getAccounts();
        accounts.addAccount(new Account_SMTP("Main_SMTP"));
        accounts.addAccount(new Account_IMAP("Main_IMAP"));
        accounts.getAccount(1).addIdentity(new Identity("Hash9","hash9@localhost","Main"));
        XMLPreferences.savePreferances();
        XMLPreferences.saveProfiles();*/
        /*Account_IMAP mac = new Account_IMAP();
        mac.setName("Mac");
        mac.setHost("192.168.0.107");
        mac.setPort(143);
        mac.setDefaultIdentity(0);
        mac.setPassword("Sig27ma");
        mac.setUsername("hash9");*/
        /*Account_POP3 pop3 = new Account_POP3();
        pop3.setName("POP3");
        pop3.setHost("192.168.0.1");
        pop3.setPort(110 );
        pop3.setDefaultIdentity(0);
        pop3.setPassword("War");
        pop3.setUsername("kieran");
        pop3.setLeaveMailOnServer(true);
        pop3.setShowAsIMAP(true);*/
        /*Account_POP3 mac_2 = new Account_POP3();
        mac_2.setName("Mac 2");
        mac_2.setHost("192.168.0.107");
        mac_2.setPort(110 );
        mac_2.setDefaultIdentity(0);
        mac_2.setPassword("Sig27ma");
        mac_2.setUsername("hash9");
        mac_2.setLeaveMailOnServer(true);
        mac_2.setShowAsIMAP(true);*/
        /*Account_IMAP wired = new Account_IMAP();
        wired.setName("Mac 2");
        wired.setHost("127.0.0.1");
        wired.setPort(143);
        wired.setDefaultIdentity(0);
        wired.setPassword("tri3i9");
        wired.setUsername("hash9");*/
        //XMLPreferences.getPreferances().getAccounts().getAccount(5);
        //Preferences.getPreferances().getAccounts().removeAccount(5);
        /*Account_IMAP Server_2 = new Account_IMAP();
        Server_2.setName("Server");
        Server_2.setHost("192.168.0.100");
        Server_2.setPort(143);
        Server_2.setDefaultIdentity(0);
        Server_2.setPassword("Sig27ma");
        Server_2.setUsername("hash9");
        Preferences.getPreferances().getAccounts().addAccount(Server_2);*/
        /*Account_NNTP a = new Account_NNTP("Mozilla Mail/News");
        Identity id = new Identity();
        id.setEMail("grendel@eternal.undonet.com");
        id.setName("Grendel Develoment Build");
        a.addIdentity(id);
        a.setHost("news.mozilla.org");
        a.setPort(-1);
        Preferences.getPreferances().getAccounts().addAccount(a);*/
        Account_IMAP Server_2 = new Account_IMAP();
        Server_2.setName("Alt Server");
        Server_2.setHost("192.168.0.100");
        Server_2.setPort(145);
        Server_2.setDefaultIdentity(0);
        Server_2.setPassword("Sig27ma");
        Server_2.setUsername("hash9");
        Preferences.getPreferances().getAccounts().addAccount(Server_2);
        Preferences.save();
    }
}
