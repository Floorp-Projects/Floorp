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
 *   Ben Goodger <ben@netscape.com> (Original Author)
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

namespace Silverstone.Manticore.Bookmarks
{
  using System;
  using System.Drawing;
  using System.Collections;
  using System.ComponentModel;
  using System.Windows.Forms;

  using Silverstone.Manticore.App;
  using Silverstone.Manticore.Core;
  using Silverstone.Manticore.Toolkit;
  using Silverstone.Manticore.Bookmarks;
  using Silverstone.Manticore.Browser;

  /// <summary>
	/// Summary description for BookmarksWindow.
	/// </summary>
	public class BookmarksWindow : ManticoreWindow
	{
    private StatusBar mStatusBar;
    private StatusBarPanel mStatusBarPanel;
    private BookmarksTreeView mBookmarksTree;

    private BaseTreeBuilder mBuilder = null;

		public BookmarksWindow()
		{
      mType = "BookmarksWindow";
      
      Init();
    }

    protected void Init()
    {
      // Set up UI
      InitializeComponent();

      base.Init();
    }
    
    #region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
      mStatusBar = new StatusBar();
      mStatusBarPanel = new StatusBarPanel();
      ((ISupportInitialize)mStatusBarPanel).BeginInit();
      SuspendLayout();

      mStatusBar.Name = "statusBar";
      mStatusBar.Panels.Add(mStatusBarPanel);
      mStatusBar.TabIndex = 0;
      mStatusBar.Text = "";
      mStatusBarPanel.Text = "";
      Controls.Add(mStatusBar);

      mBookmarksTree = new BookmarksTreeView("BookmarksRoot");
      mBookmarksTree.Dock = DockStyle.Fill;
      mBookmarksTree.ImageIndex = -1;
      mBookmarksTree.Name = "bookmarksTree";
      mBookmarksTree.SelectedImageIndex = -1;
      mBookmarksTree.Size = new System.Drawing.Size(584, 409);
      mBookmarksTree.TabIndex = 1;

      mBookmarksTree.AfterSelect += new TreeViewEventHandler(OnTreeAfterSelect);
      mBookmarksTree.DoubleClick += new EventHandler(OnTreeDoubleClick);

      Controls.Add(mBookmarksTree);
      mBookmarksTree.Build();

      
      AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      ClientSize = new System.Drawing.Size(584, 429);
      Name = "BookmarksWindow";
      Text = Environment.UserName + "'s Bookmarks";
      ((ISupportInitialize)(mStatusBarPanel)).EndInit();
      ResumeLayout(false);
    }
		#endregion

    protected void OnTreeAfterSelect(Object sender, TreeViewEventArgs e)
    {
      ManticoreTreeNode node = e.Node as ManticoreTreeNode;
      Bookmarks bmks = ServiceManager.Bookmarks;
      String bookmarkURL = bmks.GetBookmarkAttribute(node.Data as String, "url");
      mStatusBar.Text = bookmarkURL;
    }

    protected void OnTreeDoubleClick(Object sender, EventArgs e)
    {
      ManticoreTreeNode node = mBookmarksTree.SelectedNode as ManticoreTreeNode;
      Bookmarks bmks = ServiceManager.Bookmarks;
      String bookmarkURL = bmks.GetBookmarkAttribute(node.Data as String, "url");
      if (bookmarkURL != "") 
      {
        WindowMediator wm = ServiceManager.WindowMediator;
        BrowserWindow window = wm.GetMostRecentWindow("BrowserWindow") as BrowserWindow;
        if (window != null)
          window.LoadURL(bookmarkURL);
      }
    }

	}
}
