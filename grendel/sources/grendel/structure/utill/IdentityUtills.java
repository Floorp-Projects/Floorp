/*
 * IdentityUtills.java
 *
 * Created on 24 August 2005, 20:28
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.utill;

import grendel.prefs.Preferences;
import grendel.prefs.accounts.Account__Receive;
import grendel.prefs.accounts.Account__Send;
import grendel.prefs.accounts.Identity;
import grendel.structure.sending.AccountIdentity;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 *
 * @author hash9
 */
public final class IdentityUtills {
    
    /** Creates a new instance of IdentityUtills */
    private IdentityUtills() {
    }
    
    public static ArrayList<AccountIdentity> getAllAccountIdentitys() {
        ArrayList<AccountIdentity> ai_a = new ArrayList<AccountIdentity>();
        List<Account__Receive> recieve_accounts =  Preferences.getPreferances().getAccounts().getReciveAccounts();
        for (Account__Receive account: recieve_accounts) {
            Collection<Identity> identities = account.getCollectionIdentities();
            Account__Send send_account = account.getSendAccount();
            for (Identity id: identities) {
                ai_a.add(new AccountIdentity(send_account, id));
            }
        }
        return ai_a;
    }
}
