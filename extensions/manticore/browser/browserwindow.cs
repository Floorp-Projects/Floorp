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

namespace Silverstone.Manticore.BrowserWindow
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;

  using Silverstone.Manticore.App;
  using Silverstone.Manticore.Toolkit.Menus;
  using Silverstone.Manticore.Toolkit.Toolbars;
  using Silverstone.Manticore.AboutDialog;
  using Silverstone.Manticore.OpenDialog;

  using Silverstone.Manticore.LayoutAbstraction;

  public class BrowserWindow : System.Windows.Forms.Form 
  {
    private System.ComponentModel.Container components;

    private BrowserMenuBuilder mMenuBuilder;
    private BrowserToolbarBuilder mToolbarBuilder;

    protected internal WebBrowser webBrowser;

    protected internal StatusBar statusBar;

    protected internal ManticoreApp application;

    public BrowserWindow(ManticoreApp app)
    {
      application = app;

      Console.WriteLine("init component");
      // Set up UI
      InitializeComponent();
    }

    public override void Dispose()
    {
      base.Dispose();
      components.Dispose();
    }

    private void InitializeComponent()
    {
      this.components = new System.ComponentModel.Container();

      // XXX read these from a settings file
      this.Width = 640;
      this.Height = 480;
      
      this.Text = "Manticore"; // XXX localize

      mMenuBuilder = new BrowserMenuBuilder("browser\\browser-menu.xml", this);
      mMenuBuilder.Build();
      this.Menu = mMenuBuilder.mainMenu;
      Console.WriteLine("foopy");

      mToolbarBuilder = new BrowserToolbarBuilder("browser\\browser-toolbar.xml", this);
//      mToolbarBuilder.Build();
//      this.Controls.Add(mToolbarBuilder.mToolbar);
//    mToolbarBuilder.mToolbar.Dock = DockStyle.Fill;
      
      // Show the resize handle
      this.SizeGripStyle = SizeGripStyle.Auto;

      webBrowser = new WebBrowser();
      this.Controls.Add(webBrowser);

      // Set up the Status Bar
      statusBar = new StatusBar();
      
      StatusBarPanel docStatePanel = new StatusBarPanel();
      StatusBarPanel statusPanel = new StatusBarPanel();
      StatusBarPanel progressPanel = new StatusBarPanel();
      StatusBarPanel zonePanel = new StatusBarPanel();

      docStatePanel.Text = "X";
      progressPanel.Text = "[|||||         ]";
      zonePanel.Text = "Internet Region";
      statusPanel.Text = "Document Done";
      statusPanel.AutoSize = StatusBarPanelAutoSize.Spring;
      
      statusBar.Panels.AddRange(new StatusBarPanel[] {docStatePanel, statusPanel, progressPanel, zonePanel});
      statusBar.ShowPanels = true;

      this.Controls.Add(statusBar);
    }

    private void LayoutStartup()
    {
      // XXX - add a pref to control this, blank, or last page visited.
      // Visit the homepage
      String homepageURL = "http://www.silverstone.net.nz/";
      webBrowser.LoadURL(homepageURL, false);
    }

    ///////////////////////////////////////////////////////////////////////////
    // Menu Command Handlers
    public void OpenNewBrowser()
    {
      application.OpenNewBrowser();
    }

    public void Open()
    {
      OpenDialog dlg = new OpenDialog();
      if (dlg.ShowDialog() == DialogResult.OK)
        webBrowser.LoadURL(dlg.URL, false);
    }
  }

  public class BrowserMenuBuilder : MenuBuilder
  {
    private BrowserWindow browserWindow;

    public BrowserMenuBuilder(String file, BrowserWindow window) : base(file)
    {
      browserWindow = window;
    }

    public override void OnCommand(Object sender, EventArgs e)
    {
      CommandMenuItem menuitem = (CommandMenuItem) sender;
      Console.WriteLine(menuitem.Command);
      switch (menuitem.Command) {
      case "file-new-window":
        browserWindow.OpenNewBrowser();
        break;
      case "file-open":
        browserWindow.Open();
        break;
      case "view-go-home":
        browserWindow.webBrowser.GoHome();
        break;
      case "view-layout-gecko":
        browserWindow.webBrowser.SwitchLayoutEngine("gecko");
        break;
      case "view-layout-ie":
        browserWindow.webBrowser.SwitchLayoutEngine("trident");
        break;
      case "help-about":
        AboutDialog dlg = new AboutDialog(browserWindow);
        break;
      }
    }
  }

  public class BrowserToolbarBuilder : ToolbarBuilder
  {
    private BrowserWindow mBrowserWindow;

    public BrowserToolbarBuilder(String file, BrowserWindow window) : base(file)
    {
      mBrowserWindow = window;
    }

    public override void OnCommand(Object sender, EventArgs e)
    {
    }
  }
}


