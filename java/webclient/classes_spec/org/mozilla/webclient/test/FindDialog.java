/** Ashu -- this is the Find Dialog Class 
 instantiated within the EMWindow class, this
 will act as the Modal dialog for calling Find/Find Next 
 functions for CurrentPage
*/

package org.mozilla.webclient.test;

import java.awt.*;
import java.awt.event.*;

public class FindDialog extends WorkDialog implements ActionListener, ItemListener{
	static private int	_defaultTextFieldSize = 40;
	private Button		findButton;
	private Button		clearButton;
	private Button		closeButton;
        private Checkbox        backwardsCheckBox;
        private Checkbox        matchcaseCheckBox;
	private String		question;
	private TextField	textField;
	private boolean		wasClosed;
        private boolean         wasCleared;
        public  boolean         backwards;
        public  boolean         matchcase;
	private ButtonPanel	buttonPanel = new ButtonPanel();
	
	
	public FindDialog(Frame frame, DialogClient client, String title, 
			      String question, String initialResponse) {
		this(frame, client, title, question, initialResponse, 
		     _defaultTextFieldSize, false);
	}

	public FindDialog(Frame frame, DialogClient client, String title, 
			      String question) {
		this(frame, client, title, question, null, 
		     _defaultTextFieldSize, false);
	}

	public FindDialog(Frame frame, DialogClient client, String title, 
			      String question, int textFieldSize) {
		this(frame, client, title, question, null, 
		     textFieldSize, false);
	}

	public FindDialog(Frame frame, DialogClient client, String title, 
			      String question, String initialResponse, int textFieldSize) {
	       	this(frame, client, title, question, initialResponse, 
		     textFieldSize, false);
	}

	public FindDialog(Frame frame, DialogClient client, String title, 
			      String question, String initialResponse, 
			      int textFieldSize, boolean modal) {
		super(frame, client, title, modal);
		QuestionPanel questionPanel;
		findButton = addButton("Find");
		clearButton = addButton("Clear");
		closeButton = addButton("Close");
		backwardsCheckBox = addCheckBox("Backwards");
		matchcaseCheckBox = addCheckBox("Matchcase");

		findButton.addActionListener(this);
		clearButton.addActionListener(this);
		closeButton.addActionListener(this);
		backwardsCheckBox.addItemListener(this);
		matchcaseCheckBox.addItemListener(this);

		questionPanel = new QuestionPanel(this, question, initialResponse, 
						  textFieldSize);
		textField = questionPanel.getTextField();
		setWorkPanel(questionPanel);
	}

	public void actionPerformed(ActionEvent ae) {
	  if(ae.getSource() == closeButton) {
	    wasClosed = true;
	    wasCleared = false;
	    dispose(true);
	  }
	  else if(ae.getSource() == clearButton) {
	    wasCleared = true;
	    wasClosed = false;
	    setTextField("");
	    dispose(false);
	  }
	  else if(ae.getSource() == findButton) {
	    wasCleared = false;
	    wasClosed = false;
	    dispose(false);
	  }
	  /*	  else if(ae.getSource() == backwardsCheckBox) {
	    if(backwardsCheckBox.getState())
	      backwards = true;
	    else 
	      backwards = false;
	  }
	  else if(ae.getSource() == matchcaseCheckBox) {
	    if(matchcaseCheckBox.getState())
	      matchcase = true;
	    else 
	      matchcase = false;
	  } */
	}


  public void itemStateChanged(ItemEvent e) {
    if(e.getSource() == backwardsCheckBox) {
      if(backwardsCheckBox.getState())
	backwards = true;
      else 
	backwards = false;
    }
    else if(e.getSource() ==  matchcaseCheckBox) {
      if(matchcaseCheckBox.getState())
	matchcase = true;
      else 
	matchcase = false;
    }
  }

	public void setVisible(boolean b) {
		textField.requestFocus();
		super.setVisible(b);
	}

	public void returnInTextField() {
		findButton.requestFocus();
	}

	public TextField getTextField() {
		return textField;
	}

        public void setTextField(String string) {
	  //	       questionPanel.setTextField(string);
	       textField.setText(string);
	}

	public String getAnswer() {
		return textField.getText();
	}

	public boolean wasClosed() {
		return wasClosed;
	}

        public boolean wasCleared() {
		return wasCleared;
	}

	private void setQuestion(String question) {
		this.question = question;
	}
}


class QuestionPanel extends Postcard {
	private TextField	field;
	private FindDialog	dialog;
	
	public QuestionPanel(FindDialog dialog, String question) {
		this(dialog, question, null, 0);
	}

	public QuestionPanel(FindDialog dialog, String question, int columns) {
		this(dialog, question, null, columns);
	}

	public QuestionPanel(FindDialog myDialog, String question, String initialResponse, int cols) {
		super(new Panel());
		Panel panel = getPanel();
		this.dialog = myDialog;
		panel.setLayout(new GridLayout());
		panel.add(new Label(question));

		if(initialResponse != null) {
			if(cols != 0)
				panel.add(field = new TextField(initialResponse, cols));
			else
				panel.add(field = new TextField(initialResponse));
		}
		else {
			if(cols != 0) panel.add(field = new TextField(cols));
			else		panel.add(field = new TextField());
		}

		field.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent event) {
				dialog.returnInTextField();
			}
		} );
	}

	public TextField getTextField() {
		return field;
	}

        public void setTextField(String string) {
	        field.setText(string);
	}

}
	
