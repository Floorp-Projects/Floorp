/** Ashu -- this is the Postcard Class 
 *  This acts a generic Panel for the Dialog
 */

package org.mozilla.webclient.test;

import java.awt.*;


public class Postcard extends Panel {
  private Panel panel, panelContainer = new Panel();
  

  public Postcard(Panel panel) {
    if(panel != null) setPanel(panel);
    setLayout(new GridLayout());
    add(panelContainer);
  }

  public Panel getPanel() {
    if(panelContainer.getComponentCount() == 1)
      return (Panel)panelContainer.getComponent(0);
    else
      return null;
  }

  public void setPanel(Panel panel) {
    if(panelContainer.getComponentCount() == 1) {
      panelContainer.remove(getComponent(0));
    }
    this.panel = panel;
    panelContainer.add(panel);
  }

  public Insets getInsets() {
    return new Insets(10,10,10,10);
  }

}
