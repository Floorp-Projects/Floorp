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

  using Silverstone.Manticore.Toolkit;
  using Silverstone.Manticore.Core;

  /// <summary>
	/// Summary description for FileBookmark.
	/// </summary>
	public class FileBookmark : System.Windows.Forms.Form
	{
    private Label label1;
    private Label label2;
    private Label label3;
    private BookmarksTreeView mFolderTree;
    private Button mOKButton;
    private Button mCancelButton;
    private Button mUseDefaultButton;
    private Button mNewFolderButton;
    private TextBox mLocationField;
    private TextBox mNameField;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public FileBookmark(String aURL, String aTitle)
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

      mNameField.Text = aTitle;
      mLocationField.Text = aURL;

      //
      // Button XP Theme support. 
      //
      mOKButton.FlatStyle = FlatStyle.System;
      mCancelButton.FlatStyle = FlatStyle.System;
      mUseDefaultButton.FlatStyle = FlatStyle.System;
      mNewFolderButton.FlatStyle = FlatStyle.System;

      // 
      // folderTree
      // 
      mFolderTree = new BookmarksTreeView("BookmarksRoot");
      mFolderTree.Anchor = (((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
        | System.Windows.Forms.AnchorStyles.Left) 
        | System.Windows.Forms.AnchorStyles.Right);
      mFolderTree.ImageIndex = -1;
      mFolderTree.Location = new System.Drawing.Point(64, 88);
      mFolderTree.Name = "folderTree";
      mFolderTree.SelectedImageIndex = -1;
      mFolderTree.Size = new System.Drawing.Size(208, 144);
      mFolderTree.TabIndex = 5;

      // Only show folders in this |TreeView|
      mFolderTree.AddCriteria(new String[] {"container", "true"});

      Controls.Add(mFolderTree);
      mFolderTree.Build();

      Bitmap bmp = new Bitmap(@"resources\bookmark.png");
      bmp.MakeTransparent(ColorTranslator.FromOle(0x00FF00));
//      this.Icon = new Icon(
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose(bool aDisposing)
		{
			if (aDisposing && components != null)
        components.Dispose();
			base.Dispose(aDisposing);
		}

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
      this.mLocationField = new System.Windows.Forms.TextBox();
      this.mCancelButton = new System.Windows.Forms.Button();
      this.mNameField = new System.Windows.Forms.TextBox();
      this.mNewFolderButton = new System.Windows.Forms.Button();
      this.mUseDefaultButton = new System.Windows.Forms.Button();
      this.mOKButton = new System.Windows.Forms.Button();
      this.label1 = new System.Windows.Forms.Label();
      this.label2 = new System.Windows.Forms.Label();
      this.label3 = new System.Windows.Forms.Label();
      this.SuspendLayout();
      // 
      // mLocationField
      // 
      this.mLocationField.Anchor = ((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
        | System.Windows.Forms.AnchorStyles.Right);
      this.mLocationField.Location = new System.Drawing.Point(64, 48);
      this.mLocationField.Name = "mLocationField";
      this.mLocationField.Size = new System.Drawing.Size(296, 20);
      this.mLocationField.TabIndex = 3;
      this.mLocationField.Text = "";
      // 
      // mCancelButton
      // 
      this.mCancelButton.Anchor = (System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right);
      this.mCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
      this.mCancelButton.Location = new System.Drawing.Point(288, 248);
      this.mCancelButton.Name = "mCancelButton";
      this.mCancelButton.TabIndex = 9;
      this.mCancelButton.Text = "Cancel";
      // 
      // mNameField
      // 
      this.mNameField.Anchor = ((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
        | System.Windows.Forms.AnchorStyles.Right);
      this.mNameField.Location = new System.Drawing.Point(64, 16);
      this.mNameField.Name = "mNameField";
      this.mNameField.Size = new System.Drawing.Size(296, 20);
      this.mNameField.TabIndex = 1;
      this.mNameField.Text = "";
      // 
      // mNewFolderButton
      // 
      this.mNewFolderButton.Anchor = (System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right);
      this.mNewFolderButton.Location = new System.Drawing.Point(280, 88);
      this.mNewFolderButton.Name = "mNewFolderButton";
      this.mNewFolderButton.Size = new System.Drawing.Size(80, 23);
      this.mNewFolderButton.TabIndex = 6;
      this.mNewFolderButton.Text = "Ne&w Folder...";
      this.mNewFolderButton.Click += new System.EventHandler(this.mNewFolderButton_Click);
      // 
      // mUseDefaultButton
      // 
      this.mUseDefaultButton.Anchor = (System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right);
      this.mUseDefaultButton.Location = new System.Drawing.Point(280, 120);
      this.mUseDefaultButton.Name = "mUseDefaultButton";
      this.mUseDefaultButton.Size = new System.Drawing.Size(80, 23);
      this.mUseDefaultButton.TabIndex = 7;
      this.mUseDefaultButton.Text = "Use &Default";
      // 
      // mOKButton
      // 
      this.mOKButton.Anchor = (System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right);
      this.mOKButton.Location = new System.Drawing.Point(208, 248);
      this.mOKButton.Name = "mOKButton";
      this.mOKButton.TabIndex = 8;
      this.mOKButton.Text = "OK";
      this.mOKButton.Click += new System.EventHandler(this.mOKButton_Click);
      // 
      // label1
      // 
      this.label1.AutoSize = true;
      this.label1.Location = new System.Drawing.Point(8, 16);
      this.label1.Name = "label1";
      this.label1.Size = new System.Drawing.Size(38, 13);
      this.label1.TabIndex = 0;
      this.label1.Text = "&Name:";
      // 
      // label2
      // 
      this.label2.AutoSize = true;
      this.label2.Location = new System.Drawing.Point(8, 48);
      this.label2.Name = "label2";
      this.label2.Size = new System.Drawing.Size(50, 13);
      this.label2.TabIndex = 2;
      this.label2.Text = "&Location:";
      // 
      // label3
      // 
      this.label3.AutoSize = true;
      this.label3.Location = new System.Drawing.Point(8, 88);
      this.label3.Name = "label3";
      this.label3.Size = new System.Drawing.Size(53, 13);
      this.label3.TabIndex = 4;
      this.label3.Text = "Create in:";
      // 
      // FileBookmark
      // 
      this.AcceptButton = this.mOKButton;
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.CancelButton = this.mCancelButton;
      this.ClientSize = new System.Drawing.Size(376, 277);
      this.ControlBox = false;
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.mCancelButton,
                                                                  this.mOKButton,
                                                                  this.mUseDefaultButton,
                                                                  this.mNewFolderButton,
                                                                  this.label3,
                                                                  this.label2,
                                                                  this.label1,
                                                                  this.mLocationField,
                                                                  this.mNameField});
      this.MinimumSize = new System.Drawing.Size(384, 300);
      this.Name = "FileBookmark";
      this.ShowInTaskbar = false;
      this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Show;
      this.Text = "File Bookmark";
      this.ResumeLayout(false);

    }
		#endregion

    private void mNewFolderButton_Click(object sender, System.EventArgs e)
    {
      mFolderTree.NewFolder();
    }

    private void mOKButton_Click(object sender, System.EventArgs e)
    {
      String destinationFolder = "Bookmarks"; // XXX Parameterize this
      ManticoreTreeNode node = mFolderTree.SelectedNode as ManticoreTreeNode;
      if (node != null) 
        destinationFolder = node.Data as String;
      Bookmarks bmks = ServiceManager.Bookmarks;
      String bookmarkID = bmks.CreateBookmark(mNameField.Text, destinationFolder, -1);
      bmks.SetBookmarkAttribute(bookmarkID, "url", mLocationField.Text);
      Close();
    }
	}
}
