/*
 * MessageEvent.java
 *
 * Created on 18 August 2005, 23:51
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events.folder;
import grendel.structure.Folder;
import grendel.structure.events.Event;

/**
 *
 * @author hash9
 */
public abstract class FolderEvent implements Event {
    Folder referance;
    /**
     * Creates a new instance of FolderEvent
     */
    public FolderEvent(Folder referance) {
        this.referance = referance;
    }

    public Folder getFolder() {
        return referance;
    }
    

    
}
