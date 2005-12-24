/*
 * MessageList.java
 *
 * Created on 19 August 2005, 12:51
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure;

import java.util.Collection;
import java.util.List;
import java.util.Vector;

/**
 *
 * @author hash9
 */
public class FolderList extends Vector<Folder> implements List<Folder> {
    
    public FolderList(int initialCapacity, int capacityIncrement) {
        super(initialCapacity, capacityIncrement);
    }
    
    /**
     * Constructs an empty list with the specified initial capacity and
     * with its capacity increment equal to zero.
     *
     * @param   initialCapacity   the initial capacity of the vector.
     * @exception IllegalArgumentException if the specified initial capacity
     *               is negative
     */
    public FolderList(int initialCapacity) {
        super(initialCapacity);
    }
    
    /**
     * Constructs an empty vector so that its internal data array
     * has size <tt>10</tt> and its standard capacity increment is
     * zero.
     */
    public FolderList() {
        super();
    }
    
    /**
     * Constructs a list containing the elements of the specified
     * collection, in the order they are returned by the collection's
     * iterator.
     *
     * @param c the collection whose elements are to be placed into this
     *       vector.
     * @throws NullPointerException if the specified collection is null.
     * @since   1.2
     */
    public FolderList(Collection<Folder> c) {
        super (c);
    }
    
    public Folder getByName(String name) {
        for (Folder f: this) {
            if (name.equalsIgnoreCase(f.getName())) {
                return f;
            }
        }
        return null;
    }
}
