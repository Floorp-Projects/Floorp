/*
 * AccountIdentitiy.java
 *
 * Created on 23 August 2005, 16:55
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.sending;

import grendel.prefs.accounts.Account;
import grendel.prefs.accounts.Account__Send;
import grendel.prefs.accounts.Identity;

/**
 *
 * @author hash9
 */
public class AccountIdentity {
    private Identity ident;
    private Account__Send account;
    
    /**
     * Creates a new instance of AccountIdentity
     */
    public AccountIdentity(Account__Send account, Identity ident) {
        this.ident = ident;
        this.account = account;
    }

    public Account__Send getAccount() {
        return account;
    }

    public Identity getIdentity() {
        return ident;
    }


}
