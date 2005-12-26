/*
 * AttachmentRequested.java
 *
 * Created on 07 September 2005, 21:46
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.URL.attachment;

import grendel.URL.URLRequestedEvent;
import grendel.URL.URLRequestedListener;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import java.io.IOException;

/**
 *
 * @author hash9
 */
public class AttachmentRequested implements URLRequestedListener {
  
  /** Creates a new instance of AttachmentRequested */
  public AttachmentRequested() {
  }
  
  public void URLRequested(URLRequestedEvent e) {
    if (!e.isConsumed()) {
      if (e.getUrl().getProtocol().equalsIgnoreCase("attachment")) {
        if (! e.isConsumed()) {
          // Wait incase some where else consumes the event
          synchronized(this) {
            try {
              this.wait(50);
            } catch (InterruptedException ex) {
              ex.printStackTrace();
            }
          }
          if (! e.isConsumed()) {
            try {
              AttachmentURLConnection uca = (AttachmentURLConnection) e.getUrl().openConnection();
              e.consume();
              uca.save();
            } catch (IOException ioe) {
              NoticeBoard.publish(new ExceptionNotice(ioe));
            }
          }
        }
      }
    }
  }
}
