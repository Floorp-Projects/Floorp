/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Giao Nguyen
 * <grail@cafebabe.org>.  Portions created by Giao Nguyen are
 * Copyright (C) 1999 Giao Nguyen. All
 * Rights Reserved.
 *
 * Contributor(s): Morgan Schweers <morgan@vixen.com>
 */

package grendel.ui;


import java.util.Properties;

import java.net.URL;

import java.io.IOException;

import java.awt.Container;

import javax.swing.JComponent;

import org.w3c.dom.Node;
import org.w3c.dom.Element;

public abstract class XMLWidgetBuilder {
  /**
   * The properties bundle that the builder will reference to for 
   * things like getReferencedLabel.
   */
  protected Properties properties = new Properties();

  /**
   * Reference point into the CLASSPATH for locating the XML file.
   */
  protected Class ref;

  /**
   * Set the reference point for URL location.
   * 
   * @param ref the reference point for local urls to be loaded from.
   */
  public void setReference(Class ref) {
    this.ref = ref;
  }

  /**
   * Set the element as the item containing configuration for the 
   * builder. This would usually be the link tag in the head.
   *
   * @param config the element containing configuration data
   */
  public void setConfiguration(Element config) {
    try {
      URL linkURL;
      Class local = ((ref == null) ? getClass() : ref);;
      // get the string properties
      if (config.getAttribute("href").length() != 0
	  && config.getAttribute("role").equals("stringprops")
	  && config.getTagName().equals("link")) {

        // this should never return null
	linkURL = local.getResource(config.getAttribute("href"));
	properties.load(linkURL.openStream());
      }
    } catch (IOException io) {
      io.printStackTrace();
    }
  }

  /**
   * Get a label referenced by the string properties file.
   * @param current the element to process
   * @param attr the attribute to look up
   * @return the string as post lookup, or the string pre lookup if 
   * the lookup failed
   */
  public String getReferencedLabel(Element current, String attr) {
    String label = current.getAttribute(attr);
    
    // if it starts with a '$' we crossreference to properties
    if (label != null && label.length() > 0 
        && label.charAt(0) == '$') {
      String key = label.substring(1);
      label = properties.getProperty(key, label);
    }
    
    return label;
  }
}
