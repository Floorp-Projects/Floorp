/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Giao Nguyen <grail@cafebabe.org>, 13 Jan 1999.
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
  protected Properties properties;

  /**
   * Reference point into the CLASSPATH for locating the XML file.
   */
  protected Class ref;

  /**
   * Process the node.
   * 
   * @param node the node tho be processed
   * @param component the parent component that the node will be added to 
   * (if applicable)
   */
  protected abstract void processNode(Node node, JComponent component);

  /**
   * Set the element as the item containing configuration for the 
   * builder. This would usually be the link tag in the head.
   *
   * @param config the element containing configuration data
   */
  public void setConfiguration(Element config) {
    try {
      URL linkURL;
      // get the string properties
      if (config.getAttribute("href") != null 
	  && config.getAttribute("role").equals("stringprops")
	  && config.getTagName().equals("link")) {
	linkURL = ref.getResource(config.getAttribute("href"));
	properties = new Properties();
	if (linkURL != null) properties.load(linkURL.openStream());
      }
    } catch (IOException io) {
      io.printStackTrace();
    }
  }

  public String getReferencedLabel(Element current, String attr) {
    String label = current.getAttribute(attr);
    
    if (properties == null) return label;
    
    // if it starts with a '$' we crossreference to properties
    if (label != null && label.charAt(0) == '$') {
      String key = label.substring(1);
      label = properties.getProperty(key, label);
    }
    
    return label;
  }
}
