/***************************************************************
* FindFilesDialog ---------------------------------------
*  Controller for dialog.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var dialog;

//////////////////////////////////////////////////

window.addEventListener("load", FindFilesDialog_initialize, false);

function FindFilesDialog_initialize()
{
  dialog = new FindFilesDialog();
  doSetOKCancel(gDoOKFunc);
}

////////////////////////////////////////////////////////////////////////////
//// class FindFilesDialog

function FindFilesDialog()
{
}

FindFilesDialog.prototype = 
{
  browse: function()
  {
    var txf = document.getElementById("txfSearchPath");
    var defaultPath = txf.getAttribute("value");
    var file = FilePickerUtils.pickDir("Select Search Path", defaultPath);
    if (file) {
      txf.setAttribute("value", file.path);
    }
  }
};

////////////////////////////////////////////////////////////////////////////
//// dialogOverlay stuff

function gDoOKFunc() {
  window.close();
  window.arguments[0].processDialog(window);

  return true;
}

