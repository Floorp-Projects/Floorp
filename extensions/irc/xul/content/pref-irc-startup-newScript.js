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
        fp.init(window, "Choose a JavaScript script", nsIFilePicker.modeOpen);
        
        fp.appendFilter("JavaScript script (*.js)", "*.js");
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
