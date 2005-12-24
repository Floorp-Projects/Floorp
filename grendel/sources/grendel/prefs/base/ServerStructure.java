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

import grendel.prefs.accounts.Account;
import grendel.prefs.accounts.Account_IMAP;
import grendel.prefs.accounts.Account__Local;
import grendel.prefs.accounts.Account_NNTP;
import grendel.prefs.accounts.Account_POP3;
import grendel.prefs.accounts.Account__Server;

public class ServerStructure {
    String myBerkeleyDirectory = "";
    
    private Account a;
    
    public ServerStructure() {
        //this.a = new Account();
    }
    
    
    public ServerStructure(Account a) {
        this.a = a;
    }
    
    public ServerStructure(String aDescription) {
        a = new Account_IMAP();
        a.setName(aDescription);
    }
    
    public String getDescription() {
        return a.getName();
    }
    
    public String getHost() {
        if (a instanceof Account__Server) {
            return ((Account__Server)a).getHost();
        } else {
            return "";
        }
    }
    
    public int getPort() {
        if (a instanceof Account__Server) {
            return ((Account__Server)a).getPort();
        } else {
            return -1;
        }
    }
    
    public String getType() {
        if (a instanceof Account_IMAP) {
            return "imap";
        } else if (a instanceof Account_NNTP) {
            return "nntp";
        } else if (a instanceof Account_POP3) {
            return "pop3";
        } else if (a instanceof Account__Local) {
            return "berkeley";
        } else {
            return "smtp";
        }
    }
    
    public String getUsername() {
        if (a instanceof Account__Server) {
            return ((Account__Server)a).getUsername();
        } else {
            return "";
        }
    }
    
    public String getPassword() {
        if (a instanceof Account__Server) {
            return ((Account__Server)a).getPassword();
        } else {
            return "";
        }
    }
    
    public int getDefaultIdentity() {
        return a.getDefaultIdentity();
    }
    
    public String getBerkeleyDirectory() {
        return myBerkeleyDirectory;
    }
    
    public boolean getPOP3LeaveMailOnServer() {
        if (a instanceof Account_POP3) {
            return ((Account_POP3)a).leaveMailOnServer();
        } else {
            return false;
        }
    }
    
    public boolean getPOP3ShowAsImap() {
        if (a instanceof Account_POP3) {
            return ((Account_POP3)a).showAsIMAP();
        } else {
            return false;
        }
    }
    
    public void setDescription(String aDescription) {
        a.setName(aDescription);
    }
    
    public void setHost(String aHost) {
        if (a instanceof Account__Server) {
            ((Account__Server)a).setHost(aHost);
        }
    }
    
    public void setPort(int aPort) {
        if (a instanceof Account__Server) {
            ((Account__Server)a).setPort(aPort);
        }
    }
    
    public void setType(int type) {
        if (a!=null) {
            switch (type) {
                case 1: {
                    //a = new Account();
                }
                break;
                case 2: {
                    a = new Account_POP3();
                }
                break;
                case 3: {
                    a = new Account_IMAP();
                }
                break;
                case 4: {
                    a = new Account_NNTP();
                }
                break;
            }
        }
    }
    
    public void setUsername(String aUsername) {
        if (a instanceof Account__Server) {
            ((Account__Server)a).setUsername(aUsername);
        }
    }
    
    public void setPassword(String aPassword) {
        if (a instanceof Account__Server) {
            ((Account__Server)a).setPassword(aPassword);
        }
    }
    
    public void setDefaultIdentity(int aDefaultIdentity) {
        a.setDefaultIdentity(aDefaultIdentity);
    }
    
    public void setBerkeleyDirectory(String aBerkeleyDirectory) {
        myBerkeleyDirectory = aBerkeleyDirectory;
    }
    
    public void setPOP3LeaveMailOnServer(boolean aPOP3LeaveMailOnServer) {
        if (a instanceof Account_POP3) {
            ((Account_POP3)a).setLeaveMailOnServer(aPOP3LeaveMailOnServer);
        }
    }
    
    public void setPOP3ShowAsImap(boolean aPOP3ShowAsImap) {
        if (a instanceof Account_POP3) {
            ((Account_POP3)a).setShowAsIMAP(aPOP3ShowAsImap);
        }
    }
    
    public Account getAccount() {
        return a;
    }
}