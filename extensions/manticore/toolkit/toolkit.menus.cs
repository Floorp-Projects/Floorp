
namespace Silverstone.Manticore.Toolkit.Menus
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;

  using System.Collections;

  using System.IO;
  using System.Xml;

  public class MenuBuilder
  {
    protected internal String menuDefinitionFile;

    public MainMenu mainMenu;
    public Hashtable items;

    public MenuBuilder(String file)
    {
      menuDefinitionFile = file;
      mainMenu = new MainMenu();
      items = new Hashtable();
    }

    public void Build()
    {
      XmlTextReader reader;
      reader = new XmlTextReader(menuDefinitionFile);
      
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();

      Recurse(reader, mainMenu);
    }

    protected void Recurse(XmlTextReader reader, Menu root)
    {
      String inner = reader.ReadInnerXml();
    
      NameTable nt = new NameTable();
      XmlNamespaceManager nsmgr = new XmlNamespaceManager(nt);
      XmlParserContext ctxt = new XmlParserContext(null, nsmgr, null, XmlSpace.None);
      XmlTextReader reader2 = new XmlTextReader(inner, XmlNodeType.Element, ctxt);

      MenuItem menuitem;

      while (reader2.Read()) {
        if (reader2.NodeType == XmlNodeType.Element) {
          switch (reader2.LocalName) {
          case "menu":
            // Menuitem. Find the name, accesskey, command and id strings
            String[] values = new String[3] {"", "", ""};
            String[] names = new String[3] {"label", "accesskey", "command"};
            for (Byte i = 0; i < names.Length; ++i) {
              if (reader2.MoveToAttribute(names[i]) &&
                  reader2.ReadAttributeValue())
                values[i] = reader2.Value; // XXX need to handle entities
              reader2.MoveToElement();
            }

            // Handle Accesskey
            int idx = values[0].ToLower().IndexOf(values[1].ToLower());
            if (idx != -1)
              values[0] = values[0].Insert(idx, "&");
            else 
              values[0] += " (&" + values[1].ToUpper() + ")";

            // Create menu item and attach an event handler
            menuitem = new CommandMenuItem(values[0], 
                                           new EventHandler(OnCommand),
                                           values[2]);
            if (values[2] != "")
              items.Add(values[2], menuitem);
            root.MenuItems.Add(menuitem);
            Recurse(reader2, menuitem);
            break;
          case "menuseparator":
            menuitem = new MenuItem("-");
            root.MenuItems.Add(menuitem);
            break;
          }
        }
      }
    }

    public virtual void OnCommand(Object sender, EventArgs e)
    { 
      // Implement in derived classes
    }
  }

  public class CommandMenuItem : MenuItem
  {
    private string command;
    public string Command 
    {
      get {
        return command;
      }
    }

    public CommandMenuItem(String label, 
                           EventHandler handler, 
                           String cmd) : base(label, handler)
    {
      command = cmd;
    }
  }
}


