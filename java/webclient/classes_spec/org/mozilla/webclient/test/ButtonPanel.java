/** Ashu -- this is the ButtonPanel Class 
 *  This acts a generic Button Panel
 * for using in any Dialog
 */

package org.mozilla.webclient.test;

import java.awt.*;
import javax.swing.*;
import java.lang.*;

public class ButtonPanel extends Panel {
  Panel buttonPanel = new Panel();
  JSeparator separator = new JSeparator();


  public ButtonPanel() {
    int buttonPanelOrient = FlowLayout.CENTER;
    setLayout(new BorderLayout(0,5));
    buttonPanelOrient = FlowLayout.CENTER;
    buttonPanel.setLayout(new FlowLayout(buttonPanelOrient));
    add(separator, "North");
    add(buttonPanel, "Center");
  }

  public void add(Button button) {
    buttonPanel.add(button);
  }

  public Button add(String buttonLabel) {
    Button addMe = new Button(buttonLabel);
    buttonPanel.add(addMe);
    return addMe;
  }

  protected String paramString() {
    return super.paramString() + "buttons=" + getComponentCount();
  }
}
    
