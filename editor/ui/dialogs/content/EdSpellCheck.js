// OnOK(), Undo(), and Cancel() are in EdDialogCommon.js
// applyChanges() must be implemented here

var edAppCore;
var toolkitCore;
var misspelledWord;

// dialog initialization code
function Startup()
{
  dump("Doing Startup...\n");
  toolkitCore = XPAppCoresManager.Find("ToolkitCore");
  if (!toolkitCore) {
    toolkitCore = new ToolkitCore();
    if (toolkitCore)
      toolkitCore.Init("ToolkitCore");
  }
  if(!toolkitCore) {
    dump("toolkitCore not found!!! And we can't close the dialog!\n");
  }

  // NEVER create an edAppCore here - we must find parent editor's

  // temporary while this window is opend with ShowWindowWithArgs
  dump("Getting parent appcore\n");
  var editorName = document.getElementById("args").getAttribute("value");
  dump("Got editorAppCore called " + editorName + "\n");
  edAppCore = XPAppCoresManager.Find(editorName);  
  if(!edAppCore || !toolkitCore) {
    dump("EditoredAppCore not found!!!\n");
    toolkitCore.CloseWindow(window);
  }
  dump("EditoredAppCore found for Spell Checker dialog\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  if (!dialog)
  {
    dump("Failed to create dialog object!!!\n");
    toolkitCore.CloseWindow(window);
  }

  dialog.wordInput = document.getElementById("Word");
  dialog.suggestedList = document.getElementById("SuggestedList");
  dialog.languageList = document.getElementById("");
  if (!dialog.wordInput ||
      !dialog.suggestedList  ||
      !dialog.languageList )
  {
    dump("Not all dialog controls were found!!!\n");
  }
  // NOTE: We shouldn't have been created if there was no misspelled word
  
  misspelledWord = edAppCore.getFirstMisspelledWord();
  
  if (misspelledWord != "") {
    dump("First misspelled word = "+misspelledWord+"\n");
    // Put word in input field
    dialog.wordInput.value = misspelledWord;
    FillSuggestedList();
  } else {
    dump("No misspelled word found\n");
  }

  dialog.suggestedList.focus();  
}
function NextWord()
{
  misspelledWord = edAppCore.getNextMisspelledWord();
  dialog.wordInput.value = misspelledWord;
  if (misspelledWord == "") {
    dump("FINISHED SPELL CHECKING\n");
    FillSuggestedList("(no suggestions)");
  } else {
    FillSuggestedList();
  }
}

function CheckWord()
{
  dump("SpellCheck: CheckWord\n");
  word = dialog.wordInput.value;
  dump("Word in edit field = "+word+"\n");
  if (word != "") {
    isMisspelled = edAppCore.checkCurrentWord(word);
    misspelledWord = word;
  }
}

function SelectSuggestedWord()
{
  dump("SpellCheck: SelectSuggestedWord\n");
  dialog.wordInput.value = dialog.suggestedList.value;
}

function Ignore()
{
  dump("SpellCheck: Ignore\n");
  NextWord();
}

function IgnoreAll()
{
  dump("SpellCheck: IgnoreAll\n");
  if (misspelledWord != "") {
    edAppCore.ignoreWordAllOccurrences(misspelledWord);
  }
  NextWord();
}

function Replace()
{
  dump("SpellCheck: Replace\n");
  newWord = dialog.wordInput.value;
  if (misspelledWord != "" && misspelledWord != newWord) {
    isMisspelled = edAppCore.replaceWord(misspelledWord, newWord, false);
  }
  NextWord();
}

function ReplaceAll()
{
  dump("SpellCheck: ReplaceAll\n");
  newWord = dialog.wordInput.value;
  if (misspelledWord != "" && misspelledWord != newWord) {
    isMisspelled = edAppCore.replaceWord(misspelledWord, newWord, true);
  }
  NextWord();
}

function AddToDictionary()
{
  dump("SpellCheck: AddToDictionary\n");
  if (misspelledWord != "") {
    edAppCore.addWordToDictionary(misspelledWord);
  }
}

function EditDictionary()
{
  dump("SpellCheck: EditDictionary TODO: IMPLEMENT DIALOG\n");
}

function SelectLanguage()
{
  // A bug in combobox prevents this from working
  dump("SpellCheck: SelectLanguage.\n");
}

function Close()
{
  dump("SpellCheck: Close (can't close yet - CRASHES!)\n");
  edAppCore.closeSpellChecking();
}

function Help()
{
  dump("SpellCheck: Help me Rhonda, help, help me Rhonda\n");
}

function FillSuggestedList(firstWord)
{
  list = dialog.suggestedList;

  // Clear the current contents of the list
  while (list.hasChildNodes()) {
    dump("Removing list option node, value = "+list.lastChild.value+"\n");
    list.removeChild(list.lastChild);
  }
  // We may have the initial word
  if (firstWord && firstWord != "") {
    dump("First Word = "+firstWord+"\n");
    AppendSuggestedList(firstWord);
  }

  // Get suggested words until an empty string is returned
  do {
    word = edAppCore.getSuggestedWord();
    dump("Suggested Word = "+word+"\n");
    if (word != "") {
      AppendSuggestedList(word);
    }
  } while (word != "");
}

function AppendSuggestedList(word)
{
  optionNode = document.createElement("option");
  if (optionNode) {
    dump("Appending word to an OPTION node: "+word+"\n");
    optionNode.setAttribute("value", word);
    dialog.suggestedList.appendChild(optionNode);    
  } else {
    dump("Failed to create OPTION node: "+word+"\n");
  }
}
