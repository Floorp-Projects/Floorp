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

namespace Silverstone.Manticore.Browser
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;
  using System.Net;

  using Silverstone.Manticore.Core;
  using Silverstone.Manticore.App;
  using Silverstone.Manticore.Toolkit;
  using Silverstone.Manticore.Layout;
  using Silverstone.Manticore.Bookmarks;

  public class BrowserWindow : ManticoreWindow, IController
  {
    private MenuBuilder mMenuBuilder;
    private BrowserToolbarBuilder mToolbarBuilder;

    private LocationBar mLocationBar;

    private WebBrowser mWebBrowser;
    public WebBrowser WebBrowser
    {
      get 
      {
        return mWebBrowser;
      }
    }

    private StatusBar mStatusBar;
    private StatusBarPanel mProgressMeter;
    private StatusBarPanel mStatusPanel;

    private String mSessionURL = "";

    /// <summary>
    /// Webpage Title
    /// </summary>
    private String mTitle = "";

    public BrowserWindow()
    {
      Init();
    }

    public BrowserWindow(String aURL)
    {
      mSessionURL = aURL;

      Init();
    }

    protected void Init()
    {
      mType = "BrowserWindow";

      // Set up UI
      InitializeComponent();

      base.Init();
    }

    private void InitializeComponent()
    {
      // XXX read these from a settings file
      this.Width = 640;
      this.Height = 480;
      
      this.Text = "Manticore"; // XXX localize

      mMenuBuilder = new MenuBuilder("browser\\browser-menu.xml", this);
      mMenuBuilder.Build();
      mMenuBuilder.OnCommand += new EventHandler(OnMenuCommand);

      // Show the resize handle
      this.SizeGripStyle = SizeGripStyle.Auto;

      // Set up the Status Bar
      mStatusBar = new StatusBar();
      
      StatusBarPanel docStatePanel = new StatusBarPanel();
      mStatusPanel = new StatusBarPanel();
      mProgressMeter = new StatusBarPanel();
      StatusBarPanel zonePanel = new StatusBarPanel();

      docStatePanel.Text = "X";
      zonePanel.Text = "Internet Region";
      mStatusPanel.Text = "Document Done";
      mStatusPanel.AutoSize = StatusBarPanelAutoSize.Spring;
      

      mStatusBar.Panels.AddRange(new StatusBarPanel[] {docStatePanel, mStatusPanel, mProgressMeter, zonePanel});
      mStatusBar.ShowPanels = true;
      
      this.Controls.Add(mStatusBar);

      mToolbarBuilder = new BrowserToolbarBuilder("browser\\browser-toolbar.xml", this);

      mLocationBar = new LocationBar();
      mLocationBar.Top = mToolbarBuilder.Bounds.Top + mToolbarBuilder.Bounds.Height;
      mLocationBar.Left = 0;
      mLocationBar.Width = ClientRectangle.Width;
      mLocationBar.Anchor = AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Top;
      mLocationBar.LocationBarCommit += new LocationBar.LocationBarEventHandler(OnLocationCommit);
      mLocationBar.LocationBarModified += new LocationBar.LocationBarEventHandler(OnLocationModified);
      this.Controls.Add(mLocationBar);

      mWebBrowser = new WebBrowser(this);
      mWebBrowser.Dock = DockStyle.Bottom;
      mWebBrowser.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
      mWebBrowser.Top = mLocationBar.Top + mLocationBar.Height;
      mWebBrowser.Width = ClientRectangle.Width;
      mWebBrowser.Height = ClientRectangle.Height - mWebBrowser.Top - mStatusBar.Height;
      this.Controls.Add(mWebBrowser);

      // Start Page handler
      this.VisibleChanged += new EventHandler(LoadStartPage);
    }

    ///////////////////////////////////////////////////////////////////////////
    // URL Loading

    /// <summary>
    /// The currently loaded document's URL.
    /// </summary>
    public String URL 
    {
      get {
        return mWebBrowser.URL;
      }
    }

    public void LoadURL(String aURL)
    {
      mUpdatedURLBar = false;
      mWebBrowser.LoadURL(aURL, false);
    }

    protected bool mShouldLoadHomePage = true;
    public bool ShouldLoadHomePage
    {
      get 
      {
        return mShouldLoadHomePage;
      }
      set 
      {
        if (value != mShouldLoadHomePage)
          mShouldLoadHomePage = value;
      }
    }
    
    private void LoadStartPage(object sender, EventArgs e)
    {
      if (!mShouldLoadHomePage)
        return;

      int startMode = ServiceManager.Preferences.GetIntPref("browser.homepage.mode");
      switch (startMode) {
      case 0:
        // Don't initialize jack.
        break;
      case 1:
        // Load the homepage
        mWebBrowser.GoHome();
        break;
      case 2:
        // Load the session document.
        mWebBrowser.LoadURL(mSessionURL, false);
        break;
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Location Bar
    protected void OnLocationCommit(Object aSender, LocationBarEventArgs aLbea)
    {
      string url = ServiceManager.Bookmarks.ResolveKeyword(aLbea.Text);
      if (url == "") 
        url = aLbea.Text;

      mUserTyped = false;
      LoadURL(url);
    }

    protected bool mUserTyped = false;
    protected bool mUpdatedURLBar = false;
    protected void OnLocationModified(Object aSender, LocationBarEventArgs aLbea)
    {
      mUserTyped = true;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Window Creation
    public static BrowserWindow OpenBrowser()
    {
      BrowserWindow window = new BrowserWindow();
      window.Show();
      return window;
    }

    public static BrowserWindow OpenBrowserWithURL(String aURL)
    {
      BrowserWindow window = new BrowserWindow(aURL);
      window.Show();
      return window;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Menu Command Handlers

    public void Open()
    {
      OpenDialog dlg = new OpenDialog();
      if (dlg.ShowDialog() == DialogResult.OK)
        mWebBrowser.LoadURL(dlg.URL, false);
    }

    public void SavePageAs()
    {
      SaveFileDialog dlg = new SaveFileDialog();
      dlg.AddExtension = true;
      dlg.InitialDirectory = FileLocator.GetFolderPath(FileLocator.SpecialFolders.ssfPERSONAL); // XXX persist this. 

      // XXX I want to point out that this is a really lame hack. We need
      //     a URL parser (not a URI parser, a URL parser). 
      string name = mTitle.Replace("\"", "'");
      name = name.Replace("*", " ");
      name = name.Replace(":", " ");
      name = name.Replace("?", " ");
      name = name.Replace("<", "(");
      name = name.Replace(">", ")");
      name = name.Replace("\\", "-");
      name = name.Replace("/", "-");
      name = name.Replace("|", "-");
      dlg.FileName = name;

      dlg.Title = "Save Page As...";
      dlg.ValidateNames = true;
      dlg.OverwritePrompt = true;

      WebRequest req = WebRequest.Create(mWebBrowser.URL);
      WebResponse resp = req.GetResponse();
      string contentType = resp.ContentType;
      switch (contentType) 
      {
        case "text/html":
        case "text/xhtml":
          dlg.Filter = "Web Page, complete (*.htm;*.html)|*.htm*|Web Page, HTML only (*.htm;*.html)|*.htm*|Text only (*.txt)|*.txt";
          dlg.DefaultExt = "html";
          break;
        default:
          string extension = MIMEService.GetExtensionForMIMEType(contentType);
          string description = MIMEService.GetDescriptionForMIMEType(contentType);
          description += " (*" + extension + ")";
          dlg.Filter = description + "|*" + extension + "|All Files (*.*)|*.*";
          dlg.DefaultExt = extension.Substring(1, extension.Length-1);
          break;
      }

      dlg.FileOk += new CancelEventHandler(OnSavePageAsOK);
      dlg.ShowDialog();
    }

    public void OnSavePageAsOK(Object sender, CancelEventArgs e)
    {
      if (e.Cancel != true) 
      {
        SaveFileDialog dlg = sender as SaveFileDialog;
        Console.WriteLine("{0}", dlg.FileName);
      }
    }

    public void Quit() 
    {
      ManticoreApp app = ServiceManager.App;
      app.Quit();
    }

    public Object currentLayoutEngine
    {
      get {
        return mWebBrowser.currentLayoutEngine;
      }
    }

    private int previousProgress = 0;
    public void OnProgress(int aProgress, int aProgressMax) 
    {
      if (aProgress > 0 && aProgress > previousProgress) {
        int percentage = (int) (aProgress / aProgressMax);
        String text = percentage + "% complete";
        mProgressMeter.Text = text;
      }
      // XXX we really would rather set this in BeforeNavigate2, but we
      //     can't get that event to fire for some reason. 
      if (mUpdatedURLBar) 
        mUpdatedURLBar = false;
    }

    // XXX we probably will need to extend this to take as a parameter
    //     more data from the NavigateComplete event
    public void OnNavigateComplete2(string aURL)
    {
      if (!mUpdatedURLBar && !mUserTyped) 
      {
        mLocationBar.Text = aURL;
        mUpdatedURLBar = true;
      }
    }

    public void OnTitleChange(String aTitle)
    {
      mTitle = aTitle;
      this.Text = (aTitle == "about:blank") ? "No page to display" : aTitle;
    }

    public void OnStatusTextChange(String aStatusText)
    {
      mStatusPanel.Text = aStatusText;
    }

    /// <summary>
    /// Fired when a |MenuItem| built by the |MenuBuilder| is selected.
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    public void OnMenuCommand(Object sender, EventArgs e)
    {
      ManticoreMenuItem item = sender as ManticoreMenuItem;
      DoCommand(item.Command, item.Data);
    }

    ///////////////////////////////////////////////////////////////////////////
    // IController Implementation

    public void DoCommand(String aCommand)
    {
      DoCommand(aCommand, null);
    }

    public void DoCommand(String aCommand, Object aData) 
    {
      Console.WriteLine(aCommand);
      switch (aCommand) 
      {
        case "file-new-window":
          OpenBrowser();
          break;
        case "file-open":
          Open();
          break;
        case "file-save-as":
          SavePageAs();
          break;
        case "file-save-form":
          TestForm frm = new TestForm();
          frm.Show();
          break;
        case "file-exit":
          Quit();
          break;
        case "view-statusbar":
          if (mStatusBar.Visible)
            mStatusBar.Hide();
          else
            mStatusBar.Show();
          break;
        case "view-go-back":
          mWebBrowser.GoBack();
          break;
        case "view-go-forward":
          mWebBrowser.GoForward();
          break;
        case "view-go-home":
          mWebBrowser.GoHome();
          break;
        case "view-reload":
          mWebBrowser.RefreshPage();
          break;
        case "view-stop":
          mWebBrowser.Stop();
          break;
        case "view-layout-gecko":
          mWebBrowser.SwitchLayoutEngine("gecko");
          break;
        case "view-layout-ie":
          mWebBrowser.SwitchLayoutEngine("trident");
          break;
        case "bookmarks-manage":
          BookmarksWindow bm = new BookmarksWindow();
          bm.Show();
          break;
        case "bookmarks-item":
          String url = ServiceManager.Bookmarks.GetBookmarkAttribute(aData as String, "url");
          LoadURL(url);
          break;
        case "bookmarks-add":
          // XXX need to allow user to customize this. 
          Bookmarks bmks = ServiceManager.Bookmarks;
          String bookmarkID = bmks.CreateBookmark(mTitle, "Bookmarks", -1);
          bmks.SetBookmarkAttribute(bookmarkID, "url", URL);
          break;
        case "bookmarks-file":
          // XXX work on this
          FileBookmark fpWindow = new FileBookmark(URL, mTitle);
          fpWindow.ShowDialog();
          break;
        case "help-about":
          AboutDialog aboutDialog = new AboutDialog(this);
          aboutDialog.ShowDialog();
          break;
        case "tools-options":
          PrefsDialog prefsDialog = new PrefsDialog(this);
          prefsDialog.ShowDialog();
          break;
      }
    }

    public bool SupportsCommand(String aCommand)
    {
      // XXX implement me
      return true;
    }

    public bool IsCommandEnabled(String aCommand)
    {
      // XXX implement me
      return true;
    }
  }

  public class BrowserToolbarBuilder : ToolbarBuilder
  {
    public BrowserToolbarBuilder(String aFile, Form aForm) : base(aFile, aForm)
    {
    }
    
    public override void OnCommand(Object sender, ToolBarButtonClickEventArgs e)
    {
      CommandButtonItem item = e.Button as CommandButtonItem;
      (mForm as BrowserWindow).DoCommand(item.Command);
    }
  }
}


