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

using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;

namespace Silverstone.Manticore.Toolkit
{
	/// <summary>
	/// Summary description for TestForm.
	/// </summary>
	public class TestForm : System.Windows.Forms.Form
	{
    private System.Windows.Forms.StatusBar statusBar1;
    private System.Windows.Forms.ToolBar toolBar1;
    private System.Windows.Forms.TreeView treeView1;
    private System.Windows.Forms.TextBox textBox1;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public TestForm()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

      /*
      StripBar bar = new StripBar();
      bar.Dock = DockStyle.Top;
      // bar.BackImage = Image.FromFile("resources\\manticore.png");
      this.Controls.Add(bar);      

      StripBand bnd = new StripBand();
      bar.AddBand(bnd);

      bnd = new StripBand();
      bar.AddBand(bnd);
*/
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
      this.toolBar1 = new System.Windows.Forms.ToolBar();
      this.treeView1 = new System.Windows.Forms.TreeView();
      this.textBox1 = new System.Windows.Forms.TextBox();
      this.SuspendLayout();
      // 
      // statusBar1
      // 
      this.statusBar1.Location = new System.Drawing.Point(0, 312);
      this.statusBar1.Name = "statusBar1";
      this.statusBar1.Size = new System.Drawing.Size(416, 22);
      this.statusBar1.TabIndex = 0;
      this.statusBar1.Text = "statusBar1";
      // 
      // toolBar1
      // 
      this.toolBar1.DropDownArrows = true;
      this.toolBar1.Name = "toolBar1";
      this.toolBar1.ShowToolTips = true;
      this.toolBar1.Size = new System.Drawing.Size(416, 39);
      this.toolBar1.TabIndex = 1;
      // 
      // treeView1
      // 
      this.treeView1.Anchor = (((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
        | System.Windows.Forms.AnchorStyles.Left) 
        | System.Windows.Forms.AnchorStyles.Right);
      this.treeView1.ImageIndex = -1;
      this.treeView1.Location = new System.Drawing.Point(0, 64);
      this.treeView1.Name = "treeView1";
      this.treeView1.SelectedImageIndex = -1;
      this.treeView1.Size = new System.Drawing.Size(416, 248);
      this.treeView1.TabIndex = 3;
      // 
      // textBox1
      // 
      this.textBox1.Dock = System.Windows.Forms.DockStyle.Top;
      this.textBox1.Location = new System.Drawing.Point(0, 39);
      this.textBox1.Name = "textBox1";
      this.textBox1.Size = new System.Drawing.Size(416, 20);
      this.textBox1.TabIndex = 4;
      this.textBox1.Text = "textBox1";
      // 
      // TestForm
      // 
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.ClientSize = new System.Drawing.Size(416, 334);
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.textBox1,
                                                                  this.treeView1,
                                                                  this.toolBar1,
                                                                  this.statusBar1});
      this.Name = "TestForm";
      this.Text = "TestForm";
      this.Load += new System.EventHandler(this.TestForm_Load);
      this.ResumeLayout(false);

    }
		#endregion

    private void TestForm_Load(object sender, System.EventArgs e)
    {
    
    }
	}
}
