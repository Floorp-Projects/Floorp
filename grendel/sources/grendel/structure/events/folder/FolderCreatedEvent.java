/*
 * FolderCreatedEvent.java
 *
 * Created on 23 August 2005, 15:08
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events.folder;

import grendel.structure.Folder;

/**
 *
 * @author hash9
 */
public class FolderCreatedEvent extends FolderEvent {
    
    /** Creates a new instance of FolderCreatedEvent */
    public FolderCreatedEvent(Folder referance) {
        super(referance);
    }
    
}
