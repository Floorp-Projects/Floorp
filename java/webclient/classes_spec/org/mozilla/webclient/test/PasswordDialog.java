/** Ashu -- this is the Find Dialog Class 
 instantiated within the EMWindow class, this
 will act as the Modal dialog for calling Find/Find Next 
 functions for CurrentPage
*/

package org.mozilla.webclient.test;

import java.awt.*;
import java.awt.event.*;

import org.mozilla.webclient.Prompt;

import java.util.Properties;

public class PasswordDialog extends WorkDialog implements ActionListener, ItemListener{
	static private int	_defaultTextFieldSize = 20;
	private Button		okButton;
	private Button		cancelButton;
	private String		realm;
	private TextField	userField;
	private TextField	passField;
	private boolean		wasOk;
        private boolean         wasCanceled;
	private ButtonPanel	buttonPanel = new ButtonPanel();
    private Properties props;
	
	
    public PasswordDialog(Frame frame, DialogClient client, String title, 
			  String text, String realm, 
			  int userFieldSize, boolean modal, 
			  Properties passwordProperties) {
	super(frame, client, title, modal);
	RealmPanel realmPanel;
	okButton = addButton("Ok");
	cancelButton = addButton("Cancel");
	props = passwordProperties;
	
	okButton.addActionListener(this);
	cancelButton.addActionListener(this);
	
	realmPanel = new RealmPanel(this, text, realm, 
				    userFieldSize);
	userField = realmPanel.getUserField();
	passField = realmPanel.getPassField();
	setWorkPanel(realmPanel);
    }
    
	public void actionPerformed(ActionEvent ae) {
	  if(ae.getSource() == cancelButton) {
	    wasOk = false;
	    wasCanceled = true;
	    dispose(true);
	  }
	  else if(ae.getSource() == okButton) {
	    wasCanceled = false;
	    wasOk = true;
	    props.setProperty(Prompt.USER_NAME_KEY, userField.getText());
	    props.setProperty(Prompt.PASSWORD_KEY, passField.getText());
	    setUserField("");
	    setPassField("");
	    dispose(true);
	  }
	}


  public void itemStateChanged(ItemEvent e) {
  }
    
    public void setVisible(boolean b) {
	userField.requestFocus();
	super.setVisible(b);
    }
    
    public void returnInTextField() {
	okButton.requestFocus();
    }
    
    public TextField getUserField() {
	return userField;
    }
    
    public void setUserField(String string) {
	//	       realmPanel.setUserField(string);
	userField.setText(string);
    }

    public void setPassField(String string) {
	passField.setText(string);
    }
    
    public String getAnswer() {
	return userField.getText();
    }
    
    public boolean wasOk() {
	return wasOk;
    }
    
    public boolean wasCanceled() {
	return wasCanceled;
    }
    
    private void setRealm(String realm) {
	this.realm = realm;
    }
}


class RealmPanel extends Postcard {
    private TextField	userField;
    private TextField	passField;
    private PasswordDialog	dialog;
    
public RealmPanel(PasswordDialog myDialog, String text, 
		  String realm, int cols) 
{
    super(new Panel());
    Panel panel = getPanel();
    this.dialog = myDialog;

    // set up the stuff on top of the text fields
        Panel northPanel = new Panel();
	northPanel.setLayout(new BorderLayout());
	Label textLabel = new Label(text);
	textLabel.setBackground(Color.lightGray);
	Label realmLabel = new Label("Realm: " + realm);
	realmLabel.setBackground(Color.lightGray);
	northPanel.add(textLabel, BorderLayout.NORTH);
	northPanel.add(realmLabel, BorderLayout.CENTER);
	
	panel.setLayout(new BorderLayout());
    panel.add(northPanel, BorderLayout.NORTH);

    // set up the text fields

        // set up the user name label and field
        Panel centerPanel = new Panel();
        centerPanel.setLayout(new BorderLayout());

	Panel userPanel = new Panel();
	userPanel.setLayout(new BorderLayout());
	Label nameLabel = new Label("User Name: ");
	nameLabel.setBackground(Color.lightGray);
	userPanel.add(nameLabel, BorderLayout.WEST);
	if (cols != 0) {
	    userField = new TextField("", cols);
	}
	else {
	    userField = new TextField();
	}
	userPanel.add(userField, BorderLayout.CENTER);
	centerPanel.add(userPanel, BorderLayout.NORTH);
	
	// set up the password label and field
	Panel passPanel = new Panel();
	passPanel.setLayout(new BorderLayout());
	Label passLabel = new Label("Password: ");
	passLabel.setBackground(Color.lightGray);
	passPanel.add(passLabel, BorderLayout.WEST);
	if (cols != 0) {
	    passField = new TextField("", cols);
	}
	else {
	    passField = new TextField();
	}
	passField.setEchoChar('*');
	passPanel.add(passField, BorderLayout.CENTER);
	centerPanel.add(passPanel, BorderLayout.CENTER);
    //
	
    // add the center panel to the main panel
    panel.add(centerPanel, BorderLayout.CENTER);
}
    
public TextField getUserField() 
{
    return userField;
}
    
    public void setUserField(String string) {
	userField.setText(string);
    }
    
    public TextField getPassField() {
	return passField;
    }
    
    public void setPassField(String string) {
	passField.setText(string);
    }
    
}

