/** Ashu -- this is the CheckBoxPanel Class 
 *  This acts a generic CheckBox Panel
 * for using in any Dialog
 */

package org.mozilla.webclient.test;

import java.awt.*;
import javax.swing.*;
import java.lang.*;

public class CheckBoxPanel extends Panel {
  Panel checkBoxPanel = new Panel();
  JSeparator separator = new JSeparator();


  public CheckBoxPanel() {
    int checkBoxPanelOrient = FlowLayout.CENTER;
    setLayout(new BorderLayout(5,0));
    checkBoxPanelOrient = FlowLayout.CENTER;
    checkBoxPanel.setLayout(new FlowLayout(checkBoxPanelOrient));
    add(checkBoxPanel, "Center");
  }

  public void add(Checkbox checkBox) {
    checkBoxPanel.add(checkBox);
  }

  public Checkbox add(String checkBoxLabel) {
    Checkbox addMe = new Checkbox(checkBoxLabel);
    checkBoxPanel.add(addMe);
    return addMe;
  }

  protected String paramString() {
    return super.paramString() + "checkBoxs=" + getComponentCount();
  }
}
    
