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

import java.util.*;

/** This class contains the default Settings for MozillaTranslator.
 *
 * @author Henrik Lynggaard Hansen
 * @version 4.15
 */
public class DefaultSettings extends Object {

  private Properties prop;
  
  /** Creates new DefaultSettings */
  public DefaultSettings() 
  {
    prop = new Properties();
    
    // system settings
    prop.setProperty("System.Version","4.15");
    prop.setProperty("System.Groupware","false"); // to be used later
    prop.setProperty("System.Glossaryfile","glossary.zip");
    prop.setProperty("System.Filterfile","Filters.properties");

    //log settings
    prop.setProperty("Log.toScreen","true");
    prop.setProperty("Log.toFile","true");
    prop.setProperty("Log.fileName","MozillaTranslator.errors");

    // translation related settings
    prop.setProperty("Locale.dtd_licence","mpl_dtd.txt");
    prop.setProperty("Locale.properties_licence","mpl_properties.txt");
    
    
    // saved properties, not really needed here, it it just documentation    
    // saved.localeName 
    // saved.install
    // saved.author
    // saved.displayName
    // saved.previewURL
    // saved.version
    // saved.fileName.xpi
    // saved.dirName.package
    // saved.fileName.partial.import
    // saved.fileName.partial.export
    // saved.search.text
    // saved.search.rule
    // saved.search.field
    // saved.column.<XXX>
    // saved.subs.<XXX> (to be done later)
    
    
  }

/** Returns the default properties.
 * @return The Default properties
 */
  public Properties getDefaults()
  {
    return prop;
  }
  
}