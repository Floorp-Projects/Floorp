/*
 * AddressList.java
 *
 * Created on 02 September 2005, 22:38
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.storage.addressparser;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import javax.mail.Address;

/**
 *
 * @author hash9
 */
public class AddressList extends ArrayList<Address> {
    
    /** Creates a new instance of AddressList */
    public AddressList() {
    }
    
    public AddressList(List<Address> l) {
        super(l);
    }
    
    public AddressList(Address[] a_a) {
        super(Arrays.asList(a_a));
    }
    
    public AddressList(RFC822MailboxList mbl) {
        this(mbl.getMailboxArray());
    }
    
    /**
     * @return     An array of <b>Address</b> objects.
     *             Methods on the object are used to get strings
     *             to display.
     *
     * @see        Address
     */
    public Address[] getAddressArray() {
        return this.toArray(new Address[0]);
    }
    
}
