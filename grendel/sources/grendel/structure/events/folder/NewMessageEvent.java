/*
 * NewMessageEvent.java
 *
 * Created on 23 August 2005, 15:00
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events.folder;

import grendel.structure.Folder;
import grendel.structure.Message;

/**
 *
 * @author hash9
 */
public class NewMessageEvent extends FolderEvent {
    private Message m;
    /** Creates a new instance of NewMessageEvent */
    public NewMessageEvent(Folder referance, Message m) {
        super(referance);
    }

    public Message getMessage() {
        return m;
    }
    

}
