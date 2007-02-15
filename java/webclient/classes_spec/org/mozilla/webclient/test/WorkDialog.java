/** Ashu -- this is the WorkDialog Class 
 *  This acts a generic Dialog class with
 *  a buttons panel and a work area panel
 */

package org.mozilla.webclient.test;

import java.awt.*;
import java.awt.event.*;
import java.lang.*;

public class WorkDialog extends GJTDialog {
  private ButtonPanel buttonPanel;
  private CheckBoxPanel checkBoxPanel;
  private Panel       workPanel;

  public WorkDialog(Frame frame, DialogClient client, String title) {
    this(frame, client, title, null, false);
  }

  public WorkDialog(Frame frame, DialogClient client, String title, boolean modal) {
    this(frame, client, title, null, modal);
  }

  public WorkDialog(Frame frame, DialogClient client, String title,
		    Panel workPanel, boolean modal) {
    super(frame, title, client, modal);
    this.workPanel = workPanel;
	setLayout(new BorderLayout());
    if(workPanel != null)
      add(workPanel, "North");
    add("West", checkBoxPanel = new CheckBoxPanel());
    add("South", buttonPanel = new ButtonPanel());
  }

  public void setWorkPanel(Panel workPanel) {
    if(workPanel != null)
      remove(workPanel);
    this.workPanel = workPanel;
    add(workPanel, "Center");
    if(isShowing())
      validate();
  }

  public Button addButton(String string) {
    return buttonPanel.add(string);
  }

  public void addButton(Button button) {
    buttonPanel.add(button);
  }

  public Checkbox addCheckBox(String string) {
    return checkBoxPanel.add(string);
  }

  public void addCheckBox(Checkbox checkBox) {
    checkBoxPanel.add(checkBox);
  }
}
