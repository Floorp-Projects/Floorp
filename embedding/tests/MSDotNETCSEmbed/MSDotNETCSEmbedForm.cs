/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
		private Mozilla.Embedding.Gecko myGecko = null;
		private String myURL = null;

		public MSDotNETCSEmbedForm()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
			myGecko = new Gecko();
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
			// 
			// MSDotNETCSEmbedForm
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(744, 374);
			this.Name = "MSDotNETCSEmbed";
			this.Text = "MSDotNETCSEmbed [UNSUPPORTED]";
			this.Resize += new System.EventHandler(this.MSDotNETCSEmbedForm_Resize);
			this.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.MSDotNETCSEmbedForm_KeyPress);
			this.Load += new System.EventHandler(this.MSDotNETCSEmbedForm_Load);

		}
		#endregion

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main() 
		{
			// initialize gecko
			Gecko.InitEmbedding();

			Application.Run(new MSDotNETCSEmbedForm());

			// terminate gecko
			Gecko.TermEmbedding();
		}

		private void MSDotNETCSEmbedForm_Load(object sender, System.EventArgs e)
		{
			myURL = "www.mozilla.org";
			myGecko.OpenURL(this.Handle, myURL);
			this.Text = "MSDotNETCSEmbed [UNSUPPORTED] - " + myURL;
			myURL = "";
		}

		private void MSDotNETCSEmbedForm_Resize(object sender, System.EventArgs e)
		{
			myGecko.Resize(this.Handle);		
		}

		private void MSDotNETCSEmbedForm_KeyPress(object sender, System.Windows.Forms.KeyPressEventArgs e)
		{
			switch (e.KeyChar)
			{
				case '\r':
					myGecko.OpenURL(this.Handle, myURL);
					myURL = "";
					break;

				default:
					myURL += e.KeyChar;
					this.Text = "MSDotNETCSEmbed [UNSUPPORTED] - " + myURL;
					break;
			}
		}
	}
}
