
package org.mozilla.webclient.test;

import java.util.Properties;

public class UniversalDialogData
{
public String mDialogTitle;
public String mTitleMsg;
public String mText;
public String mCheckboxMsg;
public String mButtonText0;
public String mButtonText1;
public String mButtonText2;
public String mField1Label;
public String mField2Label;
public boolean mField1IsPasswd;
public int mNumButtons;
public int mNumEditfields;
public Properties mProps;

public UniversalDialogData(String titleMessage,
                           String dialogTitle,
                           String text,
                           String checkboxMsg,
                           String button0Text,
                           String button1Text,
                           String button2Text,
                           String editfield1Msg,
                           String editfield2Msg,
                           int numButtons,
                           int numEditfields,
                           boolean editfield1Password,
                           Properties props)
{
    mDialogTitle = dialogTitle;
    mTitleMsg = titleMessage;
    mText = text;
    mCheckboxMsg = checkboxMsg;
    mButtonText0 = button0Text;
    mButtonText1 = button1Text;
    mButtonText2 = button2Text;
    mField1Label = editfield1Msg;
    mField2Label = editfield2Msg;
    mField1IsPasswd = editfield1Password;
    mNumButtons = numButtons;
    mNumEditfields = numEditfields;
    mProps = props;
}

}
