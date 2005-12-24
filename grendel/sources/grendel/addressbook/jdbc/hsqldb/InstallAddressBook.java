/*
 * InstallAddressBook.java
 *
 * Created on 13 October 2005, 16:04
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.jdbc.hsqldb;

import grendel.addressbook.AddressBookManager;
import grendel.prefs.Preferences;
import grendel.prefs.addressbook.Addressbook_HSQLDB;

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
        Addressbook_HSQLDB addressbook = new Addressbook_HSQLDB();
        addressbook.setName("HSQLDB Test Database");
        AddressBookManager.addAddressbook(addressbook);
        Preferences.save();
    }
    
}
