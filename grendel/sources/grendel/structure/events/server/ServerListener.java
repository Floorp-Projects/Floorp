/*
 * MessageListener.java
 *
 * Created on 23 August 2005, 14:26
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events.server;
import grendel.structure.events.Listener;

/**
 *
 * @author hash9
 */
public interface ServerListener extends Listener {
    public void connected(ConnectedEvent e);
    public void disconnected(DisconnectedEvent e);
    public void autoReconnected(AutoConnectionEvent e);
}
