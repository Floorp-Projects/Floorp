const nsIFile = Components.interfaces.nsIFile;
var Compiler = new Components.Constructor("xpctools.compiler.1",
                                          "nsIXPCToolsCompiler");
var c = new Compiler;

dump("\n");
dump("Compiling all .js files under "+c.binDir.path+"\n");
dump("Will report any compile-time errors...\n\n");
dump( "compiled "+CompileJSFilesInDir(c.binDir)+" files\n\n");

function CompileJSFilesInDir(dir) {
    var count = 0;
    if(!dir.isDirectory())
        return;
    var list = dir.directoryEntries;    
    while(list.hasMoreElements()) {
        file = list.getNext().QueryInterface(nsIFile);
        if(file.isDirectory())
            count += CompileJSFilesInDir(file);
        else if(file.leafName.match(/\.js$/i)) {
            // dump("compiling... "+file.path+"\n");
            c.CompileFile(file, false);
            ++count;
        }
    }
    return count;
}

