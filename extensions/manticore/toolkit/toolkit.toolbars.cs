/* -*- Mode: C#; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is Manticore.
 * 
 * The Initial Developer of the Original Code is
 * Silverstone Interactive. Portions created by Silverstone Interactive are
 * Copyright (C) 2001 Silverstone Interactive. 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Ben Goodger <ben@netscape.com>
 *
 */

namespace Silverstone.Manticore.Toolkit.Toolbars
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;

  using System.Collections;

  using System.IO;
  using System.Xml;

  public class MToolbox : ContainerControl
  {
    
  }

  public class MToolstrip : ContainerControl
  {

  }

  public class MToolbar : ContainerControl
  {

  }

  public class MToolbarButton : ToolbarButton
  {

  }

  public class ToolbarBuilder
  {
    protected internal String toolbarDefinitionFile;

    public MToolbox toolbox;
    public Hashtable items;

    public ToolbarBuilder(String file)
    {
      toolbarDefinitionFile = file;
      toolbox = new MToolbox();
      items = new Hashtable();
    }

    public void Build()
    {
      XmlTextReader reader;
      reader = new XmlTextReader(toolbarDefinitionFile);
      
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();

      Recurse(reader, toolbox);
    }

    protected void Recurse(XmlTextReader reader, Menu root)
    {
      String inner = reader.ReadInnerXml();
    
      NameTable nt = new NameTable();
      XmlNamespaceManager nsmgr = new XmlNamespaceManager(nt);
      XmlParserContext ctxt = new XmlParserContext(null, nsmgr, null, XmlSpace.None);
      XmlTextReader reader2 = new XmlTextReader(inner, XmlNodeType.Element, ctxt);

      MenuItem menuitem;

      while (reader2.Read()) {
        if (reader2.NodeType == XmlNodeType.Element) {
          switch (reader2.LocalName) {
          case "menu":
            // Menuitem. Find the name, accesskey, command and id strings
            String[] values = new String[3] {"", "", ""};
            String[] names = new String[3] {"label", "accesskey", "command"};
            for (Byte i = 0; i < names.Length; ++i) {
              if (reader2.MoveToAttribute(names[i]) &&
                  reader2.ReadAttributeValue())
                values[i] = reader2.Value; // XXX need to handle entities
              reader2.MoveToElement();
            }

            // Handle Accesskey
            int idx = values[0].ToLower().IndexOf(values[1].ToLower());
            if (idx != -1)
              values[0] = values[0].Insert(idx, "&");
            else 
              values[0] += " (&" + values[1].ToUpper() + ")";

            // Create menu item and attach an event handler
            menuitem = new CommandMenuItem(values[0], 
                                           new EventHandler(OnCommand),
                                           values[2]);
            if (values[2] != "")
              items.Add(values[2], menuitem);
            root.MenuItems.Add(menuitem);
            Recurse(reader2, menuitem);
            break;
          case "menuseparator":
            menuitem = new MenuItem("-");
            root.MenuItems.Add(menuitem);
            break;
          }
        }
      }
    }

    public virtual void OnCommand(Object sender, EventArgs e)
    { 
      // Implement in derived classes
    }


  }
}

