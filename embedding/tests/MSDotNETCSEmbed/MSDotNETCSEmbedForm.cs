/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
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

using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Data;

// Gecko namespace
using Mozilla.Embedding;

namespace MSDotNETCSEmbed
{
	/// <summary>
	/// Summary description for Form1.
	/// </summary>
	public class MSDotNETCSEmbedForm : System.Windows.Forms.Form
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
		private Mozilla.Embedding.Gecko gecko1;
		private System.Windows.Forms.Button goButton;
		private System.Windows.Forms.TextBox urlBar;

		public MSDotNETCSEmbedForm()
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
				if (components != null) 
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
			this.gecko1 = new Mozilla.Embedding.Gecko();
			this.goButton = new System.Windows.Forms.Button();
			this.urlBar = new System.Windows.Forms.TextBox();
			this.SuspendLayout();
			// 
			// gecko1
			// 
			this.gecko1.Location = new System.Drawing.Point(0, 40);
			this.gecko1.Name = "gecko1";
			this.gecko1.Size = new System.Drawing.Size(664, 392);
			this.gecko1.TabIndex = 0;
			// 
			// goButton
			// 
			this.goButton.Location = new System.Drawing.Point(600, 8);
			this.goButton.Name = "goButton";
			this.goButton.Size = new System.Drawing.Size(56, 24);
			this.goButton.TabIndex = 1;
			this.goButton.Text = "Go";
			this.goButton.Click += new System.EventHandler(this.goButton_Click);
			// 
			// urlBar
			// 
			this.urlBar.Location = new System.Drawing.Point(8, 10);
			this.urlBar.Name = "urlBar";
			this.urlBar.Size = new System.Drawing.Size(576, 20);
			this.urlBar.TabIndex = 2;
			this.urlBar.Text = "";
			this.urlBar.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.urlBar_KeyPress);
			// 
			// MSDotNETCSEmbedForm
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(664, 429);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.urlBar,
																		  this.goButton,
																		  this.gecko1});
			this.Name = "MSDotNETCSEmbedForm";
			this.Text = "MSDotNETCSEmbed [UNSUPPORTED]";
			this.Resize += new System.EventHandler(this.MSDotNETCSEmbedForm_Resize);
			this.Load += new System.EventHandler(this.MSDotNETCSEmbedForm_Load);
			this.ResumeLayout(false);

		}
		#endregion

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main() 
		{
			Application.Run(new MSDotNETCSEmbedForm());

			// terminate gecko
			Gecko.TermEmbedding();
		}

		private void MSDotNETCSEmbedForm_Load(object sender, System.EventArgs e)
		{
			urlBar.Text = "http://www.mozilla.org";
			gecko1.OpenURL(urlBar.Text);
			this.Text = "MSDotNETCSEmbed [UNSUPPORTED] - " + urlBar.Text;
		}

		private void MSDotNETCSEmbedForm_Resize(object sender, System.EventArgs e)
		{
			gecko1.Size =
				new Size(ClientSize.Width,
						 ClientSize.Height - gecko1.Location.Y);
		}

		private void urlBar_KeyPress(object sender, System.Windows.Forms.KeyPressEventArgs e)
		{
			switch (e.KeyChar)
			{
				case '\r':
					gecko1.OpenURL(urlBar.Text);
					e.Handled = true;
					break;
			}
		}

		private void goButton_Click(object sender, System.EventArgs e)
		{
			gecko1.OpenURL(urlBar.Text);
		}
	}
}
