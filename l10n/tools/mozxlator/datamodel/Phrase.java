/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/

 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MozillaTranslator (Mozilla Localization Tool)
 *
 * The Initial Developer of the Original Code is Henrik Lynggaard Hansen
 *
 * Portions created by Henrik Lynggard Hansen are
 * Copyright (C) Henrik Lynggaard Hansen.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Henrik Lynggaard Hansen (Initial Code)
 *
 */
package org.mozilla.translator.datamodel;

import java.util.*;
import javax.swing.*;

/** 
 *
 * @author  Henrik Lynggaard
 * @version 4.0
 */
public class Phrase extends MozTreeNode {

  
  private String text;
  private String note;
  private boolean keepOriginal;
  private Phrase accessConnection;
  private Phrase commandConnection;
  
  /** Creates new Phrase */
  public Phrase(String n,MozTreeNode p,String original,String note,boolean keepOriginal)  
  {
    super(n,p);
    this.text=original;
    this.note=note;
    this.keepOriginal=keepOriginal;

    this.accessConnection=null;
    this.commandConnection=null;
  }
  
    
  public String getText()
  {
    return text;
  }
  
  public String getNote()
  {
    return note;
  }
  
  public boolean getKeepOriginal()
  {
    return keepOriginal;
  }
  
  
  public Phrase getAccessConnection()
  {
    return accessConnection;
  }
  
  public Phrase getCommandConnection()
  {
    return commandConnection;
  }
  
  public void setText(String text)
  {
    this.text=text;
  }
  
  public void setNote(String note)
  {
    this.note=note;
  }
  
  public void setKeepOriginal(boolean flag)
  {
    this.keepOriginal=flag;
  }
  
  public void setAccessConnection(Phrase phrase)
  {
    this.accessConnection=phrase;
  }
  
  public void setCommandConnection(Phrase phrase)
  {
    this.commandConnection=phrase;
  }
  
}