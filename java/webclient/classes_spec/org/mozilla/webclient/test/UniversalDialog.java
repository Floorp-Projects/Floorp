/** Ashu -- this is the Find Dialog Class 
 instantiated within the EMWindow class, this
 will act as the Modal dialog for calling Find/Find Next 
 functions for CurrentPage
*/

package org.mozilla.webclient.test;

import org.mozilla.util.Assert;
import org.mozilla.webclient.Prompt;

import java.awt.*;
import java.awt.event.*;

import java.util.Properties;

// PENDING(edburns): Apply uniform formatting throughout webclient

public class UniversalDialog extends WorkDialog implements ActionListener, ItemListener
{
static private int	_defaultTextFieldSize = 20;
public Button		[] buttons;
public TextField	[] fields;

public String mTitleMsg;
public String mText;
public String mCheckboxMsg;
public Checkbox mCheckbox;
public String mField1Label;
public String mField2Label;
public boolean mField1IsPasswd;
public Properties mProps;
	
public UniversalDialog(Frame frame, DialogClient client, String dialogTitle) 
{
    super(frame, client, dialogTitle, /* isModal */ true);
}

public void setParms(String titleMsg, 
		     String dialogTitle, String text, String checkboxMsg, 
		     String button0Text, String button1Text, 
		     String button2Text, String editfield1Msg,
		     String editfield2Msg,
		     int numButtons, int numEditfields, 
		     boolean editfield1Password,
		     Properties props)
{
    DialogPanel dialogPanel;
    int i = 0;
    buttons = null;
    fields = null;
    mTitleMsg = titleMsg;
    this.setTitle(dialogTitle);
    mText = text;
    mCheckboxMsg = checkboxMsg;
    mField1Label = editfield1Msg;
    mField2Label = editfield2Msg;
    mField1IsPasswd = editfield1Password;
    mProps = props;

    if (0 < numButtons) {
	buttons = new Button[numButtons];
	String label;
	
	for (i = 0; i < numButtons; i++) {
	    // figure out which String to use;
	    if (0 == i) {
		label = button0Text;
	    }
	    else if (1 == i) {
		label = button1Text;
	}
	    else {
		label = button2Text;
	    }
	    buttons[i] = addButton(label);
	    buttons[i].addActionListener(this);
	}
    }

    if (0 < numEditfields) {
	fields = new TextField[numEditfields];
	
	for (i = 0; i < numEditfields; i++) {
	    fields[i] = new TextField("", _defaultTextFieldSize);
	    if (mField1IsPasswd && i == 0) {
		fields[i].setEchoChar('*');
	    }
	}
    }
    
    dialogPanel = new DialogPanel(this);
    setWorkPanel(dialogPanel);
}

public void actionPerformed(ActionEvent ae) 
{
    Assert.assert(null != buttons);
    int i = 0;
    for (i = 0; i < buttons.length; i++) {
	if (ae.getSource() == buttons[i]) {
	    mProps.put(Prompt.BUTTON_PRESSED_KEY, Integer.toString(i));
	    // pull out the values from the TextFields
	    break;
	}
    }
    if (null != fields) {
	String curString;
	for (i = 0; i < fields.length; i++) {
	    curString = fields[i].getText();
	    if (0 == i) {
		mProps.put(Prompt.EDIT_FIELD_1_KEY, curString);
	    }
	    else {
		mProps.put(Prompt.EDIT_FIELD_2_KEY, curString);
	    }
	}
    }

    if (null != mCheckbox) {
	Boolean bool = new Boolean(mCheckbox.getState());
	mProps.put(Prompt.CHECKBOX_STATE_KEY, bool.toString());
    }
    dispose(true);
}


public void itemStateChanged(ItemEvent e) 
{
}
    
public void setVisible(boolean b) 
{
    if (null != fields) {
	fields[0].requestFocus();
    }
    super.setVisible(b);
}
    
}


class DialogPanel extends Postcard 
{
private UniversalDialog dialog;
    
public DialogPanel(UniversalDialog myDialog) 
{
    super(new Panel());
    Panel panel = getPanel();
    panel.setLayout(new BorderLayout());
    this.dialog = myDialog;
    int i = 0;
    
    if (null != dialog.mTitleMsg || null != dialog.mText) {
	Panel northPanel = new Panel();
	northPanel.setLayout(new BorderLayout());

	// set up the stuff on top of the text fields
	if (null != dialog.mTitleMsg) {
	    Label titleLabel = new Label(dialog.mTitleMsg);
	    titleLabel.setBackground(Color.lightGray);
	    northPanel.add(titleLabel, BorderLayout.NORTH);
	}
	if (null != dialog.mText) {
	    Label textLabel = new Label(dialog.mText);
	    textLabel.setBackground(Color.lightGray);
	    northPanel.add(textLabel, BorderLayout.CENTER);
	}
	panel.add(northPanel, BorderLayout.NORTH);
    }

    Panel centerPanel = new Panel();
    centerPanel.setLayout(new BorderLayout());
    
    if (null != dialog.fields) {
	Panel fieldPanel = new Panel();
	fieldPanel.setLayout(new BorderLayout());
	// set up the text fields
	Panel curPanel;
	Label curLabel;
	for (i = 0; i < dialog.fields.length; i++) {
	    // set up the label and field
	    curPanel = new Panel();
	    curPanel.setLayout(new BorderLayout());
	    
	    if (0 == i) {
		curLabel = new Label(dialog.mField1Label);
	    }
	    else {
		curLabel = new Label(dialog.mField2Label);
	    }
	    curLabel.setBackground(Color.lightGray);
	    curPanel.add(curLabel, BorderLayout.WEST);

	    curPanel.add(dialog.fields[i], BorderLayout.CENTER);
	    if (0 == i) {
		fieldPanel.add(curPanel, BorderLayout.NORTH);
	    }
	    else {
		fieldPanel.add(curPanel, BorderLayout.CENTER);
	    }
	}
	centerPanel.add(fieldPanel, BorderLayout.NORTH);
    }
    
    if (null != dialog.mCheckboxMsg) {
	dialog.mCheckbox = new Checkbox(dialog.mCheckboxMsg);
	dialog.mCheckbox.setBackground(Color.lightGray);
	centerPanel.add(dialog.mCheckbox, BorderLayout.CENTER);
    }
    // add the center panel to the main panel
    panel.add(centerPanel, BorderLayout.CENTER);
}
    
}

