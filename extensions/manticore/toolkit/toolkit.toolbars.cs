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

  // We want to replace use of Coolbar with our own widget, just
  // not right now.
  using stdole;
  using AxComCtl3;
  using MSDATASRC;
  using StdFormat;
  using VBRUN;
  //using ComCtl3;
  
  public class ToolbarBuilder
  {
    private String mToolbarFile;

    public AxCoolBar mToolbar;
    public Hashtable mItems;

    public ToolbarBuilder(String file)
    {
      mToolbarFile = file;
      mToolbar = new AxCoolBar();
      mItems = new Hashtable();
    }

    public void Build()
    {
      XmlTextReader reader;
      reader = new XmlTextReader(mToolbarFile);
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();

//      NameTable nt = new NameTable();
//      XmlNamespaceManager nsmgr = new XmlNamespaceManager(nt);
//      XmlParserContext ctxt = new XmlParserContext(null, nsmgr, null, XmlSpace.None);
//      XmlTextReader reader = new XmlTextReader(inner, XmlNodeType.Element, ctxt);

      int bandIndex = 0;
      Boolean needNewRow = true;
      while (reader.Read()) {
        if (reader.NodeType == XmlNodeType.Element) {
          switch (reader.LocalName) {
          case "toolstrip":
            // The next <toolbar/> we encounter should be created on a new row. 
            needNewRow = true;
            break;
          case "toolbar":
            // 
            String[] tbvalues = new String[4] {"", "", "",  ""};
            String[] tbnames = new String[4] {"id", "label", "description", "visible"};
            for (Byte i = 0; i < tbnames.Length; ++i) {
              if (reader.MoveToAttribute(tbnames[i]) &&
                  reader.ReadAttributeValue())
                tbvalues[i] = reader.Value; // XXX need to handle entities
              reader.MoveToElement();
            }

            String key = tbvalues[0];
            String label = tbvalues[1];
            String visible = tbvalues[3];
            Console.WriteLine(mToolbar.Bands);
            mToolbar.Bands.Add(bandIndex++, key, label, null, needNewRow, null, visible != "false");
            needNewRow = false;
            break;
          case "toolbarbutton":

            String[] tbbvalues = new String[2] {"", ""};
            String[] tbbnames = new String[2] {"label", "command"};
            for (Byte i = 0; i < tbbnames.Length; ++i) {
              if (reader.MoveToAttribute(tbbnames[i]) &&
                  reader.ReadAttributeValue())
                tbbvalues[i] = reader.Value; // XXX need to handle entities
              reader.MoveToElement();
            }
            Console.WriteLine(tbbvalues[0]);
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

