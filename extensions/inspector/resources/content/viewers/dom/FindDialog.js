/***************************************************************
* FindDialog ---------------------------------------------------
*  Controls the dialog box used for searching the DOM.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global variables /////////////////////

var dialog;

//////////// global constants ////////////////////

//////////////////////////////////////////////////

window.addEventListener("load", FindDialog_initialize, false);

function FindDialog_initialize()
{
  dialog = new FindDialog();
}

////////////////////////////////////////////////////////////////////////////
//// class FindDialog

function FindDialog()
{
  this.toggleType(window.arguments[0]);
  this.mOpener = window.arguments[1];
  document.getElementById("tfText1").focus();
}

FindDialog.prototype = 
{
  mType: null,

  ////////// Properties


  ////////// Methods

  doFind: function()
  {
    var el = document.getElementById("tfText1");
    if (this.mType == "id") {
      this.mOpener.doFindElementById(el.value);
    } else if (this.mType == "tag") {
      this.mOpener.doFindElementsByTagName(el.value);
    } else if (this.mType == "attr") {
      var el2 = document.getElementById("tfText2");
      this.mOpener.doFindElementsByAttr(el.value, el2.value);
    }
    this.close();
  },

  close: function()
  {
    window.close();
  },

  toggleType: function(aType)
  {
    this.mType = aType;

    if (aType == "id") {
      this.showDirection(false);
      this.setLabel1("Find Id");
      this.setLabel2(null);
    } else if (aType == "tag") {
      this.showDirection(true);
      this.setLabel1("Find Tag");
      this.setLabel2(null);
    } else if (aType == "attr") {
      this.showDirection(true);
      this.setLabel1("Attribute");
      this.setLabel2("Value");
    }

    var rd = document.getElementById("rdType_"+aType.toLowerCase());
    if (rd) {
      var rg = document.getElementById("rgType");
      rg.selectedItem = rd;
    }

  },

  setLabel1: function(aVal)
  {
    this.setLabel(1, aVal);
  },

  setLabel2: function(aVal)
  {
    this.setLabel(2, aVal);
  },

  setLabel: function(aIdx, aVal)
  {
    var el = document.getElementById("txText"+aIdx);
    var row = document.getElementById("rwRow"+aIdx);
    if (aVal) {
      el.setAttribute("value", aVal + ":");
      row.setAttribute("hide", "false");
    } else {
      row.setAttribute("hide", "true");
    }
  },

  setDirection: function(aMode)
  {
    var rd = document.getElementById("rdDir_"+aMode.toLowerCase());
    if (rd) {
      var rg = document.getElementById("rgDirection");
      rg.selectedItem = rd;
    }
  },

  showDirection: function(aTruth)
  {
    //document.getElementById("tbxDirection").setAttribute("hide", !aTruth);
  }

};

