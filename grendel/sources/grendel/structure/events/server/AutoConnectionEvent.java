/*
 * ConnectedEvent.java
 *
 * Created on 23 August 2005, 15:00
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events.server;
import grendel.structure.Server;

/**
 *
 * @author hash9
 */
public class AutoConnectionEvent extends ServerEvent {

    /** Creates a new instance of NewMessageEvent */
    public AutoConnectionEvent(Server referance) {
        super(referance);
    }

}
