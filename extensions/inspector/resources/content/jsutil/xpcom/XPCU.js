
var XPCU = 
{ 
  getService: function(aURL, aInterface)
  {
    try {
      return Components.classes[aURL].getService(Components.interfaces[aInterface]);
    } catch (ex) {
      dump("Error getting service: " + aURL + ", " + aInterface + "\n" + ex);
    }
  },

  createInstance: function (aURL, aInterface)
  {
    try {
      return Components.classes[aURL].createInstance(Components.interfaces[aInterface]);
    } catch (ex) {
      dump("Error creating instance: " + aURL + ", " + aInterface + "\n" + ex);
    }
  },

  QI: function(aEl, aIName)
  {
    try {
      return aEl.QueryInterface(Components.interfaces[aIName]);
    } catch (ex) {
      dump("Error in QI: " + aIName + "\n" + ex);
    }
  }

};