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
import grendel.prefs.Preferences;
import grendel.prefs.accounts.Account;

public class ServerArray {
    
    private static ServerArray MasterServerArray;
    
    public static synchronized ServerArray GetMaster() {
        if (MasterServerArray == null) {
            MasterServerArray = new ServerArray();
        }
        return MasterServerArray;
    }
    /*
    Vector svs = new Vector();
    Preferences prefs;
     */
    private ServerArray() {
        /*prefs = PreferencesFactory.Get();
        readPrefs();*/
    }
    
    public void readPrefs() {
/*
    for (int i=0; i<prefs.getInt("server.count",1); i++) {
      ServerStructure sv = new ServerStructure();
      sv.setDescription           (prefs.getString ("server."+i+".description","Default Server"));
      sv.setHost                  (prefs.getString ("server."+i+".host",""));
      sv.setPort                  (prefs.getInt    ("server."+i+".port",-1));
      sv.setType                  (prefs.getString ("server."+i+".type","pop3"));
      sv.setUsername              (prefs.getString ("server."+i+".username",""));
      sv.setPassword              (prefs.getString ("server."+i+".password",""));
      sv.setDefaultIdentity       (prefs.getInt    ("server."+i+".default_identity",0));
      sv.setBerkeleyDirectory     (prefs.getString ("server."+i+".berkeley.directory",""));
      sv.setPOP3ShowAsImap        (prefs.getBoolean("server."+i+".pop3.showasimap",true));
      sv.setPOP3LeaveMailOnServer (prefs.getBoolean("server."+i+".pop3.leavemailonserver",false));
      add(sv);
    }
 
    writePrefs();
 */
    }
    
    public void writePrefs() {
    /*
    prefs.putInt("server.count",size());
     
    for (int i=0; i<size(); i++) {
     
      prefs.putString ("server."+i+".description"           ,get(i).getDescription());
      prefs.putString ("server."+i+".host"                  ,get(i).getHost());
      prefs.putInt    ("server."+i+".port"                  ,get(i).getPort());
      prefs.putString ("server."+i+".type"                  ,get(i).getType());
      prefs.putString ("server."+i+".username"              ,get(i).getUsername());
      prefs.putString ("server."+i+".password"              ,get(i).getPassword());
      prefs.putInt    ("server."+i+".default_identity"      ,get(i).getDefaultIdentity());
      prefs.putString ("server."+i+".berkeley.directory"    ,get(i).getBerkeleyDirectory());
      prefs.putBoolean("server."+i+".pop3.showasimap"       ,get(i).getPOP3ShowAsImap());
      prefs.putBoolean("server."+i+".pop3.leavemailonserver",get(i).getPOP3LeaveMailOnServer());
     
    }*/
        Preferences.save();
    }
    
    public ServerStructure get(int Index) {
        Account a = Preferences.getPreferances().getAccounts().getReciveAccounts().get(Index);
        if (Index<0) Index=0;
        if (Index>Preferences.getPreferances().getAccounts().getReciveAccounts().size()) Index=0;
        if (a == null) {
            return get(Index +1);
        }
        ServerStructure ss = new ServerStructure(a);
        return ss;
    }
    
    public void add(ServerStructure aServer) {
        Preferences.getPreferances().getAccounts().addAccount(aServer.getAccount());
    }
    
    public void remove(int Index) {
        Preferences.getPreferances().getAccounts().removeAccount(Index);
    }
    
    public int size() {
        return Preferences.getPreferances().getAccounts().getReciveAccounts().size();
    }
    
}
