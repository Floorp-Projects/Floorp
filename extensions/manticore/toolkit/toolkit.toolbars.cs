/* -*- Mode: C#; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Manticore.
 *
 * The Initial Developer of the Original Code is
 * Silverstone Interactive.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com>
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


namespace Silverstone.Manticore.Toolkit
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;

  using System.Collections;

  using System.IO;
  using System.Xml;

  using AxComCtl3;
  using ComCtl3;

  public abstract class ToolbarBuilder
  {
    protected String mToolbarFile;
    protected AxCoolBar mCoolBar;
    protected Form mForm;

    public Hashtable mItems;

    // XXX band-set hack 
    protected ToolBar mToolBar;
    public Rectangle Bounds
    {
      get 
      {
        return mToolBar.Bounds;
      }
    }
   
    public ToolbarBuilder(String aToolbarFile, Form aForm)
    {
      mToolbarFile = aToolbarFile;
      mForm = aForm;
      mItems = new Hashtable();


      // XXX band-set hack 
      mToolBar = new ToolBar();
      mForm.Controls.Add(mToolBar);

      /*
      // Initialize CoolBar
      mCoolBar = new AxCoolBar();
      AxHost host = mCoolBar as AxHost;
      host.BeginInit();
      host.Dock = DockStyle.Top;
      host.EndInit();
      mForm.Controls.Add(host);
      */

      // We can't build the CoolBar until after the window is visible
      mForm.VisibleChanged += new EventHandler(Build);
    }

    public void Build(Object sender, EventArgs e)
    {
      XmlTextReader reader;
      reader = new XmlTextReader(mToolbarFile);
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();

      bool shouldBuildNewRow = true;

      while (reader.Read()) 
      {
        if (reader.NodeType == XmlNodeType.Element) 
        {
          switch (reader.LocalName) 
          {
            case "toolstrip":
              // The next <toolbar/> we encounter should be created on a new row. 
              shouldBuildNewRow = true;
              break;
            case "toolbar":
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
              bool visible = tbvalues[3] == "true";
      
              // Create and add a new toolbar.
              mToolBar.Appearance = ToolBarAppearance.Flat;
              mToolBar.ButtonClick += new ToolBarButtonClickEventHandler(this.OnCommand);
              mForm.Controls.Add(mToolBar);

              //mCoolBar.Bands.Add(-1, key, label, new Object(), true, mToolBar, true);

              shouldBuildNewRow = false;
              break;
            case "toolbarseparator": 
            {
              if (mToolBar != null) 
              {
                ToolBarButton button = new ToolBarButton();
                button.Style = ToolBarButtonStyle.Separator;
                mToolBar.Buttons.Add(button);
              }
              break;
            }
            case "toolbarbutton":
            {
              if (mToolBar != null) 
              {
                String[] tbbvalues = new String[3] {"", "", ""};
                String[] tbbnames = new String[3] {"label", "icon", "command"};
                for (int i = 0; i < tbbnames.Length; ++i) 
                {
                  if (reader.MoveToAttribute(tbbnames[i]) &&
                    reader.ReadAttributeValue())
                    tbbvalues[i] = reader.Value; // XXX need to handle entities
                  reader.MoveToElement();
                }
         
                ToolBarButton button = new CommandButtonItem(tbbvalues[1]);
                button.Text = tbbvalues[0];
                mToolBar.Buttons.Add(button);
              }
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