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

using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;

namespace Silverstone.Manticore.bookmarks
{
	/// <summary>
	/// Summary description for Test.
	/// </summary>
	public class Test : System.Windows.Forms.Form
	{
    private System.Windows.Forms.StatusBar statusBar1;
    private System.Windows.Forms.TreeView treeView1;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public Test()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
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
      this.treeView1 = new System.Windows.Forms.TreeView();
      this.SuspendLayout();
      // 
      // statusBar1
      // 
      this.statusBar1.Location = new System.Drawing.Point(0, 253);
      this.statusBar1.Name = "statusBar1";
      this.statusBar1.TabIndex = 0;
      this.statusBar1.Text = "statusBar1";
      // 
      // treeView1
      // 
      this.treeView1.Dock = System.Windows.Forms.DockStyle.Fill;
      this.treeView1.ImageIndex = -1;
      this.treeView1.Name = "treeView1";
      this.treeView1.SelectedImageIndex = -1;
      this.treeView1.TabIndex = 1;
      this.treeView1.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.treeView1_AfterSelect);
      // 
      // Test
      // 
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.ClientSize = new System.Drawing.Size(292, 273);
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.treeView1,
                                                                  this.statusBar1});
      this.Name = "Test";
      this.Text = "Test";
      this.ResumeLayout(false);

    }
		#endregion

    private void treeView1_AfterSelect(object sender, System.Windows.Forms.TreeViewEventArgs e)
    {

    }
	}
}
