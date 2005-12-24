/*
 * MessageListener.java
 *
 * Created on 23 August 2005, 14:26
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events.message;

import grendel.structure.events.Listener;

/**
 *
 * @author hash9
 */
public interface MessageListener extends Listener {
    public void flagChanged(FlagChangedEvent e);
    
    public void messageDeleted(MessageDeletedEvent e);
    
    public void messageMoved(MessageMovedEvent e);
}
