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
  using System.Drawing;
  using System.Collections;
  using System.ComponentModel;
  using System.Windows.Forms;

  using System.IO;
  using System.Xml;

  using Silverstone.Manticore.Toolkit;
  using Silverstone.Manticore.Core;

	/// <summary>
	/// Summary description for Form1.
	/// </summary>
	public class PrefsDialog : ManticoreDialog
	{
    private System.Windows.Forms.TreeView treeView1;
    private System.Windows.Forms.Button cancelButton;
    private System.Windows.Forms.Button okButton;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

    private Hashtable mNodes = null;
    private Hashtable mPanels = null;
    private PrefPanel mCurrentPanel = null;

    // LAME find a way to do global application service. 
    private Preferences mPrefs = null;

		public PrefsDialog(Form aOpener) : base(aOpener)
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

      okButton.Click += new EventHandler(OnOK);

      mNodes = new Hashtable();
      mPanels = new Hashtable();

      BrowserWindow window = mOpener as BrowserWindow;
      mPrefs = ServiceManager.Preferences;

      //
      // Initialize all the preference panels.
      //
      InitializePanels();

      // Add select handler for tree view so that we can
      // switch panels.
      treeView1.AfterSelect += new TreeViewEventHandler(OnTreeSelect);

      //
      // Initialize the category list. 
      //
      InitializeCategoryList();

      // XXX - eventually we want to remember user state. This will do
      //       for now. 
      treeView1.ExpandAll();
    }

    public void OnOK(object sender, EventArgs e)
    {
      // Call |Save| on each preferences panel...
      IEnumerator panels = mPanels.Values.GetEnumerator();
      while (panels.MoveNext()) {
        PrefPanel currPanel = panels.Current as PrefPanel;
        currPanel.Save();
      }

      // ... then flush preferences to disk for safe keepin'.
      // XXX not just yet. 
      // mPrefs.FlushUserPreferences();
    }

    public void OnTreeSelect(Object sender, TreeViewEventArgs e) 
    {
      TreeNode selectedNode = e.Node;
      String panelID = mNodes[selectedNode.GetHashCode()] as String;
      PrefPanel newPanel = mPanels[panelID] as PrefPanel;
      if (mCurrentPanel != null) 
        mCurrentPanel.Visible = false;
      if (newPanel != null) {
        newPanel.Visible = true;
        mCurrentPanel = newPanel;
      }
      else {
        if (selectedNode.FirstNode != null) {
          TreeView treeview = sender as TreeView;
          treeview.SelectedNode = selectedNode.FirstNode;
        }
      }
    }

    /// <summary>
    /// Create all the preferences panels.
    /// </summary>
    public void InitializePanels() 
    {
      BrowserDisplayPanel bdp = new BrowserDisplayPanel(this, mOpener, mPrefs);
      mPanels.Add("browser-display", bdp);
      this.Controls.Add(bdp);
    }

    /// <summary>
    /// Load the category list from XML and select the initial
    /// panel.
    /// </summary>
    private void InitializeCategoryList() 
    {
      mNodes = new Hashtable();

      XmlTextReader reader;
      reader = new XmlTextReader("browser\\PrefPanels.xml");
      
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();

      CatListRecurse(reader, treeView1);
    }

    private void CatListRecurse(XmlTextReader aReader, Object aRootNode) 
    {
      String inner = aReader.ReadInnerXml();
    
      NameTable nt = new NameTable();
      XmlNamespaceManager nsmgr = new XmlNamespaceManager(nt);
      XmlParserContext ctxt = new XmlParserContext(null, nsmgr, null, XmlSpace.None);
      XmlTextReader reader2 = new XmlTextReader(inner, XmlNodeType.Element, ctxt);

      TreeNode node;

      while (reader2.Read()) {
        if (reader2.NodeType == XmlNodeType.Element) {
          switch (reader2.LocalName) {
          case "panel":
            // Tree node with children. Retrieve label and id. Label is 
            // used for visual presentation, id is hashed against node
            // and is used as a key when looking for which panel to 
            // load. 
            String[] values = new String[2] {"", ""};
            String[] names = new String[2] {"label", "id"};
            for (int i = 0; i < names.Length; ++i) {
              if (reader2.MoveToAttribute(names[i]) &&
                reader2.ReadAttributeValue())
                values[i] = reader2.Value; 
              reader2.MoveToElement();
            }

            node = new TreeNode(values[0], 0, 0);
            if (aRootNode is TreeView) {
              TreeView rootView = aRootNode as TreeView;
              rootView.Nodes.Add(node);
            }
            else if (aRootNode is TreeNode) {
              TreeNode rootNode = aRootNode as TreeNode;
              rootNode.Nodes.Add(node);
            }

            mNodes.Add(node.GetHashCode(), values[1]);
            CatListRecurse(reader2, node);
            break;
          }
        }
      }
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
      this.cancelButton = new System.Windows.Forms.Button();
      this.treeView1 = new System.Windows.Forms.TreeView();
      this.okButton = new System.Windows.Forms.Button();
      this.SuspendLayout();
      // 
      // cancelButton
      // 
      this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
      this.cancelButton.Location = new System.Drawing.Point(408, 296);
      this.cancelButton.Name = "cancelButton";
      this.cancelButton.TabIndex = 2;
      this.cancelButton.Text = "Cancel";
      this.cancelButton.FlatStyle = FlatStyle.System;
      // 
      // treeView1
      // 
      this.treeView1.ImageIndex = -1;
      this.treeView1.Location = new System.Drawing.Point(16, 16);
      this.treeView1.Name = "treeView1";
      this.treeView1.SelectedImageIndex = -1;
      this.treeView1.Size = new System.Drawing.Size(136, 264);
      this.treeView1.TabIndex = 0;
      // 
      // okButton
      // 
      this.okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
      this.okButton.Location = new System.Drawing.Point(328, 296);
      this.okButton.Name = "okButton";
      this.okButton.TabIndex = 3;
      this.okButton.Text = "OK";
      this.okButton.FlatStyle = FlatStyle.System;
      // 
      // PrefsDialog
      // 
      this.AcceptButton = this.okButton;
      this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
      this.CancelButton = this.cancelButton;
      this.ClientSize = new System.Drawing.Size(496, 328);
      this.ControlBox = false;
      this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                  this.okButton,
                                                                  this.cancelButton,
                                                                  this.treeView1});
      this.HelpButton = true;
      this.Name = "PrefsDialog";
      this.ShowInTaskbar = false;
      this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
      this.Text = "Options";
      this.ResumeLayout(false);

    }
		#endregion
	}
}
