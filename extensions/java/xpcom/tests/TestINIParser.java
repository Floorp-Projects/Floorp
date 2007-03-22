/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

import java.io.*;
import java.util.*;

import org.mozilla.xpcom.INIParser;


public class TestINIParser {

  /**
   * @param args path of INI file to parse, or <code>null</code> to parse
   *          a sample file
   * @throws IOException
   * @throws FileNotFoundException
   */
  public static void main(String[] args) throws FileNotFoundException,
          IOException {
    if (args.length > 1) {
      System.err.println("TestINIParser [file]");
      return;
    }

    INIParser parser = null;
    File iniFile = null;
    File tempFile = null;

    try {
      if (args.length == 1) {
        iniFile = new File(args[0]);
        parser = new INIParser(iniFile);
      } else {
        tempFile = File.createTempFile("testiniparser", null);
        createSampleINIFile(tempFile);
        parser = new INIParser(tempFile);
      }

      printValidSections(parser);
    } finally {
      if (tempFile != null) {
        tempFile.delete();
      }
    }
  }

  private static void createSampleINIFile(File aFile) throws IOException {
    BufferedWriter out = new BufferedWriter(new FileWriter(aFile));
    out.write("; first comment\n");
    out.write("; second comment\n");
    out.newLine();
    out.write("[good section 1]\n");
    out.write("param1=value1\n");
    out.write("param2=value2\n");
    out.newLine();
    out.write("[invalid section] 1\n");
    out.write("blah=blippity-blah\n");
    out.newLine();
    out.write("[good section 2]       \n");
    out.write("param3=value3\n");
    out.write("param4=value4\n");
    out.newLine();
    out.write("param5=value5\n");
    out.newLine();
    out.write("invalid pair\n");
    out.write("this shouldn't appear\n");
    out.write("   ; another comment\n");

    out.close();
  }

  private static void printValidSections(INIParser aParser) {
    Iterator sectionsIter = aParser.getSections();
    while (sectionsIter.hasNext()) {
      String sectionName = (String) sectionsIter.next();
      System.out.println("[" + sectionName + "]");

      Iterator keysIter = aParser.getKeys(sectionName);
      while (keysIter.hasNext()) {
        String key = (String) keysIter.next();
        String value = aParser.getString(sectionName, key);
        System.out.println(key + " = " + value);
      }

      System.out.println();
    }
  }

}

