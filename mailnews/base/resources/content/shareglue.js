function NewBrowserWindow() {}
function NewBlankPage() {} 
function TemplatePage() {}
function WizardPage() {}
function PageSetup() {}
function PrintPreview() {}
function Print() {}

function OnLoad()
{
	messenger.SetWindow(window);
}

function OnUnload()
{
	dump("\nOnUnload from XUL\nClean up ...\n");
	messenger.OnUnload();
}

function Close() 
{
	dump("\nClose from XUL\nDo something...\n");
	messenger.Close();
}

function Exit()
{
  dump("\nExit from XUL\n");
  messenger.Exit();
}

function CharacterSet(){}

function MessengerSetDefaultCharacterSet(aCharset)
{
    dump(aCharset);dump("\n");
    messenger.SetDocumentCharset(aCharset);
	var folderResource = GetSelectedFolderResource();
	SetFolderCharset(folderResource, aCharset);
    MsgReload();
}

function NavigatorWindow()
{
	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore)
	{
		toolkitCore = new ToolkitCore();
		if (toolkitCore)
		{
			toolkitCore.Init("ToolkitCore");
		}
    }

    if (toolkitCore)
	{
      toolkitCore.ShowWindow("chrome://navigator/content/",
                             window);
    }


}
function MessengerWindow() {}
function ComposerWindow() {}
function AIMService() {}
function AddBookmark() {}
function FileBookmark() {}
function EditBookmark() {}
function Newsgroups() {}
function AddressBook() 
{
	var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
	if (!toolkitCore)
	{
		toolkitCore = new ToolkitCore();
		if (toolkitCore)
		{
			toolkitCore.Init("ToolkitCore");
		}
    }

    if (toolkitCore)
	{
      toolkitCore.ShowWindow("chrome://addressbook/content/",
                             window);
    }

}

function History() {}
function SecurityInfo() {}
function MessengerCenter() {}
function JavaConsole() {}
function PageService() {}
function MailAccount() {}
function MaillingList() {}
function FolderPermission() {}
function ManageNewsgroup() {}
function WindowList() {}
function Help() {}
function About() {}
