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
import java.util.*;

/** This class wraps the java.util.Properties, with handy mehtods
 * and a singelton pattern
 * @author Henrik Lynggaard Hansen
 * @version 4.0
 */
public class Settings extends Object {

    private static Properties prop;
    private static String fileName;

    /** This method will init the settings with default values, and then load in the
     * user settings from the file.
     * @param fn The filename for the properties file
     */
    public static void init(String fn)
    {
        FileInputStream fis;  // used to load the propeties from file
        DefaultSettings def;  // the set of system defaults

        fileName=fn;
        def = new DefaultSettings();
        prop = new Properties(def.getDefaults());

        try
        {
            fis = new FileInputStream(fileName);
            prop.load(fis);
            fis.close();
        }
        catch (Exception e)
        {
            // Silently ignore that we have no properties
        }
    }

    /** This set a settings to a string value
     * @param key The key of the property
     * @param value The new value of the property
     */
    public static void setString(String key,String value)
    {
        prop.setProperty(key,value);
    }
    public static void setBoolean(String key,boolean value)
    {
        String strValue = String.valueOf(value);
        prop.setProperty(key,strValue);
    }

    public static void setInteger(String key,int value)
    {
        String strValue = Integer.toString(value);
        prop.setProperty(key,strValue);
    }



    public static String getString(String key,String defValue)
    {
        return prop.getProperty(key, defValue);
    }

    public static boolean getBoolean(String key,boolean defValue)
    {
        boolean result;
        String tempResult;

        tempResult = prop.getProperty(key, String.valueOf(defValue));

        result = Boolean.valueOf(tempResult).booleanValue();
        return result;
    }

    public static int getInteger(String key, int defValue)
    {
        String tempResult;
        
        tempResult = prop.getProperty(key,String.valueOf(defValue));
        return Integer.parseInt(tempResult);
    }


    public static void save()
    {
        FileOutputStream fos; // used to save the properties

        try
        {
            fos = new FileOutputStream(fileName);
            prop.store(fos,"Settings for mozillaTranslator");
            fos.close();
        }
        catch (Exception e)
        {
            Log.write("Error saving property file");
        }
    }
}