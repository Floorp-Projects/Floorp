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

namespace Silverstone.Manticore.Toolkit
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;

  using System.Collections;

  using System.IO;
  using System.Xml;

  public abstract class ToolbarBuilder
  {
    private String mToolbarFile;
    private ToolBar mCurrentToolbar;

    protected Form mForm;

    public Hashtable mItems;
   
    public ToolbarBuilder(String file, Form form)
    {
      mToolbarFile = file;
      mForm = form;
      mItems = new Hashtable();
    }

    public void Build()
    {
      XmlTextReader reader;
      reader = new XmlTextReader(mToolbarFile);
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();

      while (reader.Read()) 
      {
        if (reader.NodeType == XmlNodeType.Element) 
        {
          switch (reader.LocalName) 
          {
            case "toolstrip":
              // The next <toolbar/> we encounter should be created on a new row. 
              break;
            case "toolbar":
              // 
              String[] tbvalues = new String[4] {"", "", "",  ""};
              String[] tbnames = new String[4] {"id", "label", "description", "visible"};
              for (int i = 0; i < tbnames.Length; ++i) 
              {
                if (reader.MoveToAttribute(tbnames[i]) &&
                  reader.ReadAttributeValue())
                  tbvalues[i] = reader.Value; // XXX need to handle entities
                reader.MoveToElement();
              }

              String key = tbvalues[0];
              String label = tbvalues[1];
              String visible = tbvalues[3];
      
              // Create and add a new toolbar.
              mCurrentToolbar = new ToolBar();
              mCurrentToolbar.Dock = DockStyle.Top;
              mCurrentToolbar.Appearance = ToolBarAppearance.Flat;
              mForm.Controls.Add(mCurrentToolbar);

              mCurrentToolbar.ButtonClick += new ToolBarButtonClickEventHandler(this.OnCommand);
              break;
            case "toolbarseparator": 
            {
              ToolBarButton button = new ToolBarButton();
              button.Style = ToolBarButtonStyle.Separator;
              mCurrentToolbar.Buttons.Add(button);
              break;
            }
            case "toolbarbutton":
            {
              String[] tbbvalues = new String[2] {"", ""};
              String[] tbbnames = new String[2] {"label", "command"};
              for (int i = 0; i < tbbnames.Length; ++i) 
              {
                if (reader.MoveToAttribute(tbbnames[i]) &&
                  reader.ReadAttributeValue())
                  tbbvalues[i] = reader.Value; // XXX need to handle entities
                reader.MoveToElement();
              }
       
              ToolBarButton button = new CommandButtonItem(tbbvalues[1]);
              button.Text = tbbvalues[0];
              mCurrentToolbar.Buttons.Add(button);
              break;
            }
          }
        }
      }
    }

    public abstract void OnCommand(Object sender, ToolBarButtonClickEventArgs e);
  }

  public class CommandButtonItem : ToolBarButton
  {
    private string mCommand;
    public string Command 
    {
      get 
      {
        return mCommand;
      }
    }

    public CommandButtonItem(String cmd) : base()
    {
      mCommand = cmd;
    }
  }
}