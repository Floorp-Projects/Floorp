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

import org.mozilla.translator.kernel.*;

/** 
 *
 * @author  Henrik
 * @version 
 */
public class Translation extends MozTreeNode{

  public static final int QA_NOTSEEN=0;
  public static final int QA_SKIPPED=1;
  public static final int QA_ERROR=2;
  public static final int QA_CHANGE=3;
  public static final int QA_ACCEPTED=4;
  public static final int QA_PERFECT=5;
  public static final int QA_OTHER=6;

  private String text;
  private int status;
  private String comment;
  
  
  /** Creates new Translation */
  public Translation(String n,MozTreeNode p,String t)  
  {
    super (n,p);
    this.text=t;
    this.status=QA_NOTSEEN;
    this.comment="";
  }
  
  public Translation(String n,MozTreeNode p,String t,int s,String c)
  {
    super(n,p);
    this.text=t;
    this.status=s;
    this.comment=c;
  }
  
  
  public String getText()
  {
    return text;
  }
  
  public int getStatus()
  {
    return status;
  }
  
  public String getComment()
  {
    return comment;
  }
  
 
  public void setText(String t)
  {
    this.text=t;
  }
  
  public void setStatus(int s)
  {
    this.status=s;
  }
  
  public void setComment(String comment)
  {
    this.comment=comment;
  }
  
}