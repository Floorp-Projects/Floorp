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
 *  Ben Goodger <ben@netscape.com> (Original Author)
 *
 */

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

  /// <summary>
	/// Summary description for BookmarksWindow.
	/// </summary>
	public class BookmarksWindow : ManticoreWindow
	{
    private System.Windows.Forms.StatusBar statusBar1;
    private System.Windows.Forms.StatusBarPanel statusBarPanel1;
    private BookmarksTreeView mBookmarksTree;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

    private BaseTreeBuilder mBuilder = null;

		public BookmarksWindow()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

      // 
      // mBookmarksTree
      // 
      mBookmarksTree = new BookmarksTreeView("BookmarksRoot");
      mBookmarksTree.Dock = System.Windows.Forms.DockStyle.Fill;
      mBookmarksTree.ImageIndex = -1;
      mBookmarksTree.Name = "bookmarksTree";
      mBookmarksTree.SelectedImageIndex = -1;
      mBookmarksTree.Size = new System.Drawing.Size(584, 409);
      mBookmarksTree.TabIndex = 1;

      mBookmarksTree.AfterSelect += new TreeViewEventHandler(OnTreeAfterSelect);
      mBookmarksTree.DoubleClick += new EventHandler(OnTreeDoubleClick);

      Controls.Add(mBookmarksTree);
      mBookmarksTree.Build();
    }

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if(components != null)
				{
					components.Dispose();
				}
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
      this.statusBar1 = new System.Windows.Forms.StatusBar();
      this.statusBarPanel1 = new System.Windows.Forms.StatusBarPanel();
      ((System.ComponentModel.ISupportInitialize)(this.statusBarPanel1)).BeginInit();
      this.SuspendLayout();
      // 
      // statusBar1
      // 
      this.statusBar1.Location = new System.Drawing.Point(0, 409);
      this.statusBar1.Name = "statusBar1";
      this.statusBar1.Panels.AddRange(new System.Windows.Forms.StatusBarPanel[] {
                                                                                  this.statusBarPanel1});
      this.statusBar1.Size = new System.Drawing.Size(584, 20);
      this.statusBar1.TabIndex = 0;
      this.statusBar1.Text = "statusBar1";
      // 
      // statusBarPanel1
      // 
      this.statusBarPanel1.Text = "statusBarPanel1";
      // 
      // BookmarksWindow
      // 
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.ClientSize = new System.Drawing.Size(584, 429);
      this.Controls.AddRange(new System.Windows.Forms.Control[] {this.statusBar1});
      this.Name = "BookmarksWindow";
      this.Text = "Bookmarks for %USER%";
      ((System.ComponentModel.ISupportInitialize)(this.statusBarPanel1)).EndInit();
      this.ResumeLayout(false);

    }
		#endregion

    protected void OnTreeAfterSelect(Object sender, TreeViewEventArgs e)
    {
      ManticoreTreeNode node = e.Node as ManticoreTreeNode;
      Bookmarks bmks = ServiceManager.Bookmarks;
      String bookmarkURL = bmks.GetBookmarkAttribute(node.Data as String, "url");
      statusBar1.Text = bookmarkURL;
    }

    protected void OnTreeDoubleClick(Object sender, EventArgs e)
    {
      ManticoreTreeNode node = mBookmarksTree.SelectedNode as ManticoreTreeNode;
      Bookmarks bmks = ServiceManager.Bookmarks;
      String bookmarkURL = bmks.GetBookmarkAttribute(node.Data as String, "url");
      ManticoreApp.MostRecentBrowserWindow.LoadURL(bookmarkURL);
    }

	}
}
