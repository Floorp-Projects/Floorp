
/**
* for(var p in Script.scripts) {
*
*     var script = Script.scripts[p];
*     var handle = script.handle;
*     var base   = script.base;
*     var limit  = base + script.extent;
*
*     print(script+"\n");
*
*     for(var i = base; i < limit; i++) {
*         var pc = jsd.GetClosestPC(handle,i)
*         var hascode = String(pc).length && i == jsd.GetClosestLine(handle,pc);
*         print("line "+i+" "+ (hascode ? "has code" : "has NO code"));
*     }
*     print("...............................\n");
* }
*/

function rlocals()
{
    var retval = "";
    var name = "___UNIQUE_NAME__";
    var fun = ""+
        "var text = \\\"\\\";"+
        "for(var p in ob)"+
        "{"+
        "    if(text != \\\"\\\")"+
        "       text += \\\",\\\";"+
        "    text += p;"+
        "}"+
        "return text;";

    reval(name+" = new Function(\"ob\",\""+fun+"\")");
//    show(name);
    retval = _reval([name+"("+"arguments.callee"+")"]);
    reval("delete "+name);

    return retval;
}

function e(a)
{
    return eval(a);
}