/*
 * MarkChanged.java
 *
 * Created on 18 August 2005, 23:50
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events.message;

import grendel.structure.Message;

/**
 *
 * @author hash9
 */
public class FlagChangedEvent extends MessageEvent {
    
    /**
     * Creates a new instance of FlagChangedEvent
     */
    public FlagChangedEvent(Message referance) {
        super(referance);
    }
    
}
