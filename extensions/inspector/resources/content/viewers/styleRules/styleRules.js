/***************************************************************
* StyleRulesViewer --------------------------------------------
*  The viewer for CSS style rules that apply to a DOM element.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/util.js
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

var kCSSRuleDataSourceIID = "@mozilla.org/rdf/datasource;1?name=Inspector_CSSRules";
var kCSSDecDataSourceIID  = "@mozilla.org/rdf/datasource;1?name=Inspector_CSSDec";

//////////////////////////////////////////////////

window.addEventListener("load", StyleRulesViewer_initialize, false);

function StyleRulesViewer_initialize()
{
  viewer = new StyleRulesViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class StyleRulesViewer

function StyleRulesViewer() // implements inIViewer
{
  this.mURL = window.location;
  this.mRuleTree = document.getElementById("trStyleRules");
  this.mDecTree = document.getElementById("trStyleDec");

  // create the rules datasource
  var ds = XPCU.createInstance(kCSSRuleDataSourceIID, "nsICSSRuleDataSource");
  this.mRuleDS = ds;
  this.mRuleTree.database.AddDataSource(ds);

  // create the declaration datasource
  var ds = XPCU.createInstance(kCSSDecDataSourceIID, "nsICSSDecDataSource");
  this.mDecDS = ds;
  this.mDecTree.database.AddDataSource(ds);
}

StyleRulesViewer.prototype = 
{

  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mRuleDS: null,
  mDecDS: null,
  mViewee: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "styleRules" },
  get pane() { return this.mPane },
  
  get viewee() { return this.mViewee },
  set viewee(aObject)
  {
    this.mViewee = aObject;
    this.mRuleDS.element = aObject;
    this.mRuleTree.builder.rebuild();
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.onViewerConstructed(this);
  },

  destroy: function()
  {
    this.mRuleTree.database.RemoveDataSource(this.mRuleDS);
    this.mRuleDS = null;

    this.mDecTree.database.RemoveDataSource(this.mDecDS);
    this.mDecDS = null;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  //////// rule contextual commands

  cmdNewRule: function()
  {
  },
  
  cmdToggleSelectedRule: function()
  {
  },

  cmdDeleteSelectedRule: function()
  {
  },

  cmdOpenSelectedFileInEditor: function()
  {
    var item = this.mRuleTree.selectedItems[0];
    if (item)
    {
      var path = null;

      var url = InsUtil.getDSProperty(this.mRuleDS, item.id, "FileURL");
      if (url.substr(0, 6) == "chrome") {
        // This is a tricky situation.  Finding the source of a chrome file means knowing where
        // your build tree is located, and where in the tree the file came from.  Since files
        // from the build tree get copied into chrome:// from unpredictable places, the best way
        // to find them is manually.
        // 
        // Solution to implement: Have the user pick the file location the first time it is opened.
        //  After that, keep the result in some sort of registry to look up next time it is opened.
        //  Allow this registry to be edited via preferences.
      } else if (url.substr(0, 4) == "file") {
        path = url;
      }

      if (path) {
        try {
          var exe = XPCU.createInstance("@mozilla.org/file/local;1", "nsILocalFile");
          exe.initWithPath("c:\\windows\\notepad.exe");
          exe = exe.nsIFile;
          exe.spawn([url], 1);
        } catch (ex) {
          alert("Unable to open editor.");
        }
      }
    }
  },

  //////// property contextual commands
  
  cmdNewProperty: function()
  {
    var propname = prompt("Enter the property name:", "");
    var propval = prompt("Enter the property value:", "");
    if (propname && propval){
      this.mDecDS.setCSSProperty(propname, propval, "");
      this.mDecTree.builder.rebuild();
    }
  },
  
  cmdEditSelectedProperty: function()
  {
    var item = this.mDecTree.selectedItems[0];
    var cell = item.getElementsByAttribute("ins-type", "value")[0];
    this.mDecTree.startEdit(cell);
  },

  cmdDeleteSelectedProperty: function()
  {
    var propname = this.getSelectedPropertyValue("PropertyName");
    this.mDecDS.removeCSSProperty(propname);
    this.mDecTree.builder.rebuild();
  },

  cmdSetSelectedPropertyValue: function(aItem, aNewValue)
  {
    // XXX can't depend on selection, use the item object passed into function instead
    var propname = this.getSelectedPropertyValue("PropertyName");
    var priority = this.getSelectedPropertyValue("PropertyPriority");
    this.mDecDS.setCSSProperty(propname, aNewValue, priority);
  },

  cmdToggleSelectedImportant: function()
  {
    var priority = this.getSelectedPropertyValue("PropertyPriority");
    priority = priority == "!important" ? "!blah" : "!important";
    var propname = this.getSelectedPropertyValue("PropertyName");
    var propval = this.getSelectedPropertyValue("PropertyValue");

    // sadly, we must remove it first or it don't work
    this.mDecDS.removeCSSProperty(propname); 
    this.mDecDS.setCSSProperty(propname, propval, priority);
    this.mDecTree.builder.rebuild();
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  getSelectedPropertyValue: function(aProperty)
  {
    var item = this.mDecTree.selectedItems[0];
    return InsUtil.getDSProperty(this.mDecDS, item.id, aProperty);
  },

  onCreateRulePopup: function()
  {
  },

  onRuleSelect: function()
  {
    var tree = this.mRuleTree;
    var item = tree.selectedItems[0];

    var res = gRDF.GetResource(item.id);
    this.mDecDS.refResource = res;
    tree = this.mDecTree;
    tree.builder.rebuild();
  }

};

