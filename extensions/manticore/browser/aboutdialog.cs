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

namespace Silverstone.Manticore.AboutDialog
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;

  public class AboutDialog : System.Windows.Forms.Form 
  {
    private System.ComponentModel.Container components;

    private Form parentWindow;

    public AboutDialog(Form parent)
    {
      parentWindow = parent;

      InitializeComponent();

      // Show the about dialog modally.
      this.ShowDialog();

    }

    public override void Dispose()
    {
      base.Dispose();
      components.Dispose();
    }

    private void InitializeComponent()
    {
      this.components = new System.ComponentModel.Container();

      this.Width = 253;
      this.Height = 195;

      this.Left = parentWindow.Left + ((parentWindow.Width / 2) - (this.Width / 2));
      this.Top = parentWindow.Top + ((parentWindow.Height / 2) - (this.Height / 2));
      
      // Borderless dialog
      this.FormBorderStyle = FormBorderStyle.None;

      this.BackgroundImage = Image.FromFile("resources\\manticore.png");
      
      this.Text = "About Manticore"; // XXX localize

      this.Click += new EventHandler(this.CloseAboutDialog);
    }

    public void CloseAboutDialog(Object sender, EventArgs e)
    {
      this.Close();
    }
  }
}


