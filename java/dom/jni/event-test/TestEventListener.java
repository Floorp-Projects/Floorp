package org.w3c.dom.events;

import org.w3c.dom.events.EventListener;
import org.w3c.dom.events.Event;

public class TestEventListener implements EventListener {


  public TestEventListener() {};

  /**
   * This method is called whenever an event occurs of the type for which the 
   * <code> EventListener</code> interface was registered. 
   * @param event The <code>Event</code> contains contextual information about 
   *   the event.  It also contains the <code>returnValue</code> and 
   *   <code>cancelBubble</code> properties which are used in determining 
   *   proper event flow.
   */
  public void               handleEvent(Event event) {
    try {
	    System.out.println("In JAVA: Got event" + event);
	} catch (Exception e) {
	    System.out.println("Got exception in handleEvent "+e);
    }
  }

}

