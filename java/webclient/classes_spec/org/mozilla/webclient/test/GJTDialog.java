/** Ashu -- this is the GJTDialog Class 
 *  This contains the basic dialog functionality
 *  for generic dialogs
 */

package org.mozilla.webclient.test;

import java.lang.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import java.util.*;


public class GJTDialog extends Dialog {
  protected DialogClient client;
  protected Frame frame;
  protected boolean centered;

  public GJTDialog(Frame frame, String title, DialogClient aClient, boolean modal) {
    this(frame, title, aClient, true, modal);
  }

  public GJTDialog(Frame frame, String title, DialogClient aClient, 
		   boolean centered, boolean modal) {
    super(frame, title, modal);
    setClient(aClient);
    setCentered(centered);
    setFrame(frame);

    addWindowListener(new WindowAdapter() {
      public void windowClosing(WindowEvent event) {
	dispose(true);
	if(client != null)
	  client.dialogCancelled(GJTDialog.this);
      }
    });
  }

  public void setCentered(boolean centered) {
    this.centered = centered;
  }

  public void setClient(DialogClient client) {
    this.client = client;
  }

  public void setFrame(Frame frame) {
    this.frame = frame;
  }
  
  public void dispose(boolean toDispose) {
    if(toDispose) {
      super.dispose();
      frame.toFront();
    }
    if(client != null)
      client.dialogDismissed(this);
  }

  public void setVisible(boolean visible) {
    pack();
    if(centered) {
      Dimension frameSize = getParent().getSize();
      Point frameLoc = getParent().getLocation();
      Dimension mySize = getSize();
      int x,y;
      x = frameLoc.x + (frameSize.width/2) - (mySize.width/2);
      y = frameLoc.y + (frameSize.height/2) - (mySize.height/2);
      setBounds(x,y,getSize().width,getSize().height);
    }
    super.setVisible(visible);
  }

}

