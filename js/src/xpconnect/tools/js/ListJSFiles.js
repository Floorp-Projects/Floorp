const nsIFile = Components.interfaces.nsIFile;
var Compiler = new Components.Constructor("xpctools.compiler.1","nsIXPCToolsCompiler");
var c = new Compiler;
ListJSFilesInDir(c.binDir);

function ListJSFilesInDir(dir) {
    if(!dir.isDirectory())
        return;
    var list = dir.directoryEntries;    
    while(list.hasMoreElements()) {
        file = list.getNext().QueryInterface(nsIFile);
        if(file.isDirectory())
            ListJSFilesInDir(file);
        else if(file.leafName.match(/\.js$/i))
            dump(file.path+"\n");
    }
}

