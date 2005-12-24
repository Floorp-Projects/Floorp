/*
 * SimpleEventHandeler.java
 *
 * Created on 18 August 2005, 23:54
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events;
import java.util.List;

/**
 *
 * @author hash9
 */
public abstract class AbstractEventHandeler {
    protected List<Listener> listeners;
    /**
     * Creates a new instance of AbstractEventHandeler
     */
    public AbstractEventHandeler() {
    }
    
    public void addEventListener(Listener l) {
        listeners.add(l);
    }
    
    
}
