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

namespace Silverstone.Manticore.Browser
{
  using System;
  using System.Collections;
  using System.ComponentModel;
  using System.Drawing;
  using System.Data;
  using System.Windows.Forms;
  
  using Silverstone.Manticore.Core;

  /// <summary>
	/// Summary description for UserControl1.
	/// </summary>
	public class PrefPanel : Panel
	{
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

    /// <summary>
    /// Whether or not this panel has been shown before. We defer
    /// reading preferences and populating UI for preferences panels
    /// that have not yet been shown. 
    /// </summary>
    private bool mGenerated = false;

    /// <summary>
    /// Preferences handle
    /// </summary>
    // LAME - need to use global service thingy. 
    protected internal Preferences mPrefs;

    /// <summary>
    /// Parent window (Preferences dialog)
    /// </summary>
    protected internal Form mParent;

		public PrefPanel(Form aParent, Preferences aPrefs)
		{
			// This call is required by the Windows.Forms Form Designer.
			InitializeComponent();

      // LAME oh so lame.
      mPrefs = aPrefs;
      mParent = aParent;

      // All preferences panels have these properties initially.
      this.Location = new System.Drawing.Point(160, 16);
      this.Size = new System.Drawing.Size(320, 264);
      this.TabIndex = 1;
      this.Visible = false;

      // When the preference panel is shown for the first time, we
      // need to fill its fields from preferences. 
      this.VisibleChanged += new EventHandler(VisibilityChanged);
		}

    /// <summary>
    /// Called when the visibility of the panel changes. 
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    public void VisibilityChanged(Object sender, EventArgs e) 
    {
      if (!mGenerated) {
        // The first time we display the panel, read the values 
        // for UI elements from preferences and fill the controls.
        Load();
        mGenerated = true;
      }
    }

    /// <summary>
    /// Implemented by derived class. Reads preferences for each UI element
    /// the first time this panel is shown, and populates the UI appropriately.
    /// </summary>
    public virtual void Load()
    {
      // Implemented by derived class
    }

    /// <summary>
    /// Implemented by derived class. Takes data from UI elements (which may
    /// be user-manipulated) and saves preferences. 
    /// </summary>
    public virtual void Save() 
    {
      // Implemented by derived class
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

		#region Component Designer generated code
		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			components = new System.ComponentModel.Container();
		}
		#endregion
	}
}
