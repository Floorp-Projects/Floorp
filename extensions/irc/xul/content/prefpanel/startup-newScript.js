const nsIFilePicker = Components.interfaces.nsIFilePicker;
var gFileBox;
var gData;

function Init() {
    doSetOKCancel(onOK);
    
    gFileBox = document.getElementById("czFileBox");
    
    if ("arguments" in window && window.arguments[0]) {
        gData = window.arguments[0];
    }
    
    gFileBox.focus();
}

function onChooseFile() {
    try {
    	var flClass = Components.classes["@mozilla.org/filepicker;1"];
        var fp = flClass.createInstance(nsIFilePicker);
        fp.init(window, getMsg("file_browse_Script"),
                nsIFilePicker.modeOpen);
        
        fp.appendFilter(getMsg("file_browse_Script_spec"), "*.js");
        fp.appendFilters(nsIFilePicker.filterAll);
        
        if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec 
        	    && fp.fileURL.spec.length > 0)
            gFileBox.value = fp.fileURL.spec;
    }
    catch(ex) {
    }
}

function onOK() {
    gData.url = gFileBox.value;
    gData.ok = true;
    
    return true;
}
