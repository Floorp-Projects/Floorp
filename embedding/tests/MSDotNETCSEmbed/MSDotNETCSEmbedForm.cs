/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
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
