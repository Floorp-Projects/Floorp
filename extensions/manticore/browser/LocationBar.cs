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
  using System.Collections;
  using System.ComponentModel;
  using System.Drawing;
  using System.Data;
  using System.Windows.Forms;

  /// <summary>
	/// Summary description for LocationBar.
	/// </summary>
	public class LocationBar : System.Windows.Forms.UserControl
	{
    private System.Windows.Forms.Label mAddressLabel;
    private System.Windows.Forms.TextBox mAddressBar;
    private System.Windows.Forms.Button mGoButton;
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public LocationBar()
		{
			// This call is required by the Windows.Forms Form Designer.
			InitializeComponent();

			mAddressBar.KeyDown += new KeyEventHandler(OnKeyDown);
      mAddressBar.ModifiedChanged += new EventHandler(OnAddressBarModified);
      mGoButton.Click += new EventHandler(OnGoButtonClick);
		}

    public string Text 
    {
      get 
      {
        return mAddressBar.Text;
      }
      set 
      {
        if (value != mAddressBar.Text)
          mAddressBar.Text = value;
      }
    }

    public delegate void LocationBarEventHandler(Object sender, LocationBarEventArgs e);
    
    public event LocationBarEventHandler LocationBarCommit;

    protected void OnKeyDown(Object aSender, KeyEventArgs aKea)
    {
      if (aKea.KeyCode == Keys.Enter) 
        FireLocationBarCommit();
    }

    protected void OnGoButtonClick(Object aSender, EventArgs aEa)
    {
      FireLocationBarCommit();      
    }

    protected void FireLocationBarCommit()
    {
      if (LocationBarCommit != null) 
      {
        LocationBarEventArgs lbea = new LocationBarEventArgs(mAddressBar.Text);
        LocationBarCommit(this, lbea);
      }
    }

    public event LocationBarEventHandler LocationBarModified;
    protected void OnAddressBarModified(Object aSender, EventArgs aEa)
    {
      if (LocationBarModified != null) 
      {
        LocationBarEventArgs lbea = new LocationBarEventArgs(mAddressBar.Text);
        LocationBarModified(this, lbea);
      }
    }

    protected override void OnPaint(PaintEventArgs aPea)
    {
      Graphics g = aPea.Graphics;
      g.DrawLine(SystemPens.ControlDark, 0, 0, ClientRectangle.Width, 0);
      g.DrawLine(SystemPens.ControlLight, 0, 1, ClientRectangle.Width, 1);
    }

		#region Component Designer generated code
		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
      this.mAddressLabel = new System.Windows.Forms.Label();
      this.mAddressBar = new System.Windows.Forms.TextBox();
      this.mGoButton = new System.Windows.Forms.Button();
      this.SuspendLayout();
      // 
      // mAddressLabel
      // 
      this.mAddressLabel.AutoSize = true;
      this.mAddressLabel.Location = new System.Drawing.Point(8, 6);
      this.mAddressLabel.Name = "mAddressLabel";
      this.mAddressLabel.Size = new System.Drawing.Size(49, 13);
      this.mAddressLabel.TabIndex = 0;
      this.mAddressLabel.Text = "A&ddress:";
      // 
      // mAddressBar
      // 
      this.mAddressBar.Anchor = (System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right);
      this.mAddressBar.Location = new System.Drawing.Point(64, 3);
      this.mAddressBar.Name = "mAddressBar";
      this.mAddressBar.Size = new System.Drawing.Size(336, 20);
      this.mAddressBar.TabIndex = 1;
      this.mAddressBar.Text = "";
      // 
      // mGoButton
      // 
      this.mGoButton.Anchor = (System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right);
      this.mGoButton.FlatStyle = System.Windows.Forms.FlatStyle.System;
      this.mGoButton.Location = new System.Drawing.Point(408, 2);
      this.mGoButton.Name = "mGoButton";
      this.mGoButton.Size = new System.Drawing.Size(32, 23);
      this.mGoButton.TabIndex = 2;
      this.mGoButton.Text = "Go";
      // 
      // LocationBar
      // 
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.mGoButton,
                                                                  this.mAddressBar,
                                                                  this.mAddressLabel});
      this.Name = "LocationBar";
      this.Size = new System.Drawing.Size(448, 25);
      this.ResumeLayout(false);

    }
		#endregion
	}

  public class LocationBarEventArgs : EventArgs
  {
    public LocationBarEventArgs(string aText)
    {
      mText = aText;
    }

    protected string mText;
    public string Text
    {
      get 
      {
        return mText;
      }
    }
  }
}
