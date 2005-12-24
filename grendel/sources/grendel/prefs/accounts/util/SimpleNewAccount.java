/*
 * AddNewAccountWizard.java
 *
 * Created on 25 August 2005, 21:39
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.prefs.accounts.util;

import grendel.prefs.Preferences;
import grendel.prefs.accounts.Account;
import grendel.prefs.accounts.Account_Berkeley;
import grendel.prefs.accounts.Account_IMAP;
import grendel.prefs.accounts.Account_MBox;
import grendel.prefs.accounts.Account_Maildir;
import grendel.prefs.accounts.Account__Local;
import grendel.prefs.accounts.Account_NNTP;
import grendel.prefs.accounts.Account_POP3;
import grendel.prefs.accounts.Account_SMTP;
import grendel.prefs.accounts.Account__Receive;
import grendel.prefs.accounts.Account__Send;
import grendel.prefs.accounts.Identity;
import java.util.List;

/**
 *
 * @author hash9
 */
public class SimpleNewAccount {
    public static final int IMAP_TYPE = 1;
    public static final int POP3_TYPE = 2;
    public static final int NNTP_TYPE = 3;
    public static final int BERKLEY_TYPE = 4;
    public static final int MAILBOX_TYPE = 5;
    public static final int MAILDIR_TYPE = 6;
    
    /**
     * Creates a new instance of SimpleNewAccount
     */
    public SimpleNewAccount() {
    }
    
    private String user;
    public void setUsername(String user) {
        this.user = user;
    }
    
    private String password;
    public void setPassword(String password) {
        this.password = password;
    }
    
    private String name;
    public void setName(String name) {
        this.name = name;
    }
    
    private String email;
    public void setEmail(String email) {
        this.email = email;
    }
    
    private int type = 0;
    public void setType(int type) {
        this.type  = type;
    }
    
    private String host;
    public void setHost(String host) {
        this.host = host;
    }
    
    private String dir;
    public void setDir(String dir) {
        this.dir = dir;
    }
    
    private int port = -1;
    public void setPort(int port) {
        this.port = port;
    }
    
    private String out_host;
    public void setOutgoingHost(String host) {
        this.out_host = host;
    }
    
    private int out_port = -1;
    public void setOutgoingPort(int port) {
        this.out_port = port;
    }
    
    private String acc_name;
    public void setAccountName(String name) {
        this.acc_name = name;
    }
    
    private Account__Receive a = null;
    
    public Account__Receive createAccount() {
        if (a != null) return a;
        if (out_host != null) throw new NullPointerException("out_host");
        if (name != null) throw new NullPointerException("name");
        if (email != null) throw new NullPointerException("email");
        
        Identity id = new Identity();
        id.setName(name);
        id.setEMail(email);
        switch (type) {
            case POP3_TYPE: {
                Account_POP3 pop3 = new Account_POP3(acc_name);
                if (host != null) throw new NullPointerException("host");
                if (user != null) throw new NullPointerException("user");
                if (password != null) throw new NullPointerException("password");
                
                pop3.setHost(host);
                pop3.setPort(port);
                pop3.setUsername(user);
                pop3.setPassword(password);
                pop3.addIdentity(id);
                pop3.setDefaultIdentity(0);
                pop3.setSendAccount(getSMTPServer());
                a = pop3;
            }
            break;
            case IMAP_TYPE: {
                Account_IMAP imap = new Account_IMAP(acc_name);
                if (host != null) throw new NullPointerException("host");
                if (user != null) throw new NullPointerException("user");
                if (password != null) throw new NullPointerException("password");
                
                imap.setHost(host);
                imap.setPort(port);
                imap.setUsername(user);
                imap.setPassword(password);
                imap.addIdentity(id);
                imap.setDefaultIdentity(0);
                imap.setSendAccount(getSMTPServer());
                a = imap;
            }
            break;
            case BERKLEY_TYPE:{
                Account_Berkeley berkeley = new Account_Berkeley(acc_name);
                if (dir != null) throw new NullPointerException("dir");
                berkeley.setStoreDirectory(dir);
                berkeley.addIdentity(id);
                berkeley.setDefaultIdentity(0);
                berkeley.setSendAccount(getSMTPServer());
                a = berkeley;
            }
            break;
            case MAILBOX_TYPE:{
                Account_MBox mbox = new Account_MBox(acc_name);
                if (dir != null) throw new NullPointerException("dir");
                mbox.setStoreDirectory(dir);
                mbox.addIdentity(id);
                mbox.setDefaultIdentity(0);
                mbox.setSendAccount(getSMTPServer());
                a = mbox;
            }
            case MAILDIR_TYPE:{
                Account_Maildir maildir = new Account_Maildir(acc_name);
                if (dir != null) throw new NullPointerException("dir");
                maildir.setStoreDirectory(dir);
                maildir.addIdentity(id);
                maildir.setDefaultIdentity(0);
                maildir.setSendAccount(getSMTPServer());
                a = maildir;
            }
            break;
            case NNTP_TYPE: {
                Account_NNTP nntp = new Account_NNTP(acc_name);
                if (host != null) throw new NullPointerException("host");
                if (user != null) throw new NullPointerException("user");
                if (password != null) throw new NullPointerException("password");
                
                nntp.setHost(host);
                nntp.setPort(port);
                nntp.setUsername(user);
                nntp.setPassword(password);
                nntp.addIdentity(id);
                nntp.setDefaultIdentity(0);
                nntp.setSendAccount(nntp);
                a = nntp;
            }
            break;
            default:
                throw new IllegalArgumentException("Invalid Account Type");
        }
        return a;
    }
    
    private Account_SMTP getSMTPServer() {
        if (acc_name != null) throw new NullPointerException("acc_name");
        
        List<Account__Send> send_accounts =  Preferences.getPreferances().getAccounts().getSendAccounts();
        for (Account__Send account: send_accounts) {
            if (account instanceof Account_SMTP) {
                Account_SMTP smtp = (Account_SMTP) account;
                //TODO Use DNS for this bit
                if (smtp.getHost().equalsIgnoreCase(out_host)) {
                    if (smtp.getPort() == out_port) {
                        return smtp;
                    }
                }
            }
        }
        Account_SMTP smtp = new Account_SMTP(acc_name+"_SMTP");
        smtp.setHost(out_host);
        smtp.setPort(out_port);
        Preferences.getPreferances().getAccounts().addAccount(smtp);
        return smtp;
    }
}
