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
package org.mozilla.translator.kernel;

import java.io.*;

/** This class is used to log errors
 * @author Henrik Lynggaard Hansen
 * @version 4.01
 */
public class Log extends Object {

  private static  boolean toScr;
  private static boolean toFile;
  private static String fileName;
  
/** This inits the log and readies  it for use
 *
 */
  public static void init()
  {
    toScr    = Settings.getBoolean("Log.toScreen",false);
    toFile   = Settings.getBoolean("Log.toFile",false);
    fileName = Settings.getString("Log.fileName","");
  }
  
/** This writes a piece of text to the log
 * @param text The text to be logged
 */
  public static void write(String text)
  {
    FileWriter fw;
    PrintWriter pw;
    
    if (toScr)
    {
      System.out.println(text);
    }
    if (toFile)
    {
      try
      {
        fw = new FileWriter(fileName,true);
        pw = new PrintWriter(fw);
        
        pw.println(text);
        pw.close();
      }
      catch (Exception e)
      {
        System.out.println("Error writing to log file");
        System.out.println("The message was:\n");
      }
    }
  }    
}