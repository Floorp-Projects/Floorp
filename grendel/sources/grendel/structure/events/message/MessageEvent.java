/*
 * MessageEvent.java
 *
 * Created on 18 August 2005, 23:51
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events.message;

import grendel.structure.Message;
import grendel.structure.events.Event;

/**
 *
 * @author hash9
 */
public abstract class MessageEvent implements Event {
    protected Message referance;
    /** Creates a new instance of MessageEvent */
    public MessageEvent(Message referance) {
        this.referance = referance;
    }

    public Message getMessage() {
        return referance;
    }
    

}
