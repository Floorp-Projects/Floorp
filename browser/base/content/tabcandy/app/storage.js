// Title: storage.js (revision-a)

// ##########
Storage = {
  // ----------
  init: function() {
  },
  
  // ----------
  read: function() {
    var data = {};
    var file = this.getFile();
    if(file.exists()) {
      var fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].  
                              createInstance(Components.interfaces.nsIFileInputStream);  
      var cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].  
                              createInstance(Components.interfaces.nsIConverterInputStream);  
      fstream.init(file, -1, 0, 0);  
      cstream.init(fstream, "UTF-8", 0, 0); // you can use another encoding here if you wish  
      
      let (str = {}) {  
        cstream.readString(-1, str); // read the whole file and put it in str.value  
        if(str.value)
          data = JSON.parse(str.value);  
      }  
      cstream.close(); // this closes fstream  
    }
   
    return data;
  },
  
  // ----------
  write: function(data) {
    var file = this.getFile();
    var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"].  
                             createInstance(Components.interfaces.nsIFileOutputStream);  
    foStream.init(file, 0x02 | 0x08 | 0x20, 0666, 0);   
    var str = JSON.stringify(data);
    foStream.write(str, str.length);
    foStream.close();
  },
  
  // ----------  
  getFile: function() {
    var file = Components.classes["@mozilla.org/file/directory_service;1"].  
      getService(Components.interfaces.nsIProperties).  
      get("ProfD", Components.interfaces.nsIFile);  
      
    file.append('tabcandy');
    if(!file.exists())
      file.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0777);        
      
    file.append(Switch.name + '.json');
    return file;
  }
};

Storage.init();

