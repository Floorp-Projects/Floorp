function NewBrowserWindow() {}
function NewBlankPage() {} 
function TemplatePage() {}
function WizardPage() {}
function PageSetup() {}
function PrintPreview() {}
function Print() {}
function Close() {}
function Exit() {}
function CharacterSet() {}
function SetDocumentCharacterSet(aCharset)
{
  var appCore = XPAppCoresManager.Find("MsgAppCore");  
  if (appCore == null) {
    dump("StartUp: Creating AppCore\n");
    appCore = new MsgAppCore();
  }
  if (appCore != null) {
    dump(aCharset);dump("\n");
    appCore.Init("MsgAppCore");
    appCore.SetWindow(window);
    appCore.SetDocumentCharset(aCharset);
  } else {
    dump("MsgAppCore has not been created!\n");
  }
}

function NavigatorWindow() {}
function MessengerWindow() {}
function ComposerWindow() {}
function AIMService() {}
function AddBookmark() {}
function FileBookmark() {}
function EditBookmark() {}
function Newsgroups() {}
function AddressBook() {}
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
