/*
 * InstallAddressBook.java
 *
 * Created on 13 October 2005, 16:04
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.jdbc.derby;

import grendel.addressbook.AddressBookManager;
import grendel.prefs.Preferences;
import grendel.prefs.addressbook.Addressbook_Derby;

/**
 * Add the Derby Test Address book to the main addressbook list
 * @author hash9
 */
public class InstallAddressBook {
    
    /** Creates a new instance of InstallAddressBook */
    public InstallAddressBook() {
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) {
        Addressbook_Derby addressbook = new Addressbook_Derby();
        addressbook.setName("Derby Basic Test");
        AddressBookManager.addAddressbook(addressbook);
        Preferences.save();
    }
    
}
